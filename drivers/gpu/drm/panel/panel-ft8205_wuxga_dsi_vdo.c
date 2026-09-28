// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/i2c.h>
#include "backlight_i2c.h"
#include "bias_i2c.h"

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

#include <panel_common.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
static atomic_t gesture_status = ATOMIC_INIT(0);
static atomic_t touch_upgrade_status = ATOMIC_INIT(0);
static unsigned char vendor_id = LCM_VENDOR_ID_UNKNOWN;
static void get_lcm_id(void);
static bool panel_check_gesture_mode(void);
#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif

static DEFINE_MUTEX(ft8205_share_power_mutex);
/* trans_table[old_state][new_state]; */
static int trans_table[][5] = {
/*            LCM_OFF         LCM_ON         LCM_ON1        LCM_ON2               LCM_DSB   */
/* LCM_OFF */{TRS_KEEP_CUR,   TRS_OFF_TO_ON, TRS_ON_PHASE1, TRS_NOT_PSB,    TRS_NOT_PSB},
/* LCM_ON  */{TRS_OFF,        TRS_KEEP_CUR,  TRS_NOT_PSB,   TRS_KEEP_CUR,   TRS_DSB},
/* LCM_ON1 */{TRS_NOT_PSB,    TRS_NOT_PSB,   TRS_NOT_PSB,   TRS_ON_PHASE2,  TRS_NOT_PSB},
/* LCM_ON2 */{TRS_OFF,        TRS_KEEP_CUR,  TRS_NOT_PSB,   TRS_KEEP_CUR,   TRS_DSB},
/* LCM_DSB */{TRS_DSB_TO_OFF, TRS_DSB_TO_ON, TRS_KEEP_CUR,  TRS_DSB_TO_ON,  TRS_KEEP_CUR}
};

/*
 * different states:position in power on/off and suspend/resume sequence
 * LCM_OFF: fully powered off, disable all power supplies of the panel, include vrf18/avdd/avee
 * LCM_ON:  fully powered on, the state of the first boot to the kernel
 * LCM_ON2: execute the power on timing of LCM reset, and send init code
 * LCM_ON1: enable vrf18, pull up TP reset, enable avdd and avee
 * LCM_DSB: send display-off/sleep-in code, pull down LCM/TP reset pin(no gesture mode)
 *==================================================================================================
 * first power on    power off      normal suspend   normal resume  gesture suspend   gesture resume
 *
 *  -LCM_OFF-    -LCM_ON/LCM_ON2-  -LCM_ON/LCM_ON2-    -LCM_OFF-    -LCM_ON/LCM_ON2-     -LCM_DSB-
 *     ||               ||               ||               ||               ||                ||
 *     \/               \/               \/               \/               \/                \/
 *  -LCM_ON-         -LCM_DSB-        -LCM_DSB-        -LCM_ON1-        -LCM_DSB-         -LCM_ON2-
 *                      ||               ||               ||
 *                      \/               \/               \/
 *                   -LCM_OFF-        -LCM_OFF-        -LCM_ON2-
 *==================================================================================================
 */
static char *lcm_state_str[] = {
	"LCM_OFF",
	"LCM_ON",
	"LCM_ON1",
	"LCM_ON2",
	"LCM_DSB",
};

static int old_state = LCM_OFF;
static atomic_t g_power_request = ATOMIC_INIT(0);
static struct panel_info *share_info = NULL;

static atomic_t g_power_rebooting = ATOMIC_INIT(0);

static void lcm_power_manager_lock(struct panel_info *panel, int pwr_state);
static void lcm_power_manager(struct panel_info *panel, int new_state);

static int ft8205_reboot(struct notifier_block *notifier, unsigned long val, void *v)
{
	pr_notice("[KERNEL/LCM]: panel ft8205 rebooting\n");
	if (atomic_read(&g_power_rebooting) == 0)
		atomic_inc(&g_power_rebooting);
	return NOTIFY_OK;
}

static struct notifier_block ft8205_reboot_notifier = {
	 .notifier_call = ft8205_reboot,
};

static void get_lcm_id(void)
{
	unsigned int lcm_id = 0xFF;
#if IS_ENABLED(CONFIG_IDME)
	lcm_id = idme_get_lcmid_info();
	if (lcm_id == 0xFF)
		pr_err("get lcm_id failed.\n");
#else
	pr_notice("%s, idme is not ready.\n", __func__);
#endif
	vendor_id = (unsigned char)lcm_id;
	pr_notice("[ft8205] %s, vendor id = 0x%x\n", __func__, vendor_id);
}

