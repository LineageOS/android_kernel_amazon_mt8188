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
#include "panel_common.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#define FRAME_WIDTH           (1200)
#define FRAME_HEIGHT          (2000)
/* KD BOE panel porch */
#define KD_BOE_HFP (40)    // params->dsi.horizontal_frontporch
#define KD_BOE_HSA (40)    // params->dsi.horizontal_sync_active
#define KD_BOE_HBP (48)    // params->dsi.horizontal_backporch
#define KD_BOE_VFP (62)    // params->dsi.vertical_frontporch
#define KD_BOE_VSA (4)     // params->dsi.vertical_sync_active
#define KD_BOE_VBP (36)    // params->dsi.vertical_backporch

#define TG_HSD_HFP (43)    // params->dsi.horizontal_frontporch
#define TG_HSD_HSA (40)    // params->dsi.horizontal_sync_active
#define TG_HSD_HBP (48)    // params->dsi.horizontal_backporch
#define TG_HSD_VFP (64)    // params->dsi.vertical_frontporch
#define TG_HSD_VSA (2)     // params->dsi.vertical_sync_active
#define TG_HSD_VBP (34)    // params->dsi.vertical_backporch

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

static const struct drm_display_mode tg_hsd_mode = {
	.clock = (FRAME_WIDTH + TG_HSD_HFP + TG_HSD_HSA + TG_HSD_HBP) *
		(FRAME_HEIGHT + TG_HSD_VFP + TG_HSD_VSA + TG_HSD_VBP) *
		60 / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + TG_HSD_HFP,
	.hsync_end = FRAME_WIDTH + TG_HSD_HFP + TG_HSD_HSA,
	.htotal = FRAME_WIDTH + TG_HSD_HFP + TG_HSD_HSA + TG_HSD_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + TG_HSD_VFP,
	.vsync_end = FRAME_HEIGHT + TG_HSD_VFP + TG_HSD_VSA,
	.vtotal = FRAME_HEIGHT + TG_HSD_VFP + TG_HSD_VSA + TG_HSD_VBP,
};

static atomic_t gesture_status = ATOMIC_INIT(0);
static atomic_t upgrade_status = ATOMIC_INIT(0);
static unsigned char vendor_id = 0xFF;
static int old_state = LCM_OFF;
static atomic_t g_power_request = ATOMIC_INIT(0);
static struct panel_info *share_info = NULL;

static atomic_t g_power_rebooting = ATOMIC_INIT(0);

static void get_lcm_id(void);
static bool panel_check_gesture_mode(void);
static void lcm_power_manager_lock(struct panel_info *panel, int pwr_state);
static void lcm_power_manager(struct panel_info *panel, int new_state);
#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif

static DEFINE_MUTEX(ili9882t_share_power_mutex);
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
 * LCM_OFF: fully powered off, disable all power supplies of the panel,include vrf18/avdd/avee
 * LCM_ON:  fully powered on, the state of the first boot to the kernel
 * LCM_ON2: execute the power on timing of LCM and TP reset, and send init code
 * LCM_ON1: enable vrf18, avdd and avee
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

static int ili9882t_reboot(struct notifier_block *notifier, unsigned long val, void *v)
{
	pr_notice("[KERNEL/LCM]: panel ili9882 rebooting\n");
	if (atomic_read(&g_power_rebooting) == 0)
		atomic_inc(&g_power_rebooting);
	return NOTIFY_OK;
}

static struct notifier_block ili9882t_reboot_notifier = {
	.notifier_call = ili9882t_reboot,
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
	pr_notice("[ili9882t] %s, vendor id = 0x%x\n", __func__, vendor_id);
}

