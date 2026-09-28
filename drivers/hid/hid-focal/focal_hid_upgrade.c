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
#include "focal_hid_core.h"

#define FTS_FW_REQUEST_SUPPORT 1
#define FTS_AUTO_UPGRADE_EN 1

#define FTS_FW_NAME_PREX_WITH_REQUEST_KD "focaltech_ts_hid_fw_kd"
#define FTS_FW_NAME_PREX_WITH_REQUEST_TG "focaltech_ts_hid_fw_tg"

#define FTS_CMD_ECC_INIT 0x64
#define FTS_CMD_ECC_CAL 0x65
#define FTS_CMD_ECC_READ 0x66
#define FTS_CMD_START1 0x55
#define FTS_CMD_START_DELAY 12
#define FTS_CMD_READ_ID 0x90

#define FTS_CMD_ECC_CAL_LEN 7
#define FTS_CMD_FLASH_STATUS 0x6A
#define FTS_CMD_FLASH_STATUS_LEN 2
#define BYTE_OFF_0(x) ((u8)((x) & 0xFF))
#define BYTE_OFF_8(x) ((u8)(((x) >> 8) & 0xFF))
#define BYTE_OFF_16(x) ((u8)(((x) >> 16) & 0xFF))
#define BYTE_OFF_24(x) ((u8)(((x) >> 24) & 0xFF))
#define FTS_CMD_RESET 0x07
#define FTS_DELAY_UPGRADE_RESET 100
#define AL2_FCS_COEF ((1 << 15) + (1 << 10) + (1 << 3))

//0x40
static int fts_enter_into_upgrade_mode(void)
{
	u8 cmd = CMD_ENTER_UPGRADE_MODE;
	int ret = 0;

	ret = fts_direct_write_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	ret = fts_direct_read_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		return -EINVAL;
	}

	if (cmd != 0xF0) {
		FTS_ERROR("%s fail %x\n", __func__, cmd);
		return -EINVAL;
	} else {
		FTS_INFO("%s success\n", __func__);
	}
	return 0;
}

//0x41
static int fts_check_current_state(void)
{
	u8 cmd = CMD_CHECK_CURRENT_STATE;
	u8 read_buf[2] = { 0 };
	int ret = 0;

	ret = fts_direct_write_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	ret = fts_direct_read_data(read_buf, 2);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		return -EINVAL;
	}

	if ((read_buf[0] != 0x41) || (read_buf[1] != 0x01)) {
		FTS_ERROR("cmd %x, not in upgrade mode %x fail\n", read_buf[0],
			  read_buf[1]);
		return -EINVAL;
	} else {
		FTS_INFO("enter into upgrade mode success\n");
	}
	return 0;
}

//0x42
static int fts_check_is_ready_for_upgrade(void)
{
	u8 cmd = CMD_READY_FOR_UPGRADE;
	u8 read_buf[2] = { 0 };
	int ret = 0;

	ret = fts_direct_write_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	ret = fts_direct_read_data(read_buf, 2);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		return -EINVAL;
	}

	if ((read_buf[0] != 0x42) || (read_buf[1] != 0x02)) {
		FTS_ERROR("cmd %x, not ready to upgrade %x fail\n", read_buf[0],
			  read_buf[1]);
		return -EINVAL;
	} else {
		FTS_INFO("ready to upgrade...\n");
	}
	return 0;
}

//0x43
static int fts_send_data(u8 *data, int len)
{
	u8 write_buf[72] = { 0 };
	u8 read_buf = 0;
	int packet_number = 0;
	int remainder = 0;
	int packet_len = 0;
	int i = 0;
	int offset = 0;
	int ret = 0;

	packet_number = len / FTS_UPGRADE_ONE_PACKET_LEN;
	remainder = len % FTS_UPGRADE_ONE_PACKET_LEN;
	if (remainder > 0)
		packet_number++;
	packet_len = FTS_UPGRADE_ONE_PACKET_LEN;
	FTS_INFO("write data, num:%d remainder:%d", packet_number, remainder);

	for (i = 0; i < packet_number; i++) {
		offset = i * FTS_UPGRADE_ONE_PACKET_LEN;
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;

		if (i == 0) {
			write_buf[0] = CMD_SEND_DATA;
			write_buf[1] = 0;
			memcpy(write_buf + 2, data + offset, packet_len);
		} else if (i == (packet_number - 1)) {
			write_buf[0] = CMD_SEND_DATA;
			write_buf[1] = 2;
			memcpy(write_buf + 2, data + offset, packet_len);
		} else {
			write_buf[0] = CMD_SEND_DATA;
			write_buf[1] = 1;
			memcpy(write_buf + 2, data + offset, packet_len);
		}

		ret = fts_direct_write_data(write_buf, packet_len + 2);
		if (ret) {
			FTS_ERROR("fts_direct_write_data fail\n");
			return -EINVAL;
		}

		ret = fts_direct_read_data(&read_buf, 1);
		if (ret) {
			FTS_ERROR("fts_direct_read_data fail\n");
			return -EINVAL;
		}
		if (read_buf != 0xF0) {
			FTS_ERROR("hid upgrade fail\n");
			return -1;
		}
	}

	return 0;
}

