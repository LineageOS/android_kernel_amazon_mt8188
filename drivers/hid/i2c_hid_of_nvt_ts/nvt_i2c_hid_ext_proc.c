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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "nvt_i2c_hid.h"

#define NVT_FW_VERSION "tp_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"
#define NVT_FLASH_INFO "nvt_flash_info"
#define NVT_FW_HISTORY "nvt_fw_history"

#define NORMAL_MODE 0x00
#define TEST_MODE_2 0x22
#define HANDSHAKING_HOST_READY 0xBB

#define XDATA_SECTOR_SIZE 256

static uint8_t xdata_tmp[5000] = { 0 };
static int32_t xdata[2500] = { 0 };

static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;
static struct proc_dir_entry *NVT_proc_flash_info_entry;
static struct proc_dir_entry *NVT_proc_fw_history_entry;

/*******************************************************
Description:
	Novatek touchscreen change mode function.

return:
	n.a.
*******************************************************/
void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[8] = { 0 };

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address,
		     ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HOST_CMD);

	//---set mode---
	buf[0] = EVENT_MAP_HOST_CMD;
	buf[1] = mode;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

	if (mode == NORMAL_MODE) {
		usleep_range(20000, 20000);
		nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);
		buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[1] = HANDSHAKING_HOST_READY;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
		usleep_range(20000, 20000);
	}
}

