// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI HW GAIN Control
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/regmap.h>
#include "mt8188-afe-clk.h"
#include "mt8188-afe-common.h"
#include "mt8188-reg.h"

#define HW_GAIN_TO_DAI_ID(x) (x + MT8188_AFE_IO_HW_GAIN_START)
#define DAI_TO_HW_GAIN_ID(x) (x - MT8188_AFE_IO_HW_GAIN_START)

enum {
	HW_GAIN1 = 0,
	HW_GAIN2,
	HW_GAIN_NUM,
};

struct mtk_dai_hw_gain_ctrl_reg {
	unsigned int cur;
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
};

struct mtk_dai_hw_gain_param_priv {
	unsigned int target;
	unsigned int cur;
	unsigned int up_step;
	unsigned int down_step;
	unsigned int sample_per_step;
};

struct mtk_dai_hw_gain_priv {
	struct mtk_dai_hw_gain_param_priv param[HW_GAIN_NUM];
};

enum {
	MTK_DAI_HW_GAIN_TARGET = 0,
	MTK_DAI_HW_GAIN_CURRENT,
	MTK_DAI_HW_GAIN_UP_STEP,
	MTK_DAI_HW_GAIN_DOWN_STEP,
	MTK_DAI_HW_GAIN_SAMPLE_PER_STEP,
};

static const struct mtk_dai_hw_gain_ctrl_reg
	hw_gain_ctrl_regs[HW_GAIN_NUM] = {
	[HW_GAIN1] = {
		.cur = AFE_GAIN1_CUR,
		.con0 = AFE_GAIN1_CON0,
		.con1 = AFE_GAIN1_CON1,
		.con2 = AFE_GAIN1_CON2,
		.con3 = AFE_GAIN1_CON3,
	},
	[HW_GAIN2] = {
		.cur = AFE_GAIN2_CUR,
		.con0 = AFE_GAIN2_CON0,
		.con1 = AFE_GAIN2_CON1,
		.con2 = AFE_GAIN2_CON2,
		.con3 = AFE_GAIN2_CON3,
	},
};

static const struct mtk_dai_hw_gain_ctrl_reg
	*get_hw_gain_ctrl_reg(struct mtk_base_afe *afe, int id)
{
	if ((id < 0) || (id >= HW_GAIN_NUM)) {
		dev_dbg(afe->dev, "%s invalid id\n", __func__);
		return NULL;
	}

	return &hw_gain_ctrl_regs[id];
}

static struct mtk_dai_hw_gain_param_priv *
	mtk_dai_hw_gain_param_get_priv(struct mtk_base_afe *afe,
				       int id, int sub_id)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_hw_gain_param_priv *priv = NULL;
	int dai_id = HW_GAIN_TO_DAI_ID(id);
	struct mtk_dai_hw_gain_priv *hw_gain_priv;

	if (dai_id < 0) {
		dev_info(afe->dev, "%s invalid dai_id [%d]\n", __func__, dai_id);
		return NULL;
	}
	hw_gain_priv = afe_priv->dai_priv[dai_id];

	switch (sub_id) {
	case MTK_DAI_HW_GAIN_TARGET:
		fallthrough;
	case MTK_DAI_HW_GAIN_CURRENT:
		fallthrough;
	case MTK_DAI_HW_GAIN_UP_STEP:
		fallthrough;
	case MTK_DAI_HW_GAIN_DOWN_STEP:
		fallthrough;
	case MTK_DAI_HW_GAIN_SAMPLE_PER_STEP:
		priv = &(hw_gain_priv->param[id]);
		break;
	default:
		dev_dbg(afe->dev, "%s invalid sub_id\n", __func__);
		break;
	}

	return priv;
}

static void mtk_dai_hw_gain_enable(struct snd_soc_dai *dai,
				   bool enable)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mtk_dai_hw_gain_ctrl_reg *reg;
	unsigned int msk = AFE_GAIN_CON0_GAIN_ON_MASK;
	unsigned int val;
	unsigned int id = DAI_TO_HW_GAIN_ID(dai->id);

	reg = get_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	val = (enable) ? msk : 0;

	regmap_update_bits(afe->regmap, reg->con0, msk, val);
}

static void mtk_dai_hw_gain_rate(struct snd_soc_dai *dai,
				 unsigned int id)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mtk_dai_hw_gain_ctrl_reg *reg;
	unsigned int rate = mt8188_afe_fs_timing(dai->rate);
	unsigned int msk = AFE_GAIN_CON0_GAIN_MODE_MASK;
	unsigned int val;

	reg = get_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	val = AFE_GAIN_CON0_GAIN_MODE_VAL(rate);

	regmap_update_bits(afe->regmap, reg->con0, msk, val);
}