//0x44
static int fts_flush_data2flash(void)
{
	u8 cmd = CMD_UPGRADE_FLUSH_DATA;
	u8 data[5] = { 0 };
	int ret = 0;

	ret = fts_direct_write_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	msleep(100);

	ret = fts_direct_read_data(data, 5);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		return -EINVAL;
	}

	return 0;
}

//0x45
static int fts_exit_upgrade_mode(void)
{
	u8 cmd = CMD_EXIT_UPRADE_MODE;
	int ret = 0;

	ret = fts_direct_write_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	ret = fts_direct_read_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		return -EINVAL;
	}

	if (cmd != 0xF0) {
		FTS_ERROR("exit_upgrade_mode fail %x\n", cmd);
		return -EINVAL;
	} else
		FTS_INFO("exit_upgrade_mode success\n");

	msleep(FTS_DELAY_UPGRADE_RESET);

	return 0;
}

//0x46
static int focal_read_upgrade_id(void)
{
	u8 cmd = CMD_USB_READ_UPGRADE_ID;
	u8 read_buf[3] = { 0 };
	int ret = 0;

	ret = fts_direct_write_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	ret = fts_direct_read_data(read_buf, 3);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		return -EINVAL;
	}

	if ((read_buf[0] != 0x46) || (read_buf[1] != FTS_BOOT_ID_H) ||
	    (read_buf[2] != FTS_BOOT_ID_L)) {
		FTS_ERROR("cmd %x, read id 0x%x%x fail\n", read_buf[0],
			  read_buf[1], read_buf[2]);
		return -EINVAL;
	} else {
		FTS_INFO("success read id 0x%x%x", read_buf[1], read_buf[2]);
	}
	return 0;
}

static int fts_select_upgrade_mode(u8 mode)
{
	u8 cmd[2] = { 0 };
	u8 data[2] = { 0 };
	int ret = 0;

	cmd[0] = CMD_USB_UPGRADE_MODE;
	cmd[1] = mode;
	ret = fts_direct_write_data(cmd, 2);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	ret = fts_direct_read_data(data, 2);
	if (ret) {
		FTS_ERROR("fts_direct_read_data fail\n");
		return -EINVAL;
	}

	if ((data[0] != CMD_USB_UPGRADE_MODE) || (data[1] != mode)) {
		FTS_ERROR("%s fail cmd:%x, mode:%x\n", __func__, data[0], data[1]);
		return -EINVAL;
	} else {
		FTS_INFO("%s success\n", __func__);
	}

	return 0;
}

//0x47
static int fts_erase_flash(void)
{
	u8 cmd = CMD_USB_ERASE_FLASH;
	int ret = 0;
	int i = 0;

	ret = fts_direct_write_data(&cmd, 1);
	if (ret) {
		FTS_ERROR("fts_direct_write_data fail\n");
		return -EINVAL;
	}

	msleep(2000);

	for (i = 0; i < 10; i++) {
		ret = fts_direct_read_data(&cmd, 1);
		if (ret) {
			FTS_ERROR("fts_direct_read_data fail\n");
			continue;
		}
		break;
	}

	if (cmd != 0xF0) {
		FTS_ERROR("fts_erase_flash fail %x\n", cmd);
		return -EINVAL;
	} else
		FTS_INFO("%s success\n", __func__);

	return 0;
}

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int focal_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;

	do {
		ret = focal_read_register(FTS_REG_CHIP_ID_H, &idh);
		if (idh == FTS_CHIP_ID_H) {
			FTS_INFO("TP Ready,Device ID:0x%02x", idh);
			return 0;
		} else {
			FTS_ERROR("TP Not Ready,ReadData:0x%02x,ret:%d", idh, ret);
		}
		cnt++;
	} while ((cnt * 200) < 1000);

	return -EIO;
}

