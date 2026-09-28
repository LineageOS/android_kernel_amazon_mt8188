/*
 * Copyright (C) 2022 Novatek, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/hid.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/hidraw.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "nvt_i2c_hid.h"

#define NVT_HID_I2C_DRIVER_VERSION "1.0.10"
#define NVT_TOUCH_SYSFS_LINK "nvt_touch"

/* flags */
#define I2C_HID_STARTED 0
#define I2C_HID_RESET_PENDING 1
#define I2C_HID_READ_PENDING 2

#define I2C_HID_PWR_ON 0x00
#define I2C_HID_PWR_SLEEP 0x01

#define NVT_TOUCH_WKG_REPORT_PERIOD 600

#define NVT_TOUCH_GESTURE_DISABLE 0
#define NVT_TOUCH_GESTURE_ENABLE 1
#define NVT_TOUCH_GESTURE_SWITCH_SUCCESS 0
#define NVT_TOUCH_GESTURE_SWITCH_ON_FAIL 1
#define NVT_TOUCH_GESTURE_SWITCH_OFF_FAIL 2

#define IRQ_DISABLE 0
#define IRQ_ENABLE 1

struct i2c_client *hid_i2c_client = NULL;
struct device *hid_i2c_dev;
int32_t reset_gpio;
uint32_t reset_flags;
uint32_t set_f_pkt[GET_SET_F_PKT_LENGTH];
uint32_t get_f_pkt[GET_SET_F_PKT_LENGTH];
uint8_t prefix_pkt[PREFIX_PKT_LENGTH] = { 0x00, 0x00, 0x3B, 0x00, 0x00 };
uint32_t wakeup_id;
atomic_t nvt_irq_stat;
static DEFINE_SPINLOCK(nvt_irq_spin);
static int gesture_switch = NVT_TOUCH_GESTURE_SWITCH_SUCCESS;
static bool gesture_mode = false;
static unsigned long gesture_timer;
struct input_dev *wkg_input_dev;
int32_t probe_result;
struct mutex probe_lock;
uint8_t inc_rpt_cnt;

#if PEN_TIMESTAMP_SUPPORT
static bool pen_timestamp_support = false;
static struct input_dev *pen_input = NULL;
static int32_t pen_scantime = 0;
static int32_t pen_prev_scantime = 0;
static long pen_delta_scantime = 0;
static unsigned long pen_jiffies = 0;
static int32_t pen_timestamp = 0;
#endif

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
static int nvt_disp_notifier_callback(struct notifier_block *nb,
		unsigned long value, void *v);
#endif
static int32_t nvt_ts_resume(struct device *dev);
static int32_t nvt_ts_suspend(struct device *dev);

struct i2c_hid_platform_data {
	u16 hid_descriptor_address;
	struct regulator_bulk_data supplies[3];
	int post_power_delay_ms;
};

struct i2c_hid_desc {
	__le16 wHIDDescLength;
	__le16 bcdVersion;
	__le16 wReportDescLength;
	__le16 wReportDescRegister;
	__le16 wInputRegister;
	__le16 wMaxInputLength;
	__le16 wOutputRegister;
	__le16 wMaxOutputLength;
	__le16 wCommandRegister;
	__le16 wDataRegister;
	__le16 wVendorID;
	__le16 wProductID;
	__le16 wVersionID;
	__le32 reserved;
} __packed;

struct i2c_hid_cmd {
	unsigned int registerIndex;
	__u8 opcode;
	unsigned int length;
	bool wait;
};

union command {
	u8 data[0];
	struct cmd {
		__le16 reg;
		__u8 reportTypeID;
		__u8 opcode;
	} __packed c;
};

#define I2C_HID_CMD(opcode_)                                                   \
	.opcode = opcode_, .length = 4,                                        \
	.registerIndex = offsetof(struct i2c_hid_desc, wCommandRegister)

/* fetch HID descriptor */
static const struct i2c_hid_cmd hid_descr_cmd = { .length = 2 };
/* fetch report descriptors */
static const struct i2c_hid_cmd hid_report_descr_cmd = {
	.registerIndex = offsetof(struct i2c_hid_desc, wReportDescRegister),
	.opcode = 0x00,
	.length = 2
};
/* commands */
static const struct i2c_hid_cmd hid_reset_cmd = { I2C_HID_CMD(0x01),
	.wait = true };
static const struct i2c_hid_cmd hid_get_report_cmd = { I2C_HID_CMD(0x02) };
static const struct i2c_hid_cmd hid_set_report_cmd = { I2C_HID_CMD(0x03) };
static const struct i2c_hid_cmd hid_set_power_cmd = { I2C_HID_CMD(0x08) };
static const struct i2c_hid_cmd hid_wkg_on_cmd = { I2C_HID_CMD(0x0E) };
static const struct i2c_hid_cmd hid_wkg_off_cmd = { I2C_HID_CMD(0x0F) };
static const struct i2c_hid_cmd hid_no_cmd = { .length = 0 };

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
touch_metrics_info_t touch_metrics_data;
#endif

extern void panel_gesture_mode_set_by_nt36523b(bool gst_mode);

struct i2c_hid {
	struct i2c_client *client;
	struct hid_device *hid;
	union {
		__u8 hdesc_buffer[sizeof(struct i2c_hid_desc)];
		struct i2c_hid_desc hdesc;
	};
	__le16 wHIDDescRegister;
	unsigned int bufsize;
	u8 *inbuf;
	u8 *rawbuf;
	u8 *cmdbuf;
	u8 *argsbuf;
	unsigned long flags;
	wait_queue_head_t wait;
	struct i2c_hid_platform_data pdata;
	bool irq_wake_enabled;
	struct mutex reset_lock;
};

static int __i2c_hid_command(struct i2c_client *client, const struct i2c_hid_cmd *command,
		u8 reportID, u8 reportType, u8 *args, int args_len, unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	union command *cmd = (union command *)ihid->cmdbuf;
	int ret = 0;
	struct i2c_msg msg[2];
	int msg_num = 1;

	int length = command->length;
	bool wait = command->wait;
	unsigned int registerIndex = command->registerIndex;

	/* special case for hid_descr_cmd */
	if (command == &hid_descr_cmd) {
		cmd->c.reg = ihid->wHIDDescRegister;
	} else {
		cmd->data[0] = ihid->hdesc_buffer[registerIndex];
		cmd->data[1] = ihid->hdesc_buffer[registerIndex + 1];
	}

	if (length > 2) {
		cmd->c.opcode = command->opcode;
		cmd->c.reportTypeID = reportID | reportType << 4;
	}

	memcpy(cmd->data + length, args, args_len);
	length += args_len;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = length;
	msg[0].buf = cmd->data;
	if (data_len > 0) {
		msg[1].addr = client->addr;
		msg[1].flags = client->flags & I2C_M_TEN;
		msg[1].flags |= I2C_M_RD;
		msg[1].len = data_len;
		msg[1].buf = buf_recv;
		msg_num = 2;
		set_bit(I2C_HID_READ_PENDING, &ihid->flags);
	}

	if (wait)
		set_bit(I2C_HID_RESET_PENDING, &ihid->flags);

	mutex_lock(&ts->xbuf_lock);
	ret = i2c_transfer(client->adapter, msg, msg_num);
	mutex_unlock(&ts->xbuf_lock);

	if (data_len > 0)
		clear_bit(I2C_HID_READ_PENDING, &ihid->flags);

	if (ret != msg_num)
		return ret < 0 ? ret : -EIO;

	ret = 0;

	if (wait) {
		NVT_LOG("reset waiting...\n");
		if (!wait_event_timeout(ihid->wait, !test_bit(I2C_HID_RESET_PENDING, &ihid->flags), msecs_to_jiffies(5000)))
			ret = -ENODATA;
		NVT_LOG("reset finished.\n");
	}

	return ret;
}