static const struct panel_init_cmd kd_boe_init_cmd[] = {
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x01),
	_INIT_DCS_CMD(0x00, 0x46),
	_INIT_DCS_CMD(0x01, 0x15),
	_INIT_DCS_CMD(0x02, 0x00),
	_INIT_DCS_CMD(0x03, 0x00),
	_INIT_DCS_CMD(0x04, 0xCA),
	_INIT_DCS_CMD(0x05, 0x03),
	_INIT_DCS_CMD(0x06, 0x00),
	_INIT_DCS_CMD(0x07, 0x00),
	_INIT_DCS_CMD(0xED, 0x85),
	_INIT_DCS_CMD(0x08, 0x80),
	_INIT_DCS_CMD(0x09, 0x03),
	_INIT_DCS_CMD(0x0A, 0x71),
	_INIT_DCS_CMD(0x0B, 0x00),
	_INIT_DCS_CMD(0x0C, 0x00),
	_INIT_DCS_CMD(0x0D, 0x00),
	_INIT_DCS_CMD(0x0E, 0x14),
	_INIT_DCS_CMD(0x0F, 0x00),
	_INIT_DCS_CMD(0x24, 0x00),
	_INIT_DCS_CMD(0x25, 0x00),
	_INIT_DCS_CMD(0x26, 0x00),
	_INIT_DCS_CMD(0x27, 0x00),
	_INIT_DCS_CMD(0x2C, 0x35),
	_INIT_DCS_CMD(0x31, 0x2A),
	_INIT_DCS_CMD(0x32, 0x2A),
	_INIT_DCS_CMD(0x33, 0x28),
	_INIT_DCS_CMD(0x34, 0x28),
	_INIT_DCS_CMD(0x35, 0x29),
	_INIT_DCS_CMD(0x36, 0x29),
	_INIT_DCS_CMD(0x37, 0x0C),
	_INIT_DCS_CMD(0x38, 0x0C),
	_INIT_DCS_CMD(0x39, 0x16),
	_INIT_DCS_CMD(0x3A, 0x16),
	_INIT_DCS_CMD(0x3B, 0x14),
	_INIT_DCS_CMD(0x3C, 0x14),
	_INIT_DCS_CMD(0x3D, 0x12),
	_INIT_DCS_CMD(0x3E, 0x12),
	_INIT_DCS_CMD(0x3F, 0x10),
	_INIT_DCS_CMD(0x40, 0x10),
	_INIT_DCS_CMD(0x41, 0x08),
	_INIT_DCS_CMD(0x42, 0x08),
	_INIT_DCS_CMD(0x43, 0x07),
	_INIT_DCS_CMD(0x44, 0x07),
	_INIT_DCS_CMD(0x45, 0x07),
	_INIT_DCS_CMD(0x46, 0x07),
	_INIT_DCS_CMD(0x47, 0x2A),
	_INIT_DCS_CMD(0x48, 0x2A),
	_INIT_DCS_CMD(0x49, 0x28),
	_INIT_DCS_CMD(0x4A, 0x28),
	_INIT_DCS_CMD(0x4B, 0x29),
	_INIT_DCS_CMD(0x4C, 0x29),
	_INIT_DCS_CMD(0x4D, 0x0C),
	_INIT_DCS_CMD(0x4E, 0x0C),
	_INIT_DCS_CMD(0x4F, 0x17),
	_INIT_DCS_CMD(0x50, 0x17),
	_INIT_DCS_CMD(0x51, 0x15),
	_INIT_DCS_CMD(0x52, 0x15),
	_INIT_DCS_CMD(0x53, 0x13),
	_INIT_DCS_CMD(0x54, 0x13),
	_INIT_DCS_CMD(0x55, 0x11),
	_INIT_DCS_CMD(0x56, 0x11),
	_INIT_DCS_CMD(0x57, 0x09),
	_INIT_DCS_CMD(0x58, 0x09),
	_INIT_DCS_CMD(0x59, 0x07),
	_INIT_DCS_CMD(0x5A, 0x07),
	_INIT_DCS_CMD(0x5B, 0x07),
	_INIT_DCS_CMD(0x5C, 0x07),
	_INIT_DCS_CMD(0x61, 0x2A),
	_INIT_DCS_CMD(0x62, 0x2A),
	_INIT_DCS_CMD(0x63, 0x28),
	_INIT_DCS_CMD(0x64, 0x28),
	_INIT_DCS_CMD(0x65, 0x29),
	_INIT_DCS_CMD(0x66, 0x29),
	_INIT_DCS_CMD(0x67, 0x0C),
	_INIT_DCS_CMD(0x68, 0x0C),
	_INIT_DCS_CMD(0x69, 0x11),
	_INIT_DCS_CMD(0x6A, 0x11),
	_INIT_DCS_CMD(0x6B, 0x13),
	_INIT_DCS_CMD(0x6C, 0x13),
	_INIT_DCS_CMD(0x6D, 0x15),
	_INIT_DCS_CMD(0x6E, 0x15),
	_INIT_DCS_CMD(0x6F, 0x17),
	_INIT_DCS_CMD(0x70, 0x17),
	_INIT_DCS_CMD(0x71, 0x09),
	_INIT_DCS_CMD(0x72, 0x09),
	_INIT_DCS_CMD(0x73, 0x07),
	_INIT_DCS_CMD(0x74, 0x07),
	_INIT_DCS_CMD(0x75, 0x07),
	_INIT_DCS_CMD(0x76, 0x07),
	_INIT_DCS_CMD(0x77, 0x2A),
	_INIT_DCS_CMD(0x78, 0x2A),
	_INIT_DCS_CMD(0x79, 0x28),
	_INIT_DCS_CMD(0x7A, 0x28),
	_INIT_DCS_CMD(0x7B, 0x29),
	_INIT_DCS_CMD(0x7C, 0x29),
	_INIT_DCS_CMD(0x7D, 0x0C),
	_INIT_DCS_CMD(0x7E, 0x0C),
	_INIT_DCS_CMD(0x7F, 0x10),
	_INIT_DCS_CMD(0x80, 0x10),
	_INIT_DCS_CMD(0x81, 0x12),
	_INIT_DCS_CMD(0x82, 0x12),
	_INIT_DCS_CMD(0x83, 0x14),
	_INIT_DCS_CMD(0x84, 0x14),
	_INIT_DCS_CMD(0x85, 0x16),
	_INIT_DCS_CMD(0x86, 0x16),
	_INIT_DCS_CMD(0x87, 0x08),
	_INIT_DCS_CMD(0x88, 0x08),
	_INIT_DCS_CMD(0x89, 0x07),
	_INIT_DCS_CMD(0x8A, 0x07),
	_INIT_DCS_CMD(0x8B, 0x07),
	_INIT_DCS_CMD(0x8C, 0x07),
	_INIT_DCS_CMD(0xB0, 0x11),
	_INIT_DCS_CMD(0xD1, 0x22),
	_INIT_DCS_CMD(0xDF, 0x26),
	_INIT_DCS_CMD(0xE1, 0x40),
	_INIT_DCS_CMD(0xE2, 0x02),
	_INIT_DCS_CMD(0xF7, 0x55),
	_INIT_DCS_CMD(0xE3, 0x13),
	_INIT_DCS_CMD(0xE4, 0x42),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x02),
	_INIT_DCS_CMD(0x29, 0x3A),
	_INIT_DCS_CMD(0x2A, 0x3B),
	_INIT_DCS_CMD(0x06, 0xF4),
	_INIT_DCS_CMD(0x07, 0x00),
	_INIT_DCS_CMD(0x08, 0x26),
	_INIT_DCS_CMD(0x09, 0x3E),
	_INIT_DCS_CMD(0x3A, 0x26),
	_INIT_DCS_CMD(0x3B, 0x3E),
	_INIT_DCS_CMD(0x39, 0x01),
	_INIT_DCS_CMD(0x3C, 0xFD),
	_INIT_DCS_CMD(0x53, 0x1D),
	_INIT_DCS_CMD(0x5E, 0x40),
	_INIT_DCS_CMD(0x84, 0x00),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x03),
	_INIT_DCS_CMD(0x20, 0x01),
	_INIT_DCS_CMD(0x21, 0x3C),
	_INIT_DCS_CMD(0x22, 0xFA),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x12),
	_INIT_DCS_CMD(0x87, 0x2C),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x05),
	_INIT_DCS_CMD(0x73, 0xBD),
	_INIT_DCS_CMD(0x7F, 0xA7),
	_INIT_DCS_CMD(0x6D, 0x90),
	_INIT_DCS_CMD(0x79, 0x72),
	_INIT_DCS_CMD(0x69, 0x88),
	_INIT_DCS_CMD(0x6A, 0x88),
	_INIT_DCS_CMD(0xA5, 0x3F),
	_INIT_DCS_CMD(0x61, 0xEA),
	_INIT_DCS_CMD(0xA7, 0xF1),
	_INIT_DCS_CMD(0x5F, 0x01),
	_INIT_DCS_CMD(0x62, 0x3F),
	_INIT_DCS_CMD(0x67, 0x60),
	_INIT_DCS_CMD(0x9C, 0x02),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x06),
	_INIT_DCS_CMD(0xC0, 0xD0),
	_INIT_DCS_CMD(0xC1, 0x07),
	_INIT_DCS_CMD(0xCA, 0x58),
	_INIT_DCS_CMD(0xCB, 0x02),
	_INIT_DCS_CMD(0xCE, 0x58),
	_INIT_DCS_CMD(0xCF, 0x02),
	_INIT_DCS_CMD(0x10, 0x00),
	_INIT_DCS_CMD(0xD3, 0x08),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x08),
	_INIT_DCS_CMD(0xE0, 0x00, 0x09, 0x1D, 0x3C, 0x4D, 0x41, 0x55, 0x59, 0x65, 0x65, 0x83, 0x8D, 0x94, 0xAB, 0xA6, 0xAE, 0xB6, 0xC9, 0xC2, 0xBA, 0xD3, 0xE4, 0xE9),
	_INIT_DCS_CMD(0xE1, 0x00, 0x09, 0x1D, 0x3C, 0x4D, 0x41, 0x55, 0x59, 0x65, 0x65, 0x83, 0x8D, 0x94, 0xAB, 0xA6, 0xAE, 0xB6, 0xC9, 0xC2, 0xBA, 0xD3, 0xE4, 0xE9),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x15),
	_INIT_DCS_CMD(0x31, 0xBA),
	_INIT_DCS_CMD(0x33, 0xBA),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x04),
	_INIT_DCS_CMD(0xBA, 0x81),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x0C),
	_INIT_DCS_CMD(0x00, 0x12),
	_INIT_DCS_CMD(0x01, 0x00),
	_INIT_DCS_CMD(0x02, 0x12),
	_INIT_DCS_CMD(0x03, 0x01),
	_INIT_DCS_CMD(0x04, 0x12),
	_INIT_DCS_CMD(0x05, 0x02),
	_INIT_DCS_CMD(0x06, 0x12),
	_INIT_DCS_CMD(0x07, 0x03),
	_INIT_DCS_CMD(0x08, 0x12),
	_INIT_DCS_CMD(0x09, 0x04),
	_INIT_DCS_CMD(0x0A, 0x12),
	_INIT_DCS_CMD(0x0B, 0x05),
	_INIT_DCS_CMD(0x0C, 0x12),
	_INIT_DCS_CMD(0x0D, 0x06),
	_INIT_DCS_CMD(0x0E, 0x12),
	_INIT_DCS_CMD(0x0F, 0x07),
	_INIT_DCS_CMD(0x10, 0x12),
	_INIT_DCS_CMD(0x11, 0x08),
	_INIT_DCS_CMD(0x12, 0x12),
	_INIT_DCS_CMD(0x13, 0x09),
	_INIT_DCS_CMD(0x14, 0x12),
	_INIT_DCS_CMD(0x15, 0x0A),
	_INIT_DCS_CMD(0x16, 0x12),
	_INIT_DCS_CMD(0x17, 0x0B),
	_INIT_DCS_CMD(0x18, 0x12),
	_INIT_DCS_CMD(0x19, 0x0C),
	_INIT_DCS_CMD(0x1A, 0x12),
	_INIT_DCS_CMD(0x1B, 0x0D),
	_INIT_DCS_CMD(0x1C, 0x12),
	_INIT_DCS_CMD(0x1D, 0x0E),
	_INIT_DCS_CMD(0x1E, 0x12),
	_INIT_DCS_CMD(0x1F, 0x0F),
	_INIT_DCS_CMD(0x20, 0x12),
	_INIT_DCS_CMD(0x21, 0x10),
	_INIT_DCS_CMD(0x22, 0x12),
	_INIT_DCS_CMD(0x23, 0x11),
	_INIT_DCS_CMD(0x24, 0x12),
	_INIT_DCS_CMD(0x25, 0x12),
	_INIT_DCS_CMD(0x26, 0x12),
	_INIT_DCS_CMD(0x27, 0x13),
	_INIT_DCS_CMD(0x28, 0x12),
	_INIT_DCS_CMD(0x29, 0x14),
	_INIT_DCS_CMD(0x2A, 0x12),
	_INIT_DCS_CMD(0x2B, 0x15),
	_INIT_DCS_CMD(0x2C, 0x12),
	_INIT_DCS_CMD(0x2D, 0x16),
	_INIT_DCS_CMD(0x2E, 0x12),
	_INIT_DCS_CMD(0x2F, 0x17),
	_INIT_DCS_CMD(0x30, 0x12),
	_INIT_DCS_CMD(0x31, 0x18),
	_INIT_DCS_CMD(0x32, 0x12),
	_INIT_DCS_CMD(0x33, 0x19),
	_INIT_DCS_CMD(0x34, 0x12),
	_INIT_DCS_CMD(0x35, 0x1A),
	_INIT_DCS_CMD(0x36, 0x12),
	_INIT_DCS_CMD(0x37, 0x1B),
	_INIT_DCS_CMD(0x38, 0x12),
	_INIT_DCS_CMD(0x39, 0x1C),
	_INIT_DCS_CMD(0x3A, 0x12),
	_INIT_DCS_CMD(0x3B, 0x1D),
	_INIT_DCS_CMD(0x3C, 0x12),
	_INIT_DCS_CMD(0x3D, 0x1E),
	_INIT_DCS_CMD(0x3E, 0x12),
	_INIT_DCS_CMD(0x3F, 0x1F),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x04),
	_INIT_DCS_CMD(0xBA, 0x01),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x0E),
	_INIT_DCS_CMD(0x02, 0x10),
	_INIT_DCS_CMD(0x20, 0x0E),
	_INIT_DCS_CMD(0x25, 0x14),
	_INIT_DCS_CMD(0x26, 0xA0),
	_INIT_DCS_CMD(0x27, 0x00),
	_INIT_DCS_CMD(0x29, 0x79),
	_INIT_DCS_CMD(0x2A, 0x90),
	_INIT_DCS_CMD(0xB0, 0x01),
	_INIT_DCS_CMD(0xB1, 0xC0),
	_INIT_DCS_CMD(0xC0, 0x14),
	_INIT_DCS_CMD(0xC2, 0xC0),
	_INIT_DCS_CMD(0xC3, 0xC0),
	_INIT_DCS_CMD(0xC4, 0xC0),
	_INIT_DCS_CMD(0xC5, 0xC0),
	_INIT_DCS_CMD(0xC6, 0xC0),
	_INIT_DCS_CMD(0xC7, 0xC0),
	_INIT_DCS_CMD(0xC8, 0xC0),
	_INIT_DCS_CMD(0xC9, 0xC0),
	_INIT_DCS_CMD(0x2D, 0xC6),
	_INIT_DCS_CMD(0x30, 0x00),
	_INIT_DCS_CMD(0x00, 0x81),
	_INIT_DCS_CMD(0x08, 0x14),
	_INIT_DCS_CMD(0x09, 0x00),
	_INIT_DCS_CMD(0x07, 0x21),
	_INIT_DCS_CMD(0x2B, 0x1F),
	_INIT_DCS_CMD(0x33, 0x98),
	_INIT_DCS_CMD(0x34, 0x60),
	_INIT_DCS_CMD(0x35, 0x62),
	_INIT_DCS_CMD(0x36, 0x40),
	_INIT_DCS_CMD(0x80, 0x3E),
	_INIT_DCS_CMD(0x81, 0xA0),
	_INIT_DCS_CMD(0x38, 0x74),
	_INIT_DCS_CMD(0x04, 0x10),
	_INIT_DCS_CMD(0x31, 0x06),
	_INIT_DCS_CMD(0x32, 0x9F),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x1E),
	_INIT_DCS_CMD(0x60, 0x00),
	_INIT_DCS_CMD(0x64, 0x00),
	_INIT_DCS_CMD(0x6D, 0x00),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x0B),
	_INIT_DCS_CMD(0xA6, 0x87),
	_INIT_DCS_CMD(0xA7, 0xF0),
	_INIT_DCS_CMD(0xA8, 0x06),
	_INIT_DCS_CMD(0xA9, 0x06),
	_INIT_DCS_CMD(0xAA, 0x88),
	_INIT_DCS_CMD(0xAB, 0x88),
	_INIT_DCS_CMD(0xAC, 0x07),
	_INIT_DCS_CMD(0xBD, 0xA2),
	_INIT_DCS_CMD(0xBE, 0xA1),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x00),
	_INIT_DCS_CMD(0x35, 0x00),

	_INIT_DCS_CMD(0x11),
	_INIT_DELAY_CMD(70),
	_INIT_DCS_CMD(0x29),
	_INIT_DELAY_CMD(5),
	{},
};

