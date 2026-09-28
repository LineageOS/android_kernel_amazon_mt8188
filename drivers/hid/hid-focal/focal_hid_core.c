/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2022, FocalTech Systems, Ltd., all rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#include "mtk_disp_notify.h"
#include "mtk_panel_ext.h"
#endif

#include <linux/notifier.h>
#include <linux/fb.h>
#if defined(CONFIG_DRM)
#if defined(CONFIG_DRM_PANEL)
#include <drm/drm_panel.h>
#else
#include <linux/msm_drm_notify.h>
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1 /* Early-suspend level */
#endif

#include "../hid-ids.h"
#include "focal_hid_core.h"

struct fts_hid_data *fts_hid_data;
unsigned char lcm_vendor_id = LCM_VENDOR_ID_UNKNOWN;
static DEFINE_MUTEX(fts_direct_read_mutex);
static DECLARE_COMPLETION(fts_direct_read_completion);

static int fts_resume(void);
static int fts_suspend(void);

static u8 fts_direct_read_buf[FTS_HID_BUFF_SIZE] = { 0 };

static struct proc_dir_entry *tp_fw_version_entry = NULL;

static __u8 g_aucHidRptDesc[] = {
	0x05, 0x0d,			// USAGE_PAGE (Digitizers)
	0x09, 0x04,			// USAGE (Touch Screen)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x01,			//  REPORT_ID (Touch Screen)
	0x09, 0x22,			// USAGE (Finger)
	0xa1, 0x02,			// COLLECTION (Logical)
	0x09, 0x42,			// USAGE (Tip Switch)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x25, 0x01,			// LOGICAL_MAXIMUM (1)
	0x75, 0x01,			// REPORT_SIZE (1)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x95, 0x06,			// REPORT_COUNT (6)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x09, 0x51,			// USAGE (Contact Identifier)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x05, 0x01,			// USAGE_PAGE (Generic Desk..
	0x26, (1200 & 0xFF), (1200 >> 8),	// LOGICAL_MAXIMUM
	0x75, 0x10,			// REPORT_SIZE (16)
	0x55, 0x0e,			// UNIT_EXPONENT (-2)
	0x65, 0x11,			// 0x65,0x11:UNIT(cm,SILinear)
	0x09, 0x30,			// USAGE (X)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0xAA, 0x05,		// PHYSICAL_MAXIMUM
	0x95, 0x02,			// REPORT_COUNT (2)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0x0B, 0x0A,		// PHYSICAL_MAXIMUM
	0x26, (2000 & 0xFF), (2000 >> 8),	// LOGICAL_MAXIMUM
	0x09, 0x31,			// USAGE (Y)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x05, 0x0d,			// USAGE_PAGE (Digitizers
	0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
	0x46, 0xff, 0x00,		// PHYSICAL_MAXIMUM (255)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x09, 0x48,			// USAGE (Width)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x49,			// USAGE (Height)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0xc0,				// END_COLLECTION
	0x09, 0x22,			// USAGE (Finger)
	0xa1, 0x02,			// COLLECTION (Logical)
	0x09, 0x42,			// USAGE (Tip Switch)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x25, 0x01,			// LOGICAL_MAXIMUM (1)
	0x75, 0x01,			// REPORT_SIZE (1)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x95, 0x06,			// REPORT_COUNT (6)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x09, 0x51,			// USAGE (Contact Identifier)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x05, 0x01,			// USAGE_PAGE (Generic Desk..
	0x26, (1200 & 0xFF), (1200 >> 8),	// LOGICAL_MAXIMUM
	0x75, 0x10,			// REPORT_SIZE (16)
	0x55, 0x0e,			// UNIT_EXPONENT (-2)
	0x65, 0x11,			// 0x65,0x11:UNIT(cm,SILinear)
	0x09, 0x30,			// USAGE (X)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0xAA, 0x05,		// PHYSICAL_MAXIMUM
	0x95, 0x02,			// REPORT_COUNT (2)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0x0B, 0x0A,		// PHYSICAL_MAXIMUM
	0x26, (2000 & 0xFF), (2000 >> 8),	// LOGICAL_MAXIMUM
	0x09, 0x31,			// USAGE (Y)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x05, 0x0d,			// USAGE_PAGE (Digitizers
	0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
	0x46, 0xff, 0x00,		// PHYSICAL_MAXIMUM (255)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x09, 0x48,			// USAGE (Width)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x49,			// USAGE (Height)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0xc0,				// END_COLLECTION
	0x09, 0x22,			// USAGE (Finger)
	0xa1, 0x02,			// COLLECTION (Logical)
	0x09, 0x42,			// USAGE (Tip Switch)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x25, 0x01,			// LOGICAL_MAXIMUM (1)
	0x75, 0x01,			// REPORT_SIZE (1)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x95, 0x06,			// REPORT_COUNT (6)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x09, 0x51,			// USAGE (Contact Identifier)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x05, 0x01,			// USAGE_PAGE (Generic Desk..
	0x26, (1200 & 0xFF), (1200 >> 8),	// LOGICAL_MAXIMUM
	0x75, 0x10,			// REPORT_SIZE (16)
	0x55, 0x0e,			// UNIT_EXPONENT (-2)
	0x65, 0x11,			// 0x65,0x11:UNIT(cm,SILinear)
	0x09, 0x30,			// USAGE (X)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0xAA, 0x05,		// PHYSICAL_MAXIMUM
	0x95, 0x02,			// REPORT_COUNT (2)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0x0B, 0x0A,		// PHYSICAL_MAXIMUM
	0x26, (2000 & 0xFF), (2000 >> 8),	// LOGICAL_MAXIMUM
	0x09, 0x31,			// USAGE (Y)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x05, 0x0d,			// USAGE_PAGE (Digitizers
	0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
	0x46, 0xff, 0x00,		// PHYSICAL_MAXIMUM (255)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x09, 0x48,			// USAGE (Width)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x49,			// USAGE (Height)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0xc0,				// END_COLLECTION
	0x09, 0x22,			// USAGE (Finger)
	0xa1, 0x02,			// COLLECTION (Logical)
	0x09, 0x42,			// USAGE (Tip Switch)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x25, 0x01,			// LOGICAL_MAXIMUM (1)
	0x75, 0x01,			// REPORT_SIZE (1)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x95, 0x06,			// REPORT_COUNT (6)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x09, 0x51,			// USAGE (Contact Identifier)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x05, 0x01,			// USAGE_PAGE (Generic Desk..
	0x26, (1200 & 0xFF), (1200 >> 8),	// LOGICAL_MAXIMUM
	0x75, 0x10,			// REPORT_SIZE (16)
	0x55, 0x0e,			// UNIT_EXPONENT (-2)
	0x65, 0x11,			// 0x65,0x11:UNIT(cm,SILinear)
	0x09, 0x30,			// USAGE (X)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0xAA, 0x05,	// PHYSICAL_MAXIMUM
	0x95, 0x02,			// REPORT_COUNT (2)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0x0B, 0x0A,		// PHYSICAL_MAXIMUM
	0x26, (2000 & 0xFF), (2000 >> 8),	// LOGICAL_MAXIMUM
	0x09, 0x31,			// USAGE (Y)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x35, 0x01,			// PHYSICAL_MINIMUM (1)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x05, 0x0d,			// USAGE_PAGE (Digitizers
	0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
	0x46, 0xff, 0x00,		// PHYSICAL_MAXIMUM (255)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x09, 0x48,			// USAGE (Width)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x49,			// USAGE (Height)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0xc0,				// END_COLLECTION
	0x05, 0x0d,			// USAGE_PAGE (Digitizers
	0x27, 0xff, 0xff, 0x00, 0x00,	// LOGICAL_MAXIMUM (65535)
	0x75, 0x10,			// REPORT_SIZE (16)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x09, 0x56,			// USAGE (Scan Time)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x54,			// USAGE (Contact count)
	0x25, 0x10,			// LOGICAL_MAXIMUM (10)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x85, 2,			// REPORT_ID (Feature:0X02)
	0x09, 0x55,			// USAGE(Contact Count Maximum)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x25, 10,			// LOGICAL_MAXIMUM (10)
	0xb1, 0x02,			// FEATURE (Data,Var,Abs)
	0x06, 0x00, 0xff,		// USAGE_PAGE (Vendor Defined)
	0x09, 0xc5,			// USAGE (Vendor Usage 0xC5)
	0x85, 0x05,			// REPORT_ID (Feature:0X05)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x96, 0x00, 0x01,		// REPORT_COUNT (0x100 (256))
	0xb1, 0x02,			// FEATURE (Data,Var,Abs)
	0xc0,				// END_COLLECTION
	0x06, 0x00, 0xff,		// USAGE_PAGE (Vendor Defined)
	0x09, 0x01,			// USAGE (Vendor Usage 1)
	0xa1, 0x01,			// COLLECTION (Application)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (255)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x85, 0x06,			// REPORT_ID (Feature:0X06)
	0x95, 63,			// REPORT_COUNT (63)
	0x09, 0x01,			// USAGE (Vendor Usage 1)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x01,			// USAGE (Vendor Usage 1)
	0x91, 0x02,			// OUTPUT (Data,Var,Abs)
	0xc0,				// END_COLLECTION
	0x05, 0x0d,			// USAGE_PAGE (Digitizers)
	0x09, 0x02,			// USAGE (Pen)
	0xa1, 0x01,			// COLLECTION (Application)
	0x85, 0x0B,			// REPORT_ID (Feature:11)
	0x09, 0x20,			// USAGE (Stylus)
	0xa1, 0x00,			// COLLECTION (Physical)
	0x09, 0x42,			// USAGE (Tip Switch)
	0x09, 0x44,			// USAGE (Barrel Switch)
	0x09, 0x3c,			// USAGE (invert)
	0x09, 0x45,			// USAGE (Eraser)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x25, 0x01,			// LOGICAL_MAXIMUM (1)
	0x75, 0x01,			// REPORT_SIZE (1)
	0x95, 0x04,			// REPORT_COUNT (4)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x95, 0x01,			// REPORT_COUNT (1)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x09, 0x32,			// USAGE (In Range)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x95, 0x02,			// REPORT_COUNT (2)
	0x81, 0x03,			// INPUT (Cnst,Ary,Abs)
	0x05, 0x01,			// USAGE_PAGE (Generic Desk..
	0x09, 0x30,			// USAGE (X)
	0x75, 0x10,			// REPORT_SIZE (16)
	0x95, 0x01,			// REPORT_COUNT (1)
	0xa4,				// PUSH
	0x55, 0x0e,			// UNIT_EXPONENT (-2)
	0x65, 0x11,			// 0x65,0x11:UNIT(cm,SILinear)   0x65,0x13:UNIT(Inch,EngLinear)
	0x35, 0x00,			// PHYSICAL_MINIMUM (0)
	0x46, 0xAA, 0x05,		// PHYSICAL_MAXIMUM
	0x26, (1200 & 0xFF), (1200 >> 8),	// LOGICAL_MAXIMUM
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x31,			// USAGE (Y)
	0x46, 0x0B, 0x0A,		// PHYSICAL_MAXIMUM
	0x26, (2000 & 0xFF), (2000 >> 8),	// LOGICAL_MAXIMUM
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0xb4,				// POP
	0x05, 0x0d,			// USAGE_PAGE (Digitizers)
	0x09, 0x30,			// USAGE (Tip Pressure)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x26, 0x00, 0x10,		// LOGICAL_MAXIMUM (4096)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x75, 0x10,			// REPORT_SIZE (16)
	0x09, 0x3d,			// USAGE (X Tilt)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x26, 0x50, 0x46,		// LOGICAL_MAXIMUM (18000)
	0x55, 0x0e,			// UNIT_EXPONENT (-2)
	0x65, 0x14,			// UNIT(degree,eng rot)
	0x36, 0xd8, 0xdc,		// PHYSICAL_MINIMUM (-9000)
	0x46, 0x28, 0x23,		// PHYSICAL_MAXIMUM (9000)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x09, 0x3e,			// USAGE (Y Tilt)
	0x81, 0x02,			// INPUT (Data,Var,Abs)
	0x06, 0x00, 0xff,		// USAGE_PAGE (Vendor Defined)
	0x85, 0x0C,			// REPORT_ID (PENHQA)
	0x09, 0xC5,			// USAGE (Vendor Usage 0xC5)
	0x15, 0x00,			// LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,		// LOGICAL_MAXIMUM (0xff)
	0x75, 0x08,			// REPORT_SIZE (8)
	0x96, 0x00, 0x01,		// REPORT_COUNT (0x100 (256))
	0xb1, 0x02,			// FEATURE (Data,Var,Abs)
	0xc0,				// END_COLLECTION
	0xc0,				// END_COLLECTION
};

