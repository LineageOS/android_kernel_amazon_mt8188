/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2022, Focaltech Ltd. All rights reserved.
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
#ifndef _FOCALTECH_CORE_H
#define _FOCALTECH_CORE_H
#include <linux/firmware.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input/mt.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/hid.h>
#include <linux/version.h>
#include <linux/kernel.h>

#define FTS_DEBUG_EN 0
#define WRITE_REQUEST_SIZE 72
#define READ_REQUEST_SIZE 6
#define READ_RESPONSE_SIZE 66
#define CMD_ACK 0xF0
#define FTS_HID_BUFF_SIZE 64

#define CMD_ENTER_UPGRADE_MODE 0x40
#define CMD_CHECK_CURRENT_STATE 0x41
#define CMD_READY_FOR_UPGRADE 0x42
#define CMD_SEND_DATA 0x43
#define CMD_UPGRADE_FLUSH_DATA 0x44
#define CMD_EXIT_UPRADE_MODE 0x45
#define CMD_USB_READ_UPGRADE_ID 0x46
#define CMD_USB_ERASE_FLASH 0x47
#define CMD_USB_UPGRADE_MODE 0x4B

#define CMD_READ_REGISTER 0x50
#define CMD_WRITE_REGISTER 0x51
#define FTS_REG_CHIP_ID_H 0xA3
#define FTS_REG_CHIP_ID_L 0x9F

#define FTS_BOOT_ID_H 0x82
#define FTS_BOOT_ID_L 0xB5

#define FTS_CHIP_ID_H 0x82
#define FTS_CHIP_ID_L 0x05

#define LCM_VENDOR_ID_UNKNOWN 0xFF
#define FT8205_TG_INX 2
#define FT8205_KD_BOE 4

#define FILE_NAME_LENGTH 128
#define FTS_FW_BIN_FILEPATH "/sdcard/"
#define FTS_UPGRADE_ONE_PACKET_LEN 56

#define FTS_SYSFS_ECHO_ON(buf) (buf[0] == '1')
#define FTS_SYSFS_ECHO_OFF(buf) (buf[0] == '0')
#define FTS_REG_GESTURE_EN 0xD0
#define FTS_REG_POWER_MODE 0xA5
#define FTS_REG_POWER_MODE_SLEEP 0x03

#define FTS_TOUCH_PEN 0x0B
#define FTS_TOUCH_FINGER 0x01
#define FTS_GESTURE_MODE_TOUCH_FINGER 0x60
#define FTS_ESD_CHECK_EVENT 0x62
#define FTS_TP_RECOVERY_EVENT 0X63

#define GESTURE_SINGLECLICK 0x24
#define FTS_REG_FW_VER 0xA6
#define FTS_PRIVATE_PACKAGE_FLAG 0x06
#define FLASH_MODE_UPGRADE_VALUE 0x0B
#define FTS_REG_FACE_DEC_MODE_EN 0xB0

#define TP_FW_VERSION "tp_fw_version"
#define SUPPORT_FINGER_REPORT_CHECK_RELEASE 1
#define TP_ECC_INDEX 60

#define DISABLE_ESD_CHECK 0
#define ENABLE_ESD_CHECK 1

#define ENABLE_GESTURE_MODE 1