static const struct panel_init_cmd kd_boe_init_cmd[] = {
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFA,0x5A),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFF,0x82,0x05,0x01),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xFF,0x82,0x05),
	_INIT_DCS_CMD(0x00,0x93),
	_INIT_DCS_CMD(0xC5,0x6B),
	_INIT_DCS_CMD(0x00,0x97),
	_INIT_DCS_CMD(0xC5,0x6B),
	_INIT_DCS_CMD(0x00,0x9E),
	_INIT_DCS_CMD(0xC5,0x0A),
	_INIT_DCS_CMD(0x00,0x9A),
	_INIT_DCS_CMD(0xC5,0xD7),
	_INIT_DCS_CMD(0x00,0x9C),
	_INIT_DCS_CMD(0xC5,0xD7),
	_INIT_DCS_CMD(0x00,0xB6),
	_INIT_DCS_CMD(0xC5,0x57,0x57),
	_INIT_DCS_CMD(0x00,0xB8),
	_INIT_DCS_CMD(0xC5,0x4D,0x4D),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xA5,0x04),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xD8,0xAA,0xAA),
	_INIT_DCS_CMD(0x00,0x82),
	_INIT_DCS_CMD(0xC5,0x95),
	_INIT_DCS_CMD(0x00,0x83),
	_INIT_DCS_CMD(0xC5,0x07),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xE1,0x00,0x08,0x1B,0x2D,0x37,0x42,0x54,0x62,0x64,0x71,0x73,0x87,0x7C,0x6A,0x6B,0x60),
	_INIT_DCS_CMD(0x00,0x10),
	_INIT_DCS_CMD(0xE1,0x58,0x4C,0x3D,0x33,0x2A,0x1B,0x0B,0x07),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xE2,0x00,0x08,0x1B,0x2D,0x37,0x42,0x54,0x62,0x64,0x71,0x73,0x87,0x7C,0x6A,0x6B,0x60),
	_INIT_DCS_CMD(0x00,0x10),
	_INIT_DCS_CMD(0xE2,0x58,0x4C,0x3D,0x33,0x2A,0x1B,0x0B,0x07),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xA4,0x8D),
	_INIT_DCS_CMD(0x00,0xA1),
	_INIT_DCS_CMD(0xB3,0x04,0xB0),
	_INIT_DCS_CMD(0x00,0xA3),
	_INIT_DCS_CMD(0xB3,0x07,0xD0),
	_INIT_DCS_CMD(0x00,0xA5),
	_INIT_DCS_CMD(0xB3,0x80,0x13),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCB,0x33,0x33,0x3C,0x33,0x30,0x33,0x3C),
	_INIT_DCS_CMD(0x00,0x87),
	_INIT_DCS_CMD(0xCB,0x3C),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xCB,0x33,0x3C,0x3C,0x3C,0x33,0x30,0x33,0x33),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCB,0x30,0x33,0x33,0x33,0x30,0x30,0x33),
	_INIT_DCS_CMD(0x00,0x97),
	_INIT_DCS_CMD(0xCB,0x33),
	_INIT_DCS_CMD(0x00,0x98),
	_INIT_DCS_CMD(0xCB,0xC4,0xC4,0xF4,0x04,0x04,0x04,0xF4,0xF4),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xCB,0x04,0xF0,0xF4,0xF7,0x04,0x04,0x04,0x04),
	_INIT_DCS_CMD(0x00,0xA8),
	_INIT_DCS_CMD(0xCB,0x08,0x04,0x04,0x04,0x04,0x04),
	_INIT_DCS_CMD(0x00,0xB0),
	_INIT_DCS_CMD(0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xB7),
	_INIT_DCS_CMD(0xCB,0x00),
	_INIT_DCS_CMD(0x00,0xB8),
	_INIT_DCS_CMD(0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xC0),
	_INIT_DCS_CMD(0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xC7),
	_INIT_DCS_CMD(0xCB,0x00),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCC,0x17,0x17,0x2D,0x2D,0x2C,0x2C,0x05,0x05),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xCC,0x12,0x12,0x10,0x10,0x0E,0x0E,0x0C,0x0C),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCC,0x04,0x04,0x2F,0x2F,0x2F,0x2F),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCD,0x17,0x17,0x2D,0x2D,0x2C,0x2C,0x05,0x05),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xCD,0x11,0x11,0x0F,0x0F,0x0D,0x0D,0x0B,0x0B),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCD,0x03,0x03,0x2F,0x2F,0x2F,0x2F),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xC2,0x8B,0x23,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xA8),
	_INIT_DCS_CMD(0xC2,0x04,0x03,0x00,0x00),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xC2,0x87,0x05,0x00,0x00),
	_INIT_DCS_CMD(0x00,0x84),
	_INIT_DCS_CMD(0xC2,0x86,0x05,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xC0),
	_INIT_DCS_CMD(0xC2,0x83,0x04,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xC7),
	_INIT_DCS_CMD(0xC2,0x82,0x05,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xD0),
	_INIT_DCS_CMD(0xC2,0x81,0x06,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xD7),
	_INIT_DCS_CMD(0xC2,0x80,0x07,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xE0),
	_INIT_DCS_CMD(0xC2,0x01,0x00,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xE7),
	_INIT_DCS_CMD(0xC2,0x02,0x00,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xF0),
	_INIT_DCS_CMD(0xC2,0x03,0x00,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xF7),
	_INIT_DCS_CMD(0xC2,0x04,0x00,0x81,0x03,0x7B,0x9E,0x07),
	_INIT_DCS_CMD(0x00,0xF0),
	_INIT_DCS_CMD(0xCC,0x96,0x94,0x94,0xD0,0x65),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xC0,0x00,0x8C,0x00,0xEE,0x00,0x10),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xC0,0x00,0x8C,0x00,0xEE,0x00,0x10),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xC0,0x00,0xE2,0x00,0xE2,0x00,0x1C),
	_INIT_DCS_CMD(0x00,0xB0),
	_INIT_DCS_CMD(0xC0,0x00,0xA7,0x00,0xEE,0x10),
	_INIT_DCS_CMD(0x00,0xA3),
	_INIT_DCS_CMD(0xC1,0x00,0x24,0x00,0x24,0x00,0x04),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xCE,0x01,0x81,0x08,0x21,0x00,0x45,0x00,0xE4,0x00,0x2A,0x00,0x3A),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xCE,0x00,0x88,0x11,0xB6,0x00,0x88,0x80,0x08,0x21,0x00,0x03,0x00,0x24,0x16),
	_INIT_DCS_CMD(0x00,0xA0),
	_INIT_DCS_CMD(0xCE,0x10,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xB0),
	_INIT_DCS_CMD(0xCE,0x32,0x00,0x00),
	_INIT_DCS_CMD(0x00,0xCC),
	_INIT_DCS_CMD(0xCE,0x07,0xF4),
	_INIT_DCS_CMD(0x00,0xD0),
	_INIT_DCS_CMD(0xCE,0x01,0x00,0x0D,0x01,0x01,0x00,0xE6,0x01),
	_INIT_DCS_CMD(0x00,0xE1),
	_INIT_DCS_CMD(0xCE,0x07,0x02,0x92,0x02,0x37,0x00,0x8F),
	_INIT_DCS_CMD(0x00,0xF0),
	_INIT_DCS_CMD(0xCE,0x00,0x19,0x0C,0x01,0x5D,0x02,0x04,0x00,0xFE,0x2D),
	_INIT_DCS_CMD(0x00,0xB0),
	_INIT_DCS_CMD(0xCF,0x07,0x07,0xD8,0xDC),
	_INIT_DCS_CMD(0x00,0xB5),
	_INIT_DCS_CMD(0xCF,0x03,0x03,0xA6,0xAA),
	_INIT_DCS_CMD(0x00,0xC0),
	_INIT_DCS_CMD(0xCF,0x07,0x07,0xDA,0xDE),
	_INIT_DCS_CMD(0x00,0xC5),
	_INIT_DCS_CMD(0xCF,0x00,0x07,0x1C,0xD0),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xC1,0x22),
	_INIT_DCS_CMD(0x00,0x9C),
	_INIT_DCS_CMD(0xC1,0x08),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0x35,0x01),
	_INIT_DCS_CMD(0x00,0x9F),
	_INIT_DCS_CMD(0xC5,0x40),
	_INIT_DCS_CMD(0x00,0x98),
	_INIT_DCS_CMD(0xC5,0x54),
	_INIT_DCS_CMD(0x00,0x91),
	_INIT_DCS_CMD(0xC5,0x4C),
	_INIT_DCS_CMD(0x00,0x8C),
	_INIT_DCS_CMD(0xCF,0x40,0x40),
	_INIT_DCS_CMD(0x00,0x93),
	_INIT_DCS_CMD(0xC4,0x90),
	_INIT_DCS_CMD(0x00,0xD7),
	_INIT_DCS_CMD(0xC0,0xF0),
	_INIT_DCS_CMD(0x00,0xA2),
	_INIT_DCS_CMD(0xF5,0x1F),
	_INIT_DCS_CMD(0x00,0x94),
	_INIT_DCS_CMD(0xC5,0x61),
	_INIT_DCS_CMD(0x00,0x9B),
	_INIT_DCS_CMD(0xC5,0x16),
	_INIT_DCS_CMD(0x00,0xB1),
	_INIT_DCS_CMD(0xF5,0x02),
	_INIT_DCS_CMD(0x00,0x84),
	_INIT_DCS_CMD(0xC5,0xF8),
	_INIT_DCS_CMD(0x00,0x90),
	_INIT_DCS_CMD(0xE9,0x10),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xB0,0x07),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFA,0x01),
	_INIT_DCS_CMD(0x00,0xA8),
	_INIT_DCS_CMD(0xC5,0x99),
	_INIT_DCS_CMD(0x00,0xB6),
	_INIT_DCS_CMD(0xC5,0x55,0x55),
	_INIT_DCS_CMD(0x00,0xB8),
	_INIT_DCS_CMD(0xC5,0x4B,0x4B),
	_INIT_DCS_CMD(0x00,0x9A),
	_INIT_DCS_CMD(0xF5,0x00),
	_INIT_DCS_CMD(0x00,0xB2),
	_INIT_DCS_CMD(0xCE,0x8A),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFA,0x5A),
	_INIT_DCS_CMD(0x00,0x88),
	_INIT_DCS_CMD(0xB0,0x01),
	_INIT_DCS_CMD(0x00,0x00),
	_INIT_DCS_CMD(0xFF,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00,0x80),
	_INIT_DCS_CMD(0xFF,0x00,0x00),

	_INIT_DCS_CMD(0x11),
	_INIT_DELAY_CMD(85),
	_INIT_DCS_CMD(0x29),
	_INIT_DELAY_CMD(10),
	{},
};