static void mtk_dai_hw_gain_target(struct snd_soc_dai *dai,
				   unsigned int id)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mtk_dai_hw_gain_ctrl_reg *reg;
	struct mtk_dai_hw_gain_param_priv *hw_gain_param_priv;
	unsigned int msk = AFE_GAIN_CON1_TARGET_MASK;
	unsigned int val;

	reg = get_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_param_priv = mtk_dai_hw_gain_param_get_priv(afe, id,
				MTK_DAI_HW_GAIN_TARGET);
	if (!hw_gain_param_priv)
		return;

	val = hw_gain_param_priv->target;

	regmap_update_bits(afe->regmap, reg->con1, msk, val);
}

static void mtk_dai_hw_gain_current(struct snd_soc_dai *dai,
				    unsigned int id)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mtk_dai_hw_gain_ctrl_reg *reg;
	struct mtk_dai_hw_gain_param_priv *hw_gain_param_priv;
	unsigned int msk = AFE_GAIN_CUR_GAIN_MASK;
	unsigned int val;

	reg = get_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_param_priv = mtk_dai_hw_gain_param_get_priv(afe, id,
				MTK_DAI_HW_GAIN_CURRENT);
	if (!hw_gain_param_priv)
		return;

	val = hw_gain_param_priv->cur;

	regmap_update_bits(afe->regmap, reg->cur, msk, val);
}

static void mtk_dai_hw_gain_up_step(struct snd_soc_dai *dai,
				    unsigned int id)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mtk_dai_hw_gain_ctrl_reg *reg;
	struct mtk_dai_hw_gain_param_priv *hw_gain_param_priv;
	unsigned int msk = AFE_GAIN_CON3_UP_STEP_MASK;
	unsigned int val;

	reg = get_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_param_priv = mtk_dai_hw_gain_param_get_priv(afe, id,
				MTK_DAI_HW_GAIN_UP_STEP);
	if (!hw_gain_param_priv)
		return;

	val = hw_gain_param_priv->up_step;

	regmap_update_bits(afe->regmap, reg->con3, msk, val);
}

static void mtk_dai_hw_gain_down_step(struct snd_soc_dai *dai,
				      unsigned int id)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mtk_dai_hw_gain_ctrl_reg *reg;
	struct mtk_dai_hw_gain_param_priv *hw_gain_param_priv;
	unsigned int msk = AFE_GAIN_CON2_DOWN_STEP_MASK;
	unsigned int val;

	reg = get_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_param_priv = mtk_dai_hw_gain_param_get_priv(afe, id,
				MTK_DAI_HW_GAIN_DOWN_STEP);
	if (!hw_gain_param_priv)
		return;

	val = hw_gain_param_priv->down_step;

	regmap_update_bits(afe->regmap, reg->con2, msk, val);
}

static void mtk_dai_hw_gain_sample_per_step(struct snd_soc_dai *dai,
					    unsigned int id)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mtk_dai_hw_gain_ctrl_reg *reg;
	struct mtk_dai_hw_gain_param_priv *hw_gain_param_priv;
	unsigned int msk = AFE_GAIN_CON0_SAMPLE_PER_STEP_MASK;
	unsigned int val;

	reg = get_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_param_priv = mtk_dai_hw_gain_param_get_priv(afe, id,
				MTK_DAI_HW_GAIN_SAMPLE_PER_STEP);
	if (!hw_gain_param_priv)
		return;

	val = AFE_GAIN_CON0_SAMPLE_PER_STEP(hw_gain_param_priv->sample_per_step);

	regmap_update_bits(afe->regmap, reg->con0, msk, val);
}

/* dai ops */
static int mtk_dai_hw_gain_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mt8188_afe_enable_main_clock(afe);

	return 0;
}

static void mtk_dai_hw_gain_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);

	mtk_dai_hw_gain_enable(dai, false);
	mt8188_afe_disable_main_clock(afe);
}

static int mtk_dai_hw_gain_prepare(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = DAI_TO_HW_GAIN_ID(dai->id);

	dev_info(afe->dev, "%s(), id %d, stream %d\n",
		 __func__,
		 dai->id,
		 substream->stream);

	mtk_dai_hw_gain_rate(dai, id);
	mtk_dai_hw_gain_target(dai, id);
	mtk_dai_hw_gain_current(dai, id);
	mtk_dai_hw_gain_up_step(dai, id);
	mtk_dai_hw_gain_down_step(dai, id);
	mtk_dai_hw_gain_sample_per_step(dai, id);
	mtk_dai_hw_gain_enable(dai, true);

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_hw_gain_ops = {
	.startup	= mtk_dai_hw_gain_startup,
	.shutdown	= mtk_dai_hw_gain_shutdown,
	.prepare	= mtk_dai_hw_gain_prepare,
};

static const struct snd_kcontrol_new mtk_dai_hw_gain_o136_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN136, 0, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_hw_gain_o137_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN137, 1, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_hw_gain_o138_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN138, 0, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_hw_gain_o139_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN139, 1, 1, 0),
};