#if FTS_DEBUG_EN
#define FTS_DEBUG(fmt, args...)                                                \
	do {                                                                   \
		printk("[FTS_TS]%s:" fmt, __func__, ##args);                   \
	} while (0)

#define FTS_FUNC_ENTER()                                                       \
	do {                                                                   \
		printk("[FTS_TS]%s: Enter\n", __func__);                       \
	} while (0)

#define FTS_FUNC_EXIT()                                                        \
	do {                                                                   \
		printk("[FTS_TS]%s: Exit(%d)\n", __func__, __LINE__);          \
	} while (0)
#else
#define FTS_DEBUG(fmt, args...)
#define FTS_FUNC_ENTER()
#define FTS_FUNC_EXIT()
#endif

#define FTS_INFO(fmt, args...)                                                 \
	do {                                                                   \
		printk(KERN_INFO "[FTS_TS/I]%s:" fmt "\n", __func__, ##args);  \
	} while (0)

#define FTS_ERROR(fmt, args...)                                                \
	do {                                                                   \
		printk(KERN_ERR "[FTS_TS/E]%s:" fmt "\n", __func__, ##args);   \
	} while (0)

struct fts_hid_data {
	struct i2c_client *client;
	struct device *dev;
	struct mutex bus_lock;
	struct input_dev *ts_input_dev;
	struct input_dev *pen_input_dev;
	struct workqueue_struct *ts_workqueue;
	struct work_struct fwupg_work;
	struct delayed_work esdcheck_work;
	struct delayed_work  recovery_work;
	unsigned long intr_jiffies;
	u32 reset_gpio;
	int irq;
	spinlock_t irq_lock;
	struct wakeup_source *focal_upg_wakelock;
	bool suspended;
	bool irq_disabled;
	int log_level;
	bool gesture_mode;
	bool fw_loading;
	bool esd_support;
	bool lcd_disp_on;
	u8 *bus_tx_buf;
	u8 *bus_rx_buf;
	struct hid_device *hdev;
	//HID irq wake
	bool irq_wake_enabled;
#if IS_ENABLED(CONFIG_DRM_MEDIATEK)
	struct notifier_block disp_notifier;
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};

#define FTS_PEN_FIELD(f)	(((f)->logical == HID_DG_STYLUS) || \
				 ((f)->physical == HID_DG_STYLUS) || \
				 ((f)->physical == HID_DG_PEN) || \
				 ((f)->application == HID_DG_PEN) || \
				 ((f)->application == HID_DG_DIGITIZER))

#define FTS_FINGER_FIELD(f)	(((f)->logical == HID_DG_FINGER) || \
				 ((f)->physical == HID_DG_FINGER) || \
				 ((f)->application == HID_DG_TOUCHSCREEN) || \
				 ((f)->application == HID_DG_TOUCHPAD))

int focal_fwupg_init(struct fts_hid_data *ts_data);
void focal_hid2std(void);

int focal_hw_reset(int hdelayms);

int focal_hid_upgrade_bin(char *fw_name);
int focal_wait_tp_to_valid(void);
int focal_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int focal_write(u8 *writebuf, u32 writelen);
int focal_read_reg(u8 addr, u8 *value);
int focal_write_reg(u8 addr, u8 value);

void focal_irq_disable(void);
void focal_irq_enable(void);

int focal_bus_init(struct fts_hid_data *ts_data);
int fts_test_init(struct fts_hid_data *ts_data);
int fts_test_exit(struct fts_hid_data *ts_data);

int focal_write_register(u8 addr, u8 value);
int focal_read_register(u8 addr, u8 *value);
int fts_direct_read_data(u8 *buf, size_t size);
int fts_direct_write_data(u8 *buf, size_t size);
int fts_raw_event(struct hid_device *hdev, struct hid_report *report,
		  u8 *raw_data, int size);
int fts_hid_probe_entry(struct hid_device *hdev,
			const struct hid_device_id *id);
int fts_ts_remove_entry(void);
int fts_remove_sysfs(struct hid_device *hdev);
__u8 *fts_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		       unsigned int *rsize);
void focal_fwupg_release(struct fts_hid_data *ts_data);

void focal_esdcheck_suspend(struct fts_hid_data *ts_data);
void focal_esdcheck_resume(struct fts_hid_data *ts_data);

void focal_esdcheck_init(struct fts_hid_data *ts_data);
int focal_esdcheck_exit(struct fts_hid_data *ts_data);
void focal_esdcheck_switch(struct fts_hid_data *ts_data, bool enable);
int fts_update_esd_value(u8 tp_esd_value, u8 lcd_esd_value);
int fts_recovery_state(struct fts_hid_data *ts_data);
void fts_esd_hang_cnt(struct fts_hid_data *ts_data);
int focal_bus_exit(struct fts_hid_data *ts_data);

extern struct fts_hid_data *fts_hid_data;

#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif

extern void panel_gesture_mode_set_by_ft8205(bool gst_mode);
extern void ft8205_share_tp_resetpin_control(int value);
extern void panel_touch_upgrade_status_set_by_ft8205(bool upg_mode);
extern unsigned char lcm_vendor_id;

void mt_release_contacts(struct hid_device *hid);
#endif /* __FOCALTECH_CORE_H */
