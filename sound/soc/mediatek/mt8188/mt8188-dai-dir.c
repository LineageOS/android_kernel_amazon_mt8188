// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI for SPDIF DIR (Digital Interface Receiver)
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/regmap.h>
#include <linux/delay.h>
#include "mt8188-afe-clk.h"
#include "mt8188-afe-common.h"
#include "mt8188-reg.h"

static const struct snd_soc_dapm_widget mtk_dai_dir_widgets[] = {
	SND_SOC_DAPM_INPUT("SPDIF_INPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_dir_routes[] = {
	/* connected to mphone_multi */
	{"MULTI_IN1_MUX", "SPDIF_IN", "SPDIF Capture"},
	{"MULTI_IN2_MUX", "SPDIF_IN", "SPDIF Capture"},

	{"SPDIF Capture", NULL, "SPDIF_INPUT"},
};

/* dai ops */
static int mtk_dai_dir_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	/* revise to mixer ctrl */
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_INTDIR_SEL]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_INTDIR]);

	return 0;
}

static void mtk_dai_dir_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	/* revise to mixer ctrl */
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_INTDIR]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_INTDIR_SEL]);
}

static int mtk_dai_dir_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static int mtk_dai_dir_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_dir_ops = {
	.startup	= mtk_dai_dir_startup,
	.shutdown	= mtk_dai_dir_shutdown,
	.prepare	= mtk_dai_dir_prepare,
	.trigger	= mtk_dai_dir_trigger,
};

/* dai driver */
#define MTK_DIR_RATES (SNDRV_PCM_RATE_32000 |\
				 SNDRV_PCM_RATE_44100 |\
				 SNDRV_PCM_RATE_48000 |\
				 SNDRV_PCM_RATE_88200 |\
				 SNDRV_PCM_RATE_96000 |\
				 SNDRV_PCM_RATE_176400 |\
				 SNDRV_PCM_RATE_192000)

#define MTK_DIR_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			  SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_dir_driver[] = {
	{
		.name = "SPDIF_IN",
		.id = MT8188_AFE_IO_SPDIF_IN,
		.capture = {
			.stream_name = "SPDIF Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_DIR_RATES,
			.formats = MTK_DIR_FORMATS,
		},
		.ops = &mtk_dai_dir_ops,
	},
};

int mt8188_dai_dir_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_dir_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_dir_driver);

	dai->dapm_widgets = mtk_dai_dir_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_dir_widgets);
	dai->dapm_routes = mtk_dai_dir_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_dir_routes);
	return 0;
}