static u16 fts_crc16_calc_host(u8 *pbuf, u32 length)
{
	u16 ecc = 0;
	int i = 0;
	int j = 0;

	for (i = 0; i < length; i += 2) {
		ecc ^= ((pbuf[i] << 8) | (pbuf[i + 1]));
		for (j = 0; j < 16; j++) {
			if (ecc & 0x01)
				ecc = (u16)((ecc >> 1) ^ AL2_FCS_COEF);
			else
				ecc >>= 1;
		}
	}

	return ecc;
}

static int fts_fwupg_check_id(u8 idl, u8 idh)
{
	int ret = 0;
	u8 cmd = FTS_CMD_START1;
	u8 id[2] = { 0 };

	ret = focal_write(&cmd, 1);
	if (ret < 0) {
		FTS_ERROR("write cmd 0x55 fail");
		return ret;
	}

	msleep(FTS_CMD_START_DELAY);

	/* read out check sum */
	cmd = FTS_CMD_READ_ID;
	ret = focal_read(&cmd, 1, id, 2);
	if (ret < 0) {
		FTS_ERROR("read id fail");
		return ret;
	}
	FTS_INFO("read id:%x %x", id[0], id[1]);
	if (id[0] == idl && id[1] == idh)
		return ret;

	return -EINVAL;
}

/************************************************************************
 * Name: fts_fwupg_ecc_cal
 * Brief: calculate and get ecc from tp
 * Input: addr - start address need calculate ecc
 *        len - length need calculate ecc
 * Output:
 * Return: return data ecc of tp if success, otherwise return error code
 ***********************************************************************/
static int fts_fwupg_ecc_cal(u32 addr, u32 len, int crc_val)
{
	int ret = 0;
	u32 i = 0;
	u32 cmdlen = FTS_CMD_ECC_CAL_LEN;
	u8 wbuf[FTS_CMD_ECC_CAL_LEN] = { 0 };
	u8 val[FTS_CMD_FLASH_STATUS_LEN] = { 0 };
	int ecc = 0;
	int ecc_len = 2;
	int timeout_cnt = 10;

	/* crc check init */
	wbuf[0] = FTS_CMD_ECC_INIT;
	ret = focal_write(wbuf, 1);
	if (ret < 0) {
		FTS_ERROR("ecc init cmd write fail");
		return ret;
	}

	/* send commond to start crc check */
	wbuf[0] = FTS_CMD_ECC_CAL;
	wbuf[1] = BYTE_OFF_16(addr);
	wbuf[2] = BYTE_OFF_8(addr);
	wbuf[3] = BYTE_OFF_0(addr);

	wbuf[4] = BYTE_OFF_16(len);
	wbuf[5] = BYTE_OFF_8(len);
	wbuf[6] = BYTE_OFF_0(len);

	FTS_DEBUG("ecc calc start addr:0x%04x, len:%d", addr, len);
	ret = focal_write(wbuf, cmdlen);
	if (ret < 0) {
		FTS_ERROR("ecc calc cmd write fail");
		return ret;
	}

	msleep(100);

	wbuf[0] = FTS_CMD_ECC_READ;
	do {
		FTS_INFO("ecc check times:%d, %x", i, crc_val);
		/* read out crc check value */
		ret = focal_read(wbuf, 1, val, ecc_len);
		if (ret < 0) {
			FTS_ERROR("ecc read cmd write fail");
			return ret;
		}
		ecc = (int)((u16)(val[0] << 8) + val[1]);
		FTS_INFO("ecc check:%x %x", crc_val, ecc);
		timeout_cnt--;
		msleep(20);
	} while (crc_val != ecc && timeout_cnt);

	if (crc_val != ecc) {
		FTS_ERROR("crc_val != ecc after retry 10 times");
		return -EINVAL;
	}

	return ecc;
}