static int i2c_hid_command(struct i2c_client *client, const struct i2c_hid_cmd *command,
		unsigned char *buf_recv, int data_len)
{
	return __i2c_hid_command(client, command, 0, 0, NULL, 0, buf_recv, data_len);
}

static int i2c_hid_get_report(struct i2c_client *client, u8 reportType, u8 reportID,
		unsigned char *buf_recv, int data_len)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 args[3] = {0};
	int ret = 0;
	int args_len = 0;
	u16 readRegister = le16_to_cpu(ihid->hdesc.wDataRegister);

	if (reportID >= 0x0F) {
		args[args_len++] = reportID;
		reportID = 0x0F;
	}

	args[args_len++] = readRegister & 0xFF;
	args[args_len++] = readRegister >> 8;

	ret = __i2c_hid_command(client, &hid_get_report_cmd, reportID, reportType, args, args_len,
			buf_recv, data_len);
	if (ret) {
		NVT_ERR("failed to retrieve report from device.\n");
		return ret;
	}

	return 0;
}

static int i2c_hid_set_or_send_report(struct i2c_client *client, u8 reportType, u8 reportID,
		unsigned char *buf, size_t data_len, bool use_data)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	u8 *args = ihid->argsbuf;
	const struct i2c_hid_cmd *hidcmd = NULL;
	int ret = 0;
	u16 dataRegister = le16_to_cpu(ihid->hdesc.wDataRegister);
	u16 outputRegister = le16_to_cpu(ihid->hdesc.wOutputRegister);
	u16 maxOutputLength = le16_to_cpu(ihid->hdesc.wMaxOutputLength);
	u16 size = 0;
	int args_len = 0;
	int index = 0;

	if (data_len > ihid->bufsize)
		return -EINVAL;

	size = 2 + (reportID ? 1 : 0) + data_len;
	args_len = (reportID >= 0x0F ? 1 : 0) + 2 + size;

	if (!use_data && maxOutputLength == 0)
		return -ENOSYS;

	if (reportID >= 0x0F) {
		args[index++] = reportID;
		reportID = 0x0F;
	}

	if (use_data) {
		args[index++] = dataRegister & 0xFF;
		args[index++] = dataRegister >> 8;
		hidcmd = &hid_set_report_cmd;
	} else {
		args[index++] = outputRegister & 0xFF;
		args[index++] = outputRegister >> 8;
		hidcmd = &hid_no_cmd;
	}

	args[index++] = size & 0xFF;
	args[index++] = size >> 8;

	if (reportID)
		args[index++] = reportID;

	memcpy(&args[index], buf, data_len);

	ret = __i2c_hid_command(client, hidcmd, reportID, reportType, args, args_len, NULL, 0);
	if (ret) {
		NVT_ERR("failed to set a report to device.\n");
		return ret;
	}

	return data_len;
}

static int i2c_hid_set_power(struct i2c_client *client, int power_state)
{
	int ret = 0;

	if (power_state == I2C_HID_PWR_ON)
		NVT_LOG("set power on\n");
	else
		NVT_LOG("set power off\n");
	ret = __i2c_hid_command(client, &hid_set_power_cmd, power_state, 0,
				NULL, 0, NULL, 0);

	if (ret)
		NVT_ERR("failed to change power setting.\n");

	return ret;
}

static int i2c_hid_hwreset(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&ihid->reset_lock);

	ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);
	if (ret)
		goto out_unlock;

	ret = i2c_hid_command(client, &hid_reset_cmd, NULL, 0);
	if (ret) {
		NVT_ERR("failed to reset device.\n");
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
		touch_metrics_data.bootup_status = HID_HW_RESET_FAIL;
		tp_metrics_print_func(touch_metrics_data);
#endif
		i2c_hid_set_power(client, I2C_HID_PWR_SLEEP);
		goto out_unlock;
	}

	ret = i2c_hid_set_power(client, I2C_HID_PWR_ON);

out_unlock:
	mutex_unlock(&ihid->reset_lock);
	return ret;
}

#if CHECK_DATA_CHECKSUM
static int32_t nvt_ts_data_checksum(uint8_t *buf, uint8_t length)
{
	uint8_t checksum = 0;
	int32_t i = 0;

	for (i = 0; i < length - 1; i++)
		checksum += buf[i];
	checksum = (~checksum + 1);

	if (checksum != buf[length - 1]) {
		NVT_ERR("checksum not match. (buf[%d]=0x%02X, checksum=0x%02X)\n",
			length - 1, buf[length - 1], checksum);

		return -1;
	}

	return 0;
}
#endif

