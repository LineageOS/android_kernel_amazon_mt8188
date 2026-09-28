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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/version.h>
#include <linux/slab.h>

#include "nvt_i2c_hid.h"

struct nvt_ts_data *ts = NULL;

/*******************************************************
Description:
	Novatek touchscreen i2c read function.

return:
	Executive outcomes. 2---succeed. -5---I/O error
*******************************************************/
int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t addr, uint8_t *data, uint16_t len)
{
	uint8_t *buf = NULL;
	uint16_t read_len = 0, buf_size_max = 0;
	int32_t ret = -1;

	len--;
	read_len = len + 3;
	buf_size_max = read_len > 12 ? read_len : 12;
	buf = kzalloc(buf_size_max, GFP_ATOMIC);
	if (!buf) {
		NVT_ERR("No memory for %d bytes", buf_size_max);
		ret = -ENOMEM;
		goto end;
	}

	buf[0] = NVT_HID_ENG_REPORT_ID;
	buf[1] = 0x0B;
	buf[2] = 0x00;
	buf[3] = _u32_to_n_u8(ts->mmap->HID_I2C_ENG_ADDR, 0);
	buf[4] = _u32_to_n_u8(ts->mmap->HID_I2C_ENG_ADDR, 1);
	buf[5] = _u32_to_n_u8(ts->mmap->HID_I2C_ENG_ADDR, 2);
	buf[6] = _u32_to_n_u8(ts->hid_addr, 0);
	buf[7] = _u32_to_n_u8(ts->hid_addr, 1);
	buf[8] = _u32_to_n_u8(ts->hid_addr, 2);
	buf[9] = 0x00;
	buf[10] = _u32_to_n_u8(read_len, 0);
	buf[11] = _u32_to_n_u8(read_len, 1);

	ret = nvt_hid_i2c_set_feature(client, buf, 12);
	if (ret < 0) {
		NVT_ERR("Can not set start address and report length, ret = %d", ret);
		goto end;
	}

	buf[0] = NVT_HID_ENG_REPORT_ID;
	buf[1] = _u32_to_n_u8(read_len, 0);
	buf[2] = _u32_to_n_u8(read_len, 1);
	buf[3] = NVT_HID_ENG_REPORT_ID;
	ret = nvt_hid_i2c_get_feature(client, buf, read_len, buf, read_len);
	if (ret > 0)
		memcpy(data + 1, buf + 3, len);
end:
	if (buf)
		kfree(buf);
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen i2c write function.

return:
	Executive outcomes. 1---succeed. -5---I/O error
*******************************************************/
int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t addr, uint8_t *data, uint16_t len)
{
	uint8_t *buf = NULL;
	uint32_t write_len = 0;
	int32_t ret = -1;

	len--;
	write_len = len + 5;
	buf = kzalloc(sizeof(uint8_t) * (len + 6), GFP_ATOMIC);
	if (!buf) {
		NVT_ERR("kzalloc for buf failed!\n");
		return -ENOMEM;
	}

	buf[0] = NVT_HID_ENG_REPORT_ID;
	buf[1] = _u32_to_n_u8(write_len, 0);
	buf[2] = _u32_to_n_u8(write_len, 1);
	buf[3] = _u32_to_n_u8(ts->hid_addr, 0);
	buf[4] = _u32_to_n_u8(ts->hid_addr, 1);
	buf[5] = _u32_to_n_u8(ts->hid_addr, 2);
	memcpy(buf + 6, data + 1, len);

	ret = nvt_hid_i2c_set_feature(client, buf, len + 6);
	if (ret < 0) {
		NVT_ERR("Can not write data, ret = %d :", ret);
	} else {
		ret = 0;
	}
	if (buf)
		kfree(buf);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set index/page/addr address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_set_page(uint16_t i2c_addr, uint32_t addr)
{
	ts->hid_addr = addr;
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen write data to specify address.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[4] = { 0 };

	//---set xdata index---
	ret = nvt_set_page(I2C_FW_Address, addr);
	if (ret < 0) {
		NVT_ERR("set page 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

	//---write data to index---
	buf[0] = addr & 0xFF;
	buf[1] = data;
	ret = CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}
	ret = 0;

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen read value to specific register.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_read_reg(nvt_ts_reg_t reg, uint8_t *val)
{
	int32_t ret = 0;
	uint32_t addr = 0;
	uint8_t mask = 0;
	uint8_t shift = 0;
	uint8_t buf[8] = { 0 };
	uint8_t temp = 0;

	addr = reg.addr;
	mask = reg.mask;
	/* get shift */
	temp = reg.mask;
	shift = 0;
	while (1) {
		if ((temp >> shift) & 0x01)
			break;
		if (shift == 8) {
			NVT_ERR("mask all bits zero!\n");
			ret = -1;
			break;
		}
		shift++;
	}
	/* read the byte of the register is in */
	nvt_set_page(I2C_FW_Address, addr);
	buf[0] = addr & 0xFF;
	buf[1] = 0x00;
	ret = CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("CTP_I2C_READ failed!(%d)\n", ret);
		goto nvt_read_register_exit;
	}
	/* get register's value in its field of the byte */
	*val = (buf[1] & mask) >> shift;

nvt_read_register_exit:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen write value to specific register.

return:
	Executive outcomes. 0---succeed. -5---access fail.
*******************************************************/
int32_t nvt_write_reg(nvt_ts_reg_t reg, uint8_t val)
{
	int32_t ret = 0;
	uint32_t addr = 0;
	uint8_t mask = 0;
	uint8_t shift = 0;
	uint8_t buf[8] = { 0 };
	uint8_t temp = 0;

	addr = reg.addr;
	mask = reg.mask;
	/* get shift */
	temp = reg.mask;
	shift = 0;
	while (1) {
		if ((temp >> shift) & 0x01)
			break;
		if (shift == 8) {
			NVT_ERR("mask all bits zero!\n");
			break;
		}
		shift++;
	}
	/* read the byte including this register */
	nvt_set_page(I2C_FW_Address, addr);
	buf[0] = addr & 0xFF;
	buf[1] = 0x00;
	ret = CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("CTP_I2C_READ failed!(%d)\n", ret);
		goto nvt_write_register_exit;
	}
	/* set register's value in its field of the byte */
	temp = buf[1] & (~mask);
	temp |= ((val << shift) & mask);
	/* write back the whole byte including this register */
	nvt_set_page(I2C_FW_Address, addr);
	buf[0] = addr & 0xFF;
	buf[1] = temp;
	ret = CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("CTP_I2C_WRITE failed!(%d)\n", ret);
		goto nvt_write_register_exit;
	}

nvt_write_register_exit:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen reset MCU (boot) function.

return:
	n.a.
*******************************************************/
void nvt_bootloader_reset(void)
{
	NVT_LOG("start\n");

	nvt_write_addr(ts->mmap->SWRST_SIF_ADDR, 0x69);
	switch_pkt(PACKET_NORMAL);

	msleep(50);

	NVT_LOG("end\n");
}

/*******************************************************
Description:
	Novatek touchscreen clear FW status function.

return:
	Executive outcomes. 0---succeed. -1---fail.
*******************************************************/
int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[8] = { 0 };
	int32_t i = 0;
	const int32_t retry = 20;

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---clear fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

		//---read fw status---
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0xFF;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if (buf[1] == 0x00)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW status function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_status(void)
{
	uint8_t buf[8] = { 0 };
	int32_t i = 0;
	const int32_t retry = 50;

	usleep_range(20000, 20000);

	for (i = 0; i < retry; i++) {
		//---set xdata index to EVENT BUF ADDR---
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

		//---read fw status---
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

		if ((buf[1] & 0xF0) == 0xA0)
			break;

		usleep_range(10000, 10000);
	}

	if (i >= retry) {
		NVT_ERR("failed, i=%d, buf[1]=0x%02X\n", i, buf[1]);
		return -1;
	} else {
		return 0;
	}
}

/*******************************************************
Description:
	Novatek touchscreen check FW reset state function.

return:
	Executive outcomes. 0---succeed. -1---failed.
*******************************************************/
int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state)
{
	uint8_t buf[8] = { 0 };
	int32_t ret = 0;
	int32_t retry = 0;

	while (1) {
		usleep_range(10000, 10000);

		//---read reset state---
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_RESET_COMPLETE);
		buf[0] = EVENT_MAP_RESET_COMPLETE;
		buf[1] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 6);

		if ((buf[1] >= check_reset_state) && (buf[1] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}

		retry++;
		if (unlikely(retry > 100)) {
			NVT_ERR("error, retry=%d, buf[1]=0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
					retry, buf[1], buf[2], buf[3], buf[4], buf[5]);
			ret = -1;
			break;
		}
	}

	if (!ret)
		NVT_LOG("Reset state 0x%02X pass\n", check_reset_state);

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen query firmware config.
	function.

return:
	Executive outcomes. 0---success. negative---fail.
*******************************************************/
int32_t nvt_query_fw_config(uint8_t segment, uint8_t offset, uint8_t *configs,
			    uint8_t len)
{
	int32_t ret = 0;
	uint8_t buf[16] = { 0 };
	int32_t i = 0;
	const int32_t retry = 10;

	if (offset > 7 || len > 8) {
		NVT_ERR("offset=%d, len=%d\n", offset, len);
		ret = -EINVAL;
		goto out;
	}

	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = 0x67; /* query cmd */
	buf[2] = 0x00; /* clear handshake fw status */
	buf[3] = 0x01; /* query extend config. */
	buf[4] = segment; /* query which segment */
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 5);

	usleep_range(10000, 10000);

	for (i = 0; i < retry; i++) {
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0x00;
		buf[2] = 0x00;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 13);

		if (buf[1] == 0xFB)
			break;
		if (buf[2] == 0xA0)
			break;
		usleep_range(1000, 1000);
	}

	if (unlikely(i >= retry)) {
		NVT_ERR("query failed, segment=%d, buf[1]=0x%02X, buf[2]=0x%02X\n", segment, buf[1], buf[2]);
		ret = -1;
	} else if (buf[1] == 0xFB) {
		NVT_ERR("query cmd / extend config / segment not support by this FW, segment=%d\n", segment);
		ret = -1;
	} else {
		memcpy(configs, buf + 5 + offset, len);
		for (i = 0; i < len; i++)
			printk("0x%02X, ", *(configs + i));
		printk("\n");
		ret = 0;
	}