/************************************************************************
 * Name: fts_fwupg_reset_in_boot
 * Brief: RST CMD(07), reset to romboot(bootloader) in boot environment
 * Input:
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_fwupg_reset_in_boot(void)
{
	int ret = 0;
	u8 cmd = FTS_CMD_RESET;

	FTS_INFO("reset in boot environment");
	ret = focal_write(&cmd, 1);
	if (ret < 0) {
		FTS_ERROR("pram/rom/bootloader reset cmd write fail");
		return ret;
	}

	msleep(FTS_DELAY_UPGRADE_RESET);

	return 0;
}

static int fts_upgrade_firmware(u8 *buf, u32 buf_len, u8 mode)
{
	int ret = 0;
	int i = 0;
	int tp_crc = 0;
	int host_crc = 0;
	u8 hid2std_buf[3] = { 0xEB, 0xAA, 0x09 };
	int exit_upgrade_status = 0;

	host_crc = fts_crc16_calc_host(buf, buf_len);

	for (i = 0; i < 15; i++) {
		if (!fts_enter_into_upgrade_mode())
			break;
	}
	if (i >= 15) {
		FTS_ERROR("enter upgrade mode fail");
		goto out;
	}

	ret = focal_read_upgrade_id();
	if (ret) {
		FTS_ERROR("read upgrade id fail");
		goto out;
	}

	ret = fts_select_upgrade_mode(mode);
	if (ret) {
		FTS_ERROR("fts_select_upgrade_mode fail");
		goto out;
	}

	ret = fts_check_current_state();
	if (ret) {
		FTS_ERROR("fts_check_current_state fail");
		goto out;
	}

	ret = fts_erase_flash();
	if (ret) {
		FTS_ERROR("fts_erase_flash fail");
		goto out;
	}

	ret = fts_check_is_ready_for_upgrade();
	if (ret) {
		FTS_ERROR("fts_check_is_ready_for_upgrade fail");
		goto out;
	}

	ret = fts_send_data(buf, buf_len);
	if (ret) {
		FTS_ERROR("fts_send_data fail");
		goto out;
	}

	ret = fts_flush_data2flash();
	if (ret < 0) {
		FTS_ERROR("fts_flush_data2flash fail");
		goto out;
	}

	/* The hid mode does not support CRC verification,switch to i2c mode */
	ret = focal_write(hid2std_buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
		goto i2c_out;
	}
	msleep(10);

	ret = fts_fwupg_check_id(FTS_BOOT_ID_H, FTS_BOOT_ID_L);
	if (ret < 0) {
		FTS_ERROR("fts_fwupg_check_id fail");
		goto i2c_out;
	}

	tp_crc = fts_fwupg_ecc_cal(0, buf_len, host_crc);
	if (tp_crc < 0) {
		FTS_ERROR("ecc read fail");
		ret = -EINVAL;
		goto i2c_out;
	}

	FTS_INFO("tp_crc = %x, host_crc = %x\n", tp_crc, host_crc);
	if (tp_crc != host_crc) {
		FTS_ERROR("tp_crc != host_crc, upgrade fail\n");
		ret = -EINVAL;
		goto i2c_out;
	}

	ret = 0;

i2c_out:
	exit_upgrade_status = fts_fwupg_reset_in_boot();
	if (exit_upgrade_status < 0)
		FTS_ERROR("fwupg reset in boot fail, %d\n", exit_upgrade_status);

	return ret;
out:
	exit_upgrade_status = fts_exit_upgrade_mode();
	if (exit_upgrade_status < 0)
		FTS_ERROR("exit upgrade mode fail, %d\n", exit_upgrade_status);

	return ret;
}

static int focal_read_file_default(char *file_name, u8 **file_buf)
{
	int ret = 0;
	char file_path[FILE_NAME_LENGTH] = { 0 };
	struct file *filp = NULL;
	struct inode *inode;
	mm_segment_t old_fs;
	loff_t pos;
	loff_t file_len = 0;

	if ((file_name == NULL) || (file_buf == NULL)) {
		FTS_ERROR("filename/filebuf is NULL");
		return -EINVAL;
	}

	snprintf(file_path, FILE_NAME_LENGTH, "%s%s", FTS_FW_BIN_FILEPATH,
		 file_name);
	filp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		FTS_ERROR("open %s file fail", file_path);
		return -ENOENT;
	}

	inode = filp->f_inode;

	file_len = inode->i_size;
	*file_buf = (u8 *)vmalloc(file_len);
	if (*file_buf == NULL) {
		FTS_ERROR("file buf malloc fail");
		filp_close(filp, NULL);
		return -ENOMEM;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	ret = vfs_read(filp, *file_buf, file_len, &pos);
	if (ret < 0)
		FTS_ERROR("read file fail");
	FTS_INFO("file len:%d read len:%d pos:%d", (u32)file_len, ret,
		 (u32)pos);
	filp_close(filp, NULL);
	set_fs(old_fs);

	return ret;
}