static const struct panel_init_cmd tg_hsd_init_cmd[] = {
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x01),
	_INIT_DCS_CMD(0x00, 0x42),
	_INIT_DCS_CMD(0x01, 0x11),
	_INIT_DCS_CMD(0x02, 0x00),
	_INIT_DCS_CMD(0x03, 0x00),
	_INIT_DCS_CMD(0x04, 0x01),
	_INIT_DCS_CMD(0x05, 0x11),
	_INIT_DCS_CMD(0x06, 0x00),
	_INIT_DCS_CMD(0x07, 0x00),
	_INIT_DCS_CMD(0x08, 0x80),
	_INIT_DCS_CMD(0x09, 0x81),
	_INIT_DCS_CMD(0x0A, 0x71),
	_INIT_DCS_CMD(0x0B, 0x00),
	_INIT_DCS_CMD(0x0C, 0x00),
	_INIT_DCS_CMD(0x0E, 0x08),
	_INIT_DCS_CMD(0x24, 0x00),
	_INIT_DCS_CMD(0x25, 0x00),
	_INIT_DCS_CMD(0x26, 0x00),
	_INIT_DCS_CMD(0x27, 0x00),
	_INIT_DCS_CMD(0x2C, 0xD4),
	_INIT_DCS_CMD(0xB9, 0x40),
	_INIT_DCS_CMD(0xB0, 0x11),
	_INIT_DCS_CMD(0xE6, 0x32),
	_INIT_DCS_CMD(0xD1, 0x30),
	_INIT_DCS_CMD(0xD6, 0x55),
	_INIT_DCS_CMD(0xD0, 0x01),
	_INIT_DCS_CMD(0xE3, 0x93),
	_INIT_DCS_CMD(0xE4, 0x00),
	_INIT_DCS_CMD(0xE5, 0x80),
	_INIT_DCS_CMD(0x31, 0x07),
	_INIT_DCS_CMD(0x32, 0x07),
	_INIT_DCS_CMD(0x33, 0x07),
	_INIT_DCS_CMD(0x34, 0x07),
	_INIT_DCS_CMD(0x35, 0x07),
	_INIT_DCS_CMD(0x36, 0x01),
	_INIT_DCS_CMD(0x37, 0x00),
	_INIT_DCS_CMD(0x38, 0x28),
	_INIT_DCS_CMD(0x39, 0x29),
	_INIT_DCS_CMD(0x3A, 0x11),
	_INIT_DCS_CMD(0x3B, 0x13),
	_INIT_DCS_CMD(0x3C, 0x15),
	_INIT_DCS_CMD(0x3D, 0x17),
	_INIT_DCS_CMD(0x3E, 0x09),
	_INIT_DCS_CMD(0x3F, 0x0D),
	_INIT_DCS_CMD(0x40, 0x02),
	_INIT_DCS_CMD(0x41, 0x02),
	_INIT_DCS_CMD(0x42, 0x02),
	_INIT_DCS_CMD(0x43, 0x02),
	_INIT_DCS_CMD(0x44, 0x02),
	_INIT_DCS_CMD(0x45, 0x02),
	_INIT_DCS_CMD(0x46, 0x02),
	_INIT_DCS_CMD(0x47, 0x07),
	_INIT_DCS_CMD(0x48, 0x07),
	_INIT_DCS_CMD(0x49, 0x07),
	_INIT_DCS_CMD(0x4A, 0x07),
	_INIT_DCS_CMD(0x4B, 0x07),
	_INIT_DCS_CMD(0x4C, 0x01),
	_INIT_DCS_CMD(0x4D, 0x00),
	_INIT_DCS_CMD(0x4E, 0x28),
	_INIT_DCS_CMD(0x4F, 0x29),
	_INIT_DCS_CMD(0x50, 0x10),
	_INIT_DCS_CMD(0x51, 0x12),
	_INIT_DCS_CMD(0x52, 0x14),
	_INIT_DCS_CMD(0x53, 0x16),
	_INIT_DCS_CMD(0x54, 0x08),
	_INIT_DCS_CMD(0x55, 0x0C),
	_INIT_DCS_CMD(0x56, 0x02),
	_INIT_DCS_CMD(0x57, 0x02),
	_INIT_DCS_CMD(0x58, 0x02),
	_INIT_DCS_CMD(0x59, 0x02),
	_INIT_DCS_CMD(0x5A, 0x02),
	_INIT_DCS_CMD(0x5B, 0x02),
	_INIT_DCS_CMD(0x5C, 0x02),
	_INIT_DCS_CMD(0x61, 0x07),
	_INIT_DCS_CMD(0x62, 0x07),
	_INIT_DCS_CMD(0x63, 0x07),
	_INIT_DCS_CMD(0x64, 0x07),
	_INIT_DCS_CMD(0x65, 0x07),
	_INIT_DCS_CMD(0x66, 0x01),
	_INIT_DCS_CMD(0x67, 0x00),
	_INIT_DCS_CMD(0x68, 0x28),
	_INIT_DCS_CMD(0x69, 0x29),
	_INIT_DCS_CMD(0x6A, 0x16),
	_INIT_DCS_CMD(0x6B, 0x14),
	_INIT_DCS_CMD(0x6C, 0x12),
	_INIT_DCS_CMD(0x6D, 0x10),
	_INIT_DCS_CMD(0x6E, 0x0C),
	_INIT_DCS_CMD(0x6F, 0x08),
	_INIT_DCS_CMD(0x70, 0x02),
	_INIT_DCS_CMD(0x71, 0x02),
	_INIT_DCS_CMD(0x72, 0x02),
	_INIT_DCS_CMD(0x73, 0x02),
	_INIT_DCS_CMD(0x74, 0x02),
	_INIT_DCS_CMD(0x75, 0x02),
	_INIT_DCS_CMD(0x76, 0x02),
	_INIT_DCS_CMD(0x77, 0x07),
	_INIT_DCS_CMD(0x78, 0x07),
	_INIT_DCS_CMD(0x79, 0x07),
	_INIT_DCS_CMD(0x7A, 0x07),
	_INIT_DCS_CMD(0x7B, 0x07),
	_INIT_DCS_CMD(0x7C, 0x01),
	_INIT_DCS_CMD(0x7D, 0x00),
	_INIT_DCS_CMD(0x7E, 0x28),
	_INIT_DCS_CMD(0x7F, 0x29),
	_INIT_DCS_CMD(0x80, 0x17),
	_INIT_DCS_CMD(0x81, 0x15),
	_INIT_DCS_CMD(0x82, 0x13),
	_INIT_DCS_CMD(0x83, 0x11),
	_INIT_DCS_CMD(0x84, 0x0D),
	_INIT_DCS_CMD(0x85, 0x09),
	_INIT_DCS_CMD(0x86, 0x02),
	_INIT_DCS_CMD(0x87, 0x07),
	_INIT_DCS_CMD(0x88, 0x07),
	_INIT_DCS_CMD(0x89, 0x07),
	_INIT_DCS_CMD(0x8A, 0x07),
	_INIT_DCS_CMD(0x8B, 0x07),
	_INIT_DCS_CMD(0x8C, 0x07),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x02),
	_INIT_DCS_CMD(0x29, 0x3A),
	_INIT_DCS_CMD(0x2A, 0x3B),
	_INIT_DCS_CMD(0x06, 0xF4),
	_INIT_DCS_CMD(0x07, 0x00),
	_INIT_DCS_CMD(0x08, 0x24),
	_INIT_DCS_CMD(0x09, 0x40),
	_INIT_DCS_CMD(0x3A, 0x24),
	_INIT_DCS_CMD(0x3B, 0x40),
	_INIT_DCS_CMD(0x39, 0x01),
	_INIT_DCS_CMD(0x3C, 0xFD),
	_INIT_DCS_CMD(0x53, 0x1F),
	_INIT_DCS_CMD(0x5E, 0x40),
	_INIT_DCS_CMD(0x84, 0x00),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x03),
	_INIT_DCS_CMD(0x20, 0x01),
	_INIT_DCS_CMD(0x21, 0x3C),
	_INIT_DCS_CMD(0x22, 0xFA),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x12),
	_INIT_DCS_CMD(0x87, 0x2C),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x05),
	_INIT_DCS_CMD(0x73, 0xC7),
	_INIT_DCS_CMD(0x7F, 0x6B),
	_INIT_DCS_CMD(0x6D, 0x95),
	_INIT_DCS_CMD(0x79, 0x54),
	_INIT_DCS_CMD(0x69, 0x97),
	_INIT_DCS_CMD(0x6A, 0x97),
	_INIT_DCS_CMD(0xA5, 0x3F),
	_INIT_DCS_CMD(0x61, 0xDA),
	_INIT_DCS_CMD(0xA7, 0xF1),
	_INIT_DCS_CMD(0x5F, 0x01),
	_INIT_DCS_CMD(0x62, 0x3F),
	_INIT_DCS_CMD(0x1D, 0x90),
	_INIT_DCS_CMD(0x86, 0x87),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x06),
	_INIT_DCS_CMD(0xC0, 0xD0),
	_INIT_DCS_CMD(0xC1, 0x07),
	_INIT_DCS_CMD(0xCA, 0x58),
	_INIT_DCS_CMD(0xCB, 0x02),
	_INIT_DCS_CMD(0xCE, 0x58),
	_INIT_DCS_CMD(0xCF, 0x02),
	_INIT_DCS_CMD(0x92, 0x22),
	_INIT_DCS_CMD(0x67, 0x60),
	_INIT_DCS_CMD(0x10, 0x07),
	_INIT_DCS_CMD(0xD6, 0x55),
	_INIT_DCS_CMD(0xDC, 0x38),
	_INIT_DCS_CMD(0xD3, 0x08),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x08),
	_INIT_DCS_CMD(0xE0, 0x00, 0x0E, 0x28, 0x4C, 0x60, 0x56, 0x6A, 0x6F, 0x7A, 0x79, 0x92, 0x98, 0x9C, 0xB1, 0xAD, 0xB5, 0xBE, 0xD0, 0xC8, 0xBF, 0xD6, 0xE5, 0xE9),
	_INIT_DCS_CMD(0xE1, 0x00, 0x0E, 0x28, 0x4C, 0x60, 0x56, 0x6A, 0x6F, 0x7A, 0x79, 0x92, 0x98, 0x9C, 0xB1, 0xAD, 0xB5, 0xBE, 0xD0, 0xC8, 0xBF, 0xD6, 0xE5, 0xE9),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x15),
	_INIT_DCS_CMD(0x31, 0xA0),
	_INIT_DCS_CMD(0x33, 0xA0),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x04),
	_INIT_DCS_CMD(0xBA, 0x81),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x0C),
	_INIT_DCS_CMD(0x00, 0x14),
	_INIT_DCS_CMD(0x01, 0x00),
	_INIT_DCS_CMD(0x02, 0x14),
	_INIT_DCS_CMD(0x03, 0x01),
	_INIT_DCS_CMD(0x04, 0x14),
	_INIT_DCS_CMD(0x05, 0x02),
	_INIT_DCS_CMD(0x06, 0x14),
	_INIT_DCS_CMD(0x07, 0x03),
	_INIT_DCS_CMD(0x08, 0x14),
	_INIT_DCS_CMD(0x09, 0x04),
	_INIT_DCS_CMD(0x0A, 0x14),
	_INIT_DCS_CMD(0x0B, 0x05),
	_INIT_DCS_CMD(0x0C, 0x14),
	_INIT_DCS_CMD(0x0D, 0x06),
	_INIT_DCS_CMD(0x0E, 0x14),
	_INIT_DCS_CMD(0x0F, 0x07),
	_INIT_DCS_CMD(0x10, 0x14),
	_INIT_DCS_CMD(0x11, 0x08),
	_INIT_DCS_CMD(0x12, 0x14),
	_INIT_DCS_CMD(0x13, 0x09),
	_INIT_DCS_CMD(0x14, 0x14),
	_INIT_DCS_CMD(0x15, 0x0A),
	_INIT_DCS_CMD(0x16, 0x14),
	_INIT_DCS_CMD(0x17, 0x0B),
	_INIT_DCS_CMD(0x18, 0x14),
	_INIT_DCS_CMD(0x19, 0x0C),
	_INIT_DCS_CMD(0x1A, 0x14),
	_INIT_DCS_CMD(0x1B, 0x0D),
	_INIT_DCS_CMD(0x1C, 0x14),
	_INIT_DCS_CMD(0x1D, 0x0E),
	_INIT_DCS_CMD(0x1E, 0x14),
	_INIT_DCS_CMD(0x1F, 0x0F),
	_INIT_DCS_CMD(0x20, 0x14),
	_INIT_DCS_CMD(0x21, 0x10),
	_INIT_DCS_CMD(0x22, 0x14),
	_INIT_DCS_CMD(0x23, 0x11),
	_INIT_DCS_CMD(0x24, 0x14),
	_INIT_DCS_CMD(0x25, 0x12),
	_INIT_DCS_CMD(0x26, 0x14),
	_INIT_DCS_CMD(0x27, 0x13),
	_INIT_DCS_CMD(0x28, 0x14),
	_INIT_DCS_CMD(0x29, 0x14),
	_INIT_DCS_CMD(0x2A, 0x14),
	_INIT_DCS_CMD(0x2B, 0x15),
	_INIT_DCS_CMD(0x2C, 0x14),
	_INIT_DCS_CMD(0x2D, 0x16),
	_INIT_DCS_CMD(0x2E, 0x14),
	_INIT_DCS_CMD(0x2F, 0x17),
	_INIT_DCS_CMD(0x30, 0x14),
	_INIT_DCS_CMD(0x31, 0x18),
	_INIT_DCS_CMD(0x32, 0x14),
	_INIT_DCS_CMD(0x33, 0x19),
	_INIT_DCS_CMD(0x34, 0x13),
	_INIT_DCS_CMD(0x35, 0x1A),
	_INIT_DCS_CMD(0x36, 0x14),
	_INIT_DCS_CMD(0x37, 0x1B),
	_INIT_DCS_CMD(0x38, 0x14),
	_INIT_DCS_CMD(0x39, 0x1C),
	_INIT_DCS_CMD(0x3A, 0x14),
	_INIT_DCS_CMD(0x3B, 0x1D),
	_INIT_DCS_CMD(0x3C, 0x14),
	_INIT_DCS_CMD(0x3D, 0x1E),
	_INIT_DCS_CMD(0x3E, 0x14),
	_INIT_DCS_CMD(0x3F, 0x1F),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x04),
	_INIT_DCS_CMD(0xBA, 0x01),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x0E),
	_INIT_DCS_CMD(0x02, 0x10),
	_INIT_DCS_CMD(0x20, 0x0E),
	_INIT_DCS_CMD(0x25, 0x16),
	_INIT_DCS_CMD(0x26, 0x40),
	_INIT_DCS_CMD(0x27, 0x00),
	_INIT_DCS_CMD(0x29, 0x79),
	_INIT_DCS_CMD(0x2A, 0x93),
	_INIT_DCS_CMD(0xB0, 0x01),
	_INIT_DCS_CMD(0xB1, 0xC0),
	_INIT_DCS_CMD(0xC0, 0x14),
	_INIT_DCS_CMD(0xC2, 0xC0),
	_INIT_DCS_CMD(0xC3, 0xC0),
	_INIT_DCS_CMD(0xC4, 0xC0),
	_INIT_DCS_CMD(0xC5, 0xC0),
	_INIT_DCS_CMD(0xC6, 0xC0),
	_INIT_DCS_CMD(0xC7, 0xC0),
	_INIT_DCS_CMD(0xC8, 0xC0),
	_INIT_DCS_CMD(0xC9, 0xC0),
	_INIT_DCS_CMD(0x2D, 0xC3),
	_INIT_DCS_CMD(0x30, 0x00),
	_INIT_DCS_CMD(0x00, 0x81),
	_INIT_DCS_CMD(0x08, 0x14),
	_INIT_DCS_CMD(0x09, 0x00),
	_INIT_DCS_CMD(0x07, 0x21),
	_INIT_DCS_CMD(0x2B, 0x1F),
	_INIT_DCS_CMD(0x33, 0x98),
	_INIT_DCS_CMD(0x34, 0x60),
	_INIT_DCS_CMD(0x35, 0x60),
	_INIT_DCS_CMD(0x36, 0xA0),
	_INIT_DCS_CMD(0x80, 0x3E),
	_INIT_DCS_CMD(0x81, 0xA0),
	_INIT_DCS_CMD(0x38, 0x73),
	_INIT_DCS_CMD(0x04, 0x10),
	_INIT_DCS_CMD(0x31, 0x06),
	_INIT_DCS_CMD(0x32, 0x9F),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x1E),
	_INIT_DCS_CMD(0x60, 0x00),
	_INIT_DCS_CMD(0x64, 0x00),
	_INIT_DCS_CMD(0x6D, 0x00),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x0B),
	_INIT_DCS_CMD(0xA6, 0x87),
	_INIT_DCS_CMD(0xA7, 0xF0),
	_INIT_DCS_CMD(0xA8, 0x06),
	_INIT_DCS_CMD(0xA9, 0x06),
	_INIT_DCS_CMD(0xAA, 0x88),
	_INIT_DCS_CMD(0xAB, 0x88),
	_INIT_DCS_CMD(0xAC, 0x07),
	_INIT_DCS_CMD(0xBD, 0xA2),
	_INIT_DCS_CMD(0xBE, 0xA1),
	_INIT_DCS_CMD(0xFF, 0x98, 0x82, 0x00),
	_INIT_DCS_CMD(0x35, 0x00),

	_INIT_DCS_CMD(0x11),
	_INIT_DELAY_CMD(70),
	_INIT_DCS_CMD(0x29),
	_INIT_DELAY_CMD(5),
	{},
};