out:
	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen get custom settings function.

return:
	NULL
*******************************************************/
void nvt_get_fw_cus_settings(void)
{
	uint8_t buf[4] = { 0 };
#if DRIVER_ESD_PROTECT
	uint32_t cus_esd_chk_addr = 0;
	uint32_t cus_esd_addr_low = 0;
	uint32_t cus_esd_addr_med = 0;
	uint32_t cus_esd_addr_high = 0;
#endif

#if DRIVER_ESD_PROTECT
	ts->hid_addr = ts->mmap->CUS_ESD_CHECK_ADDR;
	buf[0] = ts->mmap->CUS_ESD_CHECK_ADDR & 0xff;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 4);
	cus_esd_addr_low = (uint32_t)buf[1];
	cus_esd_addr_med = (uint32_t)buf[2] << 8;
	cus_esd_addr_high = (uint32_t)buf[3] << 16;
	cus_esd_chk_addr = cus_esd_addr_low | cus_esd_addr_med | cus_esd_addr_high;
	if (cus_esd_chk_addr == ESD_ADDR) {
		NVT_LOG("Customized esd_chk_addr detected (0x%06X)\n", cus_esd_chk_addr);
		ts->mmap->ESD_CHECK_ADDR = cus_esd_chk_addr;
	} else {
		NVT_ERR("ESD driver protect is not supported in FW\n");
		ts->mmap->ESD_CHECK_ADDR = 0;
	}
