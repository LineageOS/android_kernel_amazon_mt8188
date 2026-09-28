// SPDX-License-Identifier: GPL-2.0-only
/*
 * amic.c  --  SoC audio for Generic Analog MICs
 *
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Trevor Wu <trevor.wu@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

static int wakeup_delay;
module_param(wakeup_delay, uint, 0644);

struct amic {
	int wakeup_delay;
};

static int amic_aif_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct amic *amic = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (amic->wakeup_delay)
			msleep(amic->wakeup_delay);
		break;
	}
	return 0;
}

static struct snd_soc_dai_driver amic_dai = {
	.name = "amic-aif",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.formats = SNDRV_PCM_FMTBIT_S32_LE
			| SNDRV_PCM_FMTBIT_S24_LE
			| SNDRV_PCM_FMTBIT_S16_LE,
	},
};

static int amic_component_probe(struct snd_soc_component *component)
{
	struct amic *amic;

	amic = devm_kzalloc(component->dev, sizeof(*amic), GFP_KERNEL);
	if (!amic)
		return -ENOMEM;

	device_property_read_u32(component->dev, "wakeup-delay-ms",
				 &amic->wakeup_delay);

	if (wakeup_delay)
		amic->wakeup_delay  = wakeup_delay;

	snd_soc_component_set_drvdata(component, amic);

	return 0;
}

static const struct snd_soc_dapm_widget amic_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT_E("AMIC AIF", "Capture", 0, SND_SOC_NOPM,
			       0, 0, amic_aif_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_INPUT("AMic"),
};

static const struct snd_soc_dapm_route intercon[] = {
	{"AMIC AIF", NULL, "AMic"},
};

static const struct snd_soc_component_driver soc_amic = {
	.probe			= amic_component_probe,
	.dapm_widgets		= amic_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(amic_dapm_widgets),
	.dapm_routes		= intercon,
	.num_dapm_routes	= ARRAY_SIZE(intercon),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static int amic_dev_probe(struct platform_device *pdev)
{
	int err;
	u32 chans;
	struct snd_soc_dai_driver *dai_drv = &amic_dai;

	if (pdev->dev.of_node) {
		err = of_property_read_u32(pdev->dev.of_node, "num-channels", &chans);
		if (err && (err != -EINVAL))
			return err;

		if (!err) {
			if (chans < 1 || chans > 8)
				return -EINVAL;

			dai_drv = devm_kzalloc(&pdev->dev, sizeof(*dai_drv), GFP_KERNEL);
			if (!dai_drv)
				return -ENOMEM;

			memcpy(dai_drv, &amic_dai, sizeof(*dai_drv));
			dai_drv->capture.channels_max = chans;
		}
	}

	return devm_snd_soc_register_component(&pdev->dev,
			&soc_amic, dai_drv, 1);
}

MODULE_ALIAS("platform:amic-codec");

static const struct of_device_id amic_dev_match[] = {
	{.compatible = "amic-codec"},
	{}
};
MODULE_DEVICE_TABLE(of, amic_dev_match);

static struct platform_driver amic_driver = {
	.driver = {
		.name = "amic-codec",
		.of_match_table = amic_dev_match,
	},
	.probe = amic_dev_probe,
};

module_platform_driver(amic_driver);

MODULE_DESCRIPTION("Generic AMIC driver");
MODULE_AUTHOR("Trevor Wu <trevor.wu@mediatek.com>");
MODULE_LICENSE("GPL");
