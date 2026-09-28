/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2022 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ilitek_9882t.h"

#define DTS_INT_GPIO "touch,irq-gpio"
#define DTS_RESET_GPIO "touch,reset-gpio"
#define DTS_OF_NAME "tchip,ilitek"

void ili_hid_tp_reset(void)
{
	ILI_INFO("edge delay = %d\n", ilits->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	gpio_direction_output(ilits->tp_rst, 1);
	mdelay(1);
	gpio_set_value(ilits->tp_rst, 0);
	mdelay(5);
	gpio_set_value(ilits->tp_rst, 1);
	mdelay(ilits->rst_edge_delay);
}
#if 0
void ili_hid_input_register(void)
{
	ILI_INFO();

	ilits->input = input_allocate_device();
	if (ERR_ALLOC_MEM(ilits->input)) {
		ILI_ERR("Failed to allocate touch input device\n");
		input_free_device(ilits->input);
		return;
	}

	ilits->input->name = ilits->hwif->name;
	ilits->input->phys = ilits->phys;
	ilits->input->dev.parent = ilits->dev;
	ilits->input->id.bustype = ilits->hwif->bus_type;

	/* set the supported event type for input device */
	set_bit(EV_ABS, ilits->input->evbit);
	set_bit(EV_SYN, ilits->input->evbit);
	set_bit(EV_KEY, ilits->input->evbit);
	set_bit(BTN_TOUCH, ilits->input->keybit);
	set_bit(BTN_TOOL_FINGER, ilits->input->keybit);
	set_bit(INPUT_PROP_DIRECT, ilits->input->propbit);

	input_set_abs_params(ilits->input, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, ilits->panel_wid - 1, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, ilits->panel_hei - 1, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	if (MT_PRESSURE)
		input_set_abs_params(ilits->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(ilits->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* Gesture keys register */
	input_set_capability(ilits->input, EV_KEY, KEY_POWER);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_O);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_E);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_M);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_W);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_S);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_V);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_C);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_F);

	__set_bit(KEY_GESTURE_POWER, ilits->input->keybit);
	__set_bit(KEY_GESTURE_UP, ilits->input->keybit);
	__set_bit(KEY_GESTURE_DOWN, ilits->input->keybit);
	__set_bit(KEY_GESTURE_LEFT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_RIGHT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_O, ilits->input->keybit);
	__set_bit(KEY_GESTURE_E, ilits->input->keybit);
	__set_bit(KEY_GESTURE_M, ilits->input->keybit);
	__set_bit(KEY_GESTURE_W, ilits->input->keybit);
	__set_bit(KEY_GESTURE_S, ilits->input->keybit);
	__set_bit(KEY_GESTURE_V, ilits->input->keybit);
	__set_bit(KEY_GESTURE_Z, ilits->input->keybit);
	__set_bit(KEY_GESTURE_C, ilits->input->keybit);
	__set_bit(KEY_GESTURE_F, ilits->input->keybit);

	/* register the input device to input sub-system */
	if (input_register_device(ilits->input) < 0) {
		ILI_ERR("Failed to register touch input device\n");
		input_unregister_device(ilits->input);
		input_free_device(ilits->input);
	}
}