#endif
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	Executive outcomes. 0---success. -1---fail.
*******************************************************/
int32_t nvt_get_fw_info(void)
{
	uint8_t buf[64] = { 0 };
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[0] = EVENT_MAP_FWINFO;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 39);
	if ((buf[1] + buf[2]) != 0xFF) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		if (retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			ts->fw_ver = 0;
			NVT_ERR("Set default fw_ver=%d\n", ts->fw_ver);
			ret = -1;
			goto out;
		}
	}
	ts->fw_ver = buf[1];
	ts->x_num = buf[3];
	ts->y_num = buf[4];
	ts->nvt_pid = (uint16_t)((buf[36] << 8) | buf[35]);

	NVT_LOG("fw_ver=0x%02X, fw_type=0x%02X, PID=0x%04X\n", ts->fw_ver, buf[14], ts->nvt_pid);
	NVT_LOG("x_num=%d, y_num=%d\n", ts->x_num, ts->y_num);

	if (ts->pen_support) {
		ts->pen_x_num_x = ts->x_num;
		ts->pen_x_num_y = buf[38];
		ts->pen_y_num_x = buf[37];
		ts->pen_y_num_y = ts->y_num;
		NVT_LOG("pen_x_num_x=%d, pen_x_num_y=%d\n", ts->pen_x_num_x, ts->pen_x_num_y);
		NVT_LOG("pen_y_num_x=%d, pen_y_num_y=%d\n", ts->pen_y_num_x, ts->pen_y_num_y);
	}

	ret = 0;
