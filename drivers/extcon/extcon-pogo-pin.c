// SPDX-License-Identifier: GPL-2.0-only
/**
 * drivers/extcon/extcon-pogo-pin.c - POGO PIN extcon driver
 *
 * Copyright (C) 2022 Amazon.com Inc.
 */

#include <linux/extcon-provider.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/pinctrl/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/iio/types.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/input.h>
#include "usb.h"
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#include "mtk_disp_notify.h"
#include "mtk_panel_ext.h"
#endif
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#include <linux/metricslog.h>
#endif

#define DETECT_DEFAULT_MS		500	/* ms */

#define MOISTURE_THRESHOLD_L		0
#define MOISTURE_THRESHOLD_H		605
#define KEYBOARD_THRESHOLD_L		606
#define KEYBOARD_THRESHOLD_H		770
#define NOTHING_THRESHOLD_L		810
#define NOTHING_THRESHOLD_H		990

#define WORK_INTERVAL_NOTHING		200
#define WORK_INTERVAL_WET		15000
#define WORK_INTERVAL_KEYBOARD		200

#define KEYBOARD_HEARTBEAT		3600000
#define VENDOR_ID_LAB126		0x1949
#define DEVICE_ID_LAB126_abc123_KB	0x042b
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#define KEYBOARD_METRICS_BUFF_SIZE 512
static char g_m_buf_kb[KEYBOARD_METRICS_BUFF_SIZE];
#endif

enum detect_threshold {
	THRESHOLD_L = 0,
	THRESHOLD_H = 1,
};

enum pogo_event_type {
	TYPE_NOTHING = 0,
	TYPE_MOISTURE = 1,
	TYPE_KEYBOARD = 2,
};

enum ld_state_type {
	TYPE_DRY = 0,
	TYPE_WET = 1,
};

