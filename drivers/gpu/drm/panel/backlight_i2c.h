/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _backlight_SW_H_
#define _backlight_SW_H_

#define LP8556_SLAVE_ADDR	0x2c
#define MP3314_SLAVE_ADDR	0x28
#define SY7758_SLAVE_ADDR	0x2e

struct i2c_setting_table {
	unsigned char cmd;
	unsigned char data;
};

struct backlight_i2c {
	struct i2c_client *i2c;
	struct list_head list;
	struct i2c_setting_table *table;
	unsigned int table_size;
};

extern void backlight_on(void);
#endif
