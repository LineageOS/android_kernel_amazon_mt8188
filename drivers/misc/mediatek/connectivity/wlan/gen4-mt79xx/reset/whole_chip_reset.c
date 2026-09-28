// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#define pr_fmt(fmt) "[reset] " fmt

#include <linux/bits.h>
#include <linux/kernel.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>

#include "comm_core.h"
#include "whole_chip_reset.h"

#define RST_STATUS_BT_PROBE_DONE	BIT(0)
#define RST_STATUS_WIFI_PROBE_DONE	BIT(1)
#define RST_STATUS_BT_DUMP		BIT(0)
#define RST_STATUS_WIFI_DUMP		BIT(1)

#define RST_WAKELOCK_TIMEOUT_MS		2500

enum rst_state {
	RST_STATE_UNKNOWN = 0,
	RST_STATE_IDLE,
	RST_STATE_START,
	RST_STATE_GOING,
	RST_STATE_DUMP,
	RST_STATE_MAX
};

static const char *const rst_state_names[RST_STATE_MAX] = {
	"RST_STATE_UNKNOWN",
	"RST_STATE_IDLE",
	"RST_STATE_START",
	"RST_STATE_GOING",
	"RST_STATE_DUMP",
};

static const char *const rst_module_names[RST_MODULE_MAX] = {
	"BT",
	"WIFI",
};

static const char *const rst_status_names[RST_MODULE_STATE_MAX] = {
	"RST_MODULE_STATE_PRERESET",
	"RST_MODULE_STATE_KO_INSMOD",
	"RST_MODULE_STATE_KO_RMMOD",
	"RST_MODULE_STATE_PROBE_START",
	"RST_MODULE_STATE_PROBE_DONE",
	"RST_MODULE_STATE_DUMP_START",
	"RST_MODULE_STATE_DUMP_END",
};

static DEFINE_MUTEX(rst_lock);
static enum rst_state rst_state = RST_STATE_UNKNOWN;
static unsigned int rst_probe_flags;
static unsigned int rst_dump_flags;
static bool rst_bt_probed;
static bool rst_wifi_probed;
static struct mmc_host *rst_host;
static struct wakeup_source *rst_ws;
static bool rst_comm_core;

static struct WIFI_NOTIFY_DESC wifi_notify_desc;
static struct BT_NOTIFY_DESC bt_notify_desc;

static void rst_set_state(enum rst_state state)
{
	lockdep_assert_held(&rst_lock);

	pr_info("%s: current_state[%s], next_state[%s]\n", __func__,
		rst_state_names[rst_state], rst_state_names[state]);
	rst_state = state;
}

static void rst_toggle_host(struct mmc_host *host)
{
	if (!host) {
		pr_err("no SDIO host to reset\n");
		return;
	}

	host->rescan_entered = 0;
	pr_info("[SER][L0] mmc_remove_host\n");
	mmc_remove_host(host);
	pr_info("[SER][L0] mmc_add_host\n");
	mmc_add_host(host);
}

static void rst_do_reset(void)
{
	struct mmc_host *host;

	mutex_lock(&rst_lock);
	host = rst_host;
	mutex_unlock(&rst_lock);

	pr_info("toggle reset pin\n");
	pm_wakeup_ws_event(rst_ws, RST_WAKELOCK_TIMEOUT_MS, false);

	/* COMM-CORE power cycles the chip, Wi-Fi and BT report back */
	if (rst_comm_core) {
		if (comm_core_trigger_rst())
			pr_err("COMM-CORE rst fail\n");
		return;
	}

	rst_toggle_host(host);
}