void ili_hid_input_pen_register(void)
{
	ilits->input_pen = input_allocate_device();
	if (ERR_ALLOC_MEM(ilits->input_pen)) {
		ILI_ERR("Failed to allocate touch input device\n");
		input_free_device(ilits->input_pen);
		return;
	}

	ilits->input_pen->name = PEN_INPUT_DEVICE;
	ilits->input_pen->phys = ilits->phys;
	ilits->input_pen->dev.parent = ilits->dev;
	ilits->input_pen->id.bustype = ilits->hwif->bus_type;

	/* set the supported event type for input device */
	set_bit(EV_ABS, ilits->input_pen->evbit);
	set_bit(EV_SYN, ilits->input_pen->evbit);
	set_bit(EV_KEY, ilits->input_pen->evbit);
	set_bit(BTN_TOUCH, ilits->input_pen->keybit);
	set_bit(BTN_TOOL_PEN, ilits->input_pen->keybit);
	set_bit(BTN_STYLUS, ilits->input_pen->keybit);
	set_bit(BTN_STYLUS2, ilits->input_pen->keybit);
	set_bit(INPUT_PROP_DIRECT, ilits->input_pen->propbit);

	input_set_abs_params(ilits->input_pen, ABS_X, 0, ilits->panel_wid, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_Y, 0, ilits->panel_hei, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_PRESSURE, 0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_TILT_X, -60, 60, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_TILT_Y, -60, 60, 0, 0);
	input_set_abs_params(ilits->input_pen, ABS_DISTANCE, 0, 1, 0, 0);

	/* register the input device to input sub-system */
	if (input_register_device(ilits->input_pen) < 0) {
		ILI_ERR("Failed to register touch input device\n");
		input_unregister_device(ilits->input_pen);
		input_free_device(ilits->input_pen);
	}
	ILI_INFO("Input Pen device for Single Touch\n");
	ILI_DBG("Input pen Register.\n");
}