static void i2c_hid_get_input(struct i2c_hid *ihid)
{
	int ret = 0;
	u32 ret_size = 0;
	uint32_t pen_serial = PEN_INVALID_SERIAL;
	int size = le16_to_cpu(ihid->hdesc.wMaxInputLength);
#if PEN_TIMESTAMP_SUPPORT
	unsigned long jdelta = jiffies_to_usecs(jiffies - pen_jiffies);
#endif
#if IS_ENABLED(CONFIG_STYLUS_BATTERY_ALGO)
	static u32 stylus_frame_num = 0;
	stylus_packet_info_t stylus_packet_info = { 0 };
#endif

	if (size > ihid->bufsize)
		size = ihid->bufsize;

	mutex_lock(&ts->lock);
	ret = i2c_master_recv(ihid->client, ihid->inbuf, size);
	mutex_unlock(&ts->lock);
	if (ret != size) {
		if (ret < 0)
			return;

		NVT_ERR("%s: got %d data instead of %d\n", __func__, ret, size);
		return;
	}

	ret_size = ihid->inbuf[0] | ihid->inbuf[1] << 8;

	if (!ret_size) {
		/* host or device initiated RESET completed */
		if (test_and_clear_bit(I2C_HID_RESET_PENDING, &ihid->flags))
			wake_up(&ihid->wait);
		return;
	}

	if ((ret_size > size) || (ret_size < 2)) {
		NVT_ERR("%s: incomplete report (%d/%d)\n", __func__, size,
			ret_size);
		return;
	}

#if CHECK_DATA_CHECKSUM
	if (nvt_ts_data_checksum(ihid->inbuf, ret_size))
		return;
#endif

	if (test_bit(I2C_HID_STARTED, &ihid->flags)) {
#if PEN_TIMESTAMP_SUPPORT
		if (pen_timestamp_support && ihid->inbuf[2] == PEN_ID) {
			pen_scantime = ihid->inbuf[59] + (ihid->inbuf[60] << 8);
			pen_delta_scantime = pen_scantime - pen_prev_scantime;
			pen_jiffies = jiffies;
			if (pen_delta_scantime < 0)
				pen_delta_scantime += PEN_MAX_DELTA;
			pen_delta_scantime *= 100;
			if (jdelta > MAX_TIMESTAMP_INTERVAL)
				pen_timestamp = 0;
			else
				pen_timestamp += pen_delta_scantime;
			pen_prev_scantime = pen_scantime;
			input_event(pen_input, EV_MSC, MSC_TIMESTAMP, pen_timestamp);
		}
#endif

		if (ihid->inbuf[PEN_FLAG_INDEX] == PEN_ID) {
			memcpy(&pen_serial, ihid->inbuf + PEN_SERIAL_INDEX, sizeof(pen_serial));
			if (pen_serial != PEN_INVALID_SERIAL) {
				ts->pen_touch_time = ktime_get_boottime();

				if (ts->pen_serial != pen_serial) {
					NVT_LOG(
						"stylus_power %d, stylus_vid 0x%x, old_stylus_id 0x%x, stylus_id 0x%x\n",
						ihid->inbuf[PEN_BATTERY_STRENGTH],
						(ihid->inbuf[PEN_VENDOR_USAGE_1_H] << 8) |
						ihid->inbuf[PEN_VENDOR_USAGE_1_L],
						ts->pen_serial, pen_serial);
					ts->pen_serial = pen_serial;
#if IS_ENABLED(CONFIG_STYLUS_BATTERY_ALGO)
					stylus_frame_num = 0;
#endif
				}

				NVT_DEBUG_LOG(
					"stylus_battery_capacity %d, stylus_oem_vendor_id 0x%x, pen_serial 0x%x\n",
					ihid->inbuf[PEN_BATTERY_STRENGTH],
					(ihid->inbuf[PEN_VENDOR_USAGE_1_H] << 8) |
					ihid->inbuf[PEN_VENDOR_USAGE_1_L],
					ts->pen_serial);

#if IS_ENABLED(CONFIG_STYLUS_BATTERY_ALGO)
				if ((++stylus_frame_num % PEN_FRAME_COUNT) == 0) {
					/* get stylus capacity、stylus fw version and oem_vendor_id*/
					stylus_packet_info.pen_id = ts->pen_serial;
					stylus_packet_info.vendor_id =
						ihid->inbuf[PEN_VENDOR_USAGE_1_H] << 8 | ihid->inbuf[PEN_VENDOR_USAGE_1_L],
					stylus_packet_info.fw_version =
						ihid->inbuf[NVT_HID_PEN_FW_VERSION_H] << 8 |
						ihid->inbuf[NVT_HID_PEN_FW_VERSION_L];
					stylus_packet_info.capacity = ihid->inbuf[PEN_BATTERY_STRENGTH];
					stylus_packet_info.time = ktime_to_ms(ts->pen_touch_time);
					stylus_info_packet(stylus_packet_info);
					stylus_info_process();
				}
#endif
			}
		}

		if (wakeup_id && gesture_mode && (wakeup_id == ihid->inbuf[2]) &&
		    jiffies_to_msecs(jiffies - gesture_timer) >
			    NVT_TOUCH_WKG_REPORT_PERIOD) {
			pm_wakeup_event(&wkg_input_dev->dev, 5000);
			input_report_key(wkg_input_dev, KEY_POWER, 1);
			input_sync(wkg_input_dev);
			input_report_key(wkg_input_dev, KEY_POWER, 0);
			input_sync(wkg_input_dev);
			gesture_timer = jiffies;
			NVT_LOG("Wake up geature detected\n");
		} else {
			hid_input_report(ihid->hid, HID_INPUT_REPORT, ihid->inbuf + 2, ret_size - 2, 1);
		}
	}

	return;
}

static irqreturn_t i2c_hid_irq(int irq, void *dev_id)
{
	struct i2c_hid *ihid = dev_id;

	if (test_bit(I2C_HID_READ_PENDING, &ihid->flags))
		return IRQ_HANDLED;

	i2c_hid_get_input(ihid);

	return IRQ_HANDLED;
}

static int i2c_hid_get_report_length(struct hid_report *report)
{
	return ((report->size - 1) >> 3) + 1 + report->device->report_enum[report->type].numbered + 2;
}

static void i2c_hid_find_max_report(struct hid_device *hid, unsigned int type, unsigned int *max)
{
	struct hid_report *report = NULL;
	unsigned int size = 0;

	list_for_each_entry (report, &hid->report_enum[type].report_list, list) {
		size = i2c_hid_get_report_length(report);
		if (*max < size)
			*max = size;
	}
}

static void i2c_hid_free_buffers(struct i2c_hid *ihid)
{
	kfree(ihid->inbuf);
	kfree(ihid->rawbuf);
	kfree(ihid->argsbuf);
	kfree(ihid->cmdbuf);
	ihid->inbuf = NULL;
	ihid->rawbuf = NULL;
	ihid->cmdbuf = NULL;
	ihid->argsbuf = NULL;
	ihid->bufsize = 0;
}

static int i2c_hid_alloc_buffers(struct i2c_hid *ihid, size_t report_size)
{
	/* ReportID + optional ReportID byte + data register + size of the report + report*/
	int args_len = sizeof(__u8) + sizeof(__u8) + sizeof(__u16) + sizeof(__u16) + report_size;

	ihid->inbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->rawbuf = kzalloc(report_size, GFP_KERNEL);
	ihid->argsbuf = kzalloc(args_len, GFP_KERNEL);
	ihid->cmdbuf = kzalloc(sizeof(union command) + args_len, GFP_KERNEL);

	if (!ihid->inbuf || !ihid->rawbuf || !ihid->argsbuf || !ihid->cmdbuf) {
		i2c_hid_free_buffers(ihid);
		return -ENOMEM;
	}

	ihid->bufsize = report_size;

	return 0;
}

static int i2c_hid_get_raw_report(struct hid_device *hid, unsigned char report_number, __u8 *buf,
		size_t count, unsigned char report_type)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	size_t ret_count, ask_count;
	int ret = 0;

	if (report_type == HID_OUTPUT_REPORT)
		return -EINVAL;

	/* +2 bytes to include the size of the reply in the query buffer */
	ask_count = min(count + 2, (size_t)ihid->bufsize);

	ret = i2c_hid_get_report(client, report_type == HID_FEATURE_REPORT ? 0x03 : 0x01, report_number,
			ihid->rawbuf, ask_count);

	if (ret < 0)
		return ret;

	ret_count = ihid->rawbuf[0] | (ihid->rawbuf[1] << 8);

	if (ret_count <= 2)
		return 0;

	ret_count = min(ret_count, ask_count);

	/* The query buffer contains the size, dropping it in the reply */
	count = min(count, ret_count - 2);
	memcpy(buf, ihid->rawbuf + 2, count);

	return count;
}

static int i2c_hid_output_raw_report(struct hid_device *hid, __u8 *buf, size_t count,
		unsigned char report_type, bool use_data)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int report_id = buf[0];
	int ret = 0;

	if (report_type == HID_INPUT_REPORT)
		return -EINVAL;

	mutex_lock(&ihid->reset_lock);

	if (report_id) {
		buf++;
		count--;
	}

	ret = i2c_hid_set_or_send_report(client, report_type == HID_FEATURE_REPORT ? 0x03 : 0x02,
			report_id, buf, count, use_data);

	if (report_id && ret >= 0)
		ret++; /* add report_id to the number of transfered bytes */

	mutex_unlock(&ihid->reset_lock);

	return ret;
}

static int i2c_hid_output_report(struct hid_device *hid, __u8 *buf, size_t count)
{
	return i2c_hid_output_raw_report(hid, buf, count, HID_OUTPUT_REPORT, false);
}

