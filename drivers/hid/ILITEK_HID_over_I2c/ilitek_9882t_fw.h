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

#ifndef __ILITEK_9882T_FW_H
#define __ILITEK_9882T_FW_H

/* define names and paths for the variety of tp modules */
#define BOE_INI_NAME_PATH "/dfs/input_data/touch/ilitek_mp_boe.ini"
#define BOE_FW_FILP_PATH "/sdcard/ILITEK_FW_BOE"
#define BOE_INI_REQUEST_PATH "mp_boe.ini"
#define BOE_FW_REQUEST_PATH "ilitek_hid_kd_boe_fw.bin"
static unsigned char CTPM_FW_BOE[] = {
	0xFF,
};

#define HSD_INI_NAME_PATH "/dfs/input_data/touch/ilitek_mp_hsd.ini"
#define HSD_FW_FILP_PATH "/sdcard/ILITEK_FW_HSD"
#define HSD_INI_REQUEST_PATH "mp_hsd.ini"
#define HSD_FW_REQUEST_PATH "ilitek_hid_tg_hsd_fw.bin"
static unsigned char CTPM_FW_HSD[] = {
	0xFF,
};
#endif
