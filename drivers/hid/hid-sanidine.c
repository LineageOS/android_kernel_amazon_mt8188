/*
 * HID driver for Sanidine USB(Pogo) Keyboard
 * adapted from hid_sanidine.c
 *
 * Copyright (C) 2023 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/usb.h>
#include "usbhid/usbhid.h"
#include "hid-ids.h"
#include <linux/workqueue.h>
#include <linux/time.h>
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#include <linux/metricslog.h>
#endif

#define KEYBOARD_HEARTBEAT		3600000
#define VENDOR_ID_LAB126		0x1949
#define DEVICE_ID_LAB126_SANIDINE_KB	0x042b
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#define KEYBOARD_METRICS_BUFF_SIZE 512
static char g_m_buf_kb[KEYBOARD_METRICS_BUFF_SIZE];
#endif

struct sanidine_matrics_info {
	unsigned long elapsed_s;

	struct delayed_work heartbeat_work;
	struct wakeup_source heartbeat_wake_lock;

	ktime_t time_start;
};

static void sanidine_matrics_heartbeat_work(struct work_struct *work)
{
	struct sanidine_matrics_info *info =
		 container_of(work, struct sanidine_matrics_info, heartbeat_work.work);

	__pm_stay_awake(&info->heartbeat_wake_lock);

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	minerva_metrics_log(g_m_buf_kb, KEYBOARD_METRICS_BUFF_SIZE,
		"%s:%s:100:%s,%s,%s,%s,KeyboardVid=%d;IN,KeyboardPid=%d;IN,KeyboardHeartbeat=1;IN:us-east-1",
		METRICS_KEYBOARD_GROUP_ID, METRICS_KEYBOARD_SCHEMA_ID, PREDEFINED_ESSENTIAL_KEY,
		PREDEFINED_DEVICE_ID_KEY, PREDEFINED_CUSTOMER_ID_KEY, PREDEFINED_MODEL_KEY,
		VENDOR_ID_LAB126, DEVICE_ID_LAB126_SANIDINE_KB);
#endif
	pr_info("%s\n", __func__);
	queue_delayed_work(system_freezable_wq, &info->heartbeat_work,
			 msecs_to_jiffies(KEYBOARD_HEARTBEAT));
	__pm_relax(&info->heartbeat_wake_lock);
}

static void sanidine_to_metrics(struct sanidine_matrics_info *info)
{
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	minerva_metrics_log(g_m_buf_kb, KEYBOARD_METRICS_BUFF_SIZE,
		"%s:%s:100:%s,%s,%s,%s,KeyboardVid=%d;IN,KeyboardPid=%d;IN,KeyboardSessionDuration=%lu;IN:us-east-1",
		METRICS_KEYBOARD_GROUP_ID, METRICS_KEYBOARD_SCHEMA_ID, PREDEFINED_ESSENTIAL_KEY,
		PREDEFINED_DEVICE_ID_KEY, PREDEFINED_CUSTOMER_ID_KEY, PREDEFINED_MODEL_KEY,
		VENDOR_ID_LAB126, DEVICE_ID_LAB126_SANIDINE_KB, info->elapsed_s);
#endif
	pr_info("%s: session duration=%lu\n", __func__, info->elapsed_s);
}

static int sanidine_probe(struct hid_device *hdev,
		const struct hid_device_id *id)
{
	struct sanidine_matrics_info *info;
	int ret = 0;

	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 bInterfaceProtocol = iface->cur_altsetting->desc.bInterfaceProtocol;

	if (bInterfaceProtocol != 1)
		return 0;

	info = devm_kzalloc(&hdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		return ret;
	}

	info->elapsed_s	= 0;
	hid_set_drvdata(hdev, info);
	device_init_wakeup(&hdev->dev, true);
	wakeup_source_add(&info->heartbeat_wake_lock);
	INIT_DELAYED_WORK(&info->heartbeat_work, sanidine_matrics_heartbeat_work);

	info->time_start = ktime_get_boottime();
	queue_delayed_work(system_freezable_wq, &info->heartbeat_work, 0);

	pr_info("%s: success\n", __func__);

	return 0;
}

static void sanidine_remove(struct hid_device *hdev)
{
	struct sanidine_matrics_info *info = hid_get_drvdata(hdev);

	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 bInterfaceProtocol = (int)iface->cur_altsetting->desc.bInterfaceProtocol;

	if (bInterfaceProtocol != 1)
		return;

	info->elapsed_s = ktime_to_ms(ktime_sub(ktime_get_boottime(), info->time_start)) / 1000;
	sanidine_to_metrics(info);
	cancel_delayed_work(&info->heartbeat_work);
	wakeup_source_remove(&info->heartbeat_wake_lock);
	device_init_wakeup(&hdev->dev, false);

	pr_info("%s: success\n", __func__);
	hid_hw_stop(hdev);
}

static const struct hid_device_id sanidine_id[] = {
	{ HID_DEVICE(BUS_USB, HID_GROUP_GENERIC, USB_VENDOR_ID_LAB126, USB_DEVICE_ID_LAB126_SANIDINE_KB) },
	{ }
};
MODULE_DEVICE_TABLE(hid, sanidine_id);

static struct hid_driver sanidine_driver = {
	.name = "sanidine",
	.id_table = sanidine_id,
	.probe = sanidine_probe,
	.remove	= sanidine_remove,
};
module_hid_driver(sanidine_driver);

MODULE_LICENSE("GPL");
