// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) "[COMM-CORE] " fmt

#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_func.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
#include "mtk_disp_notify.h"
#endif

#include "comm_core.h"
#include "whole_chip_reset.h"

#define CC_VCN33_DELAY_MS	6
#define CC_START_TIMEOUT_MS	3000
#define CC_DONE_TIMEOUT_MS	4000
#define CC_CMD_TIMEOUT_MS	4000
#define CC_WAKELOCK_MS		2500

#define CC_DBG_LEVEL_STATE	2
#define CC_DBG_LEVEL_INFO	3

#define cc_state(fmt, ...)						\
	do {								\
		if (cc.dbg_level >= CC_DBG_LEVEL_STATE)			\
			pr_info(fmt, ##__VA_ARGS__);			\
	} while (0)

#define cc_info(fmt, ...)						\
	do {								\
		if (cc.dbg_level >= CC_DBG_LEVEL_INFO)			\
			pr_info(fmt, ##__VA_ARGS__);			\
	} while (0)

enum cc_module {
	CC_MODULE_BT = RST_MODULE_BT,
	CC_MODULE_WIFI = RST_MODULE_WIFI,
	CC_MODULE_SCREEN,
	CC_MODULE_RST,
	CC_MODULE_MAX
};

#define CC_FUNC_NUM	RST_MODULE_MAX
#define CC_TARGET_NUM	(CC_MODULE_SCREEN + 1)

enum cc_op_id {
	CC_OP_ON = 0,
	CC_OP_OFF,
};

enum cc_calc_result {
	CC_CALC_UNKNOWN = 0,
	CC_CALC_BUFFER,
	CC_CALC_RETURN,
	CC_CALC_HANDLE,
};

enum cc_block {
	CC_BLOCK_RST_LEFT = 0,
	CC_BLOCK_RST,
	CC_BLOCK_RST_RIGHT,
	CC_BLOCK_MAX
};

enum cc_fsm_state {
	CC_FSM_IDLE = 0,
	CC_FSM_POWER_ON_START,
	CC_FSM_POWER_ON_GOING,
	CC_FSM_POWER_ON_END,
	CC_FSM_POWER_OFF_START,
	CC_FSM_POWER_OFF_GOING,
	CC_FSM_POWER_OFF_END,
	CC_FSM_MAX
};

enum cc_func_state {
	CC_FUNC_IDLE = 0,
	CC_FUNC_PROBE_START,
	CC_FUNC_PROBE_GOING,
	CC_FUNC_PROBE_DONE,
	CC_FUNC_REMOVE_START,
	CC_FUNC_REMOVE_GOING,
	CC_FUNC_REMOVE_DONE,
	CC_FUNC_STATE_MAX
};

static const char *const cc_module_names[CC_MODULE_MAX] = {
	"BT", "WIFI", "SCREEN", "RST",
};

static const char *const cc_block_names[CC_BLOCK_MAX] = {
	"RstLeft", "Rst", "RstRight",
};

static const char *const cc_calc_names[] = {
	"UNKNOWN", "BUFFER", "RETURN", "HANDLE",
};

static const char *const cc_status_names[COMM_CORE_STATUS_MAX] = {
	"PROBE_START", "PROBE_SUCCESS", "PROBE_FAIL",
	"REMOVE_START", "REMOVE_SUCCESS", "REMOVE_FAIL",
};

static const char *const cc_fsm_names[CC_FSM_MAX] = {
	"IDLE", "POWER_ON_START", "POWER_ON_GOING", "POWER_ON_END",
	"POWER_OFF_START", "POWER_OFF_GOING", "POWER_OFF_END",
};

static const char *const cc_func_state_names[CC_FUNC_STATE_MAX] = {
	"IDLE", "PROBE_START", "PROBE_GOING", "PROBE_DONE",
	"REMOVE_START", "REMOVE_GOING", "REMOVE_DONE",
};

#define cc_on_off(on)	((on) ? "ON" : "OFF")

struct cc_op {
	struct list_head node;
	struct kref ref;
	struct completion done;
	unsigned int seq;
	enum cc_module module;
	enum cc_op_id id;
	enum cc_calc_result calc;
	int result;
};

struct cc_event {
	struct list_head node;
	enum ENUM_RST_MODULE_TYPE_T module;
	enum ENUM_COMM_CORE_STATUS_CMD_T status;
	struct mmc_host *host;
};

struct cc_block_info {
	struct cc_op *first;
	struct cc_op *last;
	struct cc_op *last_cmd[CC_TARGET_NUM];
	bool target_on[CC_TARGET_NUM];
	bool power_off;
};

struct cc_calc_info {
	struct cc_block_info block[CC_BLOCK_MAX];
	bool current_on[CC_TARGET_NUM];
	bool power_off;
	bool handle;
	enum cc_op_id handle_id;
};

struct cc_node {
	const char *name;
	const struct file_operations *fops;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct device *dev;
};

static struct {
	bool ready;
	unsigned int dbg_level;

	int vcn33_gpio;
	unsigned int vcn33_delay_ms;
	struct regulator *vcn18;
	unsigned int vcn18_enabled;

	struct kthread_worker *worker;
	struct kthread_work work;

	/* Requests, protected by op_lock */
	struct mutex op_lock;
	struct list_head active_ops;
	struct list_head wait_ops;
	unsigned int seq;

	spinlock_t event_lock;
	struct list_head events;

	struct mutex fsm_lock;
	enum cc_fsm_state fsm_state;
	enum cc_func_state func_state[CC_FUNC_NUM];
	bool func_on[CC_FUNC_NUM];
	bool screen_on;
	bool power_off;
	struct mmc_host *host;
	struct timer_list timer[CC_FUNC_NUM];
	bool timer_armed[CC_FUNC_NUM];
	unsigned long timer_fired;
	struct wakeup_source *ws;
	struct cc_calc_info calc;

	struct mutex drv_lock;
	struct sdio_driver *drv[CC_FUNC_NUM];

	struct work_struct screen_work;
	bool screen_unblank;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
	struct notifier_block disp_nb;
#endif
} cc;

static void cc_fsm_steps(enum cc_fsm_state next);
static void cc_complete_single_op(struct cc_op *op);

static bool cc_on_worker(void)
{
	return cc.worker && current == cc.worker->task;
}

static void cc_kick(void)
{
	kthread_queue_work(cc.worker, &cc.work);
}

static int cc_status_print(char *buf, size_t size)
{
	int len, i;

	len = scnprintf(buf, size, "[COMM_CORE_FSM] Current State: [%s]\n",
			cc_fsm_names[cc.fsm_state]);
	for (i = 0; i < CC_FUNC_NUM; i++) {
		len += scnprintf(buf + len, size - len,
			"[COMM_CORE_%s_MODULE_STATE] Current State: [%s]\n",
			cc_module_names[i],
			cc_func_state_names[cc.func_state[i]]);
		len += scnprintf(buf + len, size - len,
			"[COMM_CORE_%s_FUNC_STATE] Current State: [%s]\n",
			cc_module_names[i], cc_on_off(cc.func_on[i]));
	}
	len += scnprintf(buf + len, size - len,
			 "[COMM_CORE_FSM] Current Screen State: [%s]\n",
			 cc_on_off(cc.screen_on));

	return len;
}

static void cc_dump_status(void)
{
	int i;

	cc_state("[COMM_CORE_FSM] Current State: [%s]\n",
		 cc_fsm_names[cc.fsm_state]);
	for (i = 0; i < CC_FUNC_NUM; i++) {
		cc_state("[COMM_CORE_%s_MODULE_STATE] Current State: [%s]\n",
			 cc_module_names[i],
			 cc_func_state_names[cc.func_state[i]]);
		cc_state("[COMM_CORE_%s_FUNC_STATE] Current State: [%s]\n",
			 cc_module_names[i], cc_on_off(cc.func_on[i]));
	}
	cc_state("[COMM_CORE_FSM] Current Screen State: [%s]\n",
		 cc_on_off(cc.screen_on));
}

static void cc_dump_op(struct cc_op *op)
{
	cc_state("seq[%u] module[%s] op_id[%s] calculate_result[%s]\n",
		 op->seq, cc_module_names[op->module],
		 cc_on_off(op->id == CC_OP_ON), cc_calc_names[op->calc]);
}

static int cc_vcn33_ctrl(bool on)
{
	int gpio = cc.vcn33_gpio;
	int ret;

	cc_info("vcn33 enable %d\n", on);

	if (!gpio_is_valid(gpio))
		return -ENODEV;

	ret = gpio_request(gpio, "comm_core vcn33");
	if (ret < 0) {
		pr_err("gpio request %d fail: %d\n", gpio, ret);
		return ret;
	}

	ret = gpio_direction_output(gpio, on);
	if (ret) {
		pr_err("gpio direction output %d fail: %d\n", gpio, ret);
		gpio_free(gpio);
		return ret;
	}

	if (cc.vcn33_delay_ms)
		usleep_range(cc.vcn33_delay_ms * USEC_PER_MSEC,
			     cc.vcn33_delay_ms * USEC_PER_MSEC + 500);
	gpio_free(gpio);
	cc.power_off = !on;

	return 0;
}

static int cc_vcn18_ctrl(bool on)
{
	struct regulator *vcn18 = READ_ONCE(cc.vcn18);
	int ret;

	cc_info("vcn18 enable %d\n", on);

	if (!vcn18)
		return -ENODEV;

	if (on) {
		ret = regulator_enable(vcn18);
		if (ret) {
			pr_err("regulator_enable err: %d\n", ret);
			return ret;
		}
		cc.vcn18_enabled++;
	} else {
		ret = regulator_disable(vcn18);
		if (ret) {
			pr_err("regulator_disable err: %d\n", ret);
			return ret;
		}
		cc.vcn18_enabled--;
	}

	return 0;
}

static int cc_hw_power_on(void)
{
	struct mmc_host *host = cc.host;

	/* Also on failure, so that the probe failures end the power on */
	cc_fsm_steps(CC_FSM_POWER_ON_GOING);

	if (!host) {
		pr_err("bus host is NULL\n");
		return -ENODEV;
	}

	if (cc_vcn33_ctrl(true)) {
		pr_err("enable VCN33 fail\n");
		return -EIO;
	}

	if (cc_vcn18_ctrl(true)) {
		pr_err("enable VCN18 fail\n");
		cc_vcn33_ctrl(false);
		return -EIO;
	}

	cc_info("begin mmc_start_host\n");
	/* Let the non-removable card be scanned again */
	host->rescan_entered = 0;
	mmc_start_host(host);
	cc_info("end mmc_start_host\n");

	return 0;
}

static void cc_timer_arm(unsigned int module, unsigned int ms)
{
	cc_info("create timer to wait %s event of %s in %u seconds\n",
		cc.func_state[module] == CC_FUNC_PROBE_START ||
		cc.func_state[module] == CC_FUNC_PROBE_GOING ?
		"probe" : "remove", cc_module_names[module],
		ms / 1000);

	/* Nothing may queue work once the thread is going away */
	if (!READ_ONCE(cc.ready))
		return;

	cc.timer_armed[module] = true;
	mod_timer(&cc.timer[module], jiffies + msecs_to_jiffies(ms));
}

static void cc_timer_cancel(unsigned int module)
{
	cc.timer_armed[module] = false;
	del_timer(&cc.timer[module]);
}

static void cc_timer_fn(struct timer_list *t)
{
	int module = t == &cc.timer[CC_MODULE_BT] ? CC_MODULE_BT :
						    CC_MODULE_WIFI;

	set_bit(module, &cc.timer_fired);
	if (READ_ONCE(cc.ready))
		cc_kick();
}

static void cc_set_func_state(unsigned int module,
			      enum cc_func_state state)
{
	cc_state("[COMM_CORE_%s_MODULE_STATE] TRANSITION: [%s] -> [%s]\n",
		 cc_module_names[module],
		 cc_func_state_names[cc.func_state[module]],
		 cc_func_state_names[state]);
	cc.func_state[module] = state;
}

static void cc_op_release(struct kref *ref)
{
	kfree(container_of(ref, struct cc_op, ref));
}

static struct cc_op *cc_op_alloc(enum cc_module module, enum cc_op_id id,
				 bool wait)
{
	struct cc_op *op;

	op = kzalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		return NULL;

	/* One reference for the queue, one for the waiter */
	kref_init(&op->ref);
	if (wait)
		kref_get(&op->ref);
	init_completion(&op->done);
	INIT_LIST_HEAD(&op->node);
	op->module = module;
	op->id = id;
	op->result = -ETIMEDOUT;

	return op;
}

static void cc_op_queue(struct cc_op *op)
{
	op->seq = ++cc.seq;
	list_add_tail(&op->node, &cc.active_ops);
}

static int cc_op_wait(struct cc_op *op, unsigned int timeout_ms)
{
	int ret;

	if (wait_for_completion_timeout(&op->done,
					msecs_to_jiffies(timeout_ms))) {
		ret = op->result;
		cc_info("cmd seq(%u) completion result:%d\n", op->seq, ret);
	} else {
		pr_warn("cmd seq(%u) completion timeout\n", op->seq);
		ret = -ETIMEDOUT;

		mutex_lock(&cc.op_lock);
		if (!list_empty(&op->node)) {
			/* Still note the change, e.g. the screen did go off */
			op->result = ret;
			cc_complete_single_op(op);
			list_del_init(&op->node);
			kref_put(&op->ref, cc_op_release);
		}
		mutex_unlock(&cc.op_lock);

		/* A request no longer being waited for can't block others */
		cc_kick();
	}
	kref_put(&op->ref, cc_op_release);

	return ret;
}

static int cc_send_op(enum cc_module module, enum cc_op_id id,
		      unsigned int timeout_ms)
{
	struct cc_op *op;

	op = cc_op_alloc(module, id, timeout_ms);
	if (!op)
		return -ENOMEM;

	mutex_lock(&cc.op_lock);
	cc_op_queue(op);
	mutex_unlock(&cc.op_lock);
	cc_kick();

	return timeout_ms ? cc_op_wait(op, timeout_ms) : 0;
}

static void cc_complete_single_op(struct cc_op *op)
{
	bool on;

	switch (op->module) {
	case CC_MODULE_BT:
	case CC_MODULE_WIFI:
		on = op->id == CC_OP_ON && !op->result;
		cc_state("[COMM_CORE_%s_FUNC_STATE] TRANSITION: [%s] -> [%s]\n",
			 cc_module_names[op->module],
			 cc_on_off(cc.func_on[op->module]), cc_on_off(on));
		cc.func_on[op->module] = on;
		break;
	case CC_MODULE_SCREEN:
		on = op->id == CC_OP_ON;
		cc_state("[COMM_CORE_SCREEN_STATE] TRANSITION: [%s] -> [%s]\n",
			 cc_on_off(cc.screen_on), cc_on_off(on));
		cc.screen_on = on;
		break;
	default:
		break;
	}
}

static void cc_op_finish(struct cc_op *op, int result)
{
	op->result = result;
	cc_complete_single_op(op);
	complete(&op->done);
	list_del_init(&op->node);
	kref_put(&op->ref, cc_op_release);
}

static void cc_complete_ops(enum cc_module module, enum cc_op_id id,
			    int result)
{
	struct cc_op *op, *tmp;

	mutex_lock(&cc.op_lock);
	list_for_each_entry_safe(op, tmp, &cc.wait_ops, node) {
		if (op->module != module || op->id != id ||
		    completion_done(&op->done))
			continue;

		cc_info("raise signal for cmd seq[%u] with result %d\n",
			op->seq, result);
		cc_op_finish(op, result);
	}
	mutex_unlock(&cc.op_lock);
}

static bool cc_func_settled(enum cc_func_state state)
{
	return state == CC_FUNC_PROBE_DONE || state == CC_FUNC_REMOVE_DONE;
}

static void cc_check_fsm_idle(void)
{
	enum cc_func_state bt, wifi;
	enum cc_fsm_state next;
	enum cc_op_id id;
	bool on;
	int result, i;

	if (cc.fsm_state != CC_FSM_POWER_ON_GOING &&
	    cc.fsm_state != CC_FSM_POWER_OFF_GOING) {
		cc_info("only need to check during power on or power off stage\n");
		return;
	}

	on = cc.fsm_state == CC_FSM_POWER_ON_GOING;

	/* A function whose driver never showed up can't be waited for */
	for (i = 0; i < CC_FUNC_NUM; i++) {
		if (cc.func_state[i] != CC_FUNC_IDLE)
			continue;
		cc_info("%s driver never reported, treat it as removed\n",
			cc_module_names[i]);
		cc_set_func_state(i, CC_FUNC_REMOVE_DONE);
	}

	bt = cc.func_state[CC_MODULE_BT];
	wifi = cc.func_state[CC_MODULE_WIFI];
	if (!cc_func_settled(bt) || !cc_func_settled(wifi)) {
		cc_info("need to wait for all modules finish power %s\n",
			on ? "on" : "off");
		return;
	}

	if (on) {
		result = bt == CC_FUNC_PROBE_DONE &&
			 wifi == CC_FUNC_PROBE_DONE ? 0 : -EIO;
		next = CC_FSM_POWER_ON_END;
		id = CC_OP_ON;
	} else {
		result = bt == CC_FUNC_REMOVE_DONE &&
			 wifi == CC_FUNC_REMOVE_DONE ? 0 : -EIO;
		next = CC_FSM_POWER_OFF_END;
		id = CC_OP_OFF;
	}

	cc_info("WIFI %s %s while BT %s %s\n", on ? "probe" : "remove",
		(wifi == CC_FUNC_PROBE_DONE) == on ? "success" : "fail",
		on ? "probe" : "remove",
		(bt == CC_FUNC_PROBE_DONE) == on ? "success" : "fail");

	cc_fsm_steps(next);
	cc_complete_ops(CC_MODULE_SCREEN, id, result);
	cc_complete_ops(CC_MODULE_RST, id, result);
}

static void cc_status_internal(unsigned int module,
			       enum ENUM_COMM_CORE_STATUS_CMD_T status,
			       struct mmc_host *host)
{
	cc_info("receive status_cmd %s from module %s\n",
		cc_status_names[status], cc_module_names[module]);
	cc_dump_status();

	if (host && !cc.host) {
		cc.host = host;
		cc_info("init sdio host to %s\n", mmc_hostname(host));
	}

	switch (status) {
	case COMM_CORE_STATUS_PROBE_START:
		cc_set_func_state(module, CC_FUNC_PROBE_GOING);
		cc_timer_arm(module, CC_DONE_TIMEOUT_MS);
		/* The function may also show up on its own, e.g. at boot */
		if (cc.fsm_state <= CC_FSM_POWER_ON_START)
			cc_fsm_steps(CC_FSM_POWER_ON_GOING);
		break;

	case COMM_CORE_STATUS_PROBE_SUCCESS:
	case COMM_CORE_STATUS_PROBE_FAIL:
		cc_set_func_state(module,
				  status == COMM_CORE_STATUS_PROBE_SUCCESS ?
				  CC_FUNC_PROBE_DONE : CC_FUNC_REMOVE_DONE);
		cc_timer_cancel(module);
		if (cc.fsm_state != CC_FSM_POWER_ON_GOING) {
			pr_warn("receive %s message during not power on stage\n",
				cc_status_names[status]);
			break;
		}
		cc_complete_ops((enum cc_module)module, CC_OP_ON,
				status == COMM_CORE_STATUS_PROBE_SUCCESS ?
				0 : -EIO);
		cc_check_fsm_idle();
		break;

	case COMM_CORE_STATUS_REMOVE_START:
		cc_set_func_state(module, CC_FUNC_REMOVE_GOING);
		cc_timer_arm(module, CC_DONE_TIMEOUT_MS);
		if (cc.fsm_state == CC_FSM_POWER_OFF_START)
			cc_fsm_steps(CC_FSM_POWER_OFF_GOING);
		break;

	case COMM_CORE_STATUS_REMOVE_SUCCESS:
	case COMM_CORE_STATUS_REMOVE_FAIL:
		cc_set_func_state(module,
				  status == COMM_CORE_STATUS_REMOVE_SUCCESS ?
				  CC_FUNC_REMOVE_DONE : CC_FUNC_PROBE_DONE);
		cc_timer_cancel(module);
		if (cc.fsm_state != CC_FSM_POWER_OFF_GOING) {
			pr_warn("receive %s message during not power off stage\n",
				cc_status_names[status]);
			break;
		}
		cc_complete_ops((enum cc_module)module, CC_OP_OFF,
				status == COMM_CORE_STATUS_REMOVE_SUCCESS ?
				0 : -EIO);
		cc_check_fsm_idle();
		break;

	default:
		break;
	}
}

static void cc_status_sync(unsigned int module,
			   enum ENUM_COMM_CORE_STATUS_CMD_T status)
{
	mutex_lock(&cc.fsm_lock);
	cc_status_internal(module, status, NULL);
	mutex_unlock(&cc.fsm_lock);
}

static void cc_func_timeout(unsigned int module)
{
	enum ENUM_COMM_CORE_STATUS_CMD_T status;
	const char *event;

	switch (cc.func_state[module]) {
	case CC_FUNC_PROBE_START:
		event = "probe_start";
		status = COMM_CORE_STATUS_PROBE_FAIL;
		break;
	case CC_FUNC_PROBE_GOING:
		event = "probe_done";
		status = COMM_CORE_STATUS_PROBE_FAIL;
		break;
	case CC_FUNC_REMOVE_START:
		event = "remove_start";
		status = COMM_CORE_STATUS_REMOVE_FAIL;
		break;
	case CC_FUNC_REMOVE_GOING:
		event = "remove_done";
		status = COMM_CORE_STATUS_REMOVE_FAIL;
		break;
	default:
		pr_warn("module %s should not timeout at state %s\n",
			cc_module_names[module],
			cc_func_state_names[cc.func_state[module]]);
		return;
	}

	pr_warn("wait %s module's %s message timeout\n",
		cc_module_names[module], event);
	cc_status_internal(module, status, NULL);
}

static void cc_reprobe(unsigned int module)
{
	struct sdio_driver *drv;

	cc_info("only need to re-probe %s\n", cc_module_names[module]);

	mutex_lock(&cc.drv_lock);
	drv = cc.drv[module];
	if (drv)
		sdio_unregister_driver(drv);

	mutex_lock(&cc.fsm_lock);
	cc_set_func_state(module, CC_FUNC_PROBE_START);
	cc_timer_arm(module, CC_START_TIMEOUT_MS);
	mutex_unlock(&cc.fsm_lock);

	if (drv && sdio_register_driver(drv))
		pr_err("failed to register the %s driver\n",
		       cc_module_names[module]);
	mutex_unlock(&cc.drv_lock);
}

static bool cc_func_down(unsigned int module)
{
	return cc.func_state[module] == CC_FUNC_REMOVE_DONE ||
	       cc.func_state[module] == CC_FUNC_IDLE;
}

static void cc_power_on_start(void)
{
	bool bt = cc_func_down(CC_MODULE_BT);
	bool wifi = cc_func_down(CC_MODULE_WIFI);

	if (bt && wifi) {
		cc_info("let sdio bus help probe as WIFI & BT all current off\n");

		mutex_lock(&cc.fsm_lock);
		cc_set_func_state(CC_MODULE_BT, CC_FUNC_PROBE_START);
		cc_timer_arm(CC_MODULE_BT, CC_START_TIMEOUT_MS);
		cc_set_func_state(CC_MODULE_WIFI, CC_FUNC_PROBE_START);
		cc_timer_arm(CC_MODULE_WIFI, CC_START_TIMEOUT_MS);
		mutex_unlock(&cc.fsm_lock);

		if (cc_hw_power_on()) {
			pr_err("hw power on fail\n");
			cc_status_sync(CC_MODULE_BT, COMM_CORE_STATUS_PROBE_FAIL);
			cc_status_sync(CC_MODULE_WIFI,
				       COMM_CORE_STATUS_PROBE_FAIL);
		}
	} else if (bt) {
		cc_reprobe(CC_MODULE_BT);
	} else if (wifi) {
		cc_reprobe(CC_MODULE_WIFI);
	}
}

static void cc_power_off_start(void)
{
	struct mmc_host *host = cc.host;
	int i;

	mutex_lock(&cc.fsm_lock);
	for (i = 0; i < CC_FUNC_NUM; i++) {
		if (cc.func_state[i] != CC_FUNC_PROBE_DONE)
			continue;
		cc_set_func_state(i, CC_FUNC_REMOVE_START);
		cc_timer_arm(i, CC_START_TIMEOUT_MS);
	}
	mutex_unlock(&cc.fsm_lock);

	if (host) {
		cc_fsm_steps(CC_FSM_POWER_OFF_GOING);
		cc_info("begin mmc_stop_host\n");
		/* Removes both functions, which report back right away */
		mmc_stop_host(host);
		cc_info("end mmc_stop_host\n");
	} else {
		pr_err("bus host is NULL\n");
		cc_fsm_steps(CC_FSM_POWER_OFF_GOING);
	}

	/* Nothing may be left to report if no function was bound */
	mutex_lock(&cc.fsm_lock);
	if (cc.fsm_state == CC_FSM_POWER_OFF_GOING)
		cc_check_fsm_idle();
	mutex_unlock(&cc.fsm_lock);
}

static void cc_set_all_removed(void)
{
	int i;

	for (i = 0; i < CC_FUNC_NUM; i++) {
		if (cc.func_state[i] == CC_FUNC_REMOVE_DONE)
			continue;
		cc_timer_cancel(i);
		cc_set_func_state(i, CC_FUNC_REMOVE_DONE);
	}
}

static void cc_fsm_steps(enum cc_fsm_state next)
{
	enum cc_fsm_state prev = cc.fsm_state;

	for (;;) {
		cc_state("[COMM_CORE_FSM] TRANSITION: [%s] -> [%s]\n",
			 cc_fsm_names[prev], cc_fsm_names[next]);
		cc.fsm_state = next;

		switch (next) {
		case CC_FSM_IDLE:
			__pm_relax(cc.ws);
			return;
		case CC_FSM_POWER_ON_START:
			cc_power_on_start();
			return;
		case CC_FSM_POWER_ON_END:
			break;
		case CC_FSM_POWER_OFF_START:
			cc_power_off_start();
			return;
		case CC_FSM_POWER_OFF_END:
			if (cc_vcn18_ctrl(false))
				pr_warn("vcn18 turn off fail\n");
			if (cc_vcn33_ctrl(false))
				pr_warn("vcn33 turn off fail\n");
			/* Whatever was reported, nothing is left powered */
			cc_set_all_removed();
			break;
		default:
			return;
		}

		prev = next;
		next = CC_FSM_IDLE;
	}
}

static int cc_power_ctrl(bool on)
{
	if (cc.fsm_state != CC_FSM_IDLE) {
		pr_warn("should not enter as FSM state is not IDLE\n");
		cc_dump_status();
		return -EBUSY;
	}

	if (!cc.ws->active)
		pm_wakeup_ws_event(cc.ws, CC_WAKELOCK_MS, false);

	cc_fsm_steps(on ? CC_FSM_POWER_ON_START : CC_FSM_POWER_OFF_START);

	return 0;
}

static void cc_return_range(struct cc_op *first, struct cc_op *last)
{
	struct cc_op *op = first;

	for (;;) {
		op->calc = CC_CALC_RETURN;
		if (op == last)
			break;
		op = list_next_entry(op, node);
	}
}

static void cc_divide_cmds(struct cc_calc_info *calc)
{
	struct cc_block_info *block;
	struct cc_op *op;
	enum cc_block b;
	int i;

	calc->current_on[CC_MODULE_BT] = cc.func_on[CC_MODULE_BT];
	calc->current_on[CC_MODULE_WIFI] = cc.func_on[CC_MODULE_WIFI];
	calc->current_on[CC_MODULE_SCREEN] = cc.screen_on;

	list_for_each_entry(op, &cc.active_ops, node) {
		if (op->module == CC_MODULE_RST)
			b = CC_BLOCK_RST;
		else if (calc->block[CC_BLOCK_RST].first)
			b = CC_BLOCK_RST_RIGHT;
		else
			b = CC_BLOCK_RST_LEFT;

		block = &calc->block[b];
		if (!block->first) {
			block->first = op;
			for (i = 0; i < CC_TARGET_NUM; i++)
				block->target_on[i] = calc->current_on[i];
		}
		block->last = op;

		if (op->module < CC_TARGET_NUM) {
			block->last_cmd[op->module] = op;
			block->target_on[op->module] = op->id == CC_OP_ON;
		}

		cc_dump_op(op);
	}
}

static void cc_return_rst_left_last_cmds(struct cc_calc_info *calc)
{
	struct cc_block_info *block = &calc->block[CC_BLOCK_RST_LEFT];
	enum cc_func_state bt = cc.func_state[CC_MODULE_BT];
	enum cc_func_state wifi = cc.func_state[CC_MODULE_WIFI];
	struct cc_op *op;
	bool on;
	int m;

	for (m = 0; m < CC_TARGET_NUM; m++) {
		op = block->last_cmd[m];
		if (!op)
			continue;

		on = block->target_on[m];
		if (m != CC_MODULE_SCREEN) {
			if (on && cc.func_on[m]) {
				cc_info("directly return %s's last on cmd for cmd seq[%u] as %s has already on\n",
					cc_module_names[m], op->seq,
					cc_module_names[m]);
				op->calc = CC_CALC_RETURN;
			} else if (on &&
				   cc.func_state[m] == CC_FUNC_PROBE_DONE) {
				cc_info("directly return %s's last on cmd for cmd seq[%u] as %s driver already probed\n",
					cc_module_names[m], op->seq,
					cc_module_names[m]);
				op->calc = CC_CALC_RETURN;
			} else if (!on &&
				   cc.func_state[m] == CC_FUNC_REMOVE_DONE) {
				cc_info("directly return %s's last off cmd for cmd seq[%u] as %s driver already removed\n",
					cc_module_names[m], op->seq,
					cc_module_names[m]);
				op->calc = CC_CALC_RETURN;
			}
		} else {
			if (on && cc.screen_on) {
				cc_info("directly return screen's last on cmd for cmd seq[%u] as screen has already on\n",
					op->seq);
				op->calc = CC_CALC_RETURN;
			} else if (on && bt == CC_FUNC_PROBE_DONE &&
				   wifi == CC_FUNC_PROBE_DONE) {
				cc_info("directly return screen's last on cmd for cmd seq[%u] as wifi & bt already probed\n",
					op->seq);
				op->calc = CC_CALC_RETURN;
			} else if (!on && bt == CC_FUNC_REMOVE_DONE &&
				   wifi == CC_FUNC_REMOVE_DONE) {
				cc_info("directly return screen's last off cmd for cmd seq[%u] as wifi & bt already removed\n",
					op->seq);
				op->calc = CC_CALC_RETURN;
			}
		}

		if (!on && !block->power_off && op->calc != CC_CALC_RETURN) {
			cc_info("directly return %s's last off cmd for cmd seq[%u] as power should keep on\n",
				cc_module_names[m], op->seq);
			op->calc = CC_CALC_RETURN;
		}
	}
}

static void cc_return_internal_cmds(struct cc_calc_info *calc,
				    enum cc_block b)
{
	struct cc_block_info *block = &calc->block[b];
	struct cc_op *op = block->first;

	for (;;) {
		if (op->module < CC_TARGET_NUM &&
		    op != block->last_cmd[op->module]) {
			cc_info("directly return %s's non-last cmd for cmd seq[%u]\n",
				cc_module_names[op->module], op->seq);
			op->calc = CC_CALC_RETURN;
		}
		if (op == block->last)
			break;
		op = list_next_entry(op, node);
	}

	if (b == CC_BLOCK_RST_LEFT)
		cc_return_rst_left_last_cmds(calc);
}

static void cc_calculate_single_block(struct cc_calc_info *calc,
				      enum cc_block b)
{
	struct cc_block_info *block = &calc->block[b];

	if (!block->first || !block->last)
		return;

	/* A full reset of a chip that is off has nothing to do */
	if (b == CC_BLOCK_RST && block->first != block->last &&
	    cc.power_off) {
		cc_info("directly return Rst for case of rst_full && current power off\n");
		cc_return_range(block->first, block->last);
		return;
	}

	cc_info("begin calculate for block_idx[%s]\n", cc_block_names[b]);
	cc_info("wifi_current_on[%s] bt_current_on[%s] screen_current_on[%s]\n",
		cc_on_off(cc.func_on[CC_MODULE_WIFI]),
		cc_on_off(cc.func_on[CC_MODULE_BT]),
		cc_on_off(cc.screen_on));

	block->power_off = !block->target_on[CC_MODULE_BT] &&
			   !block->target_on[CC_MODULE_WIFI] &&
			   !block->target_on[CC_MODULE_SCREEN];
	cc_info("WIFI_target_on[%s] BT_target_on[%s] SCREEN_target_on[%s] final_target_power_state[%s]\n",
		cc_on_off(block->target_on[CC_MODULE_WIFI]),
		cc_on_off(block->target_on[CC_MODULE_BT]),
		cc_on_off(block->target_on[CC_MODULE_SCREEN]),
		cc_on_off(!block->power_off));

	cc_return_internal_cmds(calc, b);
}

static void cc_calculate_multi_blocks(struct cc_calc_info *calc)
{
	struct cc_block_info *left = &calc->block[CC_BLOCK_RST_LEFT];
	struct cc_block_info *rst = &calc->block[CC_BLOCK_RST];
	struct cc_block_info *right = &calc->block[CC_BLOCK_RST_RIGHT];

	calc->power_off = !calc->current_on[CC_MODULE_BT] &&
			  !calc->current_on[CC_MODULE_WIFI] &&
			  !calc->current_on[CC_MODULE_SCREEN];

	/* The reset supersedes what came before it */
	if (left->first && rst->first && right->first) {
		cc_info("directly return RstLeft for case of rstLeft && rst && rstRight\n");
		cc_return_range(left->first, left->last);
	}

	/* No reset is needed for a chip that ends up off */
	if (rst->first && rst->first != rst->last && right->first &&
	    calc->power_off) {
		cc_info("directly return Rst for case of rstLeft_xx && rst && rstRight_OFF or rst && rstRight_OFF\n");
		cc_return_range(rst->first, rst->last);
	}

	if (left->power_off && rst->first && !right->first && left->first) {
		cc_info("directly return Rst for case of rstLeft_off && rst\n");
		cc_return_range(rst->first, rst->last);
	}
}

static void cc_calculate_finish(struct cc_calc_info *calc)
{
	struct cc_op *op, *tmp;

	list_for_each_entry_safe(op, tmp, &cc.active_ops, node) {
		if (op->calc == CC_CALC_RETURN) {
			cc_info("directly return cmd for cmd seq[%u]\n",
				op->seq);
			cc_dump_op(op);
			cc_op_finish(op, 0);
		} else if (calc->handle) {
			op->calc = CC_CALC_BUFFER;
			cc_dump_op(op);
		} else {
			op->calc = CC_CALC_HANDLE;
			list_move_tail(&op->node, &cc.wait_ops);
			calc->handle = true;
			calc->handle_id = op->id;
			cc_dump_op(op);
		}
	}
}

static bool cc_calculate_cmds(void)
{
	struct cc_calc_info *calc = &cc.calc;
	bool handle;
	enum cc_op_id id;
	int i;

	if (cc.fsm_state != CC_FSM_IDLE)
		return false;

	mutex_lock(&cc.op_lock);
	if (list_empty(&cc.active_ops) || !list_empty(&cc.wait_ops)) {
		mutex_unlock(&cc.op_lock);
		return false;
	}

	cc_dump_status();
	cc_info("current power state is %s\n", cc_on_off(!cc.power_off));

	memset(calc, 0, sizeof(*calc));
	cc_divide_cmds(calc);
	for (i = 0; i < CC_BLOCK_MAX; i++)
		cc_calculate_single_block(calc, i);
	cc_calculate_multi_blocks(calc);
	cc_calculate_finish(calc);

	handle = calc->handle;
	id = calc->handle_id;
	mutex_unlock(&cc.op_lock);

	if (handle)
		cc_power_ctrl(id == CC_OP_ON);

	return true;
}

static void cc_handle_events(void)
{
	struct cc_event *evt;
	int i;

	for (;;) {
		spin_lock_irq(&cc.event_lock);
		evt = list_first_entry_or_null(&cc.events, struct cc_event,
					       node);
		if (evt)
			list_del(&evt->node);
		spin_unlock_irq(&cc.event_lock);

		if (!evt)
			break;

		mutex_lock(&cc.fsm_lock);
		cc_status_internal(evt->module, evt->status, evt->host);
		mutex_unlock(&cc.fsm_lock);
		kfree(evt);
	}

	for (i = 0; i < CC_FUNC_NUM; i++) {
		if (!test_and_clear_bit(i, &cc.timer_fired))
			continue;
		/* Skip timers that were re-armed or cancelled since */
		if (!cc.timer_armed[i] || timer_pending(&cc.timer[i]))
			continue;
		cc.timer_armed[i] = false;

		mutex_lock(&cc.fsm_lock);
		cc_func_timeout(i);
		mutex_unlock(&cc.fsm_lock);
	}
}

static void cc_work_fn(struct kthread_work *work)
{
	/* A power change may finish right away and leave requests queued */
	do {
		cc_handle_events();
	} while (cc_calculate_cmds());
}

void comm_core_register_sdio_driver(enum ENUM_RST_MODULE_TYPE_T module,
				    struct sdio_driver *drv)
{
	if (WARN_ON((unsigned int)module >= CC_FUNC_NUM))
		return;

	mutex_lock(&cc.drv_lock);
	cc.drv[module] = drv;
	mutex_unlock(&cc.drv_lock);
}
EXPORT_SYMBOL(comm_core_register_sdio_driver);

void comm_core_unregister_sdio_driver(enum ENUM_RST_MODULE_TYPE_T module)
{
	if (WARN_ON((unsigned int)module >= CC_FUNC_NUM))
		return;

	mutex_lock(&cc.drv_lock);
	cc.drv[module] = NULL;
	mutex_unlock(&cc.drv_lock);
}
EXPORT_SYMBOL(comm_core_unregister_sdio_driver);

void NotifyCommCoreStatusCmd(enum ENUM_RST_MODULE_TYPE_T module,
			     enum ENUM_COMM_CORE_STATUS_CMD_T cmd, void *data)
{
	struct sdio_func *func = data;
	struct mmc_host *host = NULL;
	struct cc_event *evt;
	unsigned long flags;

	if (!READ_ONCE(cc.ready)) {
		pr_info("skip msg as comm_core not init\n");
		return;
	}

	if ((unsigned int)module >= CC_FUNC_NUM ||
	    (unsigned int)cmd >= COMM_CORE_STATUS_MAX) {
		pr_err("invalid status %d from module %d\n", cmd, module);
		return;
	}

	/* The function may be gone by the time the report is handled */
	if (func && func->card)
		host = func->card->host;

	/* Probes and removals started by the conn_core thread itself */
	if (cc_on_worker()) {
		mutex_lock(&cc.fsm_lock);
		cc_status_internal(module, cmd, host);
		mutex_unlock(&cc.fsm_lock);
		return;
	}

	evt = kzalloc(sizeof(*evt), GFP_KERNEL);
	if (!evt)
		return;
	evt->module = module;
	evt->status = cmd;
	evt->host = host;

	spin_lock_irqsave(&cc.event_lock, flags);
	list_add_tail(&evt->node, &cc.events);
	spin_unlock_irqrestore(&cc.event_lock, flags);
	cc_kick();
}
EXPORT_SYMBOL(NotifyCommCoreStatusCmd);

int comm_core_trigger_rst(void)
{
	struct cc_op *off, *on;

	if (!READ_ONCE(cc.ready))
		return -ENODEV;

	off = cc_op_alloc(CC_MODULE_RST, CC_OP_OFF, false);
	on = cc_op_alloc(CC_MODULE_RST, CC_OP_ON, false);
	if (!off || !on) {
		kfree(off);
		kfree(on);
		return -ENOMEM;
	}

	/* Queue both so the pair is evaluated as one reset */
	mutex_lock(&cc.op_lock);
	cc_op_queue(off);
	cc_op_queue(on);
	mutex_unlock(&cc.op_lock);
	cc_kick();

	return 0;
}

static ssize_t cc_func_read(enum cc_module module, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	char buf[4];
	bool on;
	int len;

	if (!READ_ONCE(cc.ready)) {
		pr_warn("skip read action as comm_core not init\n");
		return -EIO;
	}

	mutex_lock(&cc.fsm_lock);
	on = cc.func_on[module];
	mutex_unlock(&cc.fsm_lock);

	len = scnprintf(buf, sizeof(buf), "%d\n", on);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static ssize_t cc_func_write(enum cc_module module, const char __user *ubuf,
			     size_t count)
{
	char buf[32];
	size_t len = min(count, sizeof(buf) - 1);
	unsigned int val;
	int ret;

	if (!READ_ONCE(cc.ready)) {
		pr_warn("skip write action as comm_core not init\n");
		return -EIO;
	}

	if (!len) {
		pr_err("buffer null\n");
		return -EIO;
	}

	if (copy_from_user(buf, ubuf, len)) {
		pr_err("error of copy from user\n");
		return -EIO;
	}
	buf[len] = '\0';

	/* Userspace writes "0" or "1", with or without the NUL */
	if (kstrtouint(buf, 0, &val)) {
		pr_err("input value error\n");
		return -EIO;
	}

	pr_info("receive %s %s cmd from user\n",
		module == CC_MODULE_BT ? "bt" : "wifi", val ? "on" : "off");

	ret = cc_send_op(module, val ? CC_OP_ON : CC_OP_OFF,
			 CC_CMD_TIMEOUT_MS);
	if (ret) {
		pr_err("%s %s fail, ret = %d\n", cc_module_names[module],
		       val ? "on" : "off", ret);
		return -EIO;
	}

	return count;
}

static ssize_t cc_bt_read(struct file *file, char __user *ubuf,
			  size_t count, loff_t *ppos)
{
	return cc_func_read(CC_MODULE_BT, ubuf, count, ppos);
}

static ssize_t cc_bt_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	return cc_func_write(CC_MODULE_BT, ubuf, count);
}

static ssize_t cc_wifi_read(struct file *file, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	return cc_func_read(CC_MODULE_WIFI, ubuf, count, ppos);
}

static ssize_t cc_wifi_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	return cc_func_write(CC_MODULE_WIFI, ubuf, count);
}

static ssize_t cc_dbg_read(struct file *file, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	char buf[384];
	int len;

	mutex_lock(&cc.fsm_lock);
	len = cc_status_print(buf, sizeof(buf));
	mutex_unlock(&cc.fsm_lock);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static ssize_t cc_dbg_write(struct file *file, const char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	unsigned int val;
	int ret;

	ret = kstrtouint_from_user(ubuf, count, 0, &val);
	if (ret)
		return ret;

	if (val == 100)
		cc.dbg_level = CC_DBG_LEVEL_STATE;
	else if (val == 101)
		cc.dbg_level = CC_DBG_LEVEL_INFO + 1;
	else
		return -EINVAL;

	return count;
}

static const struct file_operations cc_bt_fops = {
	.owner = THIS_MODULE,
	.read = cc_bt_read,
	.write = cc_bt_write,
	.llseek = noop_llseek,
};

static const struct file_operations cc_wifi_fops = {
	.owner = THIS_MODULE,
	.read = cc_wifi_read,
	.write = cc_wifi_write,
	.llseek = noop_llseek,
};

static const struct file_operations cc_dbg_fops = {
	.owner = THIS_MODULE,
	.read = cc_dbg_read,
	.write = cc_dbg_write,
	.llseek = noop_llseek,
};

static struct cc_node cc_nodes[] = {
	{ .name = "connsys_dbg", .fops = &cc_dbg_fops },
	{ .name = "connsys_bt", .fops = &cc_bt_fops },
	{ .name = "connsys_wifi", .fops = &cc_wifi_fops },
};

static void cc_node_destroy(struct cc_node *node)
{
	if (!IS_ERR_OR_NULL(node->dev))
		device_destroy(node->class, node->devt);
	if (!IS_ERR_OR_NULL(node->class))
		class_destroy(node->class);
	if (node->cdev.ops)
		cdev_del(&node->cdev);
	if (node->devt)
		unregister_chrdev_region(node->devt, 1);
	node->dev = NULL;
	node->class = NULL;
	node->cdev.ops = NULL;
	node->devt = 0;
}

static int cc_node_create(struct cc_node *node)
{
	int ret;

	ret = alloc_chrdev_region(&node->devt, 0, 1, "connsys");
	if (ret) {
		pr_err("fail to alloc chrdev region for %s\n", node->name);
		node->devt = 0;
		return ret;
	}

	cdev_init(&node->cdev, node->fops);
	node->cdev.owner = THIS_MODULE;
	ret = cdev_add(&node->cdev, node->devt, 1);
	if (ret) {
		pr_err("cdev_add() fails (%d)\n", ret);
		node->cdev.ops = NULL;
		goto err;
	}

	node->class = class_create(THIS_MODULE, node->name);
	if (IS_ERR(node->class)) {
		ret = PTR_ERR(node->class);
		pr_err("class create fail, error code(%d)\n", ret);
		goto err;
	}

	node->dev = device_create(node->class, NULL, node->devt, NULL, "%s",
				  node->name);
	if (IS_ERR(node->dev)) {
		ret = PTR_ERR(node->dev);
		pr_err("device create fail, error code(%d)\n", ret);
		goto err;
	}

	return 0;

err:
	cc_node_destroy(node);
	return ret;
}

static void cc_nodes_destroy(void)
{
	int i;

	for (i = ARRAY_SIZE(cc_nodes) - 1; i >= 0; i--)
		cc_node_destroy(&cc_nodes[i]);
}

static int cc_nodes_create(void)
{
	int ret, i;

	for (i = 0; i < ARRAY_SIZE(cc_nodes); i++) {
		ret = cc_node_create(&cc_nodes[i]);
		if (ret) {
			cc_nodes_destroy();
			return ret;
		}
	}

	return 0;
}

static void cc_screen_work_fn(struct work_struct *work)
{
	bool on = READ_ONCE(cc.screen_unblank);
	int ret;

	if (!READ_ONCE(cc.ready)) {
		pr_warn("skip screen notify action as comm_core not init\n");
		return;
	}

	ret = cc_send_op(CC_MODULE_SCREEN, on ? CC_OP_ON : CC_OP_OFF,
			 CC_CMD_TIMEOUT_MS);
	if (ret)
		pr_err("screen %s fail, ret = %d\n", on ? "on" : "off", ret);
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_V2)
static int cc_disp_notifier_call(struct notifier_block *nb,
				 unsigned long event, void *data)
{
	int *blank = data;

	cc_info("connsys_fb_notifier_callback event=[%lu] from MTK DISP\n",
		event);

	if (event != MTK_DISP_EVENT_BLANK || !blank)
		return 0;

	if (*blank == MTK_DISP_BLANK_UNBLANK) {
		cc_info("enter UNBLANK\n");
		WRITE_ONCE(cc.screen_unblank, true);
	} else if (*blank == MTK_DISP_BLANK_POWERDOWN) {
		cc_info("Prepare POWERDOWN\n");
		WRITE_ONCE(cc.screen_unblank, false);
	} else {
		return 0;
	}

	/* Waits for the request, keep it off system_wq */
	queue_work(system_long_wq, &cc.screen_work);

	return 0;
}

static int cc_disp_notifier_register(void)
{
	cc.disp_nb.notifier_call = cc_disp_notifier_call;
	return mtk_disp_notifier_register("comm_core_driver", &cc.disp_nb);
}

static void cc_disp_notifier_unregister(void)
{
	mtk_disp_notifier_unregister(&cc.disp_nb);
}
#else
static int cc_disp_notifier_register(void)
{
	return 0;
}

static void cc_disp_notifier_unregister(void)
{
}
#endif

static int cc_vcn18_probe(struct platform_device *pdev)
{
	struct regulator *vcn18;
	int ret;

	vcn18 = devm_regulator_get(&pdev->dev, "vcn18");
	if (IS_ERR(vcn18)) {
		ret = PTR_ERR(vcn18);
		if (ret == -EPROBE_DEFER)
			return ret;
		pr_warn("Regulator_get VCN18 fail: %d\n", ret);
		return 0;
	}

	/* Held for as long as the driver is bound, like on stock */
	ret = regulator_enable(vcn18);
	if (ret)
		pr_err("regulator_enable failed: %d\n", ret);
	else
		cc.vcn18_enabled++;

	WRITE_ONCE(cc.vcn18, vcn18);

	return 0;
}

static int cc_vcn18_remove(struct platform_device *pdev)
{
	struct regulator *vcn18 = cc.vcn18;

	WRITE_ONCE(cc.vcn18, NULL);
	for (; cc.vcn18_enabled; cc.vcn18_enabled--)
		regulator_disable(vcn18);

	return 0;
}

static const struct of_device_id cc_of_ids[] = {
	{ .compatible = "mediatek,connsys_pwr_ctrl" },
	{ }
};

static struct platform_driver cc_vcn18_driver = {
	.probe = cc_vcn18_probe,
	.remove = cc_vcn18_remove,
	.driver = {
		.name = "connsys_pwr_ctrl",
		.of_match_table = cc_of_ids,
		.suppress_bind_attrs = true,
	},
};

static int cc_gpio_parse(void)
{
	struct device_node *np;
	int gpio;

	np = of_find_compatible_node(NULL, NULL, "mediatek,connsys_pwr_ctrl");
	if (!np) {
		pr_err("parse node mediatek,connsys_pwr_ctrl fail\n");
		return -ENODEV;
	}

	gpio = of_get_named_gpio(np, "vcn33-gpio", 0);
	of_node_put(np);
	if (!gpio_is_valid(gpio)) {
		pr_err("parse gpio name vcn33-gpio fail: %d\n", gpio);
		return gpio < 0 ? gpio : -EINVAL;
	}

	cc.vcn33_gpio = gpio;
	pr_info("vcn33 gpio_num is %d\n", gpio);

	return 0;
}

static void cc_free_pending(void)
{
	struct cc_event *evt, *evt_tmp;
	struct cc_op *op, *op_tmp;

	list_for_each_entry_safe(evt, evt_tmp, &cc.events, node) {
		list_del(&evt->node);
		kfree(evt);
	}

	mutex_lock(&cc.op_lock);
	list_for_each_entry_safe(op, op_tmp, &cc.active_ops, node)
		cc_op_finish(op, -ENODEV);
	list_for_each_entry_safe(op, op_tmp, &cc.wait_ops, node)
		cc_op_finish(op, -ENODEV);
	mutex_unlock(&cc.op_lock);
}

int comm_core_init(void)
{
	int ret, i;

	mutex_init(&cc.op_lock);
	mutex_init(&cc.fsm_lock);
	mutex_init(&cc.drv_lock);
	spin_lock_init(&cc.event_lock);
	INIT_LIST_HEAD(&cc.active_ops);
	INIT_LIST_HEAD(&cc.wait_ops);
	INIT_LIST_HEAD(&cc.events);
	kthread_init_work(&cc.work, cc_work_fn);
	INIT_WORK(&cc.screen_work, cc_screen_work_fn);
	for (i = 0; i < CC_FUNC_NUM; i++)
		timer_setup(&cc.timer[i], cc_timer_fn, 0);

	cc.dbg_level = CC_DBG_LEVEL_STATE;
	cc.vcn33_gpio = -ENOENT;
	cc.vcn33_delay_ms = CC_VCN33_DELAY_MS;
	/* The display is on when the driver loads */
	cc.screen_on = true;

	ret = cc_gpio_parse();
	if (ret)
		return ret;

	ret = platform_driver_register(&cc_vcn18_driver);
	if (ret) {
		pr_err("RegVcn18 platform driver registered failed(%d)\n", ret);
		return ret;
	}

	cc.ws = wakeup_source_register(NULL, "COMM_CORE_PWR_ON_OFF");
	if (!cc.ws) {
		ret = -ENOMEM;
		goto err_driver;
	}

	cc.worker = kthread_create_worker(0, "conn_core");
	if (IS_ERR(cc.worker)) {
		ret = PTR_ERR(cc.worker);
		cc.worker = NULL;
		pr_err("thread_create fail: %d\n", ret);
		goto err_ws;
	}

	ret = cc_nodes_create();
	if (ret) {
		pr_err("chrdev init fail(%d)\n", ret);
		goto err_worker;
	}

	ret = cc_disp_notifier_register();
	if (ret) {
		pr_err("register fb_notifier fail: %d\n", ret);
		goto err_nodes;
	}

	WRITE_ONCE(cc.ready, true);
	pr_info("ready\n");

	return 0;

err_nodes:
	cc_nodes_destroy();
err_worker:
	kthread_destroy_worker(cc.worker);
	cc.worker = NULL;
err_ws:
	wakeup_source_unregister(cc.ws);
err_driver:
	platform_driver_unregister(&cc_vcn18_driver);
	return ret;
}

void comm_core_exit(void)
{
	int i;

	/* No new requests, reports or timers from here on */
	WRITE_ONCE(cc.ready, false);

	cc_disp_notifier_unregister();
	cancel_work_sync(&cc.screen_work);
	cc_nodes_destroy();

	for (i = 0; i < CC_FUNC_NUM; i++)
		del_timer_sync(&cc.timer[i]);
	kthread_destroy_worker(cc.worker);
	cc.worker = NULL;
	cc_free_pending();

	wakeup_source_unregister(cc.ws);
	platform_driver_unregister(&cc_vcn18_driver);
}