static const struct snd_soc_dapm_widget mtk_dai_hw_gain_widgets[] = {
	SND_SOC_DAPM_MIXER("I136", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I137", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I138", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I139", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("O136", SND_SOC_NOPM, 0, 0,
		mtk_dai_hw_gain_o136_mix, ARRAY_SIZE(mtk_dai_hw_gain_o136_mix)),
	SND_SOC_DAPM_MIXER("O137", SND_SOC_NOPM, 0, 0,
		mtk_dai_hw_gain_o137_mix, ARRAY_SIZE(mtk_dai_hw_gain_o137_mix)),
	SND_SOC_DAPM_MIXER("O138", SND_SOC_NOPM, 0, 0,
		mtk_dai_hw_gain_o138_mix, ARRAY_SIZE(mtk_dai_hw_gain_o138_mix)),
	SND_SOC_DAPM_MIXER("O139", SND_SOC_NOPM, 0, 0,
		mtk_dai_hw_gain_o139_mix, ARRAY_SIZE(mtk_dai_hw_gain_o139_mix)),

	SND_SOC_DAPM_INPUT("HW_GAIN_INPUT"),
	SND_SOC_DAPM_OUTPUT("HW_GAIN_OUTPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_hw_gain_routes[] = {
	{"I136", NULL, "HW_GAIN1 Capture"},
	{"I137", NULL, "HW_GAIN1 Capture"},
	{"I138", NULL, "HW_GAIN2 Capture"},
	{"I139", NULL, "HW_GAIN2 Capture"},

	{"HW_GAIN1 Playback", NULL, "O136"},
	{"HW_GAIN1 Playback", NULL, "O137"},

	{"HW_GAIN2 Playback", NULL, "O138"},
	{"HW_GAIN2 Playback", NULL, "O139"},

	{"O136", "I000 Switch", "I000"},
	{"O137", "I001 Switch", "I001"},

	{"O138", "I000 Switch", "I000"},
	{"O139", "I001 Switch", "I001"},

	{"HW_GAIN_OUTPUT", NULL, "HW_GAIN1 Playback"},
	{"HW_GAIN_OUTPUT", NULL, "HW_GAIN2 Playback"},

	{"HW_GAIN1 Capture", NULL, "HW_GAIN_INPUT"},
	{"HW_GAIN2 Capture", NULL, "HW_GAIN_INPUT"},
};

static int mtk_dai_hw_gain_ctrl_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->id.device;
	unsigned int sub_id = kcontrol->id.subdevice;
	struct mtk_dai_hw_gain_param_priv *hw_gain_param_priv =
		mtk_dai_hw_gain_param_get_priv(afe, id, sub_id);
	long val = 0;

	if (!hw_gain_param_priv)
		return -EINVAL;

	switch (sub_id) {
	case MTK_DAI_HW_GAIN_TARGET:
		val = hw_gain_param_priv->target;
		break;
	case MTK_DAI_HW_GAIN_CURRENT:
		val = hw_gain_param_priv->cur;
		break;
	case MTK_DAI_HW_GAIN_UP_STEP:
		val = hw_gain_param_priv->up_step;
		break;
	case MTK_DAI_HW_GAIN_DOWN_STEP:
		val = hw_gain_param_priv->down_step;
		break;
	case MTK_DAI_HW_GAIN_SAMPLE_PER_STEP:
		val = hw_gain_param_priv->sample_per_step;
		break;
	default:
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int mtk_dai_hw_gain_ctrl_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->id.device;
	unsigned int sub_id = kcontrol->id.subdevice;
	struct mtk_dai_hw_gain_param_priv *hw_gain_param_priv =
		mtk_dai_hw_gain_param_get_priv(afe, id, sub_id);
	long val = ucontrol->value.integer.value[0];
	unsigned int *cached = 0;

	if (!hw_gain_param_priv)
		return -EINVAL;

	switch (sub_id) {
	case MTK_DAI_HW_GAIN_TARGET:
		cached = &hw_gain_param_priv->target;
		break;
	case MTK_DAI_HW_GAIN_CURRENT:
		cached = &hw_gain_param_priv->cur;
		break;
	case MTK_DAI_HW_GAIN_UP_STEP:
		cached = &hw_gain_param_priv->up_step;
		break;
	case MTK_DAI_HW_GAIN_DOWN_STEP:
		cached = &hw_gain_param_priv->down_step;
		break;
	case MTK_DAI_HW_GAIN_SAMPLE_PER_STEP:
		cached = &hw_gain_param_priv->sample_per_step;
		break;
	default:
		return -EINVAL;
	}

	if (val == *cached)
		return 0;

	*cached = val;

	return 0;
}

static const struct snd_kcontrol_new mtk_dai_hw_gain_controls[] = {
	MT8188_SOC_SINGLE_EXT("hw_gain1_target",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN1,
			      MTK_DAI_HW_GAIN_TARGET),
	MT8188_SOC_SINGLE_EXT("hw_gain2_target",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN2,
			      MTK_DAI_HW_GAIN_TARGET),
	MT8188_SOC_SINGLE_EXT("hw_gain1_current",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN1,
			      MTK_DAI_HW_GAIN_CURRENT),
	MT8188_SOC_SINGLE_EXT("hw_gain2_current",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN2,
			      MTK_DAI_HW_GAIN_CURRENT),
	MT8188_SOC_SINGLE_EXT("hw_gain1_up_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN1,
			      MTK_DAI_HW_GAIN_UP_STEP),
	MT8188_SOC_SINGLE_EXT("hw_gain2_up_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN2,
			      MTK_DAI_HW_GAIN_UP_STEP),
	MT8188_SOC_SINGLE_EXT("hw_gain1_down_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN1,
			      MTK_DAI_HW_GAIN_DOWN_STEP),
	MT8188_SOC_SINGLE_EXT("hw_gain2_down_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN2,
			      MTK_DAI_HW_GAIN_DOWN_STEP),
	MT8188_SOC_SINGLE_EXT("hw_gain1_sample_per_step",
			      SND_SOC_NOPM, 0, 0x000000ff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN1,
			      MTK_DAI_HW_GAIN_SAMPLE_PER_STEP),
	MT8188_SOC_SINGLE_EXT("hw_gain2_sample_per_step",
			      SND_SOC_NOPM, 0, 0x000000ff, 0,
			      mtk_dai_hw_gain_ctrl_get,
			      mtk_dai_hw_gain_ctrl_put,
			      HW_GAIN2,
			      MTK_DAI_HW_GAIN_SAMPLE_PER_STEP),
};

/* dai driver */
#define MTK_HW_GAIN_RATES (SNDRV_PCM_RATE_8000_48000 |\
			   SNDRV_PCM_RATE_88200 |\
			   SNDRV_PCM_RATE_96000 |\
			   SNDRV_PCM_RATE_176400 |\
			   SNDRV_PCM_RATE_192000)