static const struct panel_init_cmd gesture_mode_sleep_cmd[] = {
	_INIT_DCS_CMD(0xFF,0x98,0x82,0x06),
	_INIT_DCS_CMD(0xD0,0x0A),
	_INIT_DCS_CMD(0xD1,0x02),
	_INIT_DCS_CMD(0x03,0xF0),
	_INIT_DELAY_CMD(1),
	_INIT_DCS_CMD(0xD1,0x00),
	_INIT_DCS_CMD(0xFF,0x98,0x82,0x00),
	{},
};

static inline struct panel_info *to_info_panel(struct drm_panel *panel)
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
				break;
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
	int ret = 0xFF;

	LCM_FUNC_ENTER();
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		return ret;

	msleep(40);
	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		return ret;
	msleep(120);
	if (true == panel_check_gesture_mode() && atomic_read(&g_power_rebooting) == 0) {
		ret = panel_init_dcs_cmd(info, gesture_mode_sleep_cmd);
		if (ret < 0) {
			dev_err(info->dev, "panel failed to enter sleep : %d\n", ret);
			return ret;
		}
	}
	LCM_FUNC_EXIT();

	return 0;
}

void panel_gesture_mode_set_by_ili9882t(bool gst_mode)
{
	atomic_set(&gesture_status, gst_mode);
}
EXPORT_SYMBOL_GPL(panel_gesture_mode_set_by_ili9882t);