#if REGULATOR_POWER
void ili_hid_plat_regulator_power_on(bool status)
{
	ILI_INFO("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (ilits->vdd) {
			if (regulator_enable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_enable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	} else {
		if (ilits->vdd) {
			if (regulator_disable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_disable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	}
	atomic_set(&ilits->ice_stat, DISABLE);
	mdelay(5);
}

static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	ilits->vdd = regulator_get(ilits->dev, vdd_name);
	if (ERR_ALLOC_MEM(ilits->vdd)) {
		ILI_ERR("regulator_get VDD fail\n");
		ilits->vdd = NULL;
	}
	if (regulator_set_voltage(ilits->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		ILI_ERR("Failed to set VDD %d\n", VDD_VOLTAGE);

	ilits->vcc = regulator_get(ilits->dev, vcc_name);
	if (ERR_ALLOC_MEM(ilits->vcc)) {
		ILI_ERR("regulator_get VCC fail.\n");
		ilits->vcc = NULL;
	}
	if (regulator_set_voltage(ilits->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		ILI_ERR("Failed to set VCC %d\n", VCC_VOLTAGE);

	ili_hid_plat_regulator_power_on(true);
}
#endif
#endif
static int ilitek_plat_gpio_register(void)
{
	int ret = 0;
#if 0
	u32 flag;
	struct device_node *dev_node = ilits->dev->of_node;

	ilits->tp_int = of_get_named_gpio_flags(dev_node, DTS_INT_GPIO, 0, &flag);
	ilits->tp_rst = of_get_named_gpio_flags(dev_node, DTS_RESET_GPIO, 0, &flag);

	ILI_INFO("TP INT: %d\n", ilits->tp_int);
	ILI_INFO("TP RESET: %d\n", ilits->tp_rst);

	if (!gpio_is_valid(ilits->tp_int)) {
		ILI_ERR("Invalid INT gpio: %d\n", ilits->tp_int);
		return -EBADR;
	}

	if (!gpio_is_valid(ilits->tp_rst)) {
		ILI_ERR("Invalid RESET gpio: %d\n", ilits->tp_rst);
		return -EBADR;
	}
	ret = gpio_request(ilits->tp_int, "TP_INT");
	if (ret < 0) {
		ILI_ERR("Request IRQ GPIO failed, ret = %d\n", ret);
		gpio_free(ilits->tp_int);
		ret = gpio_request(ilits->tp_int, "TP_INT");
		if (ret < 0) {
			ILI_ERR("Retrying request INT GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}

	ret = gpio_request(ilits->tp_rst, "TP_RESET");
	if (ret < 0) {
		ILI_ERR("Request RESET GPIO failed, ret = %d\n", ret);
		gpio_free(ilits->tp_rst);
		ret = gpio_request(ilits->tp_rst, "TP_RESET");
		if (ret < 0) {
			ILI_ERR("Retrying request RESET GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}

out:
	gpio_direction_input(ilits->tp_int);
#endif
	return ret;
}

void ili_hid_irq_disable(void)
{
	unsigned long flag;
	struct i2c_client *client = ilits->i2c;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == DISABLE)
		goto out;

	if (!client->irq) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", client->irq);
		goto out;
	}

	if (atomic_read(&ilits->irq_stat)) {
		disable_irq_nosync(client->irq);
		atomic_set(&ilits->irq_stat, DISABLE);
	}
	ILI_DBG("Disable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

void ili_hid_irq_enable(void)
{
	unsigned long flag;
	struct i2c_client *client = ilits->i2c;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == ENABLE)
		goto out;

	if (!client->irq) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", client->irq);
		goto out;
	}

	if (!(atomic_read(&ilits->irq_stat))) {
		enable_irq(client->irq);
		atomic_set(&ilits->irq_stat, ENABLE);
	}
	ILI_DBG("Enable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != ilits->irq_num) {
		ILI_ERR("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&ilits->cmd_int_check) == ENABLE) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_DBG("CMD INT detected, ignore\n");
		wake_up(&(ilits->inq));
		return IRQ_HANDLED;
	}

	if (ilits->prox_near) {
		ILI_INFO("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ILI_DBG("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d, igr:%d\n",
		ilits->report, atomic_read(&ilits->tp_reset),
		atomic_read(&ilits->fw_stat), atomic_read(&ilits->tp_sw_mode),
		atomic_read(&ilits->mp_stat), atomic_read(&ilits->tp_sleep),
		atomic_read(&ilits->esd_stat),
		atomic_read(&ilits->ignore_report));

	if (!ilits->report || atomic_read(&ilits->tp_reset) ||
	    atomic_read(&ilits->ignore_report) ||
	    atomic_read(&ilits->fw_stat) || atomic_read(&ilits->tp_sw_mode) ||
	    atomic_read(&ilits->mp_stat) || atomic_read(&ilits->tp_sleep) ||
	    atomic_read(&ilits->esd_stat)) {
		ILI_DBG("ignore interrupt !\n");
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&ilits->touch_mutex)) {
		ILI_DBG("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&ilits->touch_mutex);
	mutex_unlock(&ilits->touch_mutex);
	return IRQ_HANDLED;
}

void ili_hid_irq_unregister(void)
{
	devm_free_irq(ilits->dev, ilits->irq_num, NULL);
}

int ili_hid_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;

	atomic_set(&ilits->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		ilits->irq_num = gpio_to_irq(ilits->tp_int);
		get_irq_pin = true;
	}

	ILI_INFO("ilits->irq_num = %d\n", ilits->irq_num);

	ret = devm_request_threaded_irq(ilits->dev, ilits->irq_num,
					ilitek_plat_isr_top_half,
					ilitek_plat_isr_bottom_half,
					type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	if (type == IRQF_TRIGGER_RISING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ILI_ERR("Failed to register irq handler, irq = %d, ret = %d\n",
			ilits->irq_num, ret);

	atomic_set(&ilits->irq_stat, ENABLE);

	return ret;
}
#if 1
#if SPRD_SYSFS_SUSPEND_RESUME
static ssize_t ts_suspend_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", ilits->tp_suspend ? "true" : "false");
}

static ssize_t ts_suspend_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	if ((buf[0] == '1') && !ilits->tp_suspend)
		ili_hid_sleep_handler(TP_DEEP_SLEEP);
	else if ((buf[0] == '0') && ilits->tp_suspend)
		ili_hid_sleep_handler(TP_RESUME);

	return count;
}
static DEVICE_ATTR_RW(ts_suspend);

static struct attribute *ilitek_dev_suspend_atts[] = {
	&dev_attr_ts_suspend.attr, NULL
};

static const struct attribute_group ilitek_dev_suspend_atts_group = {
	.attrs = ilitek_dev_suspend_atts,
};

static const struct attribute_group *ilitek_dev_attr_groups[] = {
	&ilitek_dev_suspend_atts_group, NULL
};

int ili_sysfs_add_device(struct device *dev)
{
	int ret = 0, i;

	for (i = 0; ilitek_dev_attr_groups[i]; i++) {
		ret = sysfs_create_group(&dev->kobj, ilitek_dev_attr_groups[i]);
		if (ret) {
			while (--i >= 0) {
				sysfs_remove_group(&dev->kobj,
						   ilitek_dev_attr_groups[i]);
			}
			break;
		}
	}

	return ret;
}

int ili_sysfs_remove_device(struct device *dev)
{
	int i;

	sysfs_remove_link(NULL, "touchscreen");
	for (i = 0; ilitek_dev_attr_groups[i]; i++) {
		sysfs_remove_group(&dev->kobj, ilitek_dev_attr_groups[i]);
	}

	return 0;
}
#else

#if 0
#if defined(CONFIG_FB) || defined(CONFIG_DRM_MSM)
#if defined(__DRM_PANEL_H__) && defined(DRM_PANEL_EARLY_EVENT_BLANK)
static struct drm_panel *active_panel;

static int ili_v3_drm_check_dt(struct device_node *np)
{
	int i = 0;
	int count = 0;
	struct device_node *node = NULL;
	struct drm_panel *panel = NULL;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0) {
		ILI_ERR("find drm_panel count(%d) fail", count);
		return 0;
	}
	ILI_INFO("find drm_panel count(%d) ", count);
	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		ILI_INFO("node%p", node);
		panel = of_drm_find_panel(node);
		ILI_INFO("panel%p ", panel);

		of_node_put(node);
		if (!IS_ERR(panel)) {
			ILI_INFO("find drm_panel successfully");
			active_panel = panel;
			return 0;
		}
	}

	ILI_ERR("no find drm_panel");
	return -ENODEV;
}

static int drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct drm_panel_notifier *evdata = data;
	int *blank = NULL;

	if (!evdata) {
		ILI_ERR("evdata is null");
		return 0;
	}

	if (!((event == DRM_PANEL_EARLY_EVENT_BLANK)
			|| (event == DRM_PANEL_EVENT_BLANK))) {
		ILI_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	ILI_INFO("DRM event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case DRM_PANEL_BLANK_UNBLANK:
		if (DRM_PANEL_EARLY_EVENT_BLANK == event) {
			ILI_INFO("resume: event = %lu, not care\n", event);
		} else if (DRM_PANEL_EVENT_BLANK == event) {
			ILI_INFO("resume: event = %lu, TP_RESUME\n", event);
		if (ili_hid_sleep_handler(TP_RESUME) < 0)
			ILI_ERR("TP resume failed\n");
		}
		break;
	case DRM_PANEL_BLANK_POWERDOWN:
		if (DRM_PANEL_EARLY_EVENT_BLANK == event) {
			ILI_INFO("suspend: event = %lu, TP_SUSPEND\n", event);
			if (ili_hid_sleep_handler(TP_DEEP_SLEEP) < 0)
				ILI_ERR("TP suspend failed\n");
		} else if (DRM_PANEL_EVENT_BLANK == event) {
			ILI_INFO("suspend: event = %lu, not care\n", event);
		}
		break;
	default:
		ILI_INFO("DRM BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}
#else
static int ilitek_plat_notifier_fb(struct notifier_block *self, unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;

	ILI_INFO("Notifier's event = %ld\n", event);

	/*
	 *	FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *	FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change occurred.
	 */
	if (evdata && evdata->data) {
		blank = evdata->data;
		switch (*blank) {
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_POWERDOWN:
#else
		case FB_BLANK_POWERDOWN:
#endif
#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_OFF:
#endif /* CONFIG_PLAT_SPRD */
			if (TP_SUSPEND_PRIO) {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EARLY_EVENT_BLANK)
#else
				if (event != FB_EARLY_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			} else {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EVENT_BLANK)
#else
				if (event != FB_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			}
			if (ili_hid_sleep_handler(TP_DEEP_SLEEP) < 0)
				ILI_ERR("TP suspend failed\n");
			break;
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_UNBLANK:
		case MSM_DRM_BLANK_NORMAL:
#else
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
#endif

#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_ON:
#endif /* CONFIG_PLAT_SPRD */

#ifdef CONFIG_DRM_MSM
			if (event == MSM_DRM_EVENT_BLANK)
#else
			if (event == FB_EVENT_BLANK)
#endif
			{
				if (ili_hid_sleep_handler(TP_RESUME) < 0)
					ILI_ERR("TP resume failed\n");

			}
			break;
		default:
			ILI_ERR("Unknown event, blank = %d\n", *blank);
			break;
		}
	}
	return NOTIFY_OK;
}
#endif /*#ifdef __DRM_PANEL_H__*/
#else
static void ilitek_plat_early_suspend(struct early_suspend *h)
{
	if (ili_hid_sleep_handler(TP_DEEP_SLEEP) < 0)
		ILI_ERR("TP suspend failed\n");
}

static void ilitek_plat_late_resume(struct early_suspend *h)
{
	if (ili_hid_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}
#endif /*defined(CONFIG_FB) || defined(CONFIG_DRM_MSM)*/
#endif // 1
#endif // 1

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
static int ilitek9882t_ts_disp_notifier_callback(struct notifier_block *self,
						 unsigned long event,
						 void *value)
{
	struct ilitek_ts_hid_data *ilits =
		container_of(self, struct ilitek_ts_hid_data, disp_notifier);
	int *data = (int *)value;

	if (ilits && value) {
		if (event == MTK_DISP_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_UNBLANK) {
				ILI_INFO("ili_ts_resume IN");
				if (ili_hid_sleep_handler(TP_RESUME) < 0) {
					ILI_ERR("TP resume failed\n");
					return -1;
				}
			}
		} else if (event == MTK_DISP_EARLY_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_POWERDOWN) {
				ILI_INFO("ili_ts_suspend IN");
				if (ili_hid_sleep_handler(TP_SUSPEND) < 0) {
					ILI_ERR("TP suspend failed\n");
					return -1;
				}
			}
		} else {
			ILI_ERR("Invalid notification callback event");
			return -1;
		}
	} else {
		ILI_ERR("ilitek9882t touch IC can not suspend or resume");
		return -1;
	}
	return 0;
}
#else
static int ilitek_plat_notifier_fb(struct notifier_block *self,
				   unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;

	ILI_INFO("Notifier's event = %ld\n", event);

	/*
	 *	FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *	FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change occurred.
	 */
	if (evdata && evdata->data) {
		blank = evdata->data;
		switch (*blank) {
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_POWERDOWN:
#else
		case FB_BLANK_POWERDOWN:
#endif
#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_OFF:
#endif /* CONFIG_PLAT_SPRD */
			if (TP_SUSPEND_PRIO) {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EARLY_EVENT_BLANK)
#else
				if (event != FB_EARLY_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			} else {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EVENT_BLANK)
#else
				if (event != FB_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			}
			if (ili_hid_sleep_handler(TP_DEEP_SLEEP) < 0)
				ILI_ERR("TP suspend failed\n");
			break;
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_UNBLANK:
		case MSM_DRM_BLANK_NORMAL:
#else
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
#endif

#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_ON:
#endif /* CONFIG_PLAT_SPRD */

#ifdef CONFIG_DRM_MSM
			if (event == MSM_DRM_EVENT_BLANK)
#else
			if (event == FB_EVENT_BLANK)
#endif
			{
				if (ili_hid_sleep_handler(TP_RESUME) < 0)
					ILI_ERR("TP resume failed\n");
			}
			break;
		default:
			ILI_ERR("Unknown event, blank = %d\n", *blank);
			break;
		}
	}
	return NOTIFY_OK;
}
#endif /*#ifdef __DRM_PANEL_H__*/

#endif

static void ilitek_plat_sleep_init(void)
{
	int ret;
	ILI_INFO("%s\n", __func__);

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)

	ilits->disp_notifier.notifier_call =
		ilitek9882t_ts_disp_notifier_callback;
	ret = mtk_disp_notifier_register("ilitek ic Touch",
					 &ilits->disp_notifier);
	if (ret) {
		ILI_ERR("Failed to register disp notifier client:%d", ret);
	}
#endif
}

int ilitek_hid_plat_probe(void)
{
#if 0
#ifdef __DRM_PANEL_H__
	int ret = 0;
#endif
#endif
	ILI_INFO("platform probe\n");
#if 0

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif
#ifdef __DRM_PANEL_H__
	ret = ili_v3_drm_check_dt(ilits->dev->of_node);
	if (ret) {
		ILI_ERR("[ili_v3_drm_check_dt] parse drm-panel fail");
	}
#endif
#endif
	if (ilitek_plat_gpio_register() < 0)
		ILI_ERR("Register gpio failed\n");

	//ili_hid_irq_register(ilits->irq_tirgger_type);
	atomic_set(&ilits->irq_stat, ENABLE);

	if (ili_hid_tddi_init() < 0) {
		ILI_ERR("ILITEK Driver probe failed\n");
		//ili_hid_irq_unregister();
		ili_hid_dev_remove(DISABLE);
		return -ENODEV;
	}
#if 1
#if SPRD_SYSFS_SUSPEND_RESUME
	ili_sysfs_add_device(ilits->dev);
	if (sysfs_create_link(NULL, &ilits->dev->kobj, "touchscreen") < 0)
		ILI_INFO("Failed to create link!\n");
#else
	ilitek_plat_sleep_init();
#endif
#endif
	ilits->pm_suspend = false;
	init_completion(&ilits->pm_completion);
#if 0
#if CHARGER_NOTIFIER_CALLBACK
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
	/* add_for_charger_start */
	ilitek_hid_plat_charger_init();
	/* add_for_charger_end */
#endif
#endif
#endif
	ILI_INFO("ILITEK Driver loaded successfully!#\n");
	return 0;
}

static int ilitek_tp_pm_suspend(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM SUSPEND");
	ilits->pm_suspend = true;
#if KERNEL_VERSION(3, 12, 0) >= LINUX_VERSION_CODE
	ilits->pm_completion.done = 0;
#else
	reinit_completion(&ilits->pm_completion);
#endif
	return 0;
}

static int ilitek_tp_pm_resume(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM RESUME");
	ilits->pm_suspend = false;
	complete(&ilits->pm_completion);
	return 0;
}

static int ilitek_plat_remove(void)
{
	ILI_INFO("remove plat dev\n");
#if SPRD_SYSFS_SUSPEND_RESUME
	ili_sysfs_remove_device(ilits->dev);
#endif
	ili_hid_dev_remove(ENABLE);
	return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
	.suspend = ilitek_tp_pm_suspend,
	.resume = ilitek_tp_pm_resume,
};

static const struct of_device_id tp_match_table[] = {
	{ .compatible = DTS_OF_NAME },
	{},
};

#ifdef ROI
struct ts_device_ops ilitek_ops = {
	.chip_roi_rawdata = ili_knuckle_roi_rawdata,
	.chip_roi_switch = ili_knuckle_roi_switch,
};
#endif

struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.plat_type = TP_PLAT_QCOM,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	//.plat_probe = ilitek_hid_plat_probe,
	.plat_remove = ilitek_plat_remove,
	.pm = &tp_pm_ops,
};

//static int __init ilitek_hid_plat_dev_init(void)
int ilitek_hid_plat_dev_init(struct i2c_client *client)
{
	ILI_INFO("ILITEK TP driver init for MTK\n");
	//if (ili_hid_dev_init(&hwif) < 0) {
	if (ili_hid_dev_init(&hwif, client) < 0) {
		ILI_ERR("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	return 0;
}

//static void __exit ilitek_hid_plat_dev_exit(void)
void ilitek_hid_plat_dev_exit(void)
{
	ILI_INFO("remove plat dev\n");
	ili_hid_dev_remove(ENABLE);
}

#if 0
#ifdef __DRM_PANEL_H__
late_initcall(ilitek_hid_plat_dev_init);
#else
module_init(ilitek_hid_plat_dev_init);
#endif
module_exit(ilitek_hid_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
#endif