static const struct panel_init_cmd tg_inx_init_cmd[] = {
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xFA, 0x5A),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xFF, 0x82,0x05,0x01),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xFF, 0x82,0x05),
	_INIT_DCS_CMD(0x00, 0x93),
	_INIT_DCS_CMD(0xC5, 0x6B),
	_INIT_DCS_CMD(0x00, 0x97),
	_INIT_DCS_CMD(0xC5, 0x6B),
	_INIT_DCS_CMD(0x00, 0x9E),
	_INIT_DCS_CMD(0xC5, 0x0A),
	_INIT_DCS_CMD(0x00, 0x9A),
	_INIT_DCS_CMD(0xC5, 0xCD),
	_INIT_DCS_CMD(0x00, 0x9C),
	_INIT_DCS_CMD(0xC5, 0xCD),
	_INIT_DCS_CMD(0x00, 0xB6),
	_INIT_DCS_CMD(0xC5, 0x57,0x57),
	_INIT_DCS_CMD(0x00, 0xB8),
	_INIT_DCS_CMD(0xC5, 0x43,0x43),
	_INIT_DCS_CMD(0x00, 0xA0),
	_INIT_DCS_CMD(0xA5, 0x04),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xD8, 0xB4,0xB4),
	_INIT_DCS_CMD(0x00, 0x82),
	_INIT_DCS_CMD(0xC5, 0x95),
	_INIT_DCS_CMD(0x00, 0x83),
	_INIT_DCS_CMD(0xC5, 0x07),
	_INIT_DCS_CMD(0x00, 0xD9),
	_INIT_DCS_CMD(0xCB, 0x40),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xE1, 0x00,0x04,0x10,0x1D,0x25,0x2F,0x3F,0x4C,0x50,0x5D,0x61,0x78,0x89,0x74,0x71,0x65),
	_INIT_DCS_CMD(0x00, 0x10),
	_INIT_DCS_CMD(0xE1, 0x5B,0x4F,0x3F,0x34,0x2B,0x1E,0x16,0x14),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xE2, 0x00,0x04,0x10,0x1D,0x25,0x2F,0x3F,0x4C,0x50,0x5D,0x61,0x78,0x89,0x74,0x71,0x65),
	_INIT_DCS_CMD(0x00, 0x10),
	_INIT_DCS_CMD(0xE2, 0x5B,0x4F,0x3F,0x34,0x2B,0x1B,0x0C,0x07),
	_INIT_DCS_CMD(0x00, 0xA1),
	_INIT_DCS_CMD(0xB3, 0x04,0xB0),
	_INIT_DCS_CMD(0x00, 0xA3),
	_INIT_DCS_CMD(0xB3, 0x07,0xD0),
	_INIT_DCS_CMD(0x00, 0xA5),
	_INIT_DCS_CMD(0xB3, 0x80,0x13),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xCB, 0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C),
	_INIT_DCS_CMD(0x00, 0x87),
	_INIT_DCS_CMD(0xCB, 0x3C),
	_INIT_DCS_CMD(0x00, 0x88),
	_INIT_DCS_CMD(0xCB, 0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C,0x3C),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xCB, 0x03,0x3C,0x3C,0x3C,0x3C,0x3C),
	_INIT_DCS_CMD(0x00, 0x97),
	_INIT_DCS_CMD(0xCB, 0x33),
	_INIT_DCS_CMD(0x00, 0x98),
	_INIT_DCS_CMD(0xCB, 0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34),
	_INIT_DCS_CMD(0x00, 0xA0),
	_INIT_DCS_CMD(0xCB, 0x34,0x34,0x34,0x37,0x34,0x34,0x34,0x34),
	_INIT_DCS_CMD(0x00, 0xA8),
	_INIT_DCS_CMD(0xCB, 0xC8,0x34,0x34,0x34,0x34,0x34),
	_INIT_DCS_CMD(0x00, 0xB0),
	_INIT_DCS_CMD(0xCB, 0x00,0x00,0x00,0x00,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xB7),
	_INIT_DCS_CMD(0xCB, 0x00),
	_INIT_DCS_CMD(0x00, 0xB8),
	_INIT_DCS_CMD(0xCB, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xC0),
	_INIT_DCS_CMD(0xCB, 0x00,0x00,0x00,0x00,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xC7),
	_INIT_DCS_CMD(0xCB, 0x00),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xCC, 0x2F,0x2F,0x2F,0x27,0x31,0x32,0x01,0x2C),
	_INIT_DCS_CMD(0x00, 0x88),
	_INIT_DCS_CMD(0xCC, 0x2D,0x1B,0x1D,0x1F,0x21,0x0B,0x0D,0x0F),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xCC, 0x11,0x13,0x15,0x03,0x2F,0x2F),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xCD, 0x2F,0x2F,0x2F,0x27,0x31,0x32,0x02,0x2C),
	_INIT_DCS_CMD(0x00, 0x88),
	_INIT_DCS_CMD(0xCD, 0x2D,0x1C,0x1E,0x20,0x22,0x0C,0x0E,0x10),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xCD, 0x12,0x14,0x16,0x04,0x2F,0x2F),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xC2, 0x8C,0x0A,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0x84),
	_INIT_DCS_CMD(0xC2, 0x8B,0x0A,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xC0),
	_INIT_DCS_CMD(0xC2, 0x86,0x87,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xC7),
	_INIT_DCS_CMD(0xC2, 0x85,0x86,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xD0),
	_INIT_DCS_CMD(0xC2, 0x84,0x85,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xD7),
	_INIT_DCS_CMD(0xC2, 0x83,0x84,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xE0),
	_INIT_DCS_CMD(0xC2, 0x82,0x83,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xE7),
	_INIT_DCS_CMD(0xC2, 0x81,0x82,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xF0),
	_INIT_DCS_CMD(0xC2, 0x80,0x81,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xF7),
	_INIT_DCS_CMD(0xC2, 0x01,0x80,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xC3, 0x02,0x8B,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0x87),
	_INIT_DCS_CMD(0xC3, 0x03,0x8A,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xC3, 0x04,0x89,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0x97),
	_INIT_DCS_CMD(0xC3, 0x05,0x88,0x00,0x06,0x7A,0xAA,0x0B),
	_INIT_DCS_CMD(0x00, 0xA0),
	_INIT_DCS_CMD(0xC2, 0x06,0x46,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xA4),
	_INIT_DCS_CMD(0xC2, 0x05,0x46,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xA8),
	_INIT_DCS_CMD(0xC2, 0x04,0x46,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xAC),
	_INIT_DCS_CMD(0xC2, 0x03,0x46,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xB0),
	_INIT_DCS_CMD(0xC2, 0x02,0x46,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xB4),
	_INIT_DCS_CMD(0xC2, 0x01,0x46,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xB8),
	_INIT_DCS_CMD(0xC2, 0x00,0x06,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xBC),
	_INIT_DCS_CMD(0xC2, 0x01,0x06,0x7A,0xAA),
	_INIT_DCS_CMD(0x00, 0xA0),
	_INIT_DCS_CMD(0xC3, 0x00,0x70,0xD3,0x0A,0x7A,0xAA,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xA8),
	_INIT_DCS_CMD(0xC3, 0x00,0x70,0xD4,0x0B,0x7A,0xAA,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xF0),
	_INIT_DCS_CMD(0xCC, 0x3D,0x8E,0x8E,0x18,0x18),
	_INIT_DCS_CMD(0x00, 0xA2),
	_INIT_DCS_CMD(0xF3, 0x55),
	_INIT_DCS_CMD(0x00, 0xE0),
	_INIT_DCS_CMD(0xC3, 0x27,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xE4),
	_INIT_DCS_CMD(0xC3, 0x27,0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xEC),
	_INIT_DCS_CMD(0xC3, 0xC2,0xC2),
	_INIT_DCS_CMD(0x00, 0xE8),
	_INIT_DCS_CMD(0xC3, 0x44,0x00,0x0C,0x70),
	_INIT_DCS_CMD(0x00, 0xEE),
	_INIT_DCS_CMD(0xC3, 0x02),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xC0, 0x00,0x92,0x00,0xDC,0x00,0x10),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xC0, 0x00,0x92,0x00,0xDC,0x00,0x10),
	_INIT_DCS_CMD(0x00, 0xA0),
	_INIT_DCS_CMD(0xC0, 0x00,0xF0,0x00,0xD0,0x00,0x1C),
	_INIT_DCS_CMD(0x00, 0xB0),
	_INIT_DCS_CMD(0xC0, 0x00,0xA7,0x00,0xDC,0x10),
	_INIT_DCS_CMD(0x00, 0xA3),
	_INIT_DCS_CMD(0xC1, 0x00,0x28,0x00,0x28,0x00,0x04),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xCE, 0x01,0x81,0x08,0x21,0x00,0x4E,0x00,0xE2,0x00,0x2A,0x00,0x3A),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xCE, 0x00,0x88,0x0C,0x7C,0x00,0x44,0x80,0x08,0x21,0x00,0x03,0x00,0x1C,0x14),
	_INIT_DCS_CMD(0x00, 0xA0),
	_INIT_DCS_CMD(0xCE, 0x10,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xB0),
	_INIT_DCS_CMD(0xCE, 0x62,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0xCC),
	_INIT_DCS_CMD(0xCE, 0x07,0xEE),
	_INIT_DCS_CMD(0x00, 0xD0),
	_INIT_DCS_CMD(0xCE, 0x01,0x00,0x0A,0x01,0x01,0x00,0xDC,0x01),
	_INIT_DCS_CMD(0x00, 0xE1),
	_INIT_DCS_CMD(0xCE, 0x07,0x02,0x20,0x02,0x37,0x00,0x87),
	_INIT_DCS_CMD(0x00, 0xF0),
	_INIT_DCS_CMD(0xCE, 0x00,0x11,0x08,0x01,0x44,0x01,0xED,0x00,0xEC,0x0F),
	_INIT_DCS_CMD(0x00, 0xB0),
	_INIT_DCS_CMD(0xCF, 0x07,0x07,0xD5,0xD9),
	_INIT_DCS_CMD(0x00, 0xB5),
	_INIT_DCS_CMD(0xCF, 0x03,0x03,0xA6,0xAA),
	_INIT_DCS_CMD(0x00, 0xC0),
	_INIT_DCS_CMD(0xCF, 0x07,0x07,0xDA,0xDE),
	_INIT_DCS_CMD(0x00, 0xC5),
	_INIT_DCS_CMD(0xCF, 0x00,0x07,0x1C,0xD0),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xC1, 0x22),
	_INIT_DCS_CMD(0x00, 0x9C),
	_INIT_DCS_CMD(0xC1, 0x08),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0x35, 0x01),
	_INIT_DCS_CMD(0x00, 0x9F),
	_INIT_DCS_CMD(0xC5, 0x00),
	_INIT_DCS_CMD(0x00, 0x98),
	_INIT_DCS_CMD(0xC5, 0x54),
	_INIT_DCS_CMD(0x00, 0x91),
	_INIT_DCS_CMD(0xC5, 0x4C),
	_INIT_DCS_CMD(0x00, 0x8C),
	_INIT_DCS_CMD(0xCF, 0x40,0x40),
	_INIT_DCS_CMD(0x00, 0x93),
	_INIT_DCS_CMD(0xC4, 0x90),
	_INIT_DCS_CMD(0x00, 0xD7),
	_INIT_DCS_CMD(0xC0, 0xF0),
	_INIT_DCS_CMD(0x00, 0xA2),
	_INIT_DCS_CMD(0xF5, 0x1F),
	_INIT_DCS_CMD(0x00, 0x90),
	_INIT_DCS_CMD(0xE9, 0x10),
	_INIT_DCS_CMD(0x00, 0xB1),
	_INIT_DCS_CMD(0xF5, 0x02),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xA4, 0x2C),
	_INIT_DCS_CMD(0x00, 0x84),
	_INIT_DCS_CMD(0xC5, 0xFD),
	_INIT_DCS_CMD(0x00, 0x88),
	_INIT_DCS_CMD(0xB0, 0x07),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xFA, 0x01),
	_INIT_DCS_CMD(0x00, 0xA8),
	_INIT_DCS_CMD(0xC5, 0x99),
	_INIT_DCS_CMD(0x00, 0xB6),
	_INIT_DCS_CMD(0xC5, 0x55,0x55),
	_INIT_DCS_CMD(0x00, 0xB8),
	_INIT_DCS_CMD(0xC5, 0x41,0x41),
	_INIT_DCS_CMD(0x00, 0x9A),
	_INIT_DCS_CMD(0xF5, 0x00),
	_INIT_DCS_CMD(0x00, 0xB2),
	_INIT_DCS_CMD(0xCE, 0x94),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xFA, 0x5A),
	_INIT_DCS_CMD(0x00, 0x88),
	_INIT_DCS_CMD(0xB0, 0x01),
	_INIT_DCS_CMD(0x00, 0x00),
	_INIT_DCS_CMD(0xFF, 0x00,0x00,0x00),
	_INIT_DCS_CMD(0x00, 0x80),
	_INIT_DCS_CMD(0xFF, 0x00,0x00),

	_INIT_DCS_CMD(0x11),
	_INIT_DELAY_CMD(85),
	_INIT_DCS_CMD(0x29),
	_INIT_DELAY_CMD(10),
	{},
};