out:

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen set customized command function.

return:
	Executive outcomes. 0---success. -5---fail.
*******************************************************/
int32_t nvt_set_custom_cmd(uint8_t cmd)
{
	int32_t ret = 0;
	int32_t i = 0;
	const int32_t retry = 20;
	uint8_t buf[4] = { 0 };

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
	//---set cmd---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = cmd;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

	usleep_range(10000, 10000);

	for (i = 0; i < retry; i++) {
		//---read cmd status---
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);
		buf[0] = EVENT_MAP_HOST_CMD;
		buf[1] = 0xFF;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);
		if (buf[1] == 0x00)
			break;
		usleep_range(1000, 1000);
	}

	if (unlikely(i >= retry)) {
		NVT_ERR("send cmd 0x%02X failed, buf[1]=0x%02X\n", cmd, buf[1]);
		ret = -EIO;
	} else {
		NVT_LOG("send cmd 0x%02X success, tried %d times\n", cmd, i);
		ret = 0;
	}

	return ret;
}

void nvt_read_fw_history(uint32_t fw_history_addr)
{
	uint8_t i = 0;
	uint8_t buf[65] = {0};
	char str[128];

	if (fw_history_addr == 0)
		return;

	nvt_set_page(I2C_FW_Address, fw_history_addr);

	buf[0] = (uint8_t)(fw_history_addr & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 64 + 1); //read 64bytes history

	//print all data
	NVT_LOG("fw history 0x%X: \n", fw_history_addr);
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str), "%02X %02X %02X %02X %02X %02X %02X %02X  "
				"%02X %02X %02X %02X %02X %02X %02X %02X\n",
				buf[1 + i * 16], buf[2 + i * 16], buf[3 + i * 16], buf[4 + i * 16],
				buf[5 + i * 16], buf[6 + i * 16], buf[7 + i * 16], buf[8 + i * 16],
				buf[9 + i * 16], buf[10 + i * 16], buf[11 + i * 16], buf[12 + i * 16],
				buf[13 + i * 16], buf[14 + i * 16], buf[15 + i * 16], buf[16 + i * 16]);
		NVT_LOG("%s", str);
	}

	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR);
	msleep(500);
}

