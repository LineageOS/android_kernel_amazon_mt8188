/*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <linux/reboot.h>

#ifndef __LINUX_PANEL_COMMON_H__
#define __LINUX_PANEL_COMMON_H__

#define LCD_DEBUG_EN 1
#if LCD_DEBUG_EN
#define LCM_DEBUG(fmt, args...) do { \
    printk("[KERNEL/LCM]%s:"fmt"\n", __func__, ##args); \
} while (0)

#define LCM_FUNC_ENTER() do { \
    printk("[KERNEL/LCM]%s: Enter\n", __func__); \
} while (0)

#define LCM_FUNC_EXIT() do { \
    printk("[KERNEL/LCM]%s: Exit(%d)\n", __func__, __LINE__); \
} while (0)
#else /* #if LCD_DEBUG_EN*/
#define LCM_DEBUG(fmt, args...)
#define LCM_FUNC_ENTER()
#define LCM_FUNC_EXIT()
#endif

#define LCM_INFO(fmt, args...) do { \
    printk(KERN_INFO "[KERNEL/LCM]]%s:"fmt"\n", __func__, ##args); \
} while (0)

#define LCM_ERROR(fmt, args...) do { \
    printk(KERN_ERR "[[KERNEL/LCM]]%s:"fmt"\n", __func__, ##args); \
} while (0)

#define LCM_NT36523N_TG_INX         0
#define LCM_NT36523B_TG_INX         1
#define LCM_FT8205_TG_INX           2
#define LCM_ILI9882T_TG_HSD         3
#define LCM_FT8205_KD_BOE           4
#define LCM_NT36523N_KD_HSD         5
#define LCM_NT36523B_KD_HSD         6
#define LCM_ILI9882T_KD_BOE         7
#define LCM_VENDOR_ID_UNKNOWN       0xFF

/*
 * power on sequnce: vrf18/AVDD/AVEE -> LP11 -> RESET high
 * the LP11 will be enable after resume_power() and before resume()
 * so   set vrf18/AVDD/AVEE at resume_power() //TRS_ON_PHASE1
 * then set RESET at resume()                 //TRS_ON_PHASE2
*/
#define TRS_OFF_TO_ON        0
#define TRS_OFF              1
#define TRS_ON_PHASE1        2
#define TRS_ON_PHASE2        3
#define TRS_DSB              4
#define TRS_DSB_TO_ON        5
#define TRS_DSB_TO_OFF       6
#define TRS_KEEP_CUR         0xFD /* keep current state */
#define TRS_NOT_PSB          0xFE /* not possible */

#define LCM_OFF             0
#define LCM_ON              1
#define LCM_ON1             2
#define LCM_ON2             3
#define LCM_DSB             4

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int bpc;

	/**
	 * @width_mm: width of the panel's active display area
	 * @height_mm: height of the panel's active display area
	 */
	struct {
		unsigned int width_mm;
		unsigned int height_mm;
	} size;

	unsigned long mode_flags;
	enum mipi_dsi_pixel_format format;
	const struct panel_init_cmd *init_cmds;
	unsigned int lanes;
	bool discharge_on_disable;
};

struct panel_info {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;
	struct device *dev;
	const struct panel_desc *desc;
	struct backlight_device *backlight;
	bool enabled;

	enum drm_panel_orientation orientation;
	struct regulator *pp1800;
	struct gpio_desc *avee;
	struct gpio_desc *avdd;
	struct gpio_desc *enable_gpio;
	struct gpio_desc *leden_gpio;
	struct gpio_desc *ctp_rst_gpio;

	bool prepared_power;
	bool prepared;
};

enum dsi_cmd_type {
	INIT_DCS_CMD,
	DELAY_CMD,
};

struct panel_init_cmd {
	enum dsi_cmd_type type;
	size_t len;
	const char *data;
};

#define _INIT_DCS_CMD(...) { \
	.type = INIT_DCS_CMD, \
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }

#define _INIT_DELAY_CMD(...) { \
	.type = DELAY_CMD,\
	.len = sizeof((char[]){__VA_ARGS__}), \
	.data = (char[]){__VA_ARGS__} }


#endif /* __LINUX_PANEL_COMMON_H__ */
