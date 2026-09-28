// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include "mdp_larb.h"
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/pm_runtime.h>


/*for larb4,5,6,7 */
#define MDP_LARB_NUM 4

struct mdp_larb_data {
	unsigned int larb_id;
	struct device *dev;
};

static struct device *larb_dev[MDP_LARB_NUM] = {NULL};

int mdp_larb_power_on(void)
{
	int i;

	for (i = 0; i < MDP_LARB_NUM; i++) {
		if (!larb_dev[i])
			return -ENODEV;

		pm_runtime_get_sync(larb_dev[i]);
	}

	return 0;
}
EXPORT_SYMBOL(mdp_larb_power_on);

int mdp_larb_power_off(void)
{
	int i;

	for (i = 0; i < MDP_LARB_NUM; i++) {
		if (!larb_dev[i])
			return -ENODEV;

		pm_runtime_put_sync(larb_dev[i]);
	}

	return 0;
}
EXPORT_SYMBOL(mdp_larb_power_off);

struct device *mdp_larb_get_dev(unsigned int larb_id)
{
	return (larb_id >= (MDP_LARB_NUM+4)) ? NULL : larb_dev[larb_id-4];
}
EXPORT_SYMBOL(mdp_larb_get_dev);

static int mdp_larb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mdp_larb_data *data;
	u32 dma_mask_bit = 0;
	s32 dma_mask_result = 0;
	int ret;

	data = devm_kzalloc(dev, sizeof(*dev), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	ret = of_property_read_u32(dev->of_node, "mediatek,larb-id",
					&data->larb_id);
	if (ret != 0) {
		dev_dbg(dev, "[%s][%d] failed to find larb-id\n",
			__func__, __LINE__);
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "dma_mask_bit",
		&dma_mask_bit);
	if (ret != 0 || !dma_mask_bit)
		dma_mask_bit = 34;
	dma_mask_result = dma_set_coherent_mask(dev,
				DMA_BIT_MASK(dma_mask_bit));
	dev_dbg(dev, "[%s][%d] set dma mask bit:%u result:%d\n",
		__func__, __LINE__, dma_mask_bit, dma_mask_result);

	platform_set_drvdata(pdev, data);

	pm_runtime_enable(dev);

	/*Store larb 4,5,6,7 devices*/
	larb_dev[data->larb_id - 4] = dev;

	return ret;
}
static int mdp_larb_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);

	return 0;
}

static const struct of_device_id mdp_larb_match[] = {
	{.compatible = "mediatek,mdp-larb",},
	{},
};

static struct platform_driver mdp_larb_driver = {
	.probe  = mdp_larb_probe,
	.remove = mdp_larb_remove,
	.driver = {
		.name   = "mdp-larb",
		.of_match_table = mdp_larb_match,
	},
};

void mdp_larb_init(void)
{
	platform_driver_register(&mdp_larb_driver);
}
EXPORT_SYMBOL(mdp_larb_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek MDP larb driver");