static bool panel_check_gesture_mode(void)
{
	bool ret = false;

	LCM_FUNC_ENTER();
	ret = atomic_read(&gesture_status);
	LCM_FUNC_EXIT();

	return ret;
}

void panel_fw_upgrade_mode_set_by_ilitek(bool upg_mode)
{
	atomic_set(&upgrade_status, upg_mode);
}
EXPORT_SYMBOL_GPL(panel_fw_upgrade_mode_set_by_ilitek);

static bool panel_check_fw_upgrade_mode(void)
{
	bool ret = false;

	LCM_FUNC_ENTER();
	ret = atomic_read(&upgrade_status);
	LCM_FUNC_EXIT();

	return ret;
}

void ili9882t_share_powerpin_on(void)
{
	mutex_lock(&ili9882t_share_power_mutex);
	/*LCM resume power with ON1->ON2, touch resume with ON.*/
	if (atomic_read(&g_power_request) == 0)
		LCM_ERROR("%s Touch poweron should NOT before LCM poweron", __func__);
	lcm_power_manager(share_info, LCM_ON);
	mutex_unlock(&ili9882t_share_power_mutex);
}
EXPORT_SYMBOL(ili9882t_share_powerpin_on);

void ili9882t_share_powerpin_off(void)
{
	mutex_lock(&ili9882t_share_power_mutex);
	lcm_power_manager(share_info, LCM_OFF);
	mutex_unlock(&ili9882t_share_power_mutex);
}
EXPORT_SYMBOL(ili9882t_share_powerpin_off);