enum ENUM_RST_MODULE_RET_TYPE_T rstNotifyWholeChipRstStatus(
				enum ENUM_RST_MODULE_TYPE_T module,
				enum ENUM_RST_MODULE_STATE_TYPE_T status,
				void *data)
{
	enum ENUM_RST_MODULE_RET_TYPE_T ret = RST_MODULE_RET_SUCCESS;
	struct sdio_func *func = data;
	bool do_reset = false;

	if ((unsigned int)module >= RST_MODULE_MAX ||
	    (unsigned int)status >= RST_MODULE_STATE_MAX)
		return RST_MODULE_RET_FAIL;

	pr_info("%s: module[%s], status[%s]\n", __func__,
		rst_module_names[module], rst_status_names[status]);

	mutex_lock(&rst_lock);

	if (func && func->card)
		rst_host = func->card->host;

	switch (status) {
	case RST_MODULE_STATE_PRERESET:
		if (rst_state == RST_STATE_IDLE) {
			rst_probe_flags = 0;
			rst_dump_flags = 0;
			rst_set_state(RST_STATE_START);
			rst_set_state(RST_STATE_GOING);
			do_reset = true;
		} else if (!rst_bt_probed || !rst_wifi_probed) {
			pr_info("WiFi or BT not probe start\n");
		}
		break;

	case RST_MODULE_STATE_KO_INSMOD:
		if (rst_state == RST_STATE_UNKNOWN)
			rst_set_state(RST_STATE_IDLE);
		break;

	case RST_MODULE_STATE_KO_RMMOD:
		if (module == RST_MODULE_BT)
			rst_bt_probed = false;
		else
			rst_wifi_probed = false;

		if (rst_bt_probed && rst_wifi_probed)
			rst_set_state(RST_STATE_IDLE);
		break;

	case RST_MODULE_STATE_PROBE_START:
		if (module == RST_MODULE_BT)
			rst_bt_probed = true;
		else
			rst_wifi_probed = true;
		break;

	case RST_MODULE_STATE_PROBE_DONE:
		if (module == RST_MODULE_BT)
			rst_probe_flags |= RST_STATUS_BT_PROBE_DONE;
		else
			rst_probe_flags |= RST_STATUS_WIFI_PROBE_DONE;

		/* Idle again once every driver that started probing is done */
		if ((!rst_bt_probed ||
		     (rst_probe_flags & RST_STATUS_BT_PROBE_DONE)) &&
		    (!rst_wifi_probed ||
		     (rst_probe_flags & RST_STATUS_WIFI_PROBE_DONE))) {
			rst_set_state(RST_STATE_IDLE);
			__pm_relax(rst_ws);
		}
		break;

	case RST_MODULE_STATE_DUMP_START:
		if (rst_state == RST_STATE_START ||
		    rst_state == RST_STATE_GOING) {
			pr_err("resetting, dump is not allowed\n");
			ret = RST_MODULE_RET_FAIL;
		} else if (module == RST_MODULE_BT &&
			   (rst_probe_flags & RST_STATUS_BT_PROBE_DONE) &&
			   !(rst_dump_flags & RST_STATUS_BT_DUMP)) {
			rst_dump_flags |= RST_STATUS_BT_DUMP;
			rst_probe_flags |= RST_STATUS_WIFI_PROBE_DONE;
			rst_set_state(RST_STATE_DUMP);
		} else if (module == RST_MODULE_WIFI &&
			   (rst_probe_flags & RST_STATUS_WIFI_PROBE_DONE) &&
			   !(rst_dump_flags & RST_STATUS_WIFI_DUMP)) {
			rst_dump_flags |= RST_STATUS_WIFI_DUMP;
			rst_set_state(RST_STATE_DUMP);
		}
		break;

	case RST_MODULE_STATE_DUMP_END:
		if (module == RST_MODULE_BT)
			rst_dump_flags &= ~RST_STATUS_BT_DUMP;
		else
			rst_dump_flags &= ~RST_STATUS_WIFI_DUMP;

		if (!rst_dump_flags && rst_state == RST_STATE_DUMP)
			rst_set_state(RST_STATE_IDLE);
		break;

	default:
		break;
	}

	mutex_unlock(&rst_lock);

	/* The drivers report back from their remove and probe paths */
	if (do_reset)
		rst_do_reset();

	return ret;
}
EXPORT_SYMBOL(rstNotifyWholeChipRstStatus);

void register_bt_notify_callback(struct BT_NOTIFY_DESC *bt_notify_cb)
{
	bt_notify_desc = *bt_notify_cb;
}
EXPORT_SYMBOL(register_bt_notify_callback);

void unregister_bt_notify_callback(void)
{
	memset(&bt_notify_desc, 0, sizeof(bt_notify_desc));
}
EXPORT_SYMBOL(unregister_bt_notify_callback);

struct BT_NOTIFY_DESC *get_bt_notify_callback(void)
{
	return &bt_notify_desc;
}
EXPORT_SYMBOL(get_bt_notify_callback);

void register_wifi_notify_callback(struct WIFI_NOTIFY_DESC *wifi_notify_cb)
{
	wifi_notify_desc = *wifi_notify_cb;
}
EXPORT_SYMBOL(register_wifi_notify_callback);

void unregister_wifi_notify_callback(void)
{
	memset(&wifi_notify_desc, 0, sizeof(wifi_notify_desc));
}
EXPORT_SYMBOL(unregister_wifi_notify_callback);

struct WIFI_NOTIFY_DESC *get_wifi_notify_callback(void)
{
	return &wifi_notify_desc;
}
EXPORT_SYMBOL(get_wifi_notify_callback);

static int __init whole_chip_reset_init(void)
{
	int ret;

	rst_ws = wakeup_source_register(NULL, "WHOLE_CHIP_RESET");
	if (!rst_ws)
		return -ENOMEM;

	/* Without COMM-CORE, resets fall back to re-adding the SDIO host */
	ret = comm_core_init();
	if (ret)
		pr_warn("WARNING: comm_core init fail: %d\n", ret);
	else
		rst_comm_core = true;

	return 0;
}

static void __exit whole_chip_reset_exit(void)
{
	if (rst_comm_core)
		comm_core_exit();
	wakeup_source_unregister(rst_ws);
}

module_init(whole_chip_reset_init);
module_exit(whole_chip_reset_exit);

MODULE_DESCRIPTION("MediaTek SDIO combo chip whole-chip reset");
MODULE_LICENSE("Dual BSD/GPL");