/*******************************************************
Description:
	Novatek touchscreen check and stop crc reboot loop.

return:
	n.a.
*******************************************************/
void nvt_stop_crc_reboot(void)
{
	uint8_t buf[8] = { 0 };
	int32_t retry = 0;

	//read dummy buffer to check CRC fail reboot is happening or not

	//---change I2C index to prevent geting 0xFF, but not 0xFC---
	nvt_set_page(I2C_FW_Address, ts->mmap->CHIP_VER_TRIM_ADDR);

	//---read to check if buf is 0xFC which means IC is in CRC reboot ---
	buf[0] = ts->mmap->CHIP_VER_TRIM_ADDR & 0xFF;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 4);

	if ((buf[1] == 0xFC) ||
	    ((buf[1] == 0xFF) && (buf[2] == 0xFF) && (buf[3] == 0xFF))) {
		//IC is in CRC fail reboot loop, needs to be stopped!
		for (retry = 5; retry > 0; retry--) {
			//---write i2c cmds to reset idle : 1st---
			nvt_write_addr(ts->mmap->SWRST_SIF_ADDR, 0xAA);

			//---write i2c cmds to reset idle : 2rd---
			nvt_write_addr(ts->mmap->SWRST_SIF_ADDR, 0xAA);
			msleep(1);

			//---clear CRC_ERR_FLAG---
			nvt_set_page(I2C_FW_Address, ts->mmap->BLD_SPE_PUPS_ADDR);

			buf[0] = 0x35;
			buf[1] = 0xA5;
			CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

			//---check CRC_ERR_FLAG---
			nvt_set_page(I2C_FW_Address, ts->mmap->BLD_SPE_PUPS_ADDR);

			buf[0] = 0x35;
			buf[1] = 0x00;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

			if (buf[1] == 0xA5)
				break;
		}
		if (retry == 0)
			NVT_ERR("CRC auto reboot is not able to be stopped! buf[1]=0x%02X\n",
				buf[1]);
	}

	return;
}

void switch_pkt(uint8_t pkt_type)
{
	switch (pkt_type) {
	case PACKET_NORMAL:
		NVT_LOG("Switch packet to normal\n");
		memcpy(prefix_pkt, trim_id_table[ts->idx].prefix_pkt_normal, PREFIX_PKT_LENGTH);
		break;
	case PACKET_RECOVERY:
		NVT_LOG("Switch packet to recovery\n");
		memcpy(prefix_pkt, trim_id_table[ts->idx].prefix_pkt_recovery, PREFIX_PKT_LENGTH);
		break;
	default:
		NVT_ERR("type not supported");
	}

	set_prefix_packet();
}

int32_t match_trim_id(uint8_t *buf)
{
	uint8_t idx = 0, table_size = 0, i = 0;
	int32_t ret = 0;

	table_size = sizeof(trim_id_table) / sizeof(trim_id_table[0]);
	for (idx = 0; idx < table_size; idx++) {
		for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
			if (trim_id_table[idx].mask[i]) {
				if (buf[i] != trim_id_table[idx].id[i])
					break;
			}
		}

		if (i == NVT_ID_BYTE_MAX) {
			NVT_LOG("Found matching NVT_ID  [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]",
					buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
			/* if there are memory maps that have the same chip addr and sif addr,
			 * we could be using a differet one from the start.
			 * So, update the memory map here again*/
			ts->mmap = trim_id_table[idx].mmap;
			ts->fmap = trim_id_table[idx].fmap;
			ret = 0;
			goto end;
		}
	}

	ret = -EAGAIN;

end:
	return ret;
}

void nvt_sw_reset_then_idle(void)
{
	uint8_t buf[2] = { 0 };

	NVT_LOG("\n");
	nvt_set_page(I2C_FW_Address, ts->mmap->SWRST_SIF_ADDR);
	buf[1] = 0x55;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	msleep(50);
	nvt_set_page(I2C_FW_Address, ts->mmap->SW_IDLE_ADDR);
	buf[1] = 0x01;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	nvt_set_page(I2C_FW_Address, ts->mmap->SUSLO_ADDR);
	buf[1] = 0x55;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	nvt_set_page(I2C_FW_Address, ts->mmap->SW_IDLE_ADDR);
	buf[1] = 0x01;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	nvt_set_page(I2C_FW_Address, ts->mmap->SUSLO_ADDR);
	buf[1] = 0x55;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	msleep(50);
}

