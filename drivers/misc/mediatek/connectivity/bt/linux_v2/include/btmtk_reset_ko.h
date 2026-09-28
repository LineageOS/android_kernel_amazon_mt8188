/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __BTMTK_RESET_KO_H__
#define __BTMTK_RESET_KO_H__

#include <linux/types.h>

enum ENUM_RST_MODULE_TYPE_T {
	RST_MODULE_BT = 0,
	RST_MODULE_WIFI,
	RST_MODULE_MAX
};

enum ENUM_RST_MODULE_STATE_TYPE_T {
	RST_MODULE_STATE_PRERESET = 0,
	RST_MODULE_STATE_KO_INSMOD,
	RST_MODULE_STATE_KO_RMMOD,
	RST_MODULE_STATE_PROBE_START,
	RST_MODULE_STATE_PROBE_DONE,
	RST_MODULE_STATE_DUMP_START,
	RST_MODULE_STATE_DUMP_END,
	RST_MODULE_STATE_MAX
};

enum ENUM_RST_MODULE_RET_TYPE_T {
	RST_MODULE_RET_SUCCESS = 0,
	RST_MODULE_RET_FAIL,
	RST_MODULE_RET_MAX
};

struct WIFI_NOTIFY_DESC {
	bool (*BtNotifyWifiSubResetStep1)(u_int8_t);
};

struct BT_NOTIFY_DESC {
	int32_t (*WifiNotifyBtSubResetStep1)(int32_t);
	int32_t (*WifiNotifyReadBtMcuPc)(uint32_t *);
	int32_t (*WifiNotifyReadWifiMcuPc)(uint8_t, uint32_t *);
};

enum ENUM_RST_MODULE_RET_TYPE_T rstNotifyWholeChipRstStatus(
				enum ENUM_RST_MODULE_TYPE_T module,
				enum ENUM_RST_MODULE_STATE_TYPE_T status,
				void *data);

void register_bt_notify_callback(struct BT_NOTIFY_DESC *bt_notify_cb);
void unregister_bt_notify_callback(void);
struct WIFI_NOTIFY_DESC *get_wifi_notify_callback(void);

enum ENUM_COMM_CORE_STATUS_CMD_T {
	COMM_CORE_STATUS_PROBE_START = 0,
	COMM_CORE_STATUS_PROBE_SUCCESS,
	COMM_CORE_STATUS_PROBE_FAIL,
	COMM_CORE_STATUS_REMOVE_START,
	COMM_CORE_STATUS_REMOVE_SUCCESS,
	COMM_CORE_STATUS_REMOVE_FAIL,
	COMM_CORE_STATUS_MAX
};

struct sdio_driver;

void comm_core_register_sdio_driver(enum ENUM_RST_MODULE_TYPE_T module,
				    struct sdio_driver *drv);
void comm_core_unregister_sdio_driver(enum ENUM_RST_MODULE_TYPE_T module);
void NotifyCommCoreStatusCmd(enum ENUM_RST_MODULE_TYPE_T module,
			     enum ENUM_COMM_CORE_STATUS_CMD_T cmd, void *data);

#endif /* __BTMTK_RESET_KO_H__ */