static void get_lcm_id(void)
{
	unsigned int lcm_id = 0xFF;
#if IS_ENABLED(CONFIG_IDME)
	lcm_id = idme_get_lcmid_info();
	if (lcm_id == 0xFF)
		FTS_ERROR("get lcm_id failed.\n");
#else
	FTS_ERROR("%s, idme is not ready.\n", __func__);
#endif
	lcm_vendor_id = (unsigned char)lcm_id;
	FTS_INFO("[ft8205] %s, vendor id = 0x%x\n", __func__, lcm_vendor_id);
}

static u8 get_checksum(u8 *buf, u8 len)
{
	unsigned int i = 0;
	unsigned char Checksum = 0;

	for (i = 0; i < len; i++)
		Checksum ^= buf[i];

	Checksum++;
	return Checksum;
}

/*
Format should be：04 00 42 00 06 ff ff 05 40 46 ff ff ... ff
*/
int fts_direct_write_data(u8 *buf, size_t size)
{
	int retval;
	u8 check_sum = 0;
	int index = 0;
	u8 fts_hid_buf[FTS_HID_BUFF_SIZE] = { 0 };
	int retries = 3;
	struct hid_device *hdev = fts_hid_data->hdev;

	memset(fts_hid_buf, 0xFF, FTS_HID_BUFF_SIZE);

	fts_hid_buf[index++] = 0x06;
	fts_hid_buf[index++] = 0xFF;
	fts_hid_buf[index++] = 0xFF;
	fts_hid_buf[index++] = size + 4;
	memcpy(fts_hid_buf + index, buf, size);
	check_sum = get_checksum(fts_hid_buf + 3, size + 1);
	fts_hid_buf[4 + size] = check_sum;

	reinit_completion(&fts_direct_read_completion);
	do {
		retval = hid_hw_output_report(hdev, fts_hid_buf,
					      FTS_HID_BUFF_SIZE);
	} while ((retval == -ETIMEDOUT || retval == -EAGAIN) && --retries);

	if (retval < 0) {
		FTS_ERROR("%s: ran out of retries (last error = %d)\n", __func__, retval);
		return retval;
	}

	return 0;
}