static const unsigned int pogo_extcon_pin[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

struct pogo_extcon_info {
	enum pogo_event_type event;
	unsigned int nothing_threshold[2];
	unsigned int keyboard_threshold[2];
	unsigned int moisture_threshold[2];
	unsigned int work_interval[3];
	unsigned int current_interval;
	unsigned int wet_count;

	struct device *dev;
	struct extcon_dev *edev;
	struct extcon_dev *hall_edev;
	struct iio_channel *adc_ch;
	struct switch_dev ld_switch;
	struct wakeup_source wake_lock;
	struct wakeup_source heartbeat_wake_lock;
	struct delayed_work routine_work;
	struct delayed_work heartbeat_work;
	struct notifier_block pogo_notifier;
	struct mutex pogo_lock;
	struct mutex routine_lock;
	struct input_dev *input_dev;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	struct notifier_block disp_notifier;
#endif
	struct notifier_block hall_notifier;

	struct pinctrl *pogo_pinctrl;
	struct pinctrl_state *detect_dis;
	struct pinctrl_state *detect_en;
	struct pinctrl_state *dock_dis;

	bool is_blank;
	bool force_dis_keyboard;
	bool enable_hall_related;
};

static int pogo_extcon_pinctrl_parse_dt(struct pogo_extcon_info *info)
{
	int ret = 0;

	info->pogo_pinctrl = devm_pinctrl_get(info->dev);
	if (IS_ERR(info->pogo_pinctrl)) {
		ret = PTR_ERR(info->pogo_pinctrl);
		dev_err(info->dev, "%s: Cannot find pinctrl, ret = %d\n", __func__, ret);
		goto out;
	}

	info->detect_dis = pinctrl_lookup_state(info->pogo_pinctrl, "detect_disable");
	if (IS_ERR(info->detect_dis)) {
		ret = PTR_ERR(info->detect_dis);
		dev_err(info->dev, "%s: Cannot find pinctrl detect_disable, ret = %d\n",
			__func__, ret);
		goto out;
	}

	info->detect_en = pinctrl_lookup_state(info->pogo_pinctrl, "detect_enable");
	if (IS_ERR(info->detect_en)) {
		ret = PTR_ERR(info->detect_en);
		dev_err(info->dev, "%s: Cannot find pinctrl detect_enable, ret = %d\n",
			__func__, ret);
		goto out;
	}

	/* For HVT device, disable dock-related switch */
	info->dock_dis = pinctrl_lookup_state(info->pogo_pinctrl, "dock_disable");
	if (!IS_ERR(info->dock_dis)) {
		dev_info(info->dev, "%s: HVT device, disable dock-related switch\n", __func__);
		pinctrl_select_state(info->pogo_pinctrl, info->dock_dis);
	}

	pinctrl_select_state(info->pogo_pinctrl, info->detect_dis);

out:
	return ret;
}

static int pogo_extcon_parse_dt(struct pogo_extcon_info *info)
{
	struct device *dev = info->dev;
	struct device_node *np = dev->of_node;
	int ret = 0;
	unsigned int threshold[2] = {0};
	unsigned int work_interval[3] = {0};

	if (!np)
		return -EINVAL;

	ret = pogo_extcon_pinctrl_parse_dt(info);
	if (ret) {
		dev_err(info->dev, "%s: Failed to parse pinctrl\n",
			__func__);
		goto out;
	}

	if (of_property_read_bool(np, "extcon")) {
		dev_info(info->dev, "get extcon device\n");
		info->hall_edev = extcon_get_edev_by_phandle(info->dev, 0);
		if (IS_ERR(info->hall_edev)) {
			dev_err(info->dev, "couldn't get hall extcon device\n");
			info->enable_hall_related = false;
		} else {
			dev_info(info->dev, "get hall extcon device\n");
			info->enable_hall_related = true;
		}
	} else {
		dev_err(info->dev, "couldn't get extcon device\n");
		info->enable_hall_related = false;
	}

	ret = of_property_read_u32_array(np, "work-interval", work_interval,
		ARRAY_SIZE(work_interval));
	if (ret < 0) {
		dev_err(info->dev, "%s: read work_interval dts failed, use default parameter\n",
			__func__);
	} else {
		memcpy(&info->work_interval, work_interval, sizeof(work_interval));
	}

	ret = of_property_read_u32_array(np, "keyboard-threshold", threshold,
		ARRAY_SIZE(threshold));
	if (ret) {
		dev_err(info->dev, "%s: Failed to parse keyboard-threshold, use default value\n",
			__func__);
	} else {
		memcpy(&info->keyboard_threshold, threshold, sizeof(threshold));
	}

	ret = of_property_read_u32_array(np, "moisture-threshold", threshold,
		ARRAY_SIZE(threshold));
	if (ret) {
		dev_err(info->dev, "%s: Failed to parse moisture-threshold, use default value\n",
			__func__);
	} else {
		memcpy(&info->moisture_threshold, threshold, sizeof(threshold));
	}

	ret = of_property_read_u32_array(np, "nothing-threshold", threshold,
		ARRAY_SIZE(threshold));
	if (ret) {
		dev_err(info->dev, "%s: Failed to parse nothing-threshold, use default value\n",
			__func__);
	} else {
		memcpy(&info->nothing_threshold, threshold, sizeof(threshold));
	}

out:
	return ret;
}

static bool is_usb_port1_rdy(struct pogo_extcon_info *info)
{
	bool ready = true;
	struct device_node *node = NULL;

	node = of_parse_phandle(info->dev->of_node, "usb", 0);
	if (node) {
		ready = !of_property_read_bool(node, "init-block");
		dev_info(info->dev, "usb port1 init ready = %d\n", ready);
	} else
		dev_notice(info->dev, "usb node missing or invalid\n");
	return ready;
}

static void pogo_enable_otg_keyboard(struct pogo_extcon_info *info, bool en)
{
	dev_info(info->dev, "%s:en=%d\n", __func__, en);

	if (en) {
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, true);
	} else {
		extcon_set_state_sync(info->edev, EXTCON_USB_HOST, false);
	}
}

static int pogo_get_adc_channel(struct pogo_extcon_info *info)
{
	int ret = 0;

	info->adc_ch = devm_iio_channel_get(info->dev, "pogo_extcon_adc_channel");
	if (IS_ERR(info->adc_ch)) {
		ret = PTR_ERR(info->adc_ch);
		dev_err(info->dev, "%s: IIO channel not found: %d\n", __func__, ret);
	}

	return ret;
}