static inline struct panel_info *to_panel_info(struct drm_panel *panel)
{
	return container_of(panel, struct panel_info, base);
}

static int panel_init_dcs_cmd(struct panel_info *info, const struct panel_init_cmd *init_cmds)
{
	struct mipi_dsi_device *dsi = info->dsi;
	struct drm_panel *panel = &info->base;
	int i, err = 0;

	LCM_FUNC_ENTER();
	if (init_cmds) {
		for (i = 0; init_cmds[i].len != 0; i++) {
			const struct panel_init_cmd *cmd = &init_cmds[i];

			switch (cmd->type) {
			case DELAY_CMD:
				msleep(cmd->data[0]);
				err = 0;
				break;

			case INIT_DCS_CMD:
				err = mipi_dsi_dcs_write(dsi, cmd->data[0],
							 cmd->len <= 1 ? NULL :
							 &cmd->data[1],
							 cmd->len - 1);
				break;

			default:
				err = -EINVAL;
			}

			if (err < 0) {
				dev_err(panel->dev,
					"failed to write command %u\n", i);
				return err;
			}
		}
	}
	LCM_FUNC_EXIT();
	return 0;
}

static int panel_enter_sleep_mode(struct panel_info *info)
{
	struct mipi_dsi_device *dsi = info->dsi;
	int ret;

	LCM_FUNC_ENTER();
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		return ret;
	msleep(20);
	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		return ret;
	LCM_FUNC_EXIT();

	return 0;
}