/*
Format should be：42 00 06 ff ff 05 f0 f6 ff ff ... ff
*/
int fts_direct_read_data(u8 *buf, size_t size)
{
	int copy_size = 0;
	int ret = 0;

	ret = wait_for_completion_timeout(&fts_direct_read_completion,
					  msecs_to_jiffies(200));

	if (!ret) {
		FTS_ERROR("wait_for_completion_timeout\n");
		return -ETIMEDOUT;
	}

	copy_size = (size > FTS_HID_BUFF_SIZE) ? FTS_HID_BUFF_SIZE : size;
	mutex_lock(&fts_direct_read_mutex);

	memcpy(buf, fts_direct_read_buf + 4, copy_size);

	mutex_unlock(&fts_direct_read_mutex);

	return 0;
}

int focal_read_register(u8 addr, u8 *value)
{
	u8 cmd[2] = { 0 };
	u8 data[3] = { 0 };
	int ret = 0;

	cmd[0] = CMD_READ_REGISTER;
	cmd[1] = addr;

	mutex_lock(&fts_hid_data->bus_lock);
	ret = fts_direct_write_data(cmd, 2);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		ret = -EINVAL;
		goto out;
	}

	ret = fts_direct_read_data(data, 3);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		ret = -EINVAL;
		goto out;
	}

	if ((data[0] != CMD_READ_REGISTER) || (data[1] != addr)) {
		FTS_ERROR("%s fail cmd:%x, addr:%x\n", __func__, data[0],
			  data[1]);
		ret = -EINVAL;
		goto out;
	} else {
		*value = data[2];
	}

