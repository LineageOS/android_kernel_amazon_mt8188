// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#include <linux/of.h>
#include <linux/of_address.h>

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include "dvfsrc-exp.h"
#endif /* CONFIG_MTK_DVFSRC */
#include "mtk_cm_mgr_mt8188.h"
#include "mtk_cm_mgr_common.h"


#if IS_ENABLED(CONFIG_MTK_DRAMC)
static int cm_mgr_check_dram_type(void)
{
	int ddr_type = mtk_dramc_get_ddr_type();
	int ddr_hz = mtk_dramc_get_steps_freq(0);
	int cm_mgr_idx = -1;
	int ret = 0;

	if (ddr_type == TYPE_EXT_LPDDR4X || ddr_type == TYPE_EXT_DDR4)
		cm_mgr_idx = CM_MGR_LP4;
	pr_debug("#@# %s(%d) ddr_type 0x%x, ddr_hz %d, cm_mgr_idx 0x%x\n",
			__func__, __LINE__, ddr_type, ddr_hz, cm_mgr_idx);

	if (cm_mgr_idx >= 0)
		cm_mgr_to_sspm_command(IPI_CM_MGR_DRAM_TYPE, cm_mgr_idx);
	else
		ret = -EINVAL;

	return ret;
};
#else
static int cm_mgr_check_dram_type(void)
{
	pr_debug("#@# %s(%d) NO CONFIG_MTK_DRAMC !!!\n",
			__func__, __LINE__);
	return -EPERM;
}
#endif

static int platform_cm_mgr_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node = pdev->dev.of_node;

	ret = cm_mgr_common_init();
	if (ret) {
		pr_info("[CM_MGR] FAILED TO INIT(%d)\n", ret);
		return ret;
	}

	ret = cm_mgr_check_dram_type();
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DRAM TYPE(%d)\n", ret);
		return ret;
	}

	/* required-opps */
	ret = of_count_phandle_with_args(node, "required-opps", NULL);
	cm_mgr_set_num_perf(ret);
	pr_debug("#@# %s(%d) cm_mgr_num_perf %d\n",
			__func__, __LINE__, ret);

	if (ret > 2) {
		cm_mgr_set_num_array(ret - 2);
	} else {
		cm_mgr_set_num_array(0);
		return -EINVAL;
	}
	pr_debug("#@# %s(%d) cm_mgr_num_array %d\n",
			__func__, __LINE__, cm_mgr_get_num_array());

	ret = cm_mgr_check_dts_setting(pdev);
	if (ret) {
		pr_info("[CM_MGR] FAILED TO GET DTS DATA(%d)\n", ret);
		return ret;
	}

	cm_mgr_set_pdev(pdev);

	pr_info("[CM_MGR] platform-cm_mgr_probe Done.\n");

	return 0;
}

static int platform_cm_mgr_remove(struct platform_device *pdev)
{
	cm_mgr_common_exit();

	return 0;
}

static const struct of_device_id platform_cm_mgr_of_match[] = {
	{ .compatible = "mediatek,mt8188-cm-mgr", },
	{},
};

static struct platform_driver mtk_platform_cm_mgr_driver = {
	.probe = platform_cm_mgr_probe,
	.remove	= platform_cm_mgr_remove,
	.driver = {
		.name = "mt8188-cm_mgr",
		.owner = THIS_MODULE,
		.of_match_table = platform_cm_mgr_of_match,
	},
};

/*
 * driver initialization entry point
 */
static int __init platform_cm_mgr_init(void)
{
	return platform_driver_register(&mtk_platform_cm_mgr_driver);
}

static void __exit platform_cm_mgr_exit(void)
{
	platform_driver_unregister(&mtk_platform_cm_mgr_driver);
}

late_initcall(platform_cm_mgr_init);
module_exit(platform_cm_mgr_exit);

MODULE_DESCRIPTION("Mediatek cm_mgr driver");
MODULE_LICENSE("GPL");