static int pogo_extcon_get_detect_adc(struct pogo_extcon_info *info)
{
	int ret = 0;
	int avg_adc_mv = 0, adc1_mv = 0, adc2_mv = 0;

	mutex_lock(&info->pogo_lock);
	pinctrl_select_state(info->pogo_pinctrl, info->detect_en);
	msleep(5);
	ret = iio_read_channel_processed(info->adc_ch, &adc1_mv);
	if (ret < 0) {
		pr_err("%s: IIO channel read failed %d\n", __func__, ret);
		goto out;
	}

	msleep(5);
	ret = iio_read_channel_processed(info->adc_ch, &adc2_mv);
	if (ret < 0) {
		pr_err("%s: IIO channel read failed %d\n", __func__, ret);
		goto out;
	}
	pinctrl_select_state(info->pogo_pinctrl, info->detect_dis);

	avg_adc_mv = (adc1_mv + adc2_mv) / 2;
	mutex_unlock(&info->pogo_lock);

	return avg_adc_mv;

out:
	mutex_unlock(&info->pogo_lock);
	return ret;
}

static inline bool detect_is_in_range(int mv,
	int range_l, int range_h)
{
	return (mv >= range_l) && (mv <= range_h);
}

static int pogo_extcon_detect_event(struct pogo_extcon_info *info)
{
	int event = 0;
	int detect_vol = 0;
	int ret = 0;
	static int pre_event = 0;

	ret = pogo_extcon_get_detect_adc(info);
	if (ret < 0) {
		dev_err(info->dev, "%s: detect voltage failed, keep pre_event\n", __func__);
		return pre_event;
	}

	detect_vol = ret;

	if (detect_is_in_range(detect_vol, info->nothing_threshold[THRESHOLD_L],
			info->nothing_threshold[THRESHOLD_H])) {
		event = TYPE_NOTHING;
	} else if (detect_is_in_range(detect_vol, info->keyboard_threshold[THRESHOLD_L],
			info->keyboard_threshold[THRESHOLD_H])) {
		event = TYPE_KEYBOARD;
	} else if (detect_is_in_range(detect_vol, info->moisture_threshold[THRESHOLD_L],
			info->moisture_threshold[THRESHOLD_H])) {
		event = TYPE_MOISTURE;
	} else {
		dev_err(info->dev, "%s: detect voltage=%dmv, out of range\n", __func__, detect_vol);
	}

	if (pre_event != event)
		dev_info(info->dev, "%s: detect voltage=%dmv, pre_event=%d, event=%d\n",
			__func__, detect_vol, pre_event, event);

	pre_event = event;

	return event;
}

static void pogo_switch_set_state(struct switch_dev *sdev, int state)
{
	int pre_state = 0;

	pre_state = switch_get_state(sdev);

	if (pre_state == state)
		return;

	pr_info( "%s: set state: %d\n", __func__, state);
	switch_set_state(sdev, state);
	pre_state = state;
}

static void __pogo_report_event(struct pogo_extcon_info *info, int event)
{
	int pre_event = info->event;

	if (pre_event == event)
		return;

	info->event = event;

	//For Nothing -> Keyboard and Keyboard -> Nothing, no need to report event
	if ((pre_event == TYPE_NOTHING && event == TYPE_KEYBOARD)
		 || (pre_event == TYPE_KEYBOARD && event == TYPE_NOTHING))
		return;

	switch (event) {
	case TYPE_NOTHING:
	case TYPE_KEYBOARD:
		pogo_switch_set_state(&info->ld_switch, TYPE_DRY);
		break;
	case TYPE_MOISTURE:
		pogo_switch_set_state(&info->ld_switch, TYPE_WET);
		break;
	default:
		break;
	}
}

static void pogo_report_event(struct pogo_extcon_info *info, int event)
{
	switch (event) {
	case TYPE_NOTHING:
	case TYPE_KEYBOARD:
		info->wet_count = 0;
		break;
	case TYPE_MOISTURE:
		info->wet_count ++;
		break;
	default:
		break;
	}

	if (info->wet_count >= 3 || info->wet_count == 0)
		__pogo_report_event(info,event);
}

static inline int pogo_select_interval(struct pogo_extcon_info *info,
	enum pogo_event_type event)
{
	if (event >= 0 && event < ARRAY_SIZE(info->work_interval))
		return info->work_interval[event];

	dev_err(info->dev, "event doesn't match work_interval,"
		" use work_interval[TYPE_MOISTURE] as the default\n");
	return info->work_interval[TYPE_MOISTURE];
}