out:
	mutex_unlock(&fts_hid_data->bus_lock);

	return ret;
}

int focal_write_register(u8 addr, u8 value)
{
	u8 cmd[3] = { 0 };
	u8 data[2] = { 0 };
	int ret = 0;

	cmd[0] = CMD_WRITE_REGISTER;
	cmd[1] = addr;
	cmd[2] = value;

	mutex_lock(&fts_hid_data->bus_lock);
	ret = fts_direct_write_data(cmd, 3);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		ret = -EINVAL;
		goto out;
	}

	ret = fts_direct_read_data(data, 2);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		ret = -EINVAL;
		goto out;
	}

	if ((data[0] != CMD_WRITE_REGISTER) || (data[1] != addr)) {
		FTS_ERROR("%s fail cmd:%x, value:%x\n", __func__, data[0], data[1]);
		ret = -EINVAL;
		goto out;
	}

out:
	mutex_unlock(&fts_hid_data->bus_lock);

	return ret;
}

int focal_hw_reset(int hdelayms)
{
	FTS_INFO("tp reset");
	ft8205_share_tp_resetpin_control(0);
	msleep(1);
	ft8205_share_tp_resetpin_control(1);
	if (hdelayms)
		msleep(hdelayms);

	return 0;
}

void focal_irq_disable(void)
{
	unsigned long irqflags;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_hid_data->irq_lock, irqflags);

	if (!fts_hid_data->irq_disabled) {
		disable_irq_nosync(fts_hid_data->irq);
		fts_hid_data->irq_disabled = true;
	}

	spin_unlock_irqrestore(&fts_hid_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void focal_irq_enable(void)
{
	unsigned long irqflags = 0;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_hid_data->irq_lock, irqflags);

	if (fts_hid_data->irq_disabled) {
		enable_irq(fts_hid_data->irq);
		fts_hid_data->irq_disabled = false;
	}

	spin_unlock_irqrestore(&fts_hid_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

static int fts_process_gesture(u8 gesture_id)
{
	struct input_dev *input_dev = fts_hid_data->ts_input_dev;

	FTS_FUNC_ENTER();
	FTS_INFO("gesture_id = %d, KEY_POWER = %d\n", gesture_id, KEY_POWER);
	if (gesture_id == GESTURE_SINGLECLICK) {
		input_report_key(input_dev, KEY_POWER, 1);
		input_sync(input_dev);
		msleep(10);
		input_report_key(input_dev, KEY_POWER, 0);
		input_sync(input_dev);
	}
	return 0;
}

static void fts_recovery_func(struct work_struct *work)
{
	struct fts_hid_data *ts_data = container_of(work,
						struct fts_hid_data, recovery_work.work);

	fts_recovery_state(ts_data);
}

int check_rawdata(u8 *raw_data, int ecc_legth)
{
	int ecc_tp = raw_data[ecc_legth];
	int ecc_host = get_checksum(raw_data + 1, ecc_legth - 1);

	if(ecc_tp != ecc_host)
		return -EINVAL;

	return 0;
}

int fts_raw_event(struct hid_device *hdev, struct hid_report *report,
		  u8 *raw_data, int size)
{
	int copy_size = 0;
	int gesture_mode = fts_hid_data->gesture_mode;
	struct fts_hid_data *ts_data = fts_hid_data;
	int msc_timestamp = 0;
	u8 gesture_id = 0;
	u8 tp_esd_value = 0;
	u8 lcd_esd_value = 0;
	u8 ecc_length = 0;
	u8 touch_type = 0;
	u8 event_type = 0;
	int ecc_checksum_result = 0;

	if (raw_data == NULL)
		return -EINVAL;

	touch_type = raw_data[0];
	switch (touch_type) {
	case FTS_TOUCH_PEN:
	case FTS_TOUCH_FINGER:
		fts_hid_data->intr_jiffies = jiffies;
		fts_esd_hang_cnt(ts_data);
		ecc_checksum_result = check_rawdata(raw_data, TP_ECC_INDEX);
		if (ecc_checksum_result) {
			FTS_ERROR("fts_raw_event checksum fail.\n");
			return -EINVAL;
		}
		if (touch_type == FTS_TOUCH_PEN) {
			msc_timestamp = ((raw_data[23] << 8) + raw_data[22]);
			FTS_DEBUG("SCAN_TIME = %d\n", msc_timestamp);
			input_event(fts_hid_data->pen_input_dev, EV_MSC, MSC_TIMESTAMP, msc_timestamp);
		}
		break;
	case FTS_PRIVATE_PACKAGE_FLAG:
		ecc_length = raw_data[3];
		if (ecc_length >= FTS_HID_BUFF_SIZE || ecc_length <= 0) {
			FTS_ERROR("FTS_PRIVATE_PACKAGE_FLAG ecc_length:[%d] is wrong.\n", ecc_length);
			return -EINVAL;
		}
		ecc_checksum_result = check_rawdata(raw_data, ecc_length);
		if (ecc_checksum_result) {
			FTS_ERROR("FTS_PRIVATE_PACKAGE_FLAG checksum fail.\n");
			return -EINVAL;
		}
		event_type = raw_data[4];
		switch (event_type) {
		case FTS_GESTURE_MODE_TOUCH_FINGER:
			if (gesture_mode && ts_data->suspended) {
				gesture_id = raw_data[5];
				return fts_process_gesture(gesture_id);
			}
			break;
		case FTS_ESD_CHECK_EVENT:
			tp_esd_value = raw_data[5];
			lcd_esd_value = raw_data[6];
			return fts_update_esd_value(tp_esd_value, lcd_esd_value);
		case FTS_TP_RECOVERY_EVENT:
			FTS_INFO("%s and FTS_TP_RECOVERY_EVENT\n", __func__);
			queue_delayed_work(fts_hid_data->ts_workqueue, &fts_hid_data->recovery_work,
				msecs_to_jiffies(10));
			break;
		default:
			copy_size = (size > FTS_HID_BUFF_SIZE) ? FTS_HID_BUFF_SIZE : size;
			mutex_lock(&fts_direct_read_mutex);
			memcpy(fts_direct_read_buf, raw_data, copy_size);
			mutex_unlock(&fts_direct_read_mutex);
			complete(&fts_direct_read_completion);
			break;
		}
		break;
	default:
		return 0;
	}

	return 0;
}

static ssize_t fts_upgrade_bin_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	char fwname[FILE_NAME_LENGTH] = { 0 };

	if ((count <= 1) || (count >= FILE_NAME_LENGTH - 32)) {
		FTS_ERROR("fw bin name's length(%d) fail", (int)count);
		return -EINVAL;
	}
	memset(fwname, 0, sizeof(fwname));
	snprintf(fwname, FILE_NAME_LENGTH, "%s", buf);
	fwname[count - 1] = '\0';

	FTS_INFO("upgrade with bin file through sysfs node");

	focal_hid_upgrade_bin(fwname);

	return count;
}

static ssize_t fts_hw_reset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	focal_hw_reset(0);
	count = snprintf(buf, PAGE_SIZE, "hw reset executed\n");

	return count;
}

