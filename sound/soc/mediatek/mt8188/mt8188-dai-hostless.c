// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI Hostless Control
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/clk.h>

#include "mt8188-afe-clk.h"
#include "mt8188-afe-common.h"

static const struct snd_soc_dapm_widget mtk_dai_hostless_widgets[] = {
	SND_SOC_DAPM_MIXER("I000V", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I001V", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_INPUT("DL_VIRTUAL_INPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_hostless_routes[] = {
	{"DL_VIRTUAL_SOURCE", NULL, "DL_VIRTUAL_INPUT"},

	{"I000V", NULL, "DL_VIRTUAL_SOURCE"},
	{"I001V", NULL, "DL_VIRTUAL_SOURCE"},

	{"O176", "I168 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O177", "I169 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"ADDA_HOSTLESS_LPBK Capture", NULL, "I168"},
	{"ADDA_HOSTLESS_LPBK Capture", NULL, "I169"},

	{"O176", "I004 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O177", "I005 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O176", "I006 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O177", "I007 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O176", "I008 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O177", "I009 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O176", "I010 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"O177", "I011 Switch", "ADDA_HOSTLESS_LPBK Playback"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I004"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I005"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I006"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I007"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I008"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I009"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I010"},
	{"DMIC_HOSTLESS_RECORD Capture", NULL, "I011"},

	{"O072", "I004 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
	{"O073", "I005 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
	{"O074", "I006 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
	{"O075", "I007 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
	{"O076", "I008 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
	{"O077", "I009 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
	{"O078", "I010 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
	{"O079", "I011 Switch", "ETDM1_HOSTLESS_LPBK Playback"},
};

/* dai ops */
static int mtk_dai_hostless_startup(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	ret = snd_soc_set_runtime_hwparams(substream, afe->mtk_afe_hardware);
	if (ret)
		dev_dbg(dai->dev, "set runtime hwparams failed\n");

	return ret;
}

static const struct snd_soc_dai_ops mtk_dai_hostless_ops = {
	.startup = mtk_dai_hostless_startup,
};

/* dai driver */
#define MTK_HOSTLESS_RATES (SNDRV_PCM_RATE_8000_48000 |\
			    SNDRV_PCM_RATE_88200 |\
			    SNDRV_PCM_RATE_96000 |\
			    SNDRV_PCM_RATE_176400 |\
			    SNDRV_PCM_RATE_192000)

#define MTK_HOSTLESS_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			      SNDRV_PCM_FMTBIT_S32_LE)

#define MTK_HOSTLESS_DMIC_RATES (SNDRV_PCM_RATE_8000 |\
				 SNDRV_PCM_RATE_16000 |\
				 SNDRV_PCM_RATE_32000 |\
				 SNDRV_PCM_RATE_48000)

static struct snd_soc_dai_driver mtk_dai_hostless_driver[] = {
	/* FE dai */
	{
		.name = "ADDA_HOSTLESS_LPBK",
		.id = MT8188_AFE_HOSTLESS_FE_ADDA_LPBK,
		.playback = {
			.stream_name = "ADDA_HOSTLESS_LPBK Playback",
			.channels_min = 1,
			.channels_max = 24,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.capture = {
			.stream_name = "ADDA_HOSTLESS_LPBK Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "DMIC_HOSTLESS_RECORD",
		.id = MT8188_AFE_HOSTLESS_FE_DMIC_RECORD,
		.capture = {
			.stream_name = "DMIC_HOSTLESS_RECORD Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_HOSTLESS_DMIC_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	{
		.name = "ETDM1_HOSTLESS_LPBK",
		.id = MT8188_AFE_HOSTLESS_FE_ETDM1_LPBK,
		.playback = {
			.stream_name = "ETDM1_HOSTLESS_LPBK Playback",
			.channels_min = 1,
			.channels_max = 24,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
		.ops = &mtk_dai_hostless_ops,
	},
	/* BE dai */
	{
		.name = "DL_VIRTUAL_SOURCE",
		.id = MT8188_AFE_HOSTLESS_BE_DL_VIRTUAL_SOURCE,
		.capture = {
			.stream_name = "DL_VIRTUAL_SOURCE",
			.channels_min = 1,
			.channels_max = 32,
			.rates = MTK_HOSTLESS_RATES,
			.formats = MTK_HOSTLESS_FORMATS,
		},
	},
};

int mt8188_dai_hostless_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dev_info(afe->dev, "%s()\n", __func__);

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_hostless_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_hostless_driver);

	dai->dapm_widgets = mtk_dai_hostless_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_hostless_widgets);

	dai->dapm_routes = mtk_dai_hostless_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_hostless_routes);

	return 0;
}