static void pogo_extcon_work(struct pogo_extcon_info *info)
{
	int event = TYPE_NOTHING;
	unsigned long elapsed_s = 0;
	static ktime_t time_start;

	static int pre_event = TYPE_NOTHING;

	mutex_lock(&info->routine_lock);

	pre_event = info->event;

	event = pogo_extcon_detect_event(info);

	/* The event has not changed. Skip processing */
	if (pre_event == event)
		goto out;

	if (event == TYPE_KEYBOARD) {
		/* Other event -> Keyboard */
		if (!info->force_dis_keyboard)
			pogo_enable_otg_keyboard(info, true);
		time_start = ktime_get_boottime();
		queue_delayed_work(system_freezable_wq, &info->heartbeat_work, 0);
	} else {
		if (pre_event == TYPE_KEYBOARD) {
			/* Keyboard -> Other event */
			pogo_enable_otg_keyboard(info, false);
			input_report_switch(info->input_dev,
				SW_TABLET_MODE, false);
			input_sync(info->input_dev);
			elapsed_s = ktime_to_ms(ktime_sub(ktime_get_boottime(), time_start)) / 1000;
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
			minerva_metrics_log(g_m_buf_kb, KEYBOARD_METRICS_BUFF_SIZE,
				"%s:%s:100:%s,%s,%s,%s,KeyboardVid=%d;IN,KeyboardPid=%d;IN,KeyboardSessionDuration=%lu;IN:us-east-1",
				METRICS_KEYBOARD_GROUP_ID, METRICS_KEYBOARD_SCHEMA_ID, PREDEFINED_ESSENTIAL_KEY,
				PREDEFINED_DEVICE_ID_KEY, PREDEFINED_CUSTOMER_ID_KEY, PREDEFINED_MODEL_KEY,
				VENDOR_ID_LAB126, DEVICE_ID_LAB126_abc123_KB, elapsed_s);
#endif
			pr_info("%s: session duration=%lu\n", __func__, elapsed_s);
			cancel_delayed_work(&info->heartbeat_work);
		}
	}

	pre_event = event;

	info->current_interval = pogo_select_interval(info, event);
	pogo_report_event(info, event);

	dev_info(info->dev, "%s: event=%d, interval=%d\n",
		__func__, event, info->current_interval);

out:
	mutex_unlock(&info->routine_lock);

	return;
}

static void pogo_extcon_routine_work(struct work_struct *work)
{
	static bool usb_rdy = 0;
	struct pogo_extcon_info *info =
		 container_of(work, struct pogo_extcon_info, routine_work.work);

	__pm_stay_awake(&info->wake_lock);


	if (!usb_rdy) {
		if (is_usb_port1_rdy(info)) {
			dev_info(info->dev, "%s USB port1 init ready\n", __func__);
			usb_rdy = 1;
		} else {
			dev_info(info->dev, "%s USB port1 init not ready\n", __func__);
			info->current_interval = DETECT_DEFAULT_MS;
			goto skip;
		}
	}

	pogo_extcon_work(info);

skip:
	if (!(info->is_blank && info->event == TYPE_NOTHING))
		queue_delayed_work(system_freezable_wq, &info->routine_work,
			 msecs_to_jiffies(info->current_interval));

	__pm_relax(&info->wake_lock);
}

static void pogo_extcon_heartbeat_work(struct work_struct *work)
{
	struct pogo_extcon_info *info =
		 container_of(work, struct pogo_extcon_info, heartbeat_work.work);

	__pm_stay_awake(&info->heartbeat_wake_lock);

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	minerva_metrics_log(g_m_buf_kb, KEYBOARD_METRICS_BUFF_SIZE,
				"%s:%s:100:%s,%s,%s,%s,KeyboardVid=%d;IN,KeyboardPid=%d;IN,KeyboardHeartbeat=1;IN:us-east-1",
				METRICS_KEYBOARD_GROUP_ID, METRICS_KEYBOARD_SCHEMA_ID, PREDEFINED_ESSENTIAL_KEY,
				PREDEFINED_DEVICE_ID_KEY, PREDEFINED_CUSTOMER_ID_KEY, PREDEFINED_MODEL_KEY,
				VENDOR_ID_LAB126, DEVICE_ID_LAB126_abc123_KB);
#endif
	pr_info("%s\n", __func__);
	queue_delayed_work(system_freezable_wq, &info->heartbeat_work,
			 msecs_to_jiffies(KEYBOARD_HEARTBEAT));
	__pm_relax(&info->heartbeat_wake_lock);
}