/*******************************************************
Description:
	Novatek touchscreen get firmware pipe function.

return:
	Executive outcomes. 0---pipe 0. 1---pipe 1.
*******************************************************/
uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[8] = { 0 };

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE);

	//---read fw status---
	buf[0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
	buf[1] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

	return (buf[1] & 0x01);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata(uint32_t xdata_addr)
{
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 1] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = ts->x_num * ts->y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			//---read data---
			nvt_set_page(I2C_FW_Address, head_addr + XDATA_SECTOR_SIZE * i + transfer_len * j);
			buf[0] = transfer_len * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
			}
		}
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			//---read data---
			nvt_set_page(I2C_FW_Address, xdata_addr + data_len - residual_len + transfer_len * j);
			buf[0] = transfer_len * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++)
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
		}
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++)
		xdata[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
    Novatek touchscreen get meta data function.

return:
    n.a.
*******************************************************/
void nvt_get_mdata(int32_t *buf, uint8_t *m_x_num, uint8_t *m_y_num)
{
	*m_x_num = ts->x_num;
	*m_y_num = ts->y_num;
	memcpy(buf, xdata, ((ts->x_num * ts->y_num) * sizeof(int32_t)));
}

/*******************************************************
Description:
	Novatek touchscreen read and get number of meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_get_num_mdata(uint32_t xdata_addr, int32_t *buffer, uint32_t num)
{
	int32_t transfer_len = 0;
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[BUS_TRANSFER_LENGTH + 1] = { 0 };
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	if (BUS_TRANSFER_LENGTH <= XDATA_SECTOR_SIZE)
		transfer_len = BUS_TRANSFER_LENGTH;
	else
		transfer_len = XDATA_SECTOR_SIZE;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		//---read xdata by transfer_len
		for (j = 0; j < (XDATA_SECTOR_SIZE / transfer_len); j++) {
			nvt_set_page(I2C_FW_Address, head_addr + XDATA_SECTOR_SIZE * i + transfer_len * j);
			//---read data---
			buf[0] = transfer_len * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++)
				xdata_tmp[XDATA_SECTOR_SIZE * i + transfer_len * j + k] = buf[k + 1];
		}
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		//---read xdata by transfer_len
		for (j = 0; j < (residual_len / transfer_len + 1); j++) {
			nvt_set_page(I2C_FW_Address, xdata_addr + data_len - residual_len);
			//---read data---
			buf[0] = transfer_len * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, transfer_len + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < transfer_len; k++)
				xdata_tmp[(dummy_len + data_len - residual_len) + transfer_len * j + k] = buf[k + 1];
		}
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++)
		buffer[i] = (int16_t)(xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR);
}

/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d\n", ts->fw_ver, ts->x_num, ts->y_num);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < ts->y_num; i++) {
		for (j = 0; j < ts->x_num; j++)
			seq_printf(m, "%5d, ", xdata[i * ts->x_num + j]);
		seq_puts(m, "\n");
	}

	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_fw_version_seq_ops = {
	.start = c_start,
	.next = c_next,
	.stop = c_stop,
	.show = c_fw_version_show
};

const struct seq_operations nvt_seq_ops = {
	.start = c_start,
	.next = c_next,
	.stop = c_stop,
	.show = c_show
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("enter\n");

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("leave\n");

	return seq_open(file, &nvt_fw_version_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_fw_version_fops = {
	.proc_open = nvt_fw_version_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_fw_version_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_baseline open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_baseline_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("enter\n");

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_read_mdata(ts->mmap->BASELINE_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("leave\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_baseline_fops = {
	.proc_open = nvt_baseline_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_baseline_fops = {
	.owner = THIS_MODULE,
	.open = nvt_baseline_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_raw open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_raw_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("enter\n");

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->RAW_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("leave\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_raw_fops = {
	.proc_open = nvt_raw_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_raw_fops = {
	.owner = THIS_MODULE,
	.open = nvt_raw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_diff_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("enter\n");

	if (nvt_clear_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(ts->mmap->DIFF_PIPE0_ADDR);
	else
		nvt_read_mdata(ts->mmap->DIFF_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	mutex_unlock(&ts->lock);

	NVT_LOG("leave\n");

	return seq_open(file, &nvt_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_diff_fops = {
	.proc_open = nvt_diff_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

static int nvt_flash_info_show(struct seq_file *m, void *v)
{
	const flash_info_t *finfo = ts->match_finfo;

	seq_printf(m, "flash pid: 0x%04X\n", ts->flash_pid);
	seq_printf(m, "flash mid: 0x%02X, did: 0x%04X\n", ts->flash_mid, ts->flash_did);
	seq_printf(m, "matched flash info item:\n");
	seq_printf(m, "\tmid=0x%02X, did=0x%04X\n", finfo->mid, finfo->did);
	seq_printf(m, "\tqeb_pos=%d, qeb_order=%d\n", finfo->qeb_info.qeb_pos, finfo->qeb_info.qeb_order);
	seq_printf(m, "\trd_method=%d, prog_method=%d\n", finfo->rd_method, finfo->prog_method);
	seq_printf(m, "\twrsr_method=%d, rdsr1_cmd=0x%02X\n", finfo->wrsr_method, finfo->rdsr1_cmd);

	return 0;
}

static int32_t nvt_flash_info_open(struct inode *inode, struct file *file)
{
	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	NVT_LOG("enter\n");

	if (nvt_get_flash_info()) {
		mutex_unlock(&ts->lock);
		return -EAGAIN;
	}

	mutex_unlock(&ts->lock);

	NVT_LOG("leave\n");

	return single_open(file, nvt_flash_info_show, NULL);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_flash_info_fops = {
	.proc_open = nvt_flash_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_flash_info_fops = {
	.owner = THIS_MODULE,
	.open = nvt_flash_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int nvt_fw_history_show(struct seq_file *m, void *v)
{
	uint8_t i = 0;
	uint8_t buf[65] = {0};
	char str[128];

	NVT_LOG("enter\n");
	if (!ts->mmap->MMAP_HISTORY_EVENT0 || !ts->mmap->MMAP_HISTORY_EVENT1)
		return -EINVAL;

	if (mutex_lock_interruptible(&ts->lock))
		return -ERESTARTSYS;

	nvt_set_page(I2C_FW_Address, ts->mmap->MMAP_HISTORY_EVENT0);

	buf[0] = (uint8_t)(ts->mmap->MMAP_HISTORY_EVENT0 & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 64 + 1); //read 64bytes history

	//print all data
	NVT_LOG("fw history 0x%X: \n", ts->mmap->MMAP_HISTORY_EVENT0);
	seq_printf(m, "%X :\n", ts->mmap->MMAP_HISTORY_EVENT0);
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str),
			"%02X %02X %02X %02X %02X %02X %02X %02X  "
			"%02X %02X %02X %02X %02X %02X %02X %02X\n",
			buf[1 + i * 16], buf[2 + i * 16], buf[3 + i * 16], buf[4 + i * 16],
			buf[5 + i * 16], buf[6 + i * 16], buf[7 + i * 16], buf[8 + i * 16],
			buf[9 + i * 16], buf[10 + i * 16], buf[11 + i * 16], buf[12 + i * 16],
			buf[13 + i * 16], buf[14 + i * 16], buf[15 + i * 16], buf[16 + i * 16]);
		NVT_LOG("%s", str);
		seq_printf(m, "%s", str);
	}

	seq_printf(m, "%X :\n", ts->mmap->MMAP_HISTORY_EVENT1);
	nvt_set_page(I2C_FW_Address, ts->mmap->MMAP_HISTORY_EVENT1);

	buf[0] = (uint8_t)(ts->mmap->MMAP_HISTORY_EVENT1 & 0xFF);
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 64 + 1); //read 64bytes history

	//print all data
	NVT_LOG("fw history 0x%X: \n", ts->mmap->MMAP_HISTORY_EVENT1);
	for (i = 0; i < 4; i++) {
		snprintf(str, sizeof(str),
			"%02X %02X %02X %02X %02X %02X %02X %02X  "
			"%02X %02X %02X %02X %02X %02X %02X %02X\n",
			buf[1 + i * 16], buf[2 + i * 16], buf[3 + i * 16], buf[4 + i * 16],
			buf[5 + i * 16], buf[6 + i * 16], buf[7 + i * 16], buf[8 + i * 16],
			buf[9 + i * 16], buf[10 + i * 16], buf[11 + i * 16], buf[12 + i * 16],
			buf[13 + i * 16], buf[14 + i * 16], buf[15 + i * 16], buf[16 + i * 16]);
		NVT_LOG("%s", str);
		seq_printf(m, "%s", str);
	}
	nvt_set_page(I2C_FW_Address, ts->mmap->EVENT_BUF_ADDR);
	mutex_unlock(&ts->lock);
	NVT_LOG("leave\n");

	return 0;
}

static int32_t nvt_fw_history_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvt_fw_history_show, NULL);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops nvt_fw_history_fops = {
	.proc_open = nvt_fw_history_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations nvt_fw_history_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_history_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(void)
{
	if (!NVT_proc_fw_version_entry) {
		NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL, &nvt_fw_version_fops);
		if (NVT_proc_fw_version_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_FW_VERSION);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_VERSION);
		}
	} else {
		NVT_LOG("/proc/%s is already created\n", NVT_FW_VERSION);
	}

	if (!NVT_proc_baseline_entry) {
		NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL, &nvt_baseline_fops);
		if (NVT_proc_baseline_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_BASELINE);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_BASELINE);
		}
	} else {
		NVT_LOG("/proc/%s is already created\n", NVT_BASELINE);
	}

	if (!NVT_proc_raw_entry) {
		NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL, &nvt_raw_fops);
		if (NVT_proc_raw_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_RAW);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_RAW);
		}
	} else {
		NVT_LOG("/proc/%s is already created\n", NVT_RAW);
	}

	if (!NVT_proc_diff_entry) {
		NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL, &nvt_diff_fops);
		if (NVT_proc_diff_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_DIFF);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_DIFF);
		}
	} else {
		NVT_LOG("/proc/%s is already created\n", NVT_DIFF);
	}

	if (!NVT_proc_flash_info_entry) {
		NVT_proc_flash_info_entry = proc_create(NVT_FLASH_INFO, 0444, NULL, &nvt_flash_info_fops);
		if (NVT_proc_flash_info_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_FLASH_INFO);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_FLASH_INFO);
		}
	} else {
		NVT_LOG("/proc/%s is already created\n", NVT_FLASH_INFO);
	}

	if (!NVT_proc_fw_history_entry) {
		NVT_proc_fw_history_entry = proc_create(NVT_FW_HISTORY, 0444, NULL, &nvt_fw_history_fops);
		if (NVT_proc_fw_history_entry == NULL) {
			NVT_ERR("create proc/%s Failed!\n", NVT_FW_HISTORY);
			return -ENOMEM;
		} else {
			NVT_LOG("create proc/%s Succeeded!\n", NVT_FW_HISTORY);
		}
	} else {
		NVT_LOG("/proc/%s is already created\n", NVT_FW_HISTORY);
	}
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	deinitial function.

return:
	n.a.
*******************************************************/
void nvt_extra_proc_deinit(void)
{
	if (NVT_proc_fw_version_entry != NULL) {
		remove_proc_entry(NVT_FW_VERSION, NULL);
		NVT_proc_fw_version_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_VERSION);
	}

	if (NVT_proc_baseline_entry != NULL) {
		remove_proc_entry(NVT_BASELINE, NULL);
		NVT_proc_baseline_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_BASELINE);
	}

	if (NVT_proc_raw_entry != NULL) {
		remove_proc_entry(NVT_RAW, NULL);
		NVT_proc_raw_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_RAW);
	}

	if (NVT_proc_diff_entry != NULL) {
		remove_proc_entry(NVT_DIFF, NULL);
		NVT_proc_diff_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_DIFF);
	}

	if (NVT_proc_flash_info_entry != NULL) {
		remove_proc_entry(NVT_FLASH_INFO, NULL);
		NVT_proc_flash_info_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FLASH_INFO);
	}

	if (NVT_proc_fw_history_entry != NULL) {
		remove_proc_entry(NVT_FW_HISTORY, NULL);
		NVT_proc_fw_history_entry = NULL;
		NVT_LOG("Removed /proc/%s\n", NVT_FW_HISTORY);
	}
}
