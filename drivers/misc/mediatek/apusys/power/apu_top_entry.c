// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "apu_log.h"
#include "apu_top_entry.h"

int g_pwr_log_level = APUSYS_PWR_LOG_ERR;
int g_apupw_drv_ver;

static int __init apu_top_entry_init(void)
{
	int ret = 0;

	ret |= apu_power_init();
	ret |= apu_top_3_init();

	return ret;
}

static void __exit apu_top_entry_exit(void)
{
	apu_top_3_exit();
	apu_power_exit();
}

/* caller is middleware */
int apu_power_drv_init(struct apusys_core_info *info)
{
	pr_info("%s ++\n", __func__);
#if IS_ENABLED(CONFIG_DEBUG_FS)
	if (g_apupw_drv_ver == 3)
		return aputop_dbg_init(info);
#endif
	return 0;
}
EXPORT_SYMBOL(apu_power_drv_init);

/* caller is middleware */
void apu_power_drv_exit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	if (g_apupw_drv_ver == 3)
		aputop_dbg_exit();
#endif
}
EXPORT_SYMBOL(apu_power_drv_exit);

module_init(apu_top_entry_init);
module_exit(apu_top_entry_exit);
MODULE_LICENSE("GPL");