static int i2c_hid_raw_request(struct hid_device *hid, unsigned char reportnum, __u8 *buf,
		size_t len, unsigned char rtype, int reqtype)
{
	switch (reqtype) {
	case HID_REQ_GET_REPORT:
		return i2c_hid_get_raw_report(hid, reportnum, buf, len, rtype);
	case HID_REQ_SET_REPORT:
		if (buf[0] != reportnum)
			return -EINVAL;
		return i2c_hid_output_raw_report(hid, buf, len, rtype, true);
	default:
		return -EIO;
	}
}

static int i2c_hid_parse(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int rsize = 0;
	char *rdesc = NULL;
	int i = 0, ret = 0;
	int tries = 3;

	rsize = le16_to_cpu(hdesc->wReportDescLength);
	if (!rsize || rsize > HID_MAX_DESCRIPTOR_SIZE) {
		dbg_hid("weird size of report descriptor (%u)\n", rsize);
		return -EINVAL;
	}

	do {
		ret = i2c_hid_hwreset(client);
		if (ret)
			msleep(1000);
	} while (tries-- > 0 && ret);

	if (ret)
		return ret;

	rdesc = kzalloc(rsize, GFP_KERNEL);
	if (!rdesc) {
		NVT_ERR("couldn't allocate rdesc memory\n");
		return -ENOMEM;
	}

	ret = i2c_hid_command(client, &hid_report_descr_cmd, rdesc, rsize);
	if (ret) {
		NVT_ERR("reading report descriptor failed\n");
		kfree(rdesc);
		return -EIO;
	}

	NVT_LOG("Report Descriptor:\n");
	for (i = 0; i <= rsize / 16; i++)
		NVT_LOG("%*ph\n", 16 < (rsize - i * 16) ? 16 : (rsize - i * 16), rdesc + i * 16);

	ret = hid_parse_report(hid, rdesc, rsize);

	if (ret) {
		dbg_hid("parsing report descriptor failed\n");
		return ret;
	}

	return 0;
}

static int i2c_hid_start(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	int ret = 0;
	unsigned int bufsize = HID_MIN_BUFFER_SIZE;

	i2c_hid_find_max_report(hid, HID_INPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_OUTPUT_REPORT, &bufsize);
	i2c_hid_find_max_report(hid, HID_FEATURE_REPORT, &bufsize);

	if (bufsize > ihid->bufsize) {
		disable_irq(client->irq);
		i2c_hid_free_buffers(ihid);

		ret = i2c_hid_alloc_buffers(ihid, bufsize);
		enable_irq(client->irq);

		if (ret)
			return ret;
	}

	return 0;
}

static void i2c_hid_stop(struct hid_device *hid)
{
	hid->claimed = 0;
}

static int i2c_hid_open(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	set_bit(I2C_HID_STARTED, &ihid->flags);
	return 0;
}

static void i2c_hid_close(struct hid_device *hid)
{
	struct i2c_client *client = hid->driver_data;
	struct i2c_hid *ihid = i2c_get_clientdata(client);

	clear_bit(I2C_HID_STARTED, &ihid->flags);
}

struct hid_ll_driver nvt_i2c_hid_ll_driver = {
	.parse = i2c_hid_parse,
	.start = i2c_hid_start,
	.stop = i2c_hid_stop,
	.open = i2c_hid_open,
	.close = i2c_hid_close,
	.output_report = i2c_hid_output_report,
	.raw_request = i2c_hid_raw_request,
};
EXPORT_SYMBOL_GPL(nvt_i2c_hid_ll_driver);

static int i2c_hid_init_irq(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	unsigned long irqflags = 0;
	int ret = 0;

	NVT_LOG("Requesting IRQ: %d\n", client->irq);

	if (!irq_get_trigger_type(client->irq))
		irqflags = IRQF_TRIGGER_FALLING;

	ret = request_threaded_irq(client->irq, NULL, i2c_hid_irq, irqflags | IRQF_ONESHOT, client->name, ihid);
	if (ret < 0) {
		NVT_ERR("Could not register for %s interrupt, irq = %d, ret = %d\n", client->name, client->irq, ret);
		return ret;
	}

	return 0;
}

static int i2c_hid_fetch_hid_descriptor(struct i2c_hid *ihid)
{
	struct i2c_client *client = ihid->client;
	struct i2c_hid_desc *hdesc = &ihid->hdesc;
	unsigned int dsize;
	int ret = 0;

	NVT_LOG("Fetching the HID descriptor\n");
	ret = i2c_hid_command(client, &hid_descr_cmd, ihid->hdesc_buffer, sizeof(struct i2c_hid_desc));
	if (ret) {
		NVT_ERR("hid_descr_cmd failed\n");
		return -ENODEV;
	}

	/* check bcdVersion == 1.0 */
	if (le16_to_cpu(hdesc->bcdVersion) != 0x0100) {
		NVT_ERR("unexpected HID descriptor bcdVersion (0x%04hx)\n", le16_to_cpu(hdesc->bcdVersion));
		return -ENODEV;
	}

	/* Descriptor length should be 30 bytes as per the specification */
	dsize = le16_to_cpu(hdesc->wHIDDescLength);
	if (dsize != sizeof(struct i2c_hid_desc)) {
		NVT_ERR("weird size of HID descriptor (%u)\n", dsize);
		return -ENODEV;
	}
	NVT_LOG("HID Descriptor: %*ph\n", dsize, ihid->hdesc_buffer);
	return 0;
}

static int i2c_hid_of_probe(struct i2c_client *client, struct i2c_hid_platform_data *pdata)
{
	struct device *dev = &client->dev;
	u32 val = 0;
	int ret = 0;

	ret = of_property_read_u32(dev->of_node, "hid-descr-addr", &val);
	if (ret) {
		NVT_ERR("HID register address not provided\n");
		return -ENODEV;
	}
	if (val >> 16) {
		NVT_ERR("Bad HID register address: 0x%08x\n", val);
		return -EINVAL;
	}
	pdata->hid_descriptor_address = val;

	ret = of_property_read_u32(dev->of_node, "novatek,wakeup-id", &wakeup_id);
	if (!ret)
		NVT_LOG("wakeup gesture is supported, id = 0x%02X\n", wakeup_id);
	else
		NVT_LOG("wakeup gesture is not supported\n");

	ts->pen_support = of_property_read_bool(dev->of_node, "novatek,pen-support");
	NVT_LOG("novatek,pen-support=%d\n", ts->pen_support);

	return 0;
}

int nvt_hid_i2c_get_feature(struct i2c_client *client, uint8_t *hid_buf, uint32_t hid_len,
		uint8_t *buf_recv, uint16_t read_len)
{
	uint8_t *i2c_data, msg_num;
	uint32_t i2c_len = 0;
	int32_t ret = 0;
	struct i2c_msg msg[2];

	i2c_len = hid_len + 6;
	i2c_data = kzalloc(i2c_len, GFP_ATOMIC);
	i2c_data[0] = (uint8_t)get_f_pkt[0];
	i2c_data[1] = (uint8_t)get_f_pkt[1];
	i2c_data[2] = (uint8_t)get_f_pkt[2];
	i2c_data[3] = (uint8_t)get_f_pkt[3];
	i2c_data[4] = (uint8_t)get_f_pkt[4];
	i2c_data[5] = (uint8_t)get_f_pkt[5];
	memcpy(i2c_data + 6, hid_buf, hid_len);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 6;
	msg[0].buf = i2c_data;
	msg[1].addr = client->addr;
	msg[1].flags = client->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = read_len;
	msg[1].buf = buf_recv;
	msg_num = 2;
	mutex_lock(&ts->xbuf_lock);
	if (i2c_transfer(client->adapter, msg, msg_num) != msg_num) {
		if (!buf_recv)
			NVT_ERR("i2c read error, data = %*ph, buf_recv = NULL\n", i2c_len, i2c_data);
		else
			NVT_ERR("i2c read error, data = %*ph, buf_recv = %*ph\n", i2c_len, i2c_data, read_len, buf_recv);
		ret = -EIO;
	} else {
		ret = hid_len - 2;
	}
	mutex_unlock(&ts->xbuf_lock);
	kfree(i2c_data);
	return ret;
}

