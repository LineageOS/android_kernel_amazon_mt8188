// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2025 MediaTek Inc.
 */
#if CFG_CONN_DYNAMIC_POWER_CTRL
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/string.h>
#include <linux/version.h>

#include "precomp.h"

#define WIFI_DRIVER_NAME "mtk_ce_wifi_chrdev"
#define WIFI_DEV_MAJOR 0
#define WIFI_DEV_NAME "wmtWifi"

#define ISPRINT(a)   (((a) >= 32) && ((a) <= 126))

static int32_t WIFI_devs = 1;
static int32_t WIFI_major = WIFI_DEV_MAJOR;
static dev_t wifi_devno;
static struct cdev WIFI_cdev;
static struct class *wmtwifi_class;
static struct device *wmtwifi_dev;

static struct mutex wr_mtx;
static int32_t powered;
static uint8_t  write_processing;
static uint8_t  whole_chip_rst_ongoing;



static int atoh(const char *str, uint32_t *hval)
{
	unsigned int i;
	uint32_t val = 0;

	DBGLOG(INIT, INFO, "*str : %s, len = %zu\n", str,
			strlen((const char *)str));
	for (i = 0; i < strlen((const char *)str); i++) {
		if (str[i] >= 'a' && str[i] <= 'f')
			val = (val << 4) + (str[i] - 'a' + 10);
		else if (str[i] >= 'A' && str[i] <= 'F')
			val = (val << 4) + (str[i] - 'A' + 10);
		else if (*(str + i) >= '0' && *(str + i) <= '9')
			val = (val << 4) + (*(str + i) - '0');
	}

	*hval = val;

	return 0;
}

static int WIFI_open(struct inode *inode, struct file *file)
{
	DBGLOG(INIT, INFO, "major %d minor %d (pid %d)\n",
		imajor(inode), iminor(inode), current->pid);

	return 0;
}

static int WIFI_close(struct inode *inode, struct file *file)
{
	DBGLOG(INIT, INFO, "major %d minor %d (pid %d)\n",
		imajor(inode), iminor(inode), current->pid);

	return 0;
}

static void WIFI_write_off(int32_t *retval, size_t count)
{
	uint32_t times = 0;

	write_processing = WRITE_PROCESSING_OFF;

	if (powered == 0) {
		DBGLOG(INIT, INFO, "WIFI is already power off!\n");
		*retval = count;
		return;
	}

	while (whole_chip_rst_ongoing) {
		DBGLOG(INIT, INFO, "whole chip rst process, waiting\n");
		msleep(100);
		times++;
		if (times > 200)
			break;
	}

	if (wlan_func_off_by_chrdev()) {
		DBGLOG(INIT, ERROR, "WMT turn off WIFI fail!\n");
	} else {
		DBGLOG(INIT, INFO, "WMT turn off WIFI success!\n");
		*retval = count;
	}
	powered = 0;
}

static void WIFI_write_on(int32_t *retval, size_t count)
{
	write_processing = WRITE_PROCESSING_ON;

	if (powered == 1) {
		DBGLOG(INIT, INFO, "WIFI is already power on!\n");
		*retval = count;
		return;
	}

	if (wlan_func_on_by_chrdev()) {
		DBGLOG(INIT, ERROR, "WMT turn on WIFI fail!\n");
	} else {
		powered = 1;
		*retval = count;
		DBGLOG(INIT, INFO, "WMT turn on WIFI success!\n");
	}
}

static int32_t wifi_conv_to_printable_str(uint32_t size,
	int8_t *src, int8_t *des)
{
	int32_t i = 0;
	uint32_t write_byte = 0;

	if (size == 0)
		return 0;

	for (i = 0; i < size; i++) {

		if ((i == (size - 1)) && src[i] == '\0') {
			/* null terminate */
			break;
		}

		if (ISPRINT(src[i])) {
			/* Print directly */
			write_byte += sprintf((char *)(des + write_byte),
				"%c", src[i]);
		} else {
			/* Transfer to HEX */
			write_byte += sprintf((char *)(des + write_byte),
				" %02X", (uint8_t)src[i]);
		}
	}

	return write_byte;
}