#define MTK_HW_GAIN_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			     SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_hw_gain_driver[] = {
	{
		.name = "HW_GAIN1",
		.id = MT8188_AFE_IO_HW_GAIN1,
		.playback = {
			.stream_name = "HW_GAIN1 Playback",
			.channels_min = 1,
			.channels_max = 2,
		},
		.capture = {
			.stream_name = "HW_GAIN1 Capture",
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &mtk_dai_hw_gain_ops,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
	},
	{
		.name = "HW_GAIN2",
		.id = MT8188_AFE_IO_HW_GAIN2,
		.playback = {
			.stream_name = "HW_GAIN2 Playback",
			.channels_min = 1,
			.channels_max = 2,
		},
		.capture = {
			.stream_name = "HW_GAIN2 Capture",
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &mtk_dai_hw_gain_ops,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
	},
};

static int init_hw_gain_priv_data(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_hw_gain_priv *hw_gain_priv;
	int i;

	for (i = MT8188_AFE_IO_HW_GAIN_START;
	     i < MT8188_AFE_IO_HW_GAIN_END; i++) {
		hw_gain_priv =
			devm_kzalloc(afe->dev,
				     sizeof(struct mtk_dai_hw_gain_priv),
				     GFP_KERNEL);
		if (!hw_gain_priv)
			return -ENOMEM;

		afe_priv->dai_priv[i] = hw_gain_priv;
	}

	return 0;
}

int mt8188_dai_hw_gain_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_hw_gain_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_hw_gain_driver);

	dai->controls = mtk_dai_hw_gain_controls;
	dai->num_controls = ARRAY_SIZE(mtk_dai_hw_gain_controls);
	dai->dapm_widgets = mtk_dai_hw_gain_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_hw_gain_widgets);
	dai->dapm_routes = mtk_dai_hw_gain_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_hw_gain_routes);

	return init_hw_gain_priv_data(afe);
}