int nvt_hid_i2c_set_feature(struct i2c_client *client, uint8_t *hid_buf, uint32_t hid_len)
{
	uint8_t *i2c_data, msg_num;
	uint32_t i2c_len = 0;
	int32_t ret = 0;
	struct i2c_msg msg[1];

	i2c_len = hid_len + 8;
	i2c_data = kzalloc(i2c_len, GFP_ATOMIC);
	i2c_data[0] = (uint8_t)set_f_pkt[0];
	i2c_data[1] = (uint8_t)set_f_pkt[1];
	i2c_data[2] = (uint8_t)set_f_pkt[2];
	i2c_data[3] = (uint8_t)set_f_pkt[3];
	i2c_data[4] = (uint8_t)set_f_pkt[4];
	i2c_data[5] = (uint8_t)set_f_pkt[5];
	i2c_data[6] = (hid_len + 2) & 0xFF;
	i2c_data[7] = ((hid_len + 2) & 0xFFFF) >> 16;
	memcpy(i2c_data + 8, hid_buf, hid_len);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = i2c_len;
	msg[0].buf = i2c_data;
	msg_num = 1;
	mutex_lock(&ts->xbuf_lock);
	if (i2c_transfer(client->adapter, msg, msg_num) != msg_num) {
		NVT_ERR("i2c write error, data = %*ph\n", i2c_len, i2c_data);
		ret = -EIO;
	} else {
		ret = hid_len;
	}
	mutex_unlock(&ts->xbuf_lock);
	kfree(i2c_data);
	return ret;
}

static long device_ioctl(struct file *filep, uint32_t cmd, unsigned long arg)
{
	uint8_t *hid_buf = NULL;
	uint8_t report_id = 0;
	uint8_t *buf_recv = NULL;
	uint16_t hid_len = 0;
	uint16_t read_len = 0;
	int32_t ret = 0;
	void __user *user_arg = (void __user *)arg;

	disable_irq(ts->client->irq);
#if DRIVER_ESD_PROTECT
	if (ts->nvt_esd_wq)
		cancel_delayed_work(&ts->nvt_esd_work);
#endif
	mutex_lock(&ts->lock);
	hid_len = _IOC_SIZE(cmd) + 2;
	if (hid_len > HID_MAX_BUFFER_SIZE) {
		NVT_ERR("Invalid hid length\n");
		ret = -EINVAL;
		goto end;
	}
	hid_buf = kzalloc(hid_len, GFP_ATOMIC);
	if (!hid_buf) {
		ret = -ENOMEM;
		goto end;
	}

	hid_buf = memdup_user(user_arg, hid_len);
	report_id = hid_buf[0];

	if (_IOC_NR(cmd) == _IOC_NR(HIDIOCGFEATURE(0))) {
		read_len = hid_buf[1] + (hid_buf[2] << 8);
		buf_recv = kzalloc(read_len, GFP_ATOMIC);
		if (!buf_recv) {
			ret = -ENOMEM;
			goto end;
		}
		ret = nvt_hid_i2c_get_feature(hid_i2c_client, hid_buf, hid_len, buf_recv, read_len);
		if (ret > 0) {
			memcpy(hid_buf, buf_recv + 2, read_len - 2); // drop len
			ret = copy_to_user(user_arg, hid_buf, hid_len);
		}
		kfree(buf_recv);
	} else if (_IOC_NR(cmd) == _IOC_NR(HIDIOCSFEATURE(0))) {
		ret = nvt_hid_i2c_set_feature(hid_i2c_client, hid_buf, hid_len - 2);
	} else {
		NVT_ERR("Command %x is not HIDGET/SETFEATURE\n", cmd);
		ret = -EINVAL;
	}
end:
	mutex_unlock(&ts->lock);
#if DRIVER_ESD_PROTECT
	if (ts->nvt_esd_wq) {
		queue_delayed_work(ts->nvt_esd_wq, &ts->nvt_esd_work,
				msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
	}
#endif
	enable_irq(ts->client->irq);
	if (hid_buf != NULL) {
		kfree(hid_buf);
		hid_buf = NULL;
	}


	return ret;
}

void set_prefix_packet(void)
{
	get_f_pkt[0] = prefix_pkt[0];
	get_f_pkt[1] = prefix_pkt[1];
	get_f_pkt[2] = prefix_pkt[2];
	get_f_pkt[3] = GET_REPORT_CMD;
	get_f_pkt[4] = prefix_pkt[3];
	get_f_pkt[5] = prefix_pkt[4];
	set_f_pkt[0] = prefix_pkt[0];
	set_f_pkt[1] = prefix_pkt[1];
	set_f_pkt[2] = prefix_pkt[2];
	set_f_pkt[3] = SET_REPORT_CMD;
	set_f_pkt[4] = prefix_pkt[3];
	set_f_pkt[5] = prefix_pkt[4];
}

static ssize_t read_packet(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	char str[PAGE_SIZE];
	uint8_t str_len;
	ssize_t ret = 0;

	str_len = snprintf(str, PAGE_SIZE, "%c%c%c%c%c", prefix_pkt[0], prefix_pkt[1], prefix_pkt[2], prefix_pkt[3], prefix_pkt[4]);
	if (copy_to_user(buffer, str, str_len))
		ret = -EFAULT;
	return ret;
}

static ssize_t write_packet(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	uint8_t *buf = NULL;
	ssize_t ret = 0;

	if (count != PREFIX_PKT_LENGTH) {
		NVT_ERR("size %d != %d\n", (uint32_t)count, PREFIX_PKT_LENGTH);
		ret = -EINVAL;
		goto end;
	}

	buf = memdup_user(buffer, count);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto end;
	}

	memcpy(prefix_pkt, buf, count);
	NVT_LOG("prefix packet = %02X %02X %02X %02X %02X\n", prefix_pkt[0], prefix_pkt[1], prefix_pkt[2], prefix_pkt[3], prefix_pkt[4]);

	set_prefix_packet();
end:
	return ret;
}

const static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = read_packet,
	.write = write_packet,
	.unlocked_ioctl = device_ioctl,
};

static struct miscdevice nvt_ts_hidraw_flash_misc = {
	.name = "nvt_ts_hidraw_flash",
	.fops = &fops,
};

static ssize_t gesture_wakeup_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int32_t ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", gesture_mode);
	return ret;
}