static int pogo_notify_callback(struct notifier_block *self,
		unsigned long action, void *data)
{
	struct pogo_extcon_info *info = container_of(self,
			struct pogo_extcon_info, pogo_notifier);

	dev_info(info->dev, "%s: notify event type 0x%x\n",
		__func__, (unsigned int)action);
	switch (action) {
	case USB_PORT_DEVICE_CONNECT:
		break;
	case USB_PORT_DEVICE_DISCONNECT:
		mutex_lock(&info->routine_lock);
		if (info->event == TYPE_KEYBOARD &&
			pogo_extcon_detect_event(info) != TYPE_KEYBOARD) {
			cancel_delayed_work(&info->routine_work);
			queue_delayed_work(system_freezable_wq, &info->routine_work, 0);
		}
		mutex_unlock(&info->routine_lock);
		break;
	default:
		break;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
static int pogo_disp_notifier_callback(struct notifier_block *self,
		unsigned long event, void *value)
{
	int blank;
	struct pogo_extcon_info *info = container_of(self,
		struct pogo_extcon_info, disp_notifier);

	int *data = (int *)value;

	blank = *data;
	if (blank == info->is_blank)
		return 0;

	info->is_blank = (blank == MTK_DISP_BLANK_POWERDOWN) ? true : false;

	dev_info(info->dev, "%s: blank_state=%d, event=%d\n",
		__func__, blank, info->event);

	if (info->event == TYPE_NOTHING && event == MTK_DISP_EARLY_EVENT_BLANK) {
		switch (blank) {
		case MTK_DISP_BLANK_UNBLANK:
			queue_delayed_work(system_freezable_wq,
				&info->routine_work, 0);
			dev_info(info->dev, "%s: screen on, pogo start detecting\n",
				__func__);
			break;
		case MTK_DISP_BLANK_POWERDOWN:
			cancel_delayed_work_sync(&info->routine_work);
			dev_info(info->dev, "%s: screen off, pogo stop detecting\n",
				__func__);
			break;
		default:
			break;
		}
	}

	return 0;
}
#endif

static int pogo_hall_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct pogo_extcon_info *info = container_of(self,
			struct pogo_extcon_info, hall_notifier);

	struct input_dev *input_dev = info->input_dev;

	if (info->force_dis_keyboard == event)
		return 0;

	info->force_dis_keyboard = event;
	dev_info(info->dev, "%s: notify hall event, force disable keyboard: %s\n",
		__func__, info->force_dis_keyboard ? "true" : "false");

	mutex_lock(&info->routine_lock);
	if (!event) {
		if (info->event == TYPE_KEYBOARD &&
			pogo_extcon_detect_event(info) == TYPE_KEYBOARD) {
			pogo_enable_otg_keyboard(info, true);
			input_report_switch(input_dev,
				SW_TABLET_MODE, false);
			input_sync(input_dev);
			dev_info(info->dev, "%s: bookcover opened, enable keyboard\n",
				__func__);
			}
	} else {
		if (info->event == TYPE_KEYBOARD &&
			pogo_extcon_detect_event(info) == TYPE_KEYBOARD) {
			pogo_enable_otg_keyboard(info, false);
			dev_info(info->dev, "%s: bookcover closed, disable keyboard\n",
				__func__);
			input_report_switch(input_dev,
				SW_TABLET_MODE, true);
			input_sync(input_dev);
			}
	}
	mutex_unlock(&info->routine_lock);

	return 0;
}

static ssize_t pogo_event_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct pogo_extcon_info *info = dev->driver_data;

	return scnprintf(buffer, PAGE_SIZE, "%d\n", info->event);
}

static ssize_t pogo_id_voltage_show(struct device *dev,
		struct device_attribute *attr,
		char *buffer)
{
	struct pogo_extcon_info *info = dev->driver_data;

	return scnprintf(buffer, PAGE_SIZE, "%d\n", pogo_extcon_get_detect_adc(info));
}

