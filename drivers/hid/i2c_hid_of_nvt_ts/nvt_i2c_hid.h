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
#ifndef _LINUX_NVT_TOUCH_H
#define _LINUX_NVT_TOUCH_H

#ifndef I2C_HID_H
#define I2C_HID_H
#endif

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
#include "mtk_disp_notify.h"
#endif

#include "nvt_i2c_hid_mem_map.h"
#include "nvt_i2c_hid_flash_info.h"

#if IS_ENABLED(CONFIG_STYLUS_BATTERY_ALGO)
#include "stylus_battery_algo.h"
#endif

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
#include "hid_touch_metrics.h"
extern touch_metrics_info_t touch_metrics_data;
#endif

// Do NOT turn this on in official release driver
#define NO_PEN_SELFTEST 0
#define CHECK_DATA_CHECKSUM 1

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 9, 0)
#define HAVE_SET_FS
#endif
#define NVT_DEBUG 0
#define NVT_HID_ENG_REPORT_ID 0x0B
#define GET_SET_F_PKT_LENGTH 6
#define PREFIX_PKT_LENGTH 5
#define GET_REPORT_CMD 0x02
#define SET_REPORT_CMD 0x03
#define ESD_ADDR 0x02083E

#define PEN_FLAG_INDEX 2
#define PEN_SERIAL_INDEX 38
#define PEN_INVALID_SERIAL 0
#define PEN_DISCONNECT_MAX_TIME (10*60*1000)
#define PEN_TIMESTAMP_SUPPORT 1
#define PEN_ID 4
#define PEN_MAX_DELTA 65535
#define MAX_TIMESTAMP_INTERVAL 1000000

/* reach SAMPLE_FREQ=10 in different touch modules. 240/10=24. */
#define PEN_FRAME_COUNT 24

/* stylus GID4 */
#define PEN_VENDOR_USAGE_1_L 57
#define PEN_VENDOR_USAGE_1_H 58
/* stylus battery strength */
#define PEN_BATTERY_STRENGTH 37
/* stylus fw version */
#define NVT_HID_PEN_FW_VERSION_L 48
#define NVT_HID_PEN_FW_VERSION_H 49

#define DRIVER_ESD_PROTECT 1
#define NVT_TOUCH_ESD_CHECK_PERIOD 2000

#define _u32_to_n_u8(u32, n) (((u32) >> (8 * n)) & 0xff)
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

//---bus transfer length---
#define BUS_TRANSFER_LENGTH 256

//---I2C driver info.---
#define NVT_I2C_NAME "NVT-ts"
#define I2C_FW_Address 0x01