static ssize_t gesture_wakeup_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint8_t mode = 0;
	int wake_status = 0;

	if (kstrtou8(buf, 10, &mode) || mode > 1)
		return -EINVAL;

	switch (mode) {
	case NVT_TOUCH_GESTURE_DISABLE:
		NVT_LOG("Disable gesture Mode\n");
		if (i2c_hid_command(hid_i2c_client, &hid_wkg_off_cmd, NULL, 0)) {
			NVT_ERR("failed to disable WKG Mode.\n");
			gesture_switch = NVT_TOUCH_GESTURE_SWITCH_OFF_FAIL;
		} else {
			gesture_mode = false;
			wake_status = disable_irq_wake(hid_i2c_client->irq);
			if (wake_status)
				NVT_ERR("Failed to disable irq wake\n");
			panel_gesture_mode_set_by_nt36523b(false);
		}
		break;
	case NVT_TOUCH_GESTURE_ENABLE:
		if (i2c_hid_command(hid_i2c_client, &hid_wkg_on_cmd, NULL, 0)) {
			NVT_ERR("failed to enable WKG Mode.\n");
			gesture_switch = NVT_TOUCH_GESTURE_SWITCH_ON_FAIL;
		} else {
			gesture_mode = true;
			wake_status = enable_irq_wake(hid_i2c_client->irq);
			if (wake_status)
				NVT_ERR("Failed to enable irq wake\n");
			panel_gesture_mode_set_by_nt36523b(true);
		}
		break;
	default:
		NVT_ERR("Invalid Input.\n");
		break;
	}

	return count;
}

static ssize_t tp_pen_serial_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int32_t len = 0;
	int32_t pen_disconnect_diff = 0;
	ktime_t pen_touch_disconnect = ktime_get_boottime();

	pen_disconnect_diff = ktime_to_ms(pen_touch_disconnect) -
			ktime_to_ms(ts->pen_touch_time);
	if (pen_disconnect_diff  > PEN_DISCONNECT_MAX_TIME)
		ts->pen_serial = PEN_INVALID_SERIAL;

	len = snprintf(buf, PAGE_SIZE, "pen_serial = %08X\n", ts->pen_serial);
	if (len < 0) {
		NVT_ERR("snprintf copy string failed. (%d)\n", len);
		return -EINVAL;
	}

	return len;
}

static ssize_t nvt_touch_ready_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int32_t ret = 0;

	NVT_LOG("enter\n");

	ret = snprintf(buf, PAGE_SIZE, "%d\n", ts->touch_ready),

	NVT_LOG("exit\n");
	return ret;
}

static DEVICE_ATTR_RW(gesture_wakeup);
static DEVICE_ATTR_RO(tp_pen_serial);
static DEVICE_ATTR_RO(nvt_touch_ready);
static struct attribute *nvt_api_attrs[] = {
	&dev_attr_gesture_wakeup.attr,
	&dev_attr_tp_pen_serial.attr,
	&dev_attr_nvt_touch_ready.attr,
	NULL
};
static const struct attribute_group nvt_api_attribute_group = {
	.attrs = nvt_api_attrs,
};