void panel_gesture_mode_set_by_ft8205(bool gst_mode)
{
	atomic_set(&gesture_status, gst_mode);
}
EXPORT_SYMBOL_GPL(panel_gesture_mode_set_by_ft8205);

static bool panel_check_gesture_mode(void)
{
	bool ret = false;

	LCM_FUNC_ENTER();
	ret = atomic_read(&gesture_status);
	LCM_FUNC_EXIT();

	return ret;
}

void panel_touch_upgrade_status_set_by_ft8205(bool upg_mode)
{
	atomic_set(&touch_upgrade_status, upg_mode);
}
EXPORT_SYMBOL_GPL(panel_touch_upgrade_status_set_by_ft8205);

static bool panel_check_touch_upgrade_status(void)
{
	bool ret = false;

	LCM_FUNC_ENTER();
	ret = atomic_read(&touch_upgrade_status);
	LCM_FUNC_EXIT();

	return ret;
}

void ft8205_share_powerpin_on(void)
{
	mutex_lock(&ft8205_share_power_mutex);
	/*LCM resume power with ON1->ON2, touch resume with ON.*/
	if (atomic_read(&g_power_request) == 0)
		LCM_ERROR("%s Touch poweron should NOT before LCM poweron", __func__);
	lcm_power_manager(share_info, LCM_ON);
	mutex_unlock(&ft8205_share_power_mutex);
}
EXPORT_SYMBOL(ft8205_share_powerpin_on);

void ft8205_share_powerpin_off(void)
{
	mutex_lock(&ft8205_share_power_mutex);
	lcm_power_manager(share_info, LCM_OFF);
	mutex_unlock(&ft8205_share_power_mutex);
}
EXPORT_SYMBOL(ft8205_share_powerpin_off);

void ft8205_share_tp_resetpin_control(int value)
{
	LCM_FUNC_ENTER();
	mutex_lock(&ft8205_share_power_mutex);
	if (atomic_read(&g_power_request) <= 0) {
		LCM_ERROR("%s skip, bceause res_cnt(%d) <= 0",
				__func__, atomic_read(&g_power_request));
	} else {
		gpiod_set_value(share_info->ctp_rst_gpio, value);
	}
	mutex_unlock(&ft8205_share_power_mutex);
	LCM_FUNC_EXIT();
}
EXPORT_SYMBOL(ft8205_share_tp_resetpin_control);

static int panel_unprepare_power(struct drm_panel *panel)
{
	struct panel_info *info = to_panel_info(panel);

	LCM_FUNC_ENTER();
	if (!info->prepared_power)
		return 0;

	if ((false == panel_check_gesture_mode() &&
		false == panel_check_touch_upgrade_status()) ||
		atomic_read(&g_power_rebooting) != 0) {
		lcm_power_manager_lock(info, LCM_OFF);
		info->prepared_power = false;
	}
	LCM_FUNC_EXIT();

	return 0;
}

static int panel_unprepare(struct drm_panel *panel)
{
	struct panel_info *info = to_panel_info(panel);

	LCM_FUNC_ENTER();
	if (!info->prepared)
		return 0;

	lcm_power_manager_lock(info, LCM_DSB);
	info->prepared = false;
	LCM_FUNC_EXIT();

	return 0;
}

static int panel_prepare_power(struct drm_panel *panel)
{
	struct panel_info *info = to_panel_info(panel);

	LCM_FUNC_ENTER();
	if (info->prepared_power)
		return 0;

	lcm_power_manager_lock(info, LCM_ON1);
	info->prepared_power = true;
	LCM_FUNC_EXIT();

	return 0;
}

static int panel_prepare(struct drm_panel *panel)
{
	struct panel_info *info = to_panel_info(panel);

	LCM_FUNC_ENTER();
	if (info->prepared)
		return 0;

	lcm_power_manager_lock(info, LCM_ON2);
	info->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
	LCM_FUNC_EXIT();

	return 0;
}

static int panel_enable(struct drm_panel *panel)
{
	struct panel_info *info = to_panel_info(panel);

	LCM_FUNC_ENTER();
	if (info->enabled)
		return 0;

	if (info->backlight) {
		info->backlight->props.state &= ~BL_CORE_FBBLANK;
		info->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(info->backlight);
	}

	gpiod_set_value(info->leden_gpio, 1);
	msleep(60);
	backlight_on();
	info->enabled = true;
	LCM_FUNC_EXIT();

	return 0;
}