void nvt_sw_reset_and_idle(void)
{
	uint8_t buf[2] = { 0 };

	NVT_LOG("\n");
	nvt_set_page(I2C_FW_Address, ts->mmap->SWRST_SIF_ADDR);
	buf[1] = 0xAA;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	msleep(50);
	switch_pkt(PACKET_RECOVERY);
	nvt_set_page(I2C_FW_Address, ts->mmap->SWRST_SIF_ADDR);
	buf[1] = 0xAA;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	msleep(50);
}

int32_t check_crc_done(void)
{
	uint8_t buf[6] = { 0 }, retry = 50;
	int32_t ret = 0;

	while (--retry) {
		msleep(50);
		nvt_set_page(I2C_FW_Address, ts->mmap->CRC_FLAG_ADDR);
		buf[1] = 0;
		CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 6);
		if (buf[1] & 0x04)
			break;
	}

	if (!retry) {
		NVT_ERR("Failed to get crc done, buf[1] = %02X, buf[2] = %02X,  buf[3] = %02X, buf[4] = %02X, buf[5] = %02X\n",
				buf[1], buf[2], buf[3], buf[4], buf[5]);
		ret = -EAGAIN;
	} else {
		NVT_LOG("crc done ok");
	}

	return ret;
}

void nvt_init_cmd_reg(uint8_t idx)
{
	uint8_t buf[3] = { 0 };

	switch_pkt(PACKET_RECOVERY);
	nvt_set_page(I2C_FW_Address, ts->mmap->CMD_REG_ADDR);
	buf[1] = trim_id_table[idx].prefix_pkt_normal[0];
	buf[2] = trim_id_table[idx].prefix_pkt_normal[1];
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
	msleep(15);
	switch_pkt(PACKET_NORMAL);
}

/*******************************************************
Description:
	Novatek touchscreen check chip version trim function.

return:
	Executive outcomes. 0---NVT IC. -1---not NVT IC.
*******************************************************/
int8_t nvt_ts_check_chip_ver_trim_and_idle(void)
{
	uint8_t buf[8] = { 0 }, idx = 0;
	int32_t retry = 0;
	int32_t ret = -1;

	retry = 20;
	while (--retry) {
		for (idx = 0; idx < TRIM_TYPE_MAX; idx++) {
			NVT_LOG("Current table idx = %d\n", idx);
			ts->mmap = trim_id_table[idx].mmap;
			ts->fmap = trim_id_table[idx].fmap;
			ts->bmap = trim_id_table[idx].bmap;
			ts->idx = idx;
			nvt_init_cmd_reg(idx);
			nvt_bootloader_reset();
			nvt_init_cmd_reg(idx);
			nvt_sw_reset_and_idle();
			nvt_stop_crc_reboot();
			nvt_set_page(I2C_FW_Address, ts->mmap->CHIP_VER_TRIM_ADDR);
			buf[1] = 0x00;
			buf[2] = 0x00;
			buf[3] = 0x00;
			buf[4] = 0x00;
			buf[5] = 0x00;
			buf[6] = 0x00;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 7);
			NVT_LOG("buf[1]=0x%02X, buf[2]=0x%02X, buf[3]=0x%02X, buf[4]=0x%02X, buf[5]=0x%02X, buf[6]=0x%02X\n",
					buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
			ret = match_trim_id(buf + 1);
			if (ret == 0)
				goto end;
			msleep(50);
		}
	}

	if (!retry) {
		NVT_ERR("No matching NVT_ID found [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]\n",
			buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
		ret = -EAGAIN;
		goto end;
	}

end:
	return ret;
}

MODULE_DESCRIPTION("Novatek Touchscreen Driver");
MODULE_LICENSE("GPL");