void focal_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = { 0xEB, 0xAA, 0x09 };

	ret = focal_write(buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
	} else {
		msleep(10);
		buf[0] = buf[1] = buf[2] = 0;
		ret = focal_read(NULL, 0, buf, 3);
		if (ret < 0)
			FTS_ERROR("hid2std cmd read fail");
		else if ((buf[0] == 0xEB) && (buf[1] == 0xAA) && (buf[2] == 0x08))
			FTS_DEBUG("hidi2c change to stdi2c successful");
		else
			FTS_ERROR("hidi2c change to stdi2c not support or fail");
	}
}

static ssize_t fts_gesture_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	bool gesture_mode = fts_hid_data->gesture_mode;

	focal_read_register(FTS_REG_GESTURE_EN, &val);
	count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
			gesture_mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d\n", val);

	return count;
}

static ssize_t fts_gesture_mode_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable gesture");
		fts_hid_data->gesture_mode = 1;
		panel_gesture_mode_set_by_ft8205(true);
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable gesture");
		fts_hid_data->gesture_mode = 0;
		panel_gesture_mode_set_by_ft8205(false);
	} else {
		FTS_ERROR("Gesture Wakeup Failed! ");
	}

	return count;
}

static ssize_t gesture_wakeup_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	bool gesture_mode = fts_hid_data->gesture_mode;

	focal_read_register(FTS_REG_GESTURE_EN, &val);
	count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
			gesture_mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d\n", val);

	return count;
}