static int panel_disable(struct drm_panel *panel)
{
	struct panel_info *info = to_panel_info(panel);

	LCM_FUNC_ENTER();
	if (!info->enabled)
		return 0;

	if (info->backlight) {
		info->backlight->props.power = FB_BLANK_POWERDOWN;
		info->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(info->backlight);
	}

	gpiod_set_value(info->leden_gpio, 0);
	info->enabled = false;
	LCM_FUNC_EXIT();

	return 0;
}

static int lcm_power_controller(struct panel_info *info, int state)
{
	int ret = 0;

	LCM_FUNC_ENTER();
	switch (state) {
	case TRS_DSB:
		//deep sleep or gesture mode suspend
		ret = panel_enter_sleep_mode(info);
		if (ret < 0) {
			dev_err(info->dev, "failed to set panel off: %d\n",
					ret);
			return ret;
		}
		if ((false == panel_check_gesture_mode() &&
			false == panel_check_touch_upgrade_status()) ||
			atomic_read(&g_power_rebooting) != 0) {
			msleep(150);
			gpiod_set_value(info->enable_gpio, 0);
			gpiod_set_value(info->ctp_rst_gpio, 0);
		}
		break;
	case TRS_DSB_TO_ON:
		//gesture mode resume
		gpiod_set_value(info->enable_gpio, 0);
		usleep_range(1000, 2000);
		gpiod_set_value(info->enable_gpio, 1);
		usleep_range(52000, 54000);
		switch (vendor_id) {
		case LCM_FT8205_KD_BOE:
			ret = panel_init_dcs_cmd(info,kd_boe_init_cmd);
			break;
		case LCM_FT8205_TG_INX:
			ret = panel_init_dcs_cmd(info,tg_inx_init_cmd);
			break;
		default:
			pr_notice("Not match correct ID, loading default init code...\n");
			ret = panel_init_dcs_cmd(info,kd_boe_init_cmd);
			break;
		}
		if (ret < 0) {
			dev_err(info->dev, "failed to init panel: %d\n", ret);
			return ret;
		}
		break;
	case TRS_DSB_TO_OFF:
		//normal suspend
		usleep_range(5000, 7000);
		gpiod_set_value(info->avee, 0);
		gpiod_set_value(info->avdd, 0);
		usleep_range(10000, 10001);
		if (info->pp1800)
			regulator_disable(info->pp1800);
		break;
	case TRS_ON_PHASE1:
		//normal resume phase1
		gpiod_set_value(info->enable_gpio, 0);
		gpiod_set_value(info->ctp_rst_gpio, 0);
		usleep_range(1000, 1500);

		if (info->pp1800) {
			ret = regulator_enable(info->pp1800);
			if (ret < 0)
				dev_err(info->dev, "Enable pp1800 fail, %ld\n", ret);
			usleep_range(3000, 5000);
		}
		gpiod_set_value(info->ctp_rst_gpio, 1);
		usleep_range(6000, 7000);
		bias_on(info);
		usleep_range(5000, 7000);
		break;
	case TRS_ON_PHASE2:
		//normal resume phase2
		gpiod_set_value(info->enable_gpio, 1);
		usleep_range(4000, 5000);
		gpiod_set_value(info->enable_gpio, 0);
		usleep_range(1000, 2000);
		gpiod_set_value(info->enable_gpio, 1);
		usleep_range(52000, 54000);
		switch (vendor_id) {
		case LCM_FT8205_KD_BOE:
			ret = panel_init_dcs_cmd(info,kd_boe_init_cmd);
			break;
		case LCM_FT8205_TG_INX:
			ret = panel_init_dcs_cmd(info,tg_inx_init_cmd);
			break;
		default:
			pr_notice("Not match correct ID, loading default init code...\n");
			ret = panel_init_dcs_cmd(info,kd_boe_init_cmd);
			break;
		}
		if (ret < 0) {
			dev_err(info->dev, "failed to init panel: %d\n", ret);
			return ret;
		}
		break;
	case TRS_OFF_TO_ON:
		//normal resume phase1
		gpiod_set_value(info->enable_gpio, 0);
		gpiod_set_value(info->ctp_rst_gpio, 0);
		usleep_range(1000, 1500);

		if (info->pp1800) {
			ret = regulator_enable(info->pp1800);
			if (ret < 0)
				dev_err(info->dev, "Enable pp1800 fail, %ld\n", ret);
			usleep_range(3000, 5000);
		}
		gpiod_set_value(info->ctp_rst_gpio, 1);
		usleep_range(6000, 7000);
		bias_on(info);
		usleep_range(5000, 7000);

		//normal resume phase2
		gpiod_set_value(info->enable_gpio, 1);
		usleep_range(4000, 5000);
		gpiod_set_value(info->enable_gpio, 0);
		usleep_range(1000, 2000);
		gpiod_set_value(info->enable_gpio, 1);
		usleep_range(52000, 54000);
		switch (vendor_id) {
		case LCM_FT8205_KD_BOE:
			ret = panel_init_dcs_cmd(info,kd_boe_init_cmd);
			break;
		case LCM_FT8205_TG_INX:
			ret = panel_init_dcs_cmd(info,tg_inx_init_cmd);
			break;
		default:
			pr_notice("Not match correct ID, loading default init code...\n");
			ret = panel_init_dcs_cmd(info,kd_boe_init_cmd);
			break;
		}
		if (ret < 0) {
			dev_err(info->dev, "failed to init panel: %d\n", ret);
			return ret;
		}
		break;
	case TRS_OFF:
		ret = panel_enter_sleep_mode(info);
		if (ret < 0) {
			dev_err(info->dev, "failed to set panel off: %d\n",
					ret);
			return ret;
		}
		msleep(150);
		gpiod_set_value(info->enable_gpio, 0);
		gpiod_set_value(info->ctp_rst_gpio, 0);
		usleep_range(5000, 7000);
		gpiod_set_value(info->avee, 0);
		gpiod_set_value(info->avdd, 0);
		usleep_range(15000, 15001);
		if (info->pp1800)
			regulator_disable(info->pp1800);
		break;
	case TRS_KEEP_CUR:
		ret = -1;
		dev_info(info->dev, "keep current state\n");
		break;
	case TRS_NOT_PSB:
		ret = -1;
		dev_err(info->dev, "Unexpected state change\n");
		break;
	default:
		ret = -1;
		dev_err(info->dev, "%s undefined state (%d)\n", __func__, state);
		break;
	}
	LCM_FUNC_EXIT();
	return ret;
}

static void lcm_power_manager_lock(struct panel_info *info, int pwr_state)
{
	mutex_lock(&ft8205_share_power_mutex);
	lcm_power_manager(info, pwr_state);
	mutex_unlock(&ft8205_share_power_mutex);
}