static int nvt_i2c_hid_driver_load(void)
{
	int ret = 0;
	int field_count = 0;
	uint32_t val = 0;
	struct i2c_hid *ihid = NULL;
	struct hid_device *hid = NULL;
	__u16 hidRegister = 0;
	struct device_node *np = NULL;
	struct kobject *touchscreen_link = NULL;
	struct i2c_client *client = NULL;
	struct hid_report *report = NULL;
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	touch_metrics_data.fw_version = DEFAULT_FW_VERSION;
	touch_metrics_data.bootup_status = BOOT_UP_INIT_STATUS;
	touch_metrics_data.fw_upgrade_result = FW_UPGRADE_STATE_INIT;
#endif

	mutex_lock(&probe_lock);
	hid_i2c_dev = &hid_i2c_client->dev;
	np = hid_i2c_client->dev.of_node;

	NVT_LOG("%s : NVT_HID_I2C_DRIVER_VERSION = %s\n", __func__,	NVT_HID_I2C_DRIVER_VERSION);

	if (!ts) {
		ts = kzalloc(sizeof(struct nvt_ts_data), GFP_KERNEL);
		if (ts == NULL) {
			NVT_ERR("failed to allocated memory for nvt ts data\n");
			ret = -ENOSPC;
			goto end;
		}
	} else {
		NVT_LOG("ts is already allocated\n");
	}
	ts->touch_ready = 0;
	ts->pen_serial = PEN_INVALID_SERIAL;

	if (!ts->xbuf) {
		ts->xbuf = kzalloc(NVT_XBUF_LEN, GFP_KERNEL);
		if (ts->xbuf == NULL) {
			NVT_ERR("kzalloc for xbuf failed!\n");
			ret = -ENOSPC;
			goto end;
		}
	} else {
		NVT_LOG("ts->xbuf is already allocated\n");
	}

	if (!nvt_ts_hidraw_flash_misc.this_device) {
		ret = misc_register(&nvt_ts_hidraw_flash_misc);
		if (ret < 0) {
			NVT_ERR("Register /dev/%s failed\n", nvt_ts_hidraw_flash_misc.name);
			ret = -ENODEV;
			goto end;
		}
		NVT_LOG("create /dev/%s\n", nvt_ts_hidraw_flash_misc.name);
	} else {
		NVT_LOG("/dev/%s is already created\n", nvt_ts_hidraw_flash_misc.name);
	}

	if (!hid_i2c_client->irq) {
		NVT_ERR("HID over i2c has not been provided an Int IRQ\n");
		ret = -EINVAL;
		goto end;
	}

	if (hid_i2c_client->irq < 0) {
		if (hid_i2c_client->irq != -EPROBE_DEFER)
			NVT_ERR("HID over i2c doesn't have a valid IRQ\n");
		ret = -EINVAL;
		goto end;
	}

	ihid = devm_kzalloc(&hid_i2c_client->dev, sizeof(*ihid), GFP_KERNEL);
	if (!ihid) {
		ret = -ENOSPC;
		goto end;
	}

	ret = i2c_hid_of_probe(hid_i2c_client, &ihid->pdata);
	if (ret) {
		goto end;
	}

	if (!device_property_read_u32(&hid_i2c_client->dev, "post-power-on-delay-ms", &val))
		ihid->pdata.post_power_delay_ms = val;

	ihid->pdata.supplies[0].supply = "vddi";
	ihid->pdata.supplies[1].supply = "avdd";
	ihid->pdata.supplies[2].supply = "avee";

	ret = devm_regulator_bulk_get(&hid_i2c_client->dev, ARRAY_SIZE(ihid->pdata.supplies), ihid->pdata.supplies);
	if (ret) {
		NVT_ERR("devm regulator bulk get failed\n");
		goto end;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(ihid->pdata.supplies), ihid->pdata.supplies);
	if (ret < 0) {
		NVT_ERR("regulator bulk enable failed\n");
		goto end;
	}

	nt36523b_share_tp_resetpin_control(1);
	NVT_LOG("display share tp resetpin control init\n");

	if (ihid->pdata.post_power_delay_ms) {
		NVT_LOG("post power on delay %dms\n", ihid->pdata.post_power_delay_ms);
		msleep(ihid->pdata.post_power_delay_ms);
	}

	i2c_set_clientdata(hid_i2c_client, ihid);

	ihid->client = hid_i2c_client;

	hidRegister = ihid->pdata.hid_descriptor_address;
	ihid->wHIDDescRegister = cpu_to_le16(hidRegister);

	init_waitqueue_head(&ihid->wait);
	mutex_init(&ihid->reset_lock);

	ret = i2c_hid_alloc_buffers(ihid, HID_MIN_BUFFER_SIZE);
	if (ret < 0) {
		NVT_ERR("Not enough memory, ret =  %d\n", ret);
		goto end;
	}

	device_enable_async_suspend(&hid_i2c_client->dev);

	/* Make sure there is something at this address */
	ret = i2c_smbus_read_byte(hid_i2c_client);
	if (ret < 0) {
		NVT_ERR("nothing at this address: %d\n",ret);
		goto end;
	}

	mutex_init(&ts->lock);
	mutex_init(&ts->xbuf_lock);
	ts->client = hid_i2c_client;

	ret = nvt_ts_check_chip_ver_trim_and_idle();
	if (ret) {
		NVT_ERR("chip is not identified\n");
		goto end;
	}

	ret = nvt_get_flash_info();
	if (ret) {
		NVT_ERR("failed to get flash info\n");
		goto end;
	}

	nvt_bootloader_reset();
	msleep(1000); // do not perform write read here before hid probe

	// now probe HID
	ret = i2c_hid_fetch_hid_descriptor(ihid);
	if (ret < 0) {
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
		touch_metrics_data.bootup_status = HID_DESCRIPTOR_ERROR;
		tp_metrics_print_func(touch_metrics_data);
#endif
		goto err_regulator;
	}

	ret = i2c_hid_init_irq(hid_i2c_client);
	if (ret < 0)
		goto err_regulator;

	hid = hid_allocate_device();
	if (IS_ERR(hid)) {
		ret = PTR_ERR(hid);
		goto err_irq;
	}

	atomic_set(&nvt_irq_stat, IRQ_ENABLE);
	ihid->hid = hid;

	hid->driver_data = hid_i2c_client;
	hid->ll_driver = &nvt_i2c_hid_ll_driver;
	hid->dev.parent = &hid_i2c_client->dev;
	hid->bus = BUS_I2C;
	hid->version = le16_to_cpu(ihid->hdesc.bcdVersion);
	hid->vendor = le16_to_cpu(ihid->hdesc.wVendorID);
	hid->product = le16_to_cpu(ihid->hdesc.wProductID);

	snprintf(hid->name, sizeof(hid->name), "%s %04hX:%04hX", hid_i2c_client->name, hid->vendor, hid->product);
	strlcpy(hid->phys, dev_name(&hid_i2c_client->dev), sizeof(hid->phys));

	ret = hid_add_device(hid);
	if (ret) {
		if (ret != -ENODEV)
			NVT_ERR("can't add hid device: %d\n", ret);
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
		touch_metrics_data.bootup_status = ADD_HID_DEVICE_FAIL;
		tp_metrics_print_func(touch_metrics_data);
#endif
		goto err_mem_free;
	}


#if PEN_TIMESTAMP_SUPPORT
	list_for_each_entry(report, &hid->report_enum[HID_INPUT_REPORT].report_list, list) {
		for (field_count = 0; field_count < report->maxfield; field_count++) {
			if ((report->field[field_count]->application == HID_DG_PEN) &&
					report->field[field_count]->usage->hid == HID_DG_SCANTIME &&
					report->field[field_count]->flags == HID_MAIN_ITEM_VARIABLE) {
				NVT_LOG("Pen timestamp is set up, enable it\n");
				pen_timestamp_support = true;
				pen_input = report->field[field_count]->hidinput->input;
				input_set_capability(pen_input, EV_MSC, MSC_TIMESTAMP);
				break;
			}
		}
		if (pen_timestamp_support)
			break;
	}

	if (!pen_timestamp_support)
		NVT_LOG("Pen timestamp is not enabled");
#endif

	if (wakeup_id) {
		if (!wkg_input_dev) {
			wkg_input_dev = input_allocate_device();
			if (!wkg_input_dev) {
				NVT_ERR("can't add wkg input device");
				ret = -ENODEV;
				goto err_mem_free;
			}
			wkg_input_dev->name = "nvt-ts-wkg";
			wkg_input_dev->phys = "input/ts";
			wkg_input_dev->id.bustype = BUS_I2C;
			ret = input_register_device(wkg_input_dev);
			if (ret) {
				NVT_ERR("register input device (%s) failed. ret=%d\n", wkg_input_dev->name, ret);
				goto err_mem_free;
			}
			device_init_wakeup(&wkg_input_dev->dev, 1);
			input_set_capability(wkg_input_dev, EV_KEY, KEY_POWER);

			client = ts->client;

			touchscreen_link = kobject_create_and_add("touchscreen", NULL);
			if (touchscreen_link != NULL) {
				ret = sysfs_create_link(touchscreen_link, &client->dev.kobj, "link_input_dev");
				if (ret == -EEXIST) {
					NVT_ERR("sysfs link %s is already created.\n", "link_input_dev");
				} else if (ret == 0) {
					if (sysfs_create_group(&client->dev.kobj, &nvt_api_attribute_group))
						NVT_ERR("Failed to create wake up gesture sysfs\n");
					else
						NVT_LOG("create gesture_wakeup under /sys/touchscreen/%s Succeeded!\n", "link_input_dev");
				} else {
					NVT_ERR("sysfs create link %s failed.\n", "link_input_dev");
				}
			}
		} else {
			NVT_LOG("wkg device is already created\n");
		}
	}

	nvt_get_fw_cus_settings();

	nvt_get_fw_info();

	ret = nvt_extra_proc_init();
	if (ret) {
		NVT_ERR("nvt extra proc init failed. ret=%d\n", ret);
		goto err_extra_proc_init_failed;
	}

	ret = nvt_mp_proc_init();
	if (ret) {
		NVT_ERR("nvt mp proc init failed. ret=%d\n", ret);
		goto err_mp_proc_init_failed;
	}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	ts->disp_notifier.notifier_call = nvt_disp_notifier_callback;
	ret = mtk_disp_notifier_register("NVT Touch", &ts->disp_notifier);
	if (ret)
		NVT_ERR("Failed to register disp notifier client:%d", ret);
#endif
	ts->touch_is_awake = true;

	/* Firmware upgrade after probe completion */
	ret = nvt_fwupg_init(ts);
	if (ret) {
		NVT_ERR("init fw upgrade fail");
		goto err_nvt_fwupg_init_failed;
	}

#if DRIVER_ESD_PROTECT
	ret = nvt_esd_protect_init(ts);
	if (ret) {
		NVT_ERR("init fw esd protect fail");
		goto err_nvt_esd_protect_init_failed;
	}
#endif
	mutex_unlock(&probe_lock);

	NVT_LOG("end of probe\n");
	return 0;

#if DRIVER_ESD_PROTECT
err_nvt_esd_protect_init_failed:
	nvt_fwupg_deinit(ts);
#endif
err_nvt_fwupg_init_failed:
	nvt_mp_proc_deinit();
err_mp_proc_init_failed:
	nvt_extra_proc_deinit();
err_extra_proc_init_failed:
	input_free_device(wkg_input_dev);
	wkg_input_dev = NULL;
err_mem_free:
	hid_destroy_device(hid);
err_irq:
	free_irq(hid_i2c_client->irq, ihid);
err_regulator:
	regulator_bulk_disable(ARRAY_SIZE(ihid->pdata.supplies), ihid->pdata.supplies);
	i2c_hid_free_buffers(ihid);
	NVT_LOG("Release reset_gpio %d\n", reset_gpio);
	gpio_free(reset_gpio);
end:
	mutex_unlock(&probe_lock);
	return ret;
}

