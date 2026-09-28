// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI for SPDIF DIT (Digital Interface Transmitter)
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

static const struct snd_soc_dapm_widget mtk_dai_dit_widgets[] = {
	SND_SOC_DAPM_OUTPUT("SPDIF_OUTPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_dit_routes[] = {
	{"SPDIF Playback", NULL, "DL7"},
	{"SPDIF_OUTPUT", NULL, "SPDIF Playback"},
};

/* dai ops */
static int mtk_dai_dit_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUD_IEC_SEL]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_TOP0_SPDF]);

	return 0;
}

static void mtk_dai_dit_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_TOP0_SPDF]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUD_IEC_SEL]);
}

static int mtk_dai_dit_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static int mtk_dai_dit_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_dit_ops = {
	.startup	= mtk_dai_dit_startup,
	.shutdown	= mtk_dai_dit_shutdown,
	.prepare	= mtk_dai_dit_prepare,
	.trigger	= mtk_dai_dit_trigger,
};

/* dai driver */
#define MTK_DIT_RATES (SNDRV_PCM_RATE_32000 |\
				 SNDRV_PCM_RATE_44100 |\
				 SNDRV_PCM_RATE_48000 |\
				 SNDRV_PCM_RATE_88200 |\
				 SNDRV_PCM_RATE_96000 |\
				 SNDRV_PCM_RATE_176400 |\
				 SNDRV_PCM_RATE_192000)

#define MTK_DIT_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			  SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_dit_driver[] = {
	{
		.name = "SPDIF_OUT",
		.id = MT8188_AFE_IO_SPDIF_OUT,
		.playback = {
			.stream_name = "SPDIF Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_DIT_RATES,
			.formats = MTK_DIT_FORMATS,
		},
		.ops = &mtk_dai_dit_ops,
	},
};

int mt8188_dai_dit_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_dit_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_dit_driver);

	dai->dapm_widgets = mtk_dai_dit_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_dit_widgets);
	dai->dapm_routes = mtk_dai_dit_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_dit_routes);
	return 0;
}