static void lcm_power_manager(struct panel_info *info, int new_state)
{
	int tmp_state = LCM_ON;
	int new_trns;
	int ret = 0;

	LCM_FUNC_ENTER();

	tmp_state = old_state;

	switch (new_state) {
	case LCM_ON:
	case LCM_ON1:
		atomic_inc(&g_power_request);
		break;
	case LCM_ON2:
	case LCM_DSB:
		break;
	case LCM_OFF:
		atomic_dec(&g_power_request);
		break;
	default:
		LCM_INFO("%s,no match state", __func__);
		break;
	}

	new_trns = trans_table[old_state][new_state];

	if (new_trns == TRS_KEEP_CUR || new_trns == TRS_NOT_PSB) {
		if (new_state == LCM_ON || new_state == LCM_ON1)
			atomic_dec(&g_power_request);
		if (new_state == LCM_OFF)
			atomic_inc(&g_power_request);
	}

	ret = lcm_power_controller(info, new_trns);
	if (ret == 0)
		old_state = new_state;

	LCM_INFO("%s requesting %s, changed from %s to %s, count=%d",
		__func__, lcm_state_str[new_state],
		lcm_state_str[tmp_state], lcm_state_str[old_state],
		atomic_read(&g_power_request));

	LCM_FUNC_EXIT();
}

#define FRAME_WIDTH	(1200)
#define FRAME_HEIGHT	(2000)
/* KD BOE panel porch */
#define KD_BOE_HFP (22)    // params->dsi.horizontal_frontporch
#define KD_BOE_HSA (4)     // params->dsi.horizontal_sync_active
#define KD_BOE_HBP (7)     // params->dsi.horizontal_backporch
#define KD_BOE_VFP (230)   // params->dsi.vertical_frontporch
#define KD_BOE_VSA (4)     // params->dsi.vertical_sync_active
#define KD_BOE_VBP (20)    // params->dsi.vertical_backporch

/* TG INX panel porch */
#define TG_INX_HFP (32)    // params->dsi.horizontal_frontporch
#define TG_INX_HSA (4)     // params->dsi.horizontal_sync_active
#define TG_INX_HBP (10)    // params->dsi.horizontal_backporch
#define TG_INX_VFP (210)   // params->dsi.vertical_frontporch
#define TG_INX_VSA (4)     // params->dsi.vertical_sync_active
#define TG_INX_VBP (22)    // params->dsi.vertical_backporch

#define PHYSICAL_WIDTH			(143)
#define PHYSICAL_HEIGHT			(238)

static const struct drm_display_mode kd_boe_mode = {
	.clock = (FRAME_WIDTH + KD_BOE_HFP + KD_BOE_HSA + KD_BOE_HBP) *
		(FRAME_HEIGHT + KD_BOE_VFP + KD_BOE_VSA + KD_BOE_VBP) *
		60 / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + KD_BOE_HFP,
	.hsync_end = FRAME_WIDTH + KD_BOE_HFP + KD_BOE_HSA,
	.htotal = FRAME_WIDTH + KD_BOE_HFP + KD_BOE_HSA + KD_BOE_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + KD_BOE_VFP,
	.vsync_end = FRAME_HEIGHT + KD_BOE_VFP + KD_BOE_VSA,
	.vtotal = FRAME_HEIGHT + KD_BOE_VFP + KD_BOE_VSA + KD_BOE_VBP,
};

static const struct drm_display_mode tg_inx_mode = {
	.clock = (FRAME_WIDTH + TG_INX_HFP + TG_INX_HSA + TG_INX_HBP) *
		(FRAME_HEIGHT + TG_INX_VFP + TG_INX_VSA + TG_INX_VBP) *
		60 / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + TG_INX_HFP,
	.hsync_end = FRAME_WIDTH + TG_INX_HFP + TG_INX_HSA,
	.htotal = FRAME_WIDTH + TG_INX_HFP + TG_INX_HSA + TG_INX_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + TG_INX_VFP,
	.vsync_end = FRAME_HEIGHT + TG_INX_VFP + TG_INX_VSA,
	.vtotal = FRAME_HEIGHT + TG_INX_VFP + TG_INX_VSA + TG_INX_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct panel_info *info = to_panel_info(panel);

	LCM_FUNC_ENTER();
	gpiod_set_value(info->enable_gpio, on);
	devm_gpiod_put(info->dev, info->enable_gpio);
	LCM_FUNC_EXIT();

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	return 1;
}

static struct mtk_panel_params ext_kd_boe_params = {
	.pll_clk = 533,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
};

static struct mtk_panel_params ext_tg_inx_params = {
	.pll_clk = 533,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif

static int panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct panel_info *info = to_panel_info(panel);
	struct drm_display_mode *mode;

	LCM_FUNC_ENTER();
	switch (vendor_id) {
	case LCM_FT8205_KD_BOE:
		mode = drm_mode_duplicate(connector->dev, &kd_boe_mode);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%ux@%u\n",
				kd_boe_mode.hdisplay, kd_boe_mode.vdisplay,
				drm_mode_vrefresh(&kd_boe_mode));
			return -ENOMEM;
		}
		break;
	case LCM_FT8205_TG_INX:
		mode = drm_mode_duplicate(connector->dev, &tg_inx_mode);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%ux@%u\n",
				tg_inx_mode.hdisplay, tg_inx_mode.vdisplay,
				drm_mode_vrefresh(&tg_inx_mode));
			return -ENOMEM;
		}
		break;
	default:
		mode = drm_mode_duplicate(connector->dev, &kd_boe_mode);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%ux@%u\n",
				kd_boe_mode.hdisplay, kd_boe_mode.vdisplay,
				drm_mode_vrefresh(&kd_boe_mode));
			return -ENOMEM;
		}
		break;
	}

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = PHYSICAL_WIDTH;
	connector->display_info.height_mm = PHYSICAL_HEIGHT;
	connector->display_info.bpc = 8;
	drm_connector_set_panel_orientation(connector, info->orientation);
	LCM_FUNC_EXIT();

	return 1;
}

static const struct drm_panel_funcs panel_funcs = {
	.unprepare = panel_unprepare,
	.prepare = panel_prepare,
	.unprepare_power = panel_unprepare_power,
	.prepare_power = panel_prepare_power,
	.disable = panel_disable,
	.enable = panel_enable,
	.get_modes = panel_get_modes,
};