static DEVICE_ATTR(event, 0444, pogo_event_show, NULL);
static DEVICE_ATTR(id_voltage, 0444, pogo_id_voltage_show, NULL);

static struct attribute *pogo_sysfs_attrs[] = {
	&dev_attr_event.attr,
	&dev_attr_id_voltage.attr,
	NULL,
};

static const struct attribute_group pogo_sysfs_group_attrs = {
	.attrs = pogo_sysfs_attrs,
};

static int pogo_extcon_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct pogo_extcon_info *info;
	struct input_dev *input_dev_tlm;
	int ret;

	if (!np)
		return -EINVAL;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	info->event = TYPE_NOTHING;
	info->nothing_threshold[THRESHOLD_L]	= NOTHING_THRESHOLD_L;
	info->nothing_threshold[THRESHOLD_H] 	= NOTHING_THRESHOLD_H;
	info->keyboard_threshold[THRESHOLD_L] 	= KEYBOARD_THRESHOLD_L;
	info->keyboard_threshold[THRESHOLD_H] 	= KEYBOARD_THRESHOLD_H;
	info->moisture_threshold[THRESHOLD_L] 	= MOISTURE_THRESHOLD_L;
	info->moisture_threshold[THRESHOLD_H] 	= MOISTURE_THRESHOLD_H;
	info->work_interval[TYPE_NOTHING] 	= WORK_INTERVAL_NOTHING;
	info->work_interval[TYPE_MOISTURE] 	= WORK_INTERVAL_WET;
	info->work_interval[TYPE_KEYBOARD] 	= WORK_INTERVAL_KEYBOARD;
	info->wet_count				= 0;

	mutex_init(&info->pogo_lock);
	mutex_init(&info->routine_lock);
	wakeup_source_add(&info->wake_lock);
	wakeup_source_add(&info->heartbeat_wake_lock);
	INIT_DELAYED_WORK(&info->routine_work, pogo_extcon_routine_work);
	INIT_DELAYED_WORK(&info->heartbeat_work, pogo_extcon_heartbeat_work);

	ret = pogo_extcon_parse_dt(info);
	if (ret) {
		dev_err(info->dev, "%s: parse dts failed, ret: %d\n",
			__func__, ret);
		wakeup_source_remove(&info->wake_lock);
		wakeup_source_remove(&info->heartbeat_wake_lock);
		return ret;
	}

	ret = pogo_get_adc_channel(info);
	if (ret) {
		dev_err(dev, "%s: get adc channel failed, ret: %d\n",
			__func__, ret);
		ret = -EPROBE_DEFER;
		goto err_adc_channel;
	}

	info->edev = devm_extcon_dev_allocate(dev, pogo_extcon_pin);
	if (IS_ERR(info->edev)) {
		dev_err(dev, "%s: failed to allocate extcon device\n",
			__func__);
		ret = -ENOMEM;
		goto err_edev;
	}

	ret = devm_extcon_dev_register(dev, info->edev);
	if (ret < 0) {
		dev_err(dev, "%s: failed to register extcon device\n",
			__func__);
		goto err_edev;
	}

	info->ld_switch.name = "pogo_ld";
	info->ld_switch.index = 0;
	info->ld_switch.state = TYPE_DRY;
	ret = switch_dev_register(&info->ld_switch);
	if (ret) {
		dev_err(info->dev, "%s: switch_dev_register pogo-lde Fail\n",
			__func__);
		goto err_ld_switch;
	}

	if (info->enable_hall_related) {
		ret = extcon_get_state(info->hall_edev, EXTCON_BOOKCOVER_STATE);
		if (ret < 0)
			dev_err(info->dev, "hall state error\n");
		else
			info->force_dis_keyboard = !!ret;

		info->hall_notifier.notifier_call = pogo_hall_notifier_callback;
		ret = extcon_register_notifier(info->hall_edev, EXTCON_BOOKCOVER_STATE,
					&info->hall_notifier);
		if (ret < 0) {
			dev_err(info->dev, "couldn't register hall notifier\n");
			goto err_hall_notifier;
		}
	}

	info->pogo_notifier.notifier_call = pogo_notify_callback;
	usb_port_register_notify(&info->pogo_notifier);

	ret = sysfs_create_group(&info->dev->kobj, &pogo_sysfs_group_attrs);
	if (ret) {
		dev_err(dev, "%s: sysfs_create_group() failed, ret=%d\n",
			 __func__, ret);
		goto err_sysfs;
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	info->disp_notifier.notifier_call = pogo_disp_notifier_callback;
	ret =  mtk_disp_notifier_register("pogo-pin", &info->disp_notifier);
	if (ret) {
		dev_err(dev, "Unable to register disp_notifier: %d\n", ret);
		goto err_disp;
	}
#endif
	input_dev_tlm = devm_input_allocate_device(info->dev);
	if (IS_ERR_OR_NULL(input_dev_tlm)) {
		ret = -ENOMEM;
		goto err_input_dev;
	}
	input_dev_tlm->name = "abc123 Tablet Mode Switcher";
	input_set_capability(input_dev_tlm, EV_SW, SW_TABLET_MODE);
	ret = input_register_device(input_dev_tlm);
	if (ret)
		goto err_input_dev;

	info->input_dev = input_dev_tlm;

	platform_set_drvdata(pdev, info);
	device_init_wakeup(&pdev->dev, true);
	queue_delayed_work(system_freezable_wq, &info->routine_work, 0);

	dev_info(dev, "%s(w/o ID interrupt): probe successfully\n", __func__);

	return 0;

err_input_dev:
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
err_disp:
#endif
	sysfs_remove_group(&info->dev->kobj, &pogo_sysfs_group_attrs);
err_sysfs:
	if (info->enable_hall_related)
		extcon_unregister_notifier(info->hall_edev, EXTCON_BOOKCOVER_STATE,
					&info->hall_notifier);
err_hall_notifier:
	usb_port_unregister_notify(&info->pogo_notifier);
	switch_dev_unregister(&info->ld_switch);
err_ld_switch:
err_edev:
err_adc_channel:
	mutex_destroy(&info->pogo_lock);
	mutex_destroy(&info->routine_lock);
	device_init_wakeup(&pdev->dev, false);
	wakeup_source_remove(&info->wake_lock);
	wakeup_source_remove(&info->heartbeat_wake_lock);
	if (info)
		devm_kfree(&pdev->dev, info);
	info = NULL;
	dev_err(dev, "%s: probe failed\n", __func__);

	return ret;
}

