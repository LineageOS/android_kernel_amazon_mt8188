// SPDX-License-Identifier: GPL-2.0
/* Goodix's GF316M/GF318M/GF3118M/GF518M/GF5118M/GF516M/GF816M/GF3208/GF5206/GF5216/GF5208
 *  fingerprint sensor linux driver for TEE
 *
 * 2010 - 2022 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __GF_SPI_TEE_H
#define __GF_SPI_TEE_H

#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdev.h>
#include <linux/input.h>
// #include "mt_spi.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#else
#include <linux/notifier.h>
#endif
#include <linux/pm_wakeup.h>

/**************************debug******************************/
#define ERR_LOG  (0)
#define INFO_LOG (1)
#define DEBUG_LOG (2)

#define gf_debug(level, fmt, args...) do { \
			if (g_debug_level >= level) {\
				pr_warn("[gf] " fmt, ##args); \
			} \
		} while (0)

#define FUNC_ENTRY()  gf_debug(DEBUG_LOG, "%s, %d, enter\n", __func__, __LINE__)
#define FUNC_EXIT()  gf_debug(DEBUG_LOG, "%s, %d, exit\n", __func__, __LINE__)

/**********************IO Magic**********************/
#define GF_IOC_MAGIC	'g'

#define GF_NAV_INPUT_UP			KEY_UP
#define GF_NAV_INPUT_DOWN		KEY_DOWN
#define GF_NAV_INPUT_LEFT		KEY_LEFT
#define GF_NAV_INPUT_RIGHT		KEY_RIGHT
#define GF_NAV_INPUT_CLICK		KEY_VOLUMEDOWN
#define GF_NAV_INPUT_DOUBLE_CLICK	KEY_VOLUMEUP
#define GF_NAV_INPUT_LONG_PRESS		KEY_SEARCH
#define GF_NAV_INPUT_HEAVY		KEY_CHAT
#define GF_KEY_INPUT_HOME		KEY_HOMEPAGE
#define GF_KEY_INPUT_MENU		KEY_MENU
#define GF_KEY_INPUT_BACK		KEY_BACK
#define GF_KEY_INPUT_POWER		KEY_POWER
#define GF_KEY_INPUT_CAMERA		KEY_CAMERA

#define GF_IRQ_UNUSED   (-1)
#define GF_IRQ_UNENABLE (0)
#define GF_IRQ_ENABLE   (1)

typedef enum gf_nav_event {
	GF_NAV_NONE = 0,
	GF_NAV_FINGER_UP,
	GF_NAV_FINGER_DOWN,
	GF_NAV_UP,
	GF_NAV_DOWN,
	GF_NAV_LEFT,
	GF_NAV_RIGHT,
	GF_NAV_CLICK,
	GF_NAV_HEAVY,
	GF_NAV_LONG_PRESS,
	GF_NAV_DOUBLE_CLICK,
} gf_nav_event_t;

typedef enum gf_key_event {
	GF_KEY_NONE = 0,
	GF_KEY_HOME,
	GF_KEY_POWER,
	GF_KEY_MENU,
	GF_KEY_BACK,
	GF_KEY_CAMERA,
} gf_key_event_t;

struct gf_key {
	enum gf_key_event key;
	uint32_t value;   /* key down = 1, key up = 0 */
};

enum gf_netlink_cmd {
	GF_NETLINK_TEST = 0,
	GF_NETLINK_IRQ = 1,
	GF_NETLINK_SCREEN_OFF,
	GF_NETLINK_SCREEN_ON,
	GF_NETLINK_HAL_CTRL_LOG = 0x1 << 5,
};

struct gf_ioc_chip_info {
	u8 vendor_id;
	u8 mode;
	u8 operation;
	u8 reserved[5];
};

/* define commands */
#define GF_IOC_INIT			_IOR(GF_IOC_MAGIC, 0, u8)
#define GF_IOC_EXIT			_IO(GF_IOC_MAGIC, 1)
#define GF_IOC_RESET			_IO(GF_IOC_MAGIC, 2)
#define GF_IOC_ENABLE_IRQ		_IO(GF_IOC_MAGIC, 3)
#define GF_IOC_DISABLE_IRQ		_IO(GF_IOC_MAGIC, 4)
#define GF_IOC_ENABLE_SPI_CLK           _IOW(GF_IOC_MAGIC, 5, uint32_t)
#define GF_IOC_DISABLE_SPI_CLK		_IO(GF_IOC_MAGIC, 6)
#define GF_IOC_ENABLE_POWER		_IO(GF_IOC_MAGIC, 7)
#define GF_IOC_DISABLE_POWER		_IO(GF_IOC_MAGIC, 8)
#define GF_IOC_INPUT_KEY_EVENT		_IOW(GF_IOC_MAGIC, 9, struct gf_key)
/* fp sensor has change to sleep mode while screen off */
#define GF_IOC_ENTER_SLEEP_MODE		_IO(GF_IOC_MAGIC, 10)
#define GF_IOC_GET_FW_INFO		_IOR(GF_IOC_MAGIC, 11, u8)
#define GF_IOC_REMOVE		_IO(GF_IOC_MAGIC, 12)
#define GF_IOC_CHIP_INFO	_IOW(GF_IOC_MAGIC, 13, struct gf_ioc_chip_info)
#define GF_IOC_NAV_EVENT	_IOW(GF_IOC_MAGIC, 14, gf_nav_event_t)

#define  GF_IOC_MAXNR    18  /* THIS MACRO IS NOT USED NOW... */

struct gf_device {
	dev_t devno;
	struct cdev cdev;
	struct device *device;
	struct platform_device *pldev;
	struct class *class;
	int device_count;
	struct list_head device_entry;
	struct input_dev *input;
	struct mutex release_lock;
	/* for netlink use */
	struct sock *nl_sk;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#else
	struct notifier_block notifier;
#endif
	/* bit24-bit32 of signal count */
	/* bit16-bit23 of event type, 1: key down; 2: key up; 3: fp data ready; 4: home key */
	/* bit0-bit15 of event type, buffer status register */
	u32 event_type;
	u8 sig_count;
	u8 system_status;
	u8  need_update;
	u32 irq;
	u32 irq_count;
	int irq_enable_status;

	struct pinctrl *pinctrl_gpios;
	struct pinctrl_state *pins_irq;
	struct pinctrl_state *pins_reset_high, *pins_reset_low, *pins_power_high, *pins_power_low;
	struct wakeup_source fp_wakesrc;
};

#endif	/* __GF_SPI_TEE_H */