static int focal_read_file_request_firmware(char *file_name, u8 **file_buf)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	char fwname[FILE_NAME_LENGTH] = { 0 };

#if !FTS_FW_REQUEST_SUPPORT
	return -EINVAL;
#endif

	snprintf(fwname, FILE_NAME_LENGTH, "%s", file_name);
	ret = request_firmware(&fw, fwname, fts_hid_data->dev);
	if (ret == 0) {
		FTS_INFO("firmware(%s) request successfully", fwname);
		*file_buf = vmalloc(fw->size);
		if (*file_buf == NULL) {
			FTS_ERROR("fw buffer vmalloc fail");
			ret = -ENOMEM;
		} else {
			memcpy(*file_buf, fw->data, fw->size);
			ret = fw->size;
		}
	} else {
		FTS_ERROR("firmware(%s) request fail,ret=%d", fwname, ret);
		ret = -EIO;
	}

	if (fw != NULL) {
		release_firmware(fw);
		fw = NULL;
	}

	return ret;
}

static int focal_read_file(char *file_name, u8 **file_buf)
{
	int ret = 0;

	ret = focal_read_file_request_firmware(file_name, file_buf);
	if (ret < 0) {
		ret = focal_read_file_default(file_name, file_buf);
		if (ret < 0) {
			FTS_ERROR("get fw file(default) fail");
			return ret;
		}
	}

	return ret;
}

int focal_hid_upgrade_bin(char *fw_name)
{
	int ret = 0;
	u32 fw_file_len = 0;
	u8 *fw_file_buf = NULL;
	u8 value = 0;
	int i = 0;
	int j = 0;

	ret = focal_read_file(fw_name, &fw_file_buf);
	if ((ret < 0) || (ret < 0x120)) {
		FTS_ERROR("read fw bin file(%s) fail, len:%d", fw_name, ret);
		goto err_bin;
	}

	fw_file_len = ret;
	FTS_INFO("fw bin file len:%d", fw_file_len);

	fts_hid_data->fw_loading = true;
	panel_touch_upgrade_status_set_by_ft8205(true);
	__pm_stay_awake(fts_hid_data->focal_upg_wakelock);
	focal_esdcheck_switch(fts_hid_data, DISABLE_ESD_CHECK);
	do {
		ret = fts_upgrade_firmware(fw_file_buf, fw_file_len, FLASH_MODE_UPGRADE_VALUE);
		if (ret) {
			FTS_ERROR("upgrade fw bin fail\n");
		} else {
			for (i = 0; i < 10; i++) {
				focal_write_register(FTS_REG_FACE_DEC_MODE_EN, 0x01);
				msleep(1);
				focal_read_register(FTS_REG_FACE_DEC_MODE_EN, &value);
				if (value == 1)
					break;
				FTS_INFO("read FTS_REG_FACE_DEC_MODE_EN %x\n", value);
				msleep(10);
			}
			for (j = 0; j < 50; j++) {
				ret = focal_read_register(FTS_REG_FW_VER, &value);
				if (ret) {
					FTS_INFO("read A6 %d time\n", j);
					msleep(50);
					continue;
				}
				FTS_INFO("success upgrade to version %d\n",
					 value);
				ret = 0;
				break;
			}
			break;
		}

		i++;

	} while (ret && (i < 3));
	focal_esdcheck_switch(fts_hid_data, ENABLE_ESD_CHECK);
	__pm_relax(fts_hid_data->focal_upg_wakelock);
	panel_touch_upgrade_status_set_by_ft8205(false);

err_bin:
	fts_hid_data->fw_loading = false;
	if (fw_file_buf) {
		vfree(fw_file_buf);
		fw_file_buf = NULL;
	}

	return ret;
}

static bool fts_fwupg_check_fw_valid(void)
{
	int ret = 0;

	ret = focal_wait_tp_to_valid();
	if (ret < 0) {
		FTS_ERROR("tp fw invalid");
		return false;
	}

	FTS_INFO("tp fw valid");
	return true;
}

static bool fts_fwupg_need_upgrade(u8 *fw_file_buf)
{
	bool fwvalid = false;
	u8 fw_ver_in_host = 0;
	u8 fw_ver_in_tp = 0;

	fwvalid = fts_fwupg_check_fw_valid();
	if (fwvalid) {
		fw_ver_in_host = fw_file_buf[0x10E];

		focal_read_register(FTS_REG_FW_VER, &fw_ver_in_tp);

		FTS_INFO("fw version in tp:%x, host:%x", fw_ver_in_tp, fw_ver_in_host);
		if (fw_ver_in_tp < fw_ver_in_host)
			return true;
		else
			FTS_INFO("fw version in tp is less than or equals in host, do not need upgrade");
	} else {
		FTS_INFO("fw invalid, need upgrade fw");
		return true;
	}

	return false;
}