static int panel_add(struct panel_info *info)
{
	struct device *dev = &info->dsi->dev;
	struct device_node *backlight;
	int ret;

	LCM_FUNC_ENTER();
	info->avdd = devm_gpiod_get(dev, "avdd", GPIOD_ASIS);
	if (IS_ERR(info->avdd)) {
		dev_err(dev, "cannot get avdd-gpios %ld\n",
			PTR_ERR(info->avdd));
		return PTR_ERR(info->avdd);
	}

	info->avee = devm_gpiod_get(dev, "avee", GPIOD_ASIS);
	if (IS_ERR(info->avee)) {
		dev_err(dev, "cannot get avee-gpios %ld\n",
			PTR_ERR(info->avee));
		return PTR_ERR(info->avee);
	}

	info->pp1800 = devm_regulator_get(dev, "pp1800");
	if (IS_ERR(info->pp1800)) {
		dev_err(dev, "cannot get pp1800 %ld\n",
			PTR_ERR(info->pp1800));
		info->pp1800 = NULL;
	} else if (regulator_is_enabled(info->pp1800)) {
		ret = regulator_enable(info->pp1800);
		if (ret < 0)
			dev_err(dev, "Enable pp1800 fail, %ld\n", ret);
	}

	info->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_ASIS);
	if (IS_ERR(info->enable_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(info->enable_gpio));
		return PTR_ERR(info->enable_gpio);
	}

	info->leden_gpio = devm_gpiod_get(dev, "leden", GPIOD_ASIS);
	if (IS_ERR(info->leden_gpio)) {
		dev_err(dev, "cannot get lcden-gpios %ld\n",
			PTR_ERR(info->leden_gpio));
		return PTR_ERR(info->leden_gpio);
	}

	info->ctp_rst_gpio = devm_gpiod_get(dev, "ctp-rst", GPIOD_ASIS);
	if (IS_ERR(info->ctp_rst_gpio)) {
		dev_err(dev, "cannot get ctp-rst-gpios %ld\n",
			PTR_ERR(info->ctp_rst_gpio));
		return PTR_ERR(info->ctp_rst_gpio);
	}

	drm_panel_init(&info->base, dev, &panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ret = of_drm_get_panel_orientation(dev->of_node, &info->orientation);
	if (ret < 0) {
		dev_err(dev, "%pOF: failed to get orientation %d\n", dev->of_node, ret);
		return ret;
	}

#ifndef CONFIG_MTK_DISP_NO_LK
	info->prepared = true;
	info->enabled = true;
	info->prepared_power = true;
#endif

	info->base.funcs = &panel_funcs;
	info->base.dev = &info->dsi->dev;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		info->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!info->backlight)
			return -EPROBE_DEFER;
	}

	drm_panel_add(&info->base);

	ret = register_reboot_notifier(&ft8205_reboot_notifier);
	if (ret < 0) {
		dev_err(dev, "Failed to register ft8205 reboot notifier %d\n", ret);
		return ret;
	}

	LCM_FUNC_EXIT();

	return 0;
}

static void panel_resources_release(struct panel_info *info)
{
	struct device *dev = &info->dsi->dev;

	LCM_FUNC_ENTER();
	if (!IS_ERR(info->avdd))
		devm_gpiod_put(dev, info->avdd);

	if (!IS_ERR(info->avee))
		devm_gpiod_put(dev, info->avee);

	if (!IS_ERR(info->pp1800))
		devm_regulator_put(info->pp1800);

	if (!IS_ERR(info->enable_gpio))
		devm_gpiod_put(dev, info->enable_gpio);

	if (!IS_ERR(info->leden_gpio))
		devm_gpiod_put(dev, info->leden_gpio);

	if (!IS_ERR(info->ctp_rst_gpio))
		devm_gpiod_put(dev, info->ctp_rst_gpio);

	LCM_FUNC_EXIT();
}

static int panel_probe(struct mipi_dsi_device *dsi)
{
	struct panel_info *info;
	struct device *dev = &dsi->dev;
	int ret;

	LCM_FUNC_ENTER();

	if (vendor_id == LCM_VENDOR_ID_UNKNOWN)
		get_lcm_id();

	switch (vendor_id) {
	case LCM_FT8205_KD_BOE:
		pr_notice("[Kernel/LCM] Ft8205 panel, vendor_id = %d loading.\n", vendor_id);
		break;
	case LCM_FT8205_TG_INX:
		pr_notice("[Kernel/LCM] Ft8205 panel, vendor_id = %d loading.\n", vendor_id);
		break;
	default:
		pr_notice("[Kernel/LCM] It's not Ft8205 panel, exit.\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&dsi->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
				MIPI_DSI_MODE_LPM;
	info->dsi = dsi;
	ret = panel_add(info);
	if (ret < 0) {
		panel_resources_release(info);
		devm_kfree(&dsi->dev,info);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, info);

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		panel_resources_release(info);
		drm_panel_remove(&info->base);
		devm_kfree(&dsi->dev,info);
		return ret;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&info->base);
	switch (vendor_id) {
	case LCM_FT8205_KD_BOE:
		pr_notice("[Kernel/LCM] ft8205 kd boe, pll_clock=%d.\n",
				ext_kd_boe_params.pll_clk);
		ret = mtk_panel_ext_create(dev, &ext_kd_boe_params,
						&ext_funcs, &info->base);
		break;
	case LCM_FT8205_TG_INX:
		pr_notice("[Kernel/LCM] ft8205 tg inx, pll_clock=%d.\n",
				ext_tg_inx_params.pll_clk);
		ret = mtk_panel_ext_create(dev, &ext_tg_inx_params,
						&ext_funcs, &info->base);
		break;
	default:
		pr_notice("[Kernel/LCM] It's default panel ft8205 kd boe,pll_clock=%d.\n",
				ext_kd_boe_params.pll_clk);
		ret = mtk_panel_ext_create(dev, &ext_kd_boe_params,
						&ext_funcs, &info->base);
		ret = -ENODEV;
	}
	if (ret < 0) {
		panel_resources_release(info);
		mipi_dsi_detach(dsi);
		drm_panel_remove(&info->base);
		devm_kfree(&dsi->dev,info);
		return ret;
	}
#endif
	share_info = info;
	old_state = LCM_ON;
	atomic_inc(&g_power_request);
	LCM_FUNC_EXIT();

	return ret;
}

static int panel_remove(struct mipi_dsi_device *dsi)
{
	struct panel_info *info = mipi_dsi_get_drvdata(dsi);
	int ret;

	LCM_FUNC_ENTER();
	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	ret = unregister_reboot_notifier(&ft8205_reboot_notifier);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to unregister reboot notifier: %d\n", ret);

	if (info->base.dev)
		drm_panel_remove(&info->base);
	LCM_FUNC_EXIT();

	return 0;
}

static const struct of_device_id panel_of_match[] = {
	{ .compatible = "panel,ft8205_wuxga_dsi_vdo",},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, panel_of_match);

static struct mipi_dsi_driver panel_driver = {
	.driver = {
		.name = "panel-ft8205_wuxga_dsi_vdo",
		.of_match_table = panel_of_match,
	},
	.probe = panel_probe,
	.remove = panel_remove,
};
module_mipi_dsi_driver(panel_driver);

MODULE_DESCRIPTION("KD ft8205_wuxga_dsi_vdo 1200x2000 video mode panel driver");
MODULE_LICENSE("GPL v2");