static ssize_t gesture_wakeup_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable gesture");
		fts_hid_data->gesture_mode = 1;
		panel_gesture_mode_set_by_ft8205(true);
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable gesture");
		fts_hid_data->gesture_mode = 0;
		panel_gesture_mode_set_by_ft8205(false);
	} else {
		FTS_ERROR("Gesture Wakeup Failed! ");
	}

	return count;
}

static ssize_t fts_fw_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	u8 value = 0;
	int ret = 0;

	FTS_FUNC_ENTER();
	ret = focal_read_register(FTS_REG_FW_VER, &value);
	if (ret)
		count = snprintf(buf, PAGE_SIZE, "read version fail\n");
	else
		count = snprintf(buf, PAGE_SIZE, "%02x\n", value);

	return count;
}

static DEVICE_ATTR_RO(fts_fw_version);
static DEVICE_ATTR_WO(fts_upgrade_bin);
static DEVICE_ATTR_RO(fts_hw_reset);
static DEVICE_ATTR_RW(fts_gesture_mode);

static struct attribute *fts_attributes[] = {
	&dev_attr_fts_upgrade_bin.attr,
	&dev_attr_fts_hw_reset.attr,
	&dev_attr_fts_gesture_mode.attr,
	&dev_attr_fts_fw_version.attr,
	NULL
};

static struct attribute_group fts_attribute_group = { .attrs = fts_attributes };

static DEVICE_ATTR_RW(gesture_wakeup);

static struct attribute *fts_gesture_attrs[] = {
	&dev_attr_gesture_wakeup.attr,
	NULL
};
static const struct attribute_group fts_gesture_attribute_group = {
	.attrs = fts_gesture_attrs,
};

static ssize_t fts_node_ver_info_read(struct file *filp, char __user *buff,
					 size_t size, loff_t *pos)
{
	u8 value = 0;
	int32_t len = 0;
	int ret = 0;
	unsigned char user_buf[256] = { 0 };

	if (*pos != 0)
		return 0;

	FTS_FUNC_ENTER();
	memset(user_buf, 0, 256 * sizeof(unsigned char));
	ret = focal_read_register(FTS_REG_FW_VER, &value);
	if (ret)
		len = snprintf(user_buf, 256, "read version fail\n");
	else
		len = snprintf(user_buf, 256, "fw_ver=%02x\n", value);

	if (len < 0) {
		FTS_ERROR("snprintf copy string failed. (%d)\n", ret);
		return -EINVAL;
	}

	if (copy_to_user((char *)buff, user_buf, len))
		FTS_INFO("Failed to copy data to user space\n");

	*pos += len;

	return len;
}

static const struct proc_ops tp_fw_version_fops = {
	.proc_read = fts_node_ver_info_read,
};

static int fts_create_sysfs(struct hid_device *hdev)
{
	int ret = 0;
	struct kobject *touchscreen_link;
	struct i2c_client *client = fts_hid_data->client;

	ret = sysfs_create_group(&hdev->dev.kobj, &fts_attribute_group);
	if (ret) {
		FTS_ERROR("[EX]: sysfs_create_group() failed!!");
		sysfs_remove_group(&hdev->dev.kobj, &fts_attribute_group);
		return -ENOMEM;
	} else {
		FTS_INFO("[EX]: sysfs_create_group() succeeded!!");
	}

	/* Add sysfs Link*/
	touchscreen_link = kobject_create_and_add("touchscreen", NULL);
	if (touchscreen_link != NULL) {
		ret = sysfs_create_link(touchscreen_link, &client->dev.kobj, "link_input_dev");
		if (ret < 0) {
			FTS_ERROR("Failed to create sysfs link for input_dev %d\n", ret);
		} else {
			if (sysfs_create_group(&client->dev.kobj, &fts_gesture_attribute_group))
				FTS_ERROR("Failed to create gesture_wakeup gesture sysfs");
			else
				FTS_INFO("create gesture_wakeup Succeeded!\n");
		}
	}

	tp_fw_version_entry = proc_create(TP_FW_VERSION, 0444, NULL, &tp_fw_version_fops);
	if (tp_fw_version_entry == NULL) {
		FTS_ERROR("create proc/%s Failed!\n", TP_FW_VERSION);
		return -ENOMEM;
	} else {
		FTS_INFO("create proc/%s Succeeded!\n", TP_FW_VERSION);
	}

	return ret;
}