/*****************************************************************************
 *  Name: fts_fwupg_work
 *  Brief: 1. get fw image/file
 *         2. ic init if have
 *         3. call upgrade main function(fts_fwupg_auto_upgrade)
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static void fts_fwupg_work(struct work_struct *work)
{
	int ret = 0;
	u32 fw_file_len = 0;
	u8 *fw_file_buf = NULL;
	u8 value = 0;
	bool upgrade_flag = 0;
	int i = 0;
	int j = 0;
	char fwname[FILE_NAME_LENGTH] = { 0 };

#if !FTS_AUTO_UPGRADE_EN
	FTS_INFO("FTS_AUTO_UPGRADE_EN is disabled, not upgrade when power on");
	return;
#endif

	FTS_INFO("fw upgrade work function");

	switch (lcm_vendor_id) {
	case FT8205_TG_INX:
		ret = snprintf(fwname, FILE_NAME_LENGTH, "%s.bin",
			FTS_FW_NAME_PREX_WITH_REQUEST_TG);
		break;
	case FT8205_KD_BOE:
		ret = snprintf(fwname, FILE_NAME_LENGTH, "%s.bin",
			FTS_FW_NAME_PREX_WITH_REQUEST_KD);
		break;
	default:
		FTS_ERROR("Not FT8205 IC,No corresponding firmware file");
		ret = -EINVAL;
		break;
	}

	panel_touch_upgrade_status_set_by_ft8205(true);
	__pm_stay_awake(fts_hid_data->focal_upg_wakelock);
	focal_esdcheck_switch(fts_hid_data, DISABLE_ESD_CHECK);
	/* get fw */
	ret = focal_read_file_request_firmware(fwname, &fw_file_buf);
	if (ret < 0) {
		FTS_ERROR("get fw file(default) fail");
		goto err_bin;
	}

	fw_file_len = ret;
	FTS_INFO("fw bin file len:%d", fw_file_len);
	upgrade_flag = fts_fwupg_need_upgrade(fw_file_buf);

	fts_hid_data->fw_loading = 1;

	/* run auto upgrade */
	if (upgrade_flag) {
		do {
			ret = fts_upgrade_firmware(fw_file_buf, fw_file_len, FLASH_MODE_UPGRADE_VALUE);
			if (ret) {
				FTS_ERROR("upgrade fw bin fail\n");
			} else {
				for (j = 0; j < 50; j++) {
					ret = focal_read_register(FTS_REG_FW_VER, &value);
					if (ret) {
						FTS_INFO("read A6 %d time\n", j);
						//msleep(200);
						continue;
					}
					FTS_INFO("success upgrade to version %d\n", value);
					ret = 0;
					break;
				}
				break;
			}
			i++;
		} while (ret && (i < 3));
	}

err_bin:
	fts_hid_data->fw_loading = false;
	focal_esdcheck_switch(fts_hid_data, ENABLE_ESD_CHECK);
	__pm_relax(fts_hid_data->focal_upg_wakelock);
	panel_touch_upgrade_status_set_by_ft8205(false);
	if (fw_file_buf) {
		vfree(fw_file_buf);
		fw_file_buf = NULL;
	}

}

int focal_fwupg_init(struct fts_hid_data *ts_data)
{
	FTS_INFO("fw upgrade init function");

	fts_hid_data->focal_upg_wakelock = wakeup_source_register(NULL, "focal_upg_wakelock");
	if (!fts_hid_data->focal_upg_wakelock) {
		FTS_ERROR("wakeup source request failed\n");
		return -EINVAL;
	}

	INIT_WORK(&ts_data->fwupg_work, fts_fwupg_work);
	queue_work(ts_data->ts_workqueue, &ts_data->fwupg_work);

	return 0;
}

void focal_fwupg_release(struct fts_hid_data *ts_data)
{
	FTS_FUNC_ENTER();
	if (ts_data->ts_workqueue)
		cancel_work_sync(&ts_data->fwupg_work);
	if (fts_hid_data && fts_hid_data->focal_upg_wakelock)
		wakeup_source_unregister(fts_hid_data->focal_upg_wakelock);
	FTS_FUNC_EXIT();
}