ssize_t WIFI_write(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
	int32_t retval = -EIO;
	uint32_t enable = 0;
	int8_t local[20] = { 0 };
	uint32_t copy_size = 0;
	int8_t print_str[(sizeof(local) - 1) * 3 + 1] = { 0 };
	uint32_t total_len = 0;

	mutex_lock(&wr_mtx);
	if (count <= 0) {
		DBGLOG(INIT, ERROR, "WIFI_write invalid param\n");
		goto done;
	}

	copy_size = (sizeof(local) - 1) < (uint32_t) count ?
		(sizeof(local) - 1) : (uint32_t) count;
	if (copy_from_user(local, buf, copy_size) == 0) {
		local[copy_size] = '\0';

		total_len = wifi_conv_to_printable_str(
			copy_size, local, print_str);
		print_str[total_len] = '\0';

		DBGLOG(INIT, INFO,
			"WIFI_write %s, length %zu, copy_size %u\n",
			print_str, count, copy_size);

		if (kstrtou32(local, 0, &enable)) {
			DBGLOG(INIT, ERROR, "input value error\n");
			goto done;
		}
		if (enable == 1)
			WIFI_write_on(&retval, count);
		else if (enable == 0)
			WIFI_write_off(&retval, count);
	}
done:
	write_processing = WRITE_PROCESSING_DONE;
	mutex_unlock(&wr_mtx);
	return retval;
}

const struct file_operations WIFI_fops = {
	.open = WIFI_open,
	.release = WIFI_close,
	.write = WIFI_write,
};

int wifi_chrdev_init(void)
{
	int32_t alloc_ret = 0;
	int32_t cdev_err = 0;

	mutex_init(&wr_mtx);

	/* Allocate char device */
	if (WIFI_major) {
		wifi_devno = MKDEV(WIFI_major, 0);
		alloc_ret = register_chrdev_region(wifi_devno, WIFI_devs,
				WIFI_DRIVER_NAME);
	} else {
		alloc_ret = alloc_chrdev_region(&wifi_devno, 0, WIFI_devs,
				WIFI_DRIVER_NAME);
	}
	if (alloc_ret) {
		DBGLOG(INIT, ERROR, "Fail to register device numbers\n");
		return alloc_ret;
	}

	cdev_init(&WIFI_cdev, &WIFI_fops);
	WIFI_cdev.owner = THIS_MODULE;

	cdev_err = cdev_add(&WIFI_cdev, wifi_devno, WIFI_devs);
	if (cdev_err)
		goto error;


#if (KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE)
	wmtwifi_class = class_create(WIFI_DEV_NAME);
#else
	wmtwifi_class = class_create(THIS_MODULE, WIFI_DEV_NAME);
#endif
	if (IS_ERR(wmtwifi_class))
		goto error;
	wmtwifi_dev = device_create(wmtwifi_class, NULL, wifi_devno, NULL,
			WIFI_DEV_NAME);
	if (IS_ERR(wmtwifi_dev))
		goto error;


	DBGLOG(INIT, INFO, "%s driver(major %d %d) installed.\n",
		WIFI_DRIVER_NAME,
		WIFI_major, MAJOR(wifi_devno));

	return 0;

error:

	if (wmtwifi_dev && !IS_ERR(wmtwifi_dev)) {
		device_destroy(wmtwifi_class, wifi_devno);
		wmtwifi_dev = NULL;
	}
	if (wmtwifi_class && !IS_ERR(wmtwifi_class)) {
		class_destroy(wmtwifi_class);
		wmtwifi_class = NULL;
	}

	if (cdev_err == 0)
		cdev_del(&WIFI_cdev);

	if (alloc_ret == 0)
		unregister_chrdev_region(wifi_devno, WIFI_devs);

	return -1;
}

void wifi_chrdev_uninit(void)
{
	if (wmtwifi_dev && !IS_ERR(wmtwifi_dev)) {
		device_destroy(wmtwifi_class, wifi_devno);
		wmtwifi_dev = NULL;
	}
	if (wmtwifi_class && !IS_ERR(wmtwifi_class)) {
		class_destroy(wmtwifi_class);
		wmtwifi_class = NULL;
	}

	cdev_del(&WIFI_cdev);
	unregister_chrdev_region(wifi_devno, WIFI_devs);

	DBGLOG(INIT, INFO, "%s driver removed\n", WIFI_DRIVER_NAME);
}

void update_whole_chip_rst_status(uint8_t fgIsWholeChipRst)
{
	DBGLOG(INIT, INFO, "update_whole_chip_rst_status: %d\n",
		fgIsWholeChipRst);
	whole_chip_rst_ongoing = fgIsWholeChipRst;
}

uint8_t get_whole_chip_rst_status(void)
{
	DBGLOG(INIT, INFO, "whole_chip_rst status: %d\n",
		whole_chip_rst_ongoing);
	return whole_chip_rst_ongoing;
}

int32_t get_wifi_powered_status(void)
{
	DBGLOG(INIT, INFO, "wifi power status : %d\n", powered);
	return powered;
}

int32_t get_wifi_process_status(void)
{
	DBGLOG(INIT, INFO, "wifi write status: %d\n", write_processing);
	return write_processing;
}

#endif