void ili9882t_share_tp_resetpin_control(int value)
{
	LCM_FUNC_ENTER();
	mutex_lock(&ili9882t_share_power_mutex);
	if (atomic_read(&g_power_request) <= 0) {
		LCM_ERROR("%s skip, bceause res_cnt(%d) <= 0",
				__func__, atomic_read(&g_power_request));
	} else {
		gpiod_set_value(share_info->ctp_rst_gpio, value);
	}
	mutex_unlock(&ili9882t_share_power_mutex);
	LCM_FUNC_EXIT();
}
EXPORT_SYMBOL(ili9882t_share_tp_resetpin_control);

static int panel_unprepare_power(struct drm_panel *panel)
{
	struct panel_info *info = to_info_panel(panel);

	LCM_FUNC_ENTER();
	if (!info->prepared_power)
		return 0;

	if ((false == panel_check_gesture_mode() ||
		atomic_read(&g_power_rebooting) != 0) && (false == panel_check_fw_upgrade_mode())) {
		lcm_power_manager_lock(info, LCM_OFF);
		info->prepared_power = false;
	}
	LCM_FUNC_EXIT();

	return 0;
}

static int panel_unprepare(struct drm_panel *panel)
{
	struct panel_info *info = to_info_panel(panel);

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
	struct panel_info *info = to_info_panel(panel);

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
	struct panel_info *info = to_info_panel(panel);

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
	struct panel_info *info = to_info_panel(panel);
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
	struct panel_info *info = to_info_panel(panel);
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

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct panel_info *info = to_info_panel(panel);

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
		if ((false == panel_check_gesture_mode() ||
			atomic_read(&g_power_rebooting) != 0) && (false == panel_check_fw_upgrade_mode())) {
			msleep(2);
			gpiod_set_value(info->enable_gpio, 0);
			gpiod_set_value(info->ctp_rst_gpio, 0);
		}
		break;
	case TRS_DSB_TO_ON:
		//gesture mode resume
		gpiod_set_value(info->enable_gpio, 0);
		usleep_range(1000, 1001);
		gpiod_set_value(info->enable_gpio, 1);
		usleep_range(1000, 1001);
		gpiod_set_value(info->ctp_rst_gpio, 0);
		usleep_range(1000, 1001);
		gpiod_set_value(info->ctp_rst_gpio, 1);
		usleep_range(10000, 15001);
		switch (vendor_id) {
		case LCM_ILI9882T_KD_BOE:
			ret = panel_init_dcs_cmd(info, kd_boe_init_cmd);
			break;
		case LCM_ILI9882T_TG_HSD:
			ret = panel_init_dcs_cmd(info, tg_hsd_init_cmd);
			break;
		default:
			pr_notice("Not match correct ID, loading default init code...\n");
			ret = panel_init_dcs_cmd(info, kd_boe_init_cmd);
			break;
		}
		if (ret < 0) {
			dev_err(info->dev, "failed to init panel: %d\n", ret);
			return ret;
		}
		break;
	case TRS_DSB_TO_OFF:
		//normal suspend
		usleep_range(1000, 2001);
		gpiod_set_value(info->avee, 0);
		usleep_range(1000, 2001);
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
			usleep_range(2000, 6000);
		}

		bias_on(info);
		usleep_range(4000, 12001);
		break;
	case TRS_ON_PHASE2:
		//normal resume phase2
		gpiod_set_value(info->enable_gpio, 1);
		gpiod_set_value(info->ctp_rst_gpio, 1);
		usleep_range(10000, 15001);
		switch (vendor_id) {
		case LCM_ILI9882T_KD_BOE:
			ret = panel_init_dcs_cmd(info, kd_boe_init_cmd);
			break;
		case LCM_ILI9882T_TG_HSD:
			ret = panel_init_dcs_cmd(info, tg_hsd_init_cmd);
			break;
		default:
			pr_notice("Not match correct ID, loading default init code...\n");
			ret = panel_init_dcs_cmd(info, kd_boe_init_cmd);
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
			usleep_range(2000, 6000);
		}

		bias_on(info);
		usleep_range(4000, 12001);

		//normal resume phase2
		gpiod_set_value(info->enable_gpio, 1);
		gpiod_set_value(info->ctp_rst_gpio, 1);
		usleep_range(10000, 15001);
		switch (vendor_id) {
		case LCM_ILI9882T_KD_BOE:
			ret = panel_init_dcs_cmd(info, kd_boe_init_cmd);
			break;
		case LCM_ILI9882T_TG_HSD:
			ret = panel_init_dcs_cmd(info, tg_hsd_init_cmd);
			break;
		default:
			pr_notice("Not match correct ID, loading default init code...\n");
			ret = panel_init_dcs_cmd(info, kd_boe_init_cmd);
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
		usleep_range(1000, 2001);
		gpiod_set_value(info->avee, 0);
		usleep_range(1000, 2001);
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
	mutex_lock(&ili9882t_share_power_mutex);
	lcm_power_manager(info, pwr_state);
	mutex_unlock(&ili9882t_share_power_mutex);
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

static struct mtk_panel_params ext_ili9882t_kd_boe_params = {
	.pll_clk = 533,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
};

static struct mtk_panel_params ext_ili9882t_tg_hsd_params = {
	.pll_clk = 533,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ata_check = panel_ata_check,
};
#endif

static int panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct panel_info *info = to_info_panel(panel);
	struct drm_display_mode *mode;

	LCM_FUNC_ENTER();
	switch (vendor_id) {
	case LCM_ILI9882T_KD_BOE:
		mode = drm_mode_duplicate(connector->dev, &kd_boe_mode);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%ux@%u\n",
				kd_boe_mode.hdisplay, kd_boe_mode.vdisplay,
				drm_mode_vrefresh(&kd_boe_mode));
			return -ENOMEM;
		}
		break;
	case LCM_ILI9882T_TG_HSD:
		mode = drm_mode_duplicate(connector->dev, &tg_hsd_mode);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%ux@%u\n",
				tg_hsd_mode.hdisplay, tg_hsd_mode.vdisplay,
				drm_mode_vrefresh(&tg_hsd_mode));
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
	int ret = 0xFF;

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

	ret = register_reboot_notifier(&ili9882t_reboot_notifier);
	if (ret < 0) {
		dev_err(dev, "Failed to register ili9882t reboot notifier %d\n", ret);
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
	int ret = 0xFF;

	LCM_FUNC_ENTER();

	if (vendor_id == 0xFF)
		get_lcm_id();

	switch (vendor_id) {
	case LCM_ILI9882T_KD_BOE:
		pr_notice("[Kernel/LCM] ili9882t kd boe panel,vendor_id = %d loading.\n",
				vendor_id);
		break;
	case LCM_ILI9882T_TG_HSD:
		pr_notice("[Kernel/LCM] ili9882t tg hsd panel,vendor_id = %d loading.\n",
				vendor_id);
		break;
	default:
		pr_notice("[Kernel/LCM] It's not ili9882t panel, exit.\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&dsi->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM;
	info->dsi = dsi;
	ret = panel_add(info);
	if (ret < 0) {
		panel_resources_release(info);
		devm_kfree(&dsi->dev, info);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, info);

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		panel_resources_release(info);
		drm_panel_remove(&info->base);
		devm_kfree(&dsi->dev, info);
		return ret;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&info->base);
	switch (vendor_id) {
	case LCM_ILI9882T_KD_BOE:
		pr_notice("[Kernel/LCM] ili9882t kd boe, pll_clock=%d.\n",
				ext_ili9882t_kd_boe_params.pll_clk);
		ret = mtk_panel_ext_create(dev, &ext_ili9882t_kd_boe_params,
						&ext_funcs, &info->base);
		break;
	case LCM_ILI9882T_TG_HSD:
		pr_notice("[Kernel/LCM] ili9882t tg hsd, pll_clock=%d.\n",
				ext_ili9882t_tg_hsd_params.pll_clk);
		ret = mtk_panel_ext_create(dev, &ext_ili9882t_tg_hsd_params,
						&ext_funcs, &info->base);
		break;
	default:
		pr_notice("[Kernel/LCM] It's default panel ili9882t kd boe,pll_clock=%d.\n",
				ext_ili9882t_kd_boe_params.pll_clk);
		ret = mtk_panel_ext_create(dev, &ext_ili9882t_kd_boe_params,
						&ext_funcs, &info->base);
		break;
	}
#endif
	if (ret < 0) {
		panel_resources_release(info);
		mipi_dsi_detach(dsi);
		drm_panel_remove(&info->base);
		devm_kfree(&dsi->dev, info);
		return ret;
	}
	share_info = info;
	old_state = LCM_ON;
	atomic_inc(&g_power_request);
	LCM_FUNC_EXIT();

	return ret;
}

static int panel_remove(struct mipi_dsi_device *dsi)
{
	struct panel_info *info = mipi_dsi_get_drvdata(dsi);
	int ret = 0xFF;

	LCM_FUNC_ENTER();
	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n", ret);

	ret = unregister_reboot_notifier(&ili9882t_reboot_notifier);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to unregister reboot notifier: %d\n", ret);

	if (info->base.dev)
		drm_panel_remove(&info->base);
	LCM_FUNC_EXIT();

	return 0;
}

static const struct of_device_id panel_of_match[] = {
	{ .compatible = "panel,ili9882t_wuxga_dsi_vdo" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, panel_of_match);

static struct mipi_dsi_driver panel_driver = {
	.driver = {
		.name = "panel-ili9882t-wuxga-dsi-vdo",
		.of_match_table = panel_of_match,
	},
	.probe = panel_probe,
	.remove = panel_remove,
};
module_mipi_dsi_driver(panel_driver);

MODULE_DESCRIPTION("KD ili9882t-wuxga-dsi-vdo 1200x2000 video mode panel driver");
MODULE_LICENSE("GPL v2");