#if NVT_DEBUG
#define NVT_LOG(fmt, args...)                                                  \
	pr_err("[%s] %s %d: " fmt, NVT_I2C_NAME, __func__, __LINE__, ##args)
#else
#define NVT_LOG(fmt, args...)                                                  \
	pr_info("[%s] %s %d: " fmt, NVT_I2C_NAME, __func__, __LINE__, ##args)
#endif
#define NVT_ERR(fmt, args...)                                                  \
	pr_err("[%s] %s %d: " fmt, NVT_I2C_NAME, __func__, __LINE__, ##args)
#define NVT_DEBUG_LOG(fmt, args...)                                            \
	pr_debug("[%s] %s %d: " fmt, NVT_I2C_NAME, __func__, __LINE__, ##args)

//---Customerized func.---
#define LCM_NT36523N_TG_INX 0
#define LCM_NT36523N_KD_HSD 5
#define BOOT_UPDATE_FIRMWARE_NAME_HID_INX "novatek_ts_fw_hid_inx.bin"
#define BOOT_UPDATE_FIRMWARE_NAME_HID_HSD "novatek_ts_fw_hid_hsd.bin"

enum {
	PACKET_NORMAL,
	PACKET_RECOVERY,
};

struct nvt_ts_data {
	struct i2c_client *client;
	uint8_t fw_ver;
	uint8_t x_num;
	uint8_t y_num;
	uint8_t max_touch_num;
	struct mutex lock;
	struct wakeup_source *nvt_upg_wakelock;
	struct work_struct nvt_fwu_work;
	struct workqueue_struct *nvt_fwu_wq;
#if DRIVER_ESD_PROTECT
	struct delayed_work nvt_esd_work;
	struct workqueue_struct *nvt_esd_wq;
#endif
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	struct notifier_block disp_notifier;
#endif
	bool touch_is_awake;
	struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_flash_map *fmap;
	const struct nvt_ts_bin_map *bmap;
	uint16_t query_config_ver;
	uint16_t nvt_pid;
	uint8_t *xbuf;
	struct mutex xbuf_lock;
	bool pen_support;
	uint8_t pen_x_num_x;
	uint8_t pen_x_num_y;
	uint8_t pen_y_num_x;
	uint8_t pen_y_num_y;
	int8_t pen_phys[32];
	uint8_t flash_mid;
	uint16_t flash_did; /* 2 bytes did read by 9Fh cmd */
	const flash_info_t *match_finfo;
	uint16_t flash_pid;
	uint64_t hid_addr;
	uint8_t touch_ready;
	uint8_t idx;
	uint32_t pen_serial;
	ktime_t pen_touch_time;
};

typedef struct gcm_transfer {
	uint8_t flash_cmd;
	uint32_t flash_addr;
	uint16_t flash_checksum;
	uint8_t flash_addr_len;
	uint8_t pem_byte_len; /* performance enhanced mode / contineous read mode byte length*/
	uint8_t dummy_byte_len;
	uint8_t *tx_buf;
	uint16_t tx_len;
	uint8_t *rx_buf;
	uint16_t rx_len;
} gcm_xfer_t;

typedef enum {
	RESET_STATE_INIT = 0xA0, // IC reset
	RESET_STATE_REK, // ReK baseline
	RESET_STATE_REK_FINISH, // baseline is ready
	RESET_STATE_NORMAL_RUN, // normal run
	RESET_STATE_MAX = 0xAF
} RST_COMPLETE_STATE;

typedef enum {
	EVENT_MAP_HOST_CMD = 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE = 0x51,
	EVENT_MAP_RESET_COMPLETE = 0x60,
	EVENT_MAP_FWINFO = 0x78,
	EVENT_MAP_PROJECTID = 0x9A,
} I2C_EVENT_MAP;

#define NVT_XBUF_LEN (1025)
//---extern structures---
extern struct nvt_ts_data *ts;
extern struct i2c_client *hid_i2c_client;
extern uint32_t set_f_pkt[GET_SET_F_PKT_LENGTH];
extern uint32_t get_f_pkt[GET_SET_F_PKT_LENGTH];
extern uint8_t prefix_pkt[PREFIX_PKT_LENGTH];

//---extern functions---
extern int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address, uint8_t *buf,
		uint16_t len);
extern int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address, uint8_t *buf,
		uint16_t len);
extern void nvt_bootloader_reset(void);
extern void nvt_read_fw_history(uint32_t fw_history_addr);
extern int32_t nvt_check_fw_reset_state(RST_COMPLETE_STATE check_reset_state);
extern int32_t nvt_get_fw_info(void);
extern int32_t nvt_clear_fw_status(void);
extern int32_t nvt_check_fw_status(void);
extern int32_t nvt_set_page(uint16_t i2c_addr, uint32_t addr);
extern int32_t nvt_write_addr(uint32_t addr, uint8_t data);
extern int32_t nvt_read_reg(nvt_ts_reg_t reg, uint8_t *val);
extern int32_t nvt_write_reg(nvt_ts_reg_t reg, uint8_t val);
extern void nvt_stop_crc_reboot(void);
extern int32_t nvt_set_custom_cmd(uint8_t cmd);
extern int32_t nvt_get_flash_info(void);
extern int nvt_hid_i2c_get_feature(struct i2c_client *client, uint8_t *hid_buf, uint32_t hid_len,
		uint8_t *buf_recv, uint16_t read_len);
extern int nvt_hid_i2c_set_feature(struct i2c_client *client, uint8_t *hid_buf, uint32_t hid_len);
extern int32_t Boot_Update_Firmware(void);
extern int8_t nvt_ts_check_chip_ver_trim_and_idle(void);
extern void set_prefix_packet(void);
extern int32_t nvt_extra_proc_init(void);
extern void nvt_extra_proc_deinit(void);
extern int32_t nvt_mp_proc_init(void);
extern void nvt_mp_proc_deinit(void);
extern void switch_pkt(uint8_t pkt_type);
extern int32_t check_crc_done(void);
void nvt_sw_reset_and_idle(void);
extern void nt36523b_share_tp_resetpin_control(int value);
extern int nvt_fwupg_init(struct nvt_ts_data  *ts_data);
extern void nvt_fwupg_deinit(struct nvt_ts_data  *ts_data);
extern void nvt_fwupg_work(struct work_struct *work);
#if DRIVER_ESD_PROTECT
extern int nvt_esd_protect_init(struct nvt_ts_data  *ts_data);
extern void nvt_esd_protect_deinit(struct nvt_ts_data  *ts_data);
#endif
extern void panel_fw_upgrade_mode_set_by_novatek(bool upg_mode);
#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif
void nvt_get_fw_cus_settings(void);
#endif /* _LINUX_NVT_TOUCH_H */