static int nvt_i2c_hid_probe(struct i2c_client *client, const struct i2c_device_id *dev_id)
{
	int32_t ret = 0;
	int irq_gpio = 0;
	struct device_node *np = client->dev.of_node;

	/* Make sure there is something at this address */
	ret = i2c_smbus_read_byte(client);
	if (ret < 0) {
		dev_err(&client->dev, "nothing at this address 0x%x: %d\n", client->addr,ret);
		ret = -ENODEV;
		goto err;
	}

	irq_gpio = of_get_named_gpio(np, "irq-gpios", 0);
	dbg_hid("irq-gpio=%d\n", irq_gpio);
	client->irq = gpio_to_irq(irq_gpio);
	if (!client->irq) {
		dev_err(&client->dev, "HID over i2c has not been provided an Int IRQ\n");
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&probe_lock);
	hid_i2c_client = client;

	ret = nvt_i2c_hid_driver_load();
	if (ret) {
		NVT_ERR("nt36523n driver load failed\n");
		goto err;
	}

	return 0;
err:
	return ret;
}

static int nvt_i2c_hid_remove(struct i2c_client *client)
{
	struct i2c_hid *ihid = i2c_get_clientdata(client);
	struct hid_device *hid = NULL;

	mutex_lock(&probe_lock);
	NVT_LOG("%s\n", __func__);
	ts->touch_ready = 0;
#if DRIVER_ESD_PROTECT
	nvt_esd_protect_deinit(ts);
#endif
	nvt_fwupg_deinit(ts);
	nvt_extra_proc_deinit();
	nvt_mp_proc_deinit();
	hid = ihid->hid;
	if (hid)
		hid_destroy_device(hid);

	free_irq(client->irq, ihid);

	if (ihid->bufsize)
		i2c_hid_free_buffers(ihid);

	regulator_bulk_disable(ARRAY_SIZE(ihid->pdata.supplies), ihid->pdata.supplies);

	NVT_LOG("Release reset_gpio %d\n", reset_gpio);
	gpio_free(reset_gpio);
	mutex_unlock(&probe_lock);

	return 0;
}

void nvt_hid_irq_enable(void)
{
	unsigned long flag = 0;

	spin_lock_irqsave(&nvt_irq_spin, flag);

	if (atomic_read(&nvt_irq_stat) == IRQ_ENABLE) {
		goto out;
	} else {
		enable_irq(ts->client->irq);
		atomic_set(&nvt_irq_stat, IRQ_ENABLE);
	}

out:
	spin_unlock_irqrestore(&nvt_irq_spin, flag);
}

void nvt_hid_irq_disable(void)
{
	unsigned long flag = 0;

	spin_lock_irqsave(&nvt_irq_spin, flag);

	if (atomic_read(&nvt_irq_stat) == IRQ_DISABLE) {
		goto out;
	} else {
		disable_irq_nosync(ts->client->irq);
		atomic_set(&nvt_irq_stat, IRQ_DISABLE);
	}

out:
	spin_unlock_irqrestore(&nvt_irq_spin, flag);
}

static int32_t nvt_ts_resume(struct device *dev)
{
	if (ts->touch_is_awake) {
		NVT_LOG("Touch is already resume\n");
		return 0;
	}

#if DRIVER_ESD_PROTECT
	if (ts->nvt_esd_wq) {
		queue_delayed_work(ts->nvt_esd_wq, &ts->nvt_esd_work,
				msecs_to_jiffies(NVT_TOUCH_ESD_CHECK_PERIOD));
	}
#endif

	nvt_hid_irq_enable();
	ts->touch_is_awake = true;

	NVT_ERR("nvt ts resume.\n");
	return 0;
}

static int32_t nvt_ts_suspend(struct device *dev)
{
	int wake_status = 0;

	if (!ts->touch_is_awake) {
		NVT_LOG("Touch is already suspend\n");
		return 0;
	}

#if DRIVER_ESD_PROTECT
	if (ts->nvt_esd_wq)
		cancel_delayed_work_sync(&ts->nvt_esd_work);
#endif

	switch (gesture_switch) {
	case NVT_TOUCH_GESTURE_SWITCH_ON_FAIL:
		NVT_LOG("Enable gesture Mode\n");
		if (i2c_hid_command(hid_i2c_client, &hid_wkg_on_cmd, NULL, 0)) {
			NVT_ERR("failed to enable WKG Mode.\n");
		} else {
			gesture_mode = true;
			wake_status = enable_irq_wake(hid_i2c_client->irq);
			if (wake_status)
				NVT_ERR("Failed to enable irq wake\n");
			panel_gesture_mode_set_by_nt36523b(true);
			gesture_switch = NVT_TOUCH_GESTURE_SWITCH_SUCCESS;
		}
		break;
	case NVT_TOUCH_GESTURE_SWITCH_OFF_FAIL:
		NVT_LOG("Disable gesture Mode\n");
		if (i2c_hid_command(hid_i2c_client, &hid_wkg_off_cmd, NULL, 0)) {
			NVT_ERR("failed to disable WKG Mode.\n");
		} else {
			gesture_mode = false;
			wake_status = disable_irq_wake(hid_i2c_client->irq);
			if (wake_status)
				NVT_ERR("Failed to disable irq wake\n");
			panel_gesture_mode_set_by_nt36523b(false);
			gesture_switch = NVT_TOUCH_GESTURE_SWITCH_SUCCESS;
		}
		break;
	default:
		break;
	}

	if (!gesture_mode)
		nvt_hid_irq_disable();
	ts->touch_is_awake = false;

	NVT_LOG("nvt ts suspend.\n");
	return 0;
}

#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
static int nvt_disp_notifier_callback(struct notifier_block *nb,
		unsigned long value, void *v)
{
	int *data = NULL;
	struct nvt_ts_data *ts = container_of(nb, struct nvt_ts_data, disp_notifier);

	if (ts && v) {
		data = (int *)v;
		if (value == MTK_DISP_EARLY_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_POWERDOWN)
				nvt_ts_suspend(&ts->client->dev);
		} else if (value == MTK_DISP_EVENT_BLANK) {
			if (*data == MTK_DISP_BLANK_UNBLANK)
				nvt_ts_resume(&ts->client->dev);
		} else {
			NVT_LOG("value = %lu, not care\n", value);
		}
	} else {
		NVT_ERR("NT36523N touch IC can not suspend or resume");
		return -EINVAL;
	}

	return 0;
}
#endif

static const struct i2c_device_id nvt_i2c_hid_id_table[] = {
	{ "i2c-hid-of-nvt-ts", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, nvt_i2c_hid_id_table);

static const struct of_device_id nvt_ts_i2c_hid_of_match[] = {
	{ .compatible = "i2c-hid-of-nvt-ts" },
	{},
};
MODULE_DEVICE_TABLE(of, nvt_ts_i2c_hid_of_match);

static struct i2c_driver nvt_ts_i2c_hid_of_driver = {
	.driver = {
		.name = "i2c_hid_of_nvt_ts",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = of_match_ptr(nvt_ts_i2c_hid_of_match),
	},

	.probe = nvt_i2c_hid_probe,
	.remove = nvt_i2c_hid_remove,
	.id_table = nvt_i2c_hid_id_table,
};

module_i2c_driver(nvt_ts_i2c_hid_of_driver);

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