int fts_remove_sysfs(struct hid_device *hdev)
{
	sysfs_remove_group(&hdev->dev.kobj, &fts_attribute_group);
	sysfs_remove_group(&fts_hid_data->client->dev.kobj, &fts_gesture_attribute_group);
	if (tp_fw_version_entry != NULL) {
		remove_proc_entry(TP_FW_VERSION, NULL);
		tp_fw_version_entry = NULL;
		FTS_INFO("Removed /proc/%s\n", TP_FW_VERSION);
	}
	return 0;
}

/*
 * MacBook JIS keyboard has wrong logical maximum
 */
__u8 *fts_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		       unsigned int *rsize)
{
	FTS_FUNC_ENTER();
	if (hdev->product == 0x82B5) {
		FTS_INFO("%s 0x82B5", __func__);
		rdesc = g_aucHidRptDesc;
		*rsize = sizeof(g_aucHidRptDesc);
	}

	return rdesc;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	int *blank = (int *)data;

	FTS_FUNC_ENTER();
	if (!fts_hid_data) {
		FTS_ERROR("fts_hid_data is null");
		return 0;
	}

	if (!(event == MTK_DISP_EARLY_EVENT_BLANK ||
	      event == MTK_DISP_EVENT_BLANK)) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	FTS_INFO("MTK event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case MTK_DISP_BLANK_UNBLANK:
		if (event == MTK_DISP_EARLY_EVENT_BLANK) {
			FTS_INFO("resume: event = %lu, not care\n", event);
			fts_hid_data->lcd_disp_on = true;
		} else if (event == MTK_DISP_EVENT_BLANK) {
			//queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
			fts_resume();
		}
		break;
	case MTK_DISP_BLANK_POWERDOWN:
		if (event == MTK_DISP_EARLY_EVENT_BLANK) {
			//cancel_work_sync(&fts_data->resume_work);
			fts_suspend();
		} else if (event == MTK_DISP_EVENT_BLANK) {
			FTS_INFO("suspend: event = %lu, not care\n", event);
			fts_hid_data->lcd_disp_on = false;
		}
		break;
	default:
		FTS_INFO("MTK BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}

#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void fts_ts_early_suspend(struct early_suspend *handler)
{
	FTS_FUNC_ENTER();
	fts_suspend();
}

static void fts_ts_late_resume(struct early_suspend *handler)
{
	FTS_FUNC_ENTER();
	fts_resume();
}
#endif

static int fts_hid_parse_gpio(struct fts_hid_data *fts_hid_data)
{
	u32 reset_gpio_flags;

	FTS_FUNC_ENTER();
	fts_hid_data->reset_gpio =
		of_get_named_gpio_flags(fts_hid_data->dev->of_node,
					"fts-reset-gpios", 0,
					&reset_gpio_flags);
	if (fts_hid_data->reset_gpio < 0) {
		FTS_ERROR("of_get_named_gpio_flags fail\n");
		return -1;
	}

	if (gpio_is_valid(fts_hid_data->reset_gpio))
		ft8205_share_tp_resetpin_control(1);

	FTS_INFO("%s success\n", __func__);
	return 0;
}

static int fts_init_input_device(struct fts_hid_data *fts_hid_data)
{
	struct hid_input *fts_hid_input = NULL;
	struct hid_field *field = NULL;

	list_for_each_entry(fts_hid_input, &fts_hid_data->hdev->inputs, list) {
		field = fts_hid_input->report->field[0];
		if (FTS_PEN_FIELD(field)) {
			fts_hid_data->pen_input_dev = fts_hid_input->input;
			__set_bit(EV_MSC, fts_hid_data->pen_input_dev->evbit);
			__set_bit(MSC_TIMESTAMP, fts_hid_data->pen_input_dev->mscbit);
		}
		if(FTS_FINGER_FIELD(field)) {
			fts_hid_data->ts_input_dev = fts_hid_input->input;
			input_set_capability(fts_hid_data->ts_input_dev, EV_KEY, KEY_POWER);
		}
	}
	if(!fts_hid_data->ts_input_dev || !fts_hid_data->pen_input_dev)
		return -ENODEV;

	return 0;
}

int fts_hid_probe_entry(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret = 0;

	get_lcm_id();
	if ((lcm_vendor_id != FT8205_TG_INX) && (lcm_vendor_id != FT8205_KD_BOE)) {
		FTS_ERROR("It's not ft8205 IC,fts_ts_probe exit.");
		return -ENODEV;
	}

	FTS_FUNC_ENTER();
	fts_hid_data = kzalloc(sizeof(struct fts_hid_data), GFP_KERNEL);
	if (IS_ERR(fts_hid_data)) {
		FTS_ERROR("kzalloc fts_hid_data fail\n");
		return PTR_ERR(fts_hid_data);
	}

	/*init driver data*/
	mutex_init(&fts_hid_data->bus_lock);
	spin_lock_init(&fts_hid_data->irq_lock);

	fts_hid_data->hdev = hdev;
	fts_hid_data->client = hdev->driver_data;
	fts_hid_data->dev = &fts_hid_data->client->dev;
	fts_hid_data->irq = fts_hid_data->client->irq;
	fts_hid_data->ts_workqueue = create_singlethread_workqueue("fts_wq");

	ret = IS_ERR(fts_hid_data->ts_workqueue);
	if (ret) {
		FTS_ERROR("create fts workqueue fail");
		goto err;
	}

	ret = fts_create_sysfs(hdev);
	if (ret) {
		FTS_ERROR("fts_create_sysfs fail\n");
		goto sys_err;
	}

	ret = fts_init_input_device(fts_hid_data);
	if (ret) {
		FTS_ERROR("fts_init_input_device fail\n");
		goto sys_err;
	}

	ret = fts_hid_parse_gpio(fts_hid_data);
	if (ret) {
		FTS_ERROR("fts_hid_parse_gpio fail\n");
		goto sys_err;
	}

	/*set device as wakeup source*/
	ret = device_init_wakeup(fts_hid_data->dev, 1);
	if (ret) {
		FTS_ERROR("device_init_wakeup fail\n");
		goto sys_err;
	}

	ret = focal_bus_init(fts_hid_data);
	if (ret) {
		FTS_ERROR("init bus fail");
		goto bus_err;
	}

	ret = fts_test_init(fts_hid_data);
	if (ret) {
		FTS_ERROR("init host test fail");
		goto test_err;
	}

	if (fts_hid_data->ts_workqueue) {
		INIT_DELAYED_WORK(&fts_hid_data->recovery_work, fts_recovery_func);
	} else {
		FTS_ERROR("fts workqueue is NULL, can't run fts_recovery_func function");
		goto test_err;
	}

	focal_esdcheck_init(fts_hid_data);

	ret = focal_fwupg_init(fts_hid_data);
	if (ret) {
		FTS_ERROR("init fw upgrade fail");
		goto upg_err;
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	fts_hid_data->disp_notifier.notifier_call = fb_notifier_callback;
	ret = mtk_disp_notifier_register("focaltech ic Touch",
					 &fts_hid_data->disp_notifier);
	if (ret)
		FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);

#elif defined(CONFIG_HAS_EARLYSUSPEND)
	fts_hid_data->early_suspend.level =
		EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	fts_hid_data->early_suspend.suspend = fts_ts_early_suspend;
	fts_hid_data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&fts_hid_data->early_suspend);
#endif

	return ret;

upg_err:
	focal_fwupg_release(fts_hid_data);

test_err:
	fts_test_exit(fts_hid_data);

bus_err:
	focal_bus_exit(fts_hid_data);

sys_err:
	fts_remove_sysfs(hdev);

err:
	if (fts_hid_data)
		kfree(fts_hid_data);

	return ret;
}

int fts_ts_remove_entry(void)
{
	FTS_FUNC_ENTER();
	fts_test_exit(fts_hid_data);
	focal_fwupg_release(fts_hid_data);
	focal_esdcheck_exit(fts_hid_data);
	focal_bus_exit(fts_hid_data);
	fts_remove_sysfs(fts_hid_data->hdev);
	if (fts_hid_data->ts_workqueue)
		destroy_workqueue(fts_hid_data->ts_workqueue);
	if (fts_hid_data)
		kfree(fts_hid_data);
	FTS_FUNC_EXIT();

	return 0;
}

static int fts_suspend(void)
{
	int wake_status = 0;
	FTS_FUNC_ENTER();

	if (fts_hid_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}

	if (fts_hid_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		return 0;
	}

	focal_esdcheck_suspend(fts_hid_data);
	if (fts_hid_data->gesture_mode) {
		FTS_INFO("enter into gesture mode");
		focal_write_register(FTS_REG_GESTURE_EN, 1);
		wake_status = enable_irq_wake(fts_hid_data->irq);
		if (!wake_status)
			fts_hid_data->irq_wake_enabled = true;
		else
			FTS_ERROR("Failed to enable irq wake: %d\n", wake_status);
	} else {
		focal_write_register(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
		focal_irq_disable();
	}
	fts_hid_data->suspended = true;
	return 0;
}

static int fts_resume(void)
{
	int wake_status = 0;
	FTS_FUNC_ENTER();
	if (!fts_hid_data->suspended) {
		FTS_DEBUG("Already in awake state");
		return 0;
	}
#ifdef SUPPORT_FINGER_REPORT_CHECK_RELEASE
	mt_release_contacts(fts_hid_data->hdev);
#endif
	if (fts_hid_data->irq_wake_enabled) {
		wake_status = disable_irq_wake(fts_hid_data->irq);
		if (!wake_status)
			fts_hid_data->irq_wake_enabled = false;
		else
			FTS_ERROR("Failed to disable irq wake: %d\n", wake_status);
	}
	if (!fts_hid_data->gesture_mode)
		focal_irq_enable();

	fts_hid_data->suspended = false;
	focal_wait_tp_to_valid();
	focal_esdcheck_resume(fts_hid_data);
	return 0;
}