static int pogo_extcon_remove(struct platform_device *pdev)
{
	return 0;
}

static void pogo_extcon_shutdown(struct platform_device *pdev)
{
	struct pogo_extcon_info *info = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&info->routine_work);
	cancel_delayed_work_sync(&info->heartbeat_work);
	input_unregister_device(info->input_dev);
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	mtk_disp_notifier_unregister(&info->disp_notifier);
#endif
	if (info->enable_hall_related)
		extcon_unregister_notifier(info->hall_edev, EXTCON_BOOKCOVER_STATE,
					&info->hall_notifier);
	usb_port_unregister_notify(&info->pogo_notifier);
	device_init_wakeup(&pdev->dev, false);
	info = NULL;
}

static const struct of_device_id pogo_extcon_dt_match[] = {
	{ .compatible = "amzn,extcon-pogo-pin", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, pogo_extcon_dt_match);

static const struct platform_device_id pogo_extcon_platform_ids[] = {
	{ .name = "extcon-pogo-pin", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, pogo_extcon_platform_ids);

static struct platform_driver pogo_extcon_driver = {
	.probe		= pogo_extcon_probe,
	.remove		= pogo_extcon_remove,
	.shutdown 	= pogo_extcon_shutdown,
	.driver		= {
		.name	= "extcon-pogo-pin",
		.of_match_table = pogo_extcon_dt_match,
	},
	.id_table = pogo_extcon_platform_ids,
};

static int __init pogo_extcon_init(void)
{
	return platform_driver_register(&pogo_extcon_driver);
}

static void __exit pogo_extcon_exit(void)
{
	return platform_driver_unregister(&pogo_extcon_driver);
}
late_initcall(pogo_extcon_init);
module_exit(pogo_extcon_exit);

MODULE_AUTHOR("Yin Zhang <zhangyin@huaqin.com>");
MODULE_DESCRIPTION("POGO PIN extcon driver");
MODULE_LICENSE("GPL v2");
