// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI DMIC I/F Control
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt8188-afe-clk.h"
#include "mt8188-afe-common.h"
#include "mt8188-reg.h"

#define DMIC_MAX_CH (8)

/* DMIC HW gain configuration default value */
#define DMIC_HW_GAIN_DEFAULT_UP_STEP		539597
#define DMIC_HW_GAIN_DEFAULT_SAMPLE_PER_STEP	200
#define DMIC_HW_GAIN_DEFAULT_TARGET		9323306
#define DMIC_HW_GAIN_DEFAULT_CURRENT		524288

enum {
	DMIC0 = 0,
	DMIC1,
	DMIC2,
	DMIC3,
	DMIC_NUM,
};

struct mtk_dai_dmic_ctrl_reg {
	unsigned int con0;
};

struct mtk_dai_dmic_hw_gain_ctrl_reg {
	unsigned int bypass;
	unsigned int cur;
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
};

struct mtk_dai_dmic_hw_gain_priv {
	unsigned int en;
	unsigned int target;
	unsigned int cur;
	unsigned int up_step;
	unsigned int down_step;
	unsigned int sample_per_step;
};

enum {
	MTK_DAI_DMIC_HW_GAIN_EN = 0,
	MTK_DAI_DMIC_HW_GAIN_TARGET,
	MTK_DAI_DMIC_HW_GAIN_CURRENT,
	MTK_DAI_DMIC_HW_GAIN_UP_STEP,
	MTK_DAI_DMIC_HW_GAIN_DOWN_STEP,
	MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP,
};

struct mtk_dai_dmic_priv {
	unsigned int clk_phase_sel_ch[2];
	unsigned int dmic_src_sel[DMIC_MAX_CH];
	unsigned int iir_on;
	unsigned int two_wire_mode;
	unsigned int channels;
	unsigned int dmic_clk_mono;
	unsigned int clk_index[DMIC_NUM];
	struct mtk_dai_dmic_hw_gain_priv hw_gain[DMIC_NUM];
};

static const struct mtk_dai_dmic_ctrl_reg dmic_ctrl_regs[DMIC_NUM] = {
	[DMIC0] = {
		.con0 = AFE_DMIC0_UL_SRC_CON0,
	},
	[DMIC1] = {
		.con0 = AFE_DMIC1_UL_SRC_CON0,
	},
	[DMIC2] = {
		.con0 = AFE_DMIC2_UL_SRC_CON0,
	},
	[DMIC3] = {
		.con0 = AFE_DMIC3_UL_SRC_CON0,
	},
};

static const struct mtk_dai_dmic_ctrl_reg *get_dmic_ctrl_reg(int id)
{
	if ((id < 0) || (id >= DMIC_NUM))
		return NULL;

	return &dmic_ctrl_regs[id];
}

static const struct mtk_dai_dmic_hw_gain_ctrl_reg
	dmic_hw_gain_ctrl_regs[DMIC_NUM] = {
	[DMIC0] = {
		.bypass = DMIC_BYPASS_HW_GAIN,
		.cur = DMIC_GAIN1_CUR,
		.con0 = DMIC_GAIN1_CON0,
		.con1 = DMIC_GAIN1_CON1,
		.con2 = DMIC_GAIN1_CON2,
		.con3 = DMIC_GAIN1_CON3,
	},
	[DMIC1] = {
		.bypass = DMIC_BYPASS_HW_GAIN,
		.cur = DMIC_GAIN2_CUR,
		.con0 = DMIC_GAIN2_CON0,
		.con1 = DMIC_GAIN2_CON1,
		.con2 = DMIC_GAIN2_CON2,
		.con3 = DMIC_GAIN2_CON3,
	},
	[DMIC2] = {
		.bypass = DMIC_BYPASS_HW_GAIN,
		.cur = DMIC_GAIN3_CUR,
		.con0 = DMIC_GAIN3_CON0,
		.con1 = DMIC_GAIN3_CON1,
		.con2 = DMIC_GAIN3_CON2,
		.con3 = DMIC_GAIN3_CON3,
	},
	[DMIC3] = {
		.bypass = DMIC_BYPASS_HW_GAIN,
		.cur = DMIC_GAIN4_CUR,
		.con0 = DMIC_GAIN4_CON0,
		.con1 = DMIC_GAIN4_CON1,
		.con2 = DMIC_GAIN4_CON2,
		.con3 = DMIC_GAIN4_CON3,
	},
};

static const struct mtk_dai_dmic_hw_gain_ctrl_reg
	*get_dmic_hw_gain_ctrl_reg(struct mtk_base_afe *afe, int id)
{
	if ((id < 0) || (id >= DMIC_NUM)) {
		dev_dbg(afe->dev, "%s invalid id\n", __func__);
		return NULL;
	}

	return &dmic_hw_gain_ctrl_regs[id];
}

static void mtk_dai_dmic_enable(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv = NULL;
	const struct mtk_dai_dmic_ctrl_reg *reg = NULL;
	unsigned int channels = dai->channels;
	unsigned int val = 0;
	unsigned int msk = 0;
	unsigned int ch_base;

	if (dai->id < 0)
		return;

	dmic_priv = afe_priv->dai_priv[dai->id];

	val = msk = PWR2_TOP_CON1_DMIC_CKDIV_ON;
	regmap_update_bits(afe->regmap, PWR2_TOP_CON1, msk, val);

	msk = 0;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH1_CTL;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH2_CTL;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_SRC_ON_TMP_CTL;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_SDM_3_LEVEL_CTL;
	val = msk;

	ch_base = dmic_priv->dmic_clk_mono ? 1 : 2;
	if (channels > ch_base * 3) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[3]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base * 2) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[2]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[1]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > 0) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[0]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
}

static void mtk_dai_dmic_disable(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv = NULL;
	const struct mtk_dai_dmic_ctrl_reg *reg = NULL;
	unsigned int channels = 0;
	unsigned int val = 0;
	unsigned int msk = 0;
	unsigned int ch_base;

	if (dai->id < 0)
		return;

	dmic_priv = afe_priv->dai_priv[dai->id];
	channels = dmic_priv->channels;

	msk |= AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH1_CTL;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_MODE_3P25M_CH2_CTL;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_SRC_ON_TMP_CTL;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_IIR_ON_TMP_CTL;
	msk |= AFE_DMIC_UL_SRC_CON0_UL_SDM_3_LEVEL_CTL;

	ch_base = dmic_priv->dmic_clk_mono ? 1 : 2;
	if (channels > ch_base * 3) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[3]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base * 2) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[2]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[1]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > 0) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[0]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}

	msk = PWR2_TOP_CON1_DMIC_CKDIV_ON;
	regmap_update_bits(afe->regmap, PWR2_TOP_CON1, msk, val);
}

static struct mtk_dai_dmic_hw_gain_priv
	*mtk_dai_dmic_hw_gain_get_priv(struct mtk_base_afe *afe,
				       int id, int sub_id)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv =
		afe_priv->dai_priv[MT8188_AFE_IO_DMIC_IN];
	struct mtk_dai_dmic_hw_gain_priv *priv = NULL;

	switch (sub_id) {
	case MTK_DAI_DMIC_HW_GAIN_EN:
		fallthrough;
	case MTK_DAI_DMIC_HW_GAIN_TARGET:
		fallthrough;
	case MTK_DAI_DMIC_HW_GAIN_CURRENT:
		fallthrough;
	case MTK_DAI_DMIC_HW_GAIN_UP_STEP:
		fallthrough;
	case MTK_DAI_DMIC_HW_GAIN_DOWN_STEP:
		fallthrough;
	case MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP:
		priv = &(dmic_priv->hw_gain[id]);
		break;
	default:
		dev_dbg(afe->dev, "%s invalid sub_id\n", __func__);
		break;
	}

	return priv;
}

static void mtk_dai_dmic_hw_gain_byass(struct mtk_base_afe *afe,
	unsigned int id, bool bypass)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	unsigned int msk = 0;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	switch (id) {
	case DMIC0:
		msk = DMIC_BYPASS_HW_GAIN_DMIC1_BYPASS;
		break;
	case DMIC1:
		msk = DMIC_BYPASS_HW_GAIN_DMIC2_BYPASS;
		break;
	case DMIC2:
		msk = DMIC_BYPASS_HW_GAIN_DMIC3_BYPASS;
		break;
	case DMIC3:
		msk = DMIC_BYPASS_HW_GAIN_DMIC4_BYPASS;
		break;
	default:
		return;
	}

	val = (bypass) ? msk : 0;

	regmap_update_bits(afe->regmap, reg->bypass, msk, val);
}

static void mtk_dai_dmic_hw_gain_on(struct mtk_base_afe *afe,
	unsigned int id, bool on)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	unsigned int msk = DMIC_GAIN_CON0_GAIN_ON;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	val = (on) ? DMIC_GAIN_CON0_GAIN_ON : 0;

	regmap_update_bits(afe->regmap, reg->con0, msk, val);
}

static void mtk_dai_dmic_hw_gain_one_heart(struct mtk_base_afe *afe,
	unsigned int id)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	unsigned int msk = 0;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	switch (id) {
	case DMIC0:
		/* dmic0 is group leader and no one heart bit */
		break;
	case DMIC1:
		msk = DMIC_BYPASS_HW_GAIN2_ONE_HEART;
		break;
	case DMIC2:
		msk = DMIC_BYPASS_HW_GAIN3_ONE_HEART;
		break;
	case DMIC3:
		msk = DMIC_BYPASS_HW_GAIN4_ONE_HEART;
		break;
	default:
		return;
	}

	val = msk;

	regmap_update_bits(afe->regmap, reg->bypass, msk, val);
}

static void mtk_dai_dmic_hw_gain_target(struct mtk_base_afe *afe,
	unsigned int id)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv;
	unsigned int msk = DMIC_GAIN_CON1_TARGET_MASK;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_priv = mtk_dai_dmic_hw_gain_get_priv(afe, id,
				MTK_DAI_DMIC_HW_GAIN_TARGET);
	if (!hw_gain_priv)
		return;

	val = hw_gain_priv->target;

	regmap_update_bits(afe->regmap, reg->con1, msk, val);
}

static void mtk_dai_dmic_hw_gain_current(struct mtk_base_afe *afe,
	unsigned int id)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv;
	unsigned int msk = DMIC_GAIN_CUR_GAIN_MASK;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_priv = mtk_dai_dmic_hw_gain_get_priv(afe, id,
				MTK_DAI_DMIC_HW_GAIN_CURRENT);
	if (!hw_gain_priv)
		return;

	val = hw_gain_priv->cur;

	regmap_update_bits(afe->regmap, reg->cur, msk, val);
}

static void mtk_dai_dmic_hw_gain_up_step(struct mtk_base_afe *afe,
	unsigned int id)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv;
	unsigned int msk = DMIC_GAIN_CON3_UP_STEP;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_priv = mtk_dai_dmic_hw_gain_get_priv(afe, id,
				MTK_DAI_DMIC_HW_GAIN_UP_STEP);
	if (!hw_gain_priv)
		return;

	val = hw_gain_priv->up_step;

	regmap_update_bits(afe->regmap, reg->con3, msk, val);
}

static void mtk_dai_dmic_hw_gain_down_step(struct mtk_base_afe *afe,
	unsigned int id)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv;
	unsigned int msk = DMIC_GAIN_CON2_DOWN_STEP;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_priv = mtk_dai_dmic_hw_gain_get_priv(afe, id,
				MTK_DAI_DMIC_HW_GAIN_DOWN_STEP);
	if (!hw_gain_priv)
		return;

	val = hw_gain_priv->down_step;

	regmap_update_bits(afe->regmap, reg->con2, msk, val);
}

static void mtk_dai_dmic_hw_gain_sample_per_step(struct mtk_base_afe *afe,
	unsigned int id)
{
	const struct mtk_dai_dmic_hw_gain_ctrl_reg *reg;
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv;
	unsigned int msk = DMIC_GAIN_CON0_SAMPLE_PER_STEP_MASK;
	unsigned int val;

	reg = get_dmic_hw_gain_ctrl_reg(afe, id);
	if (!reg)
		return;

	hw_gain_priv = mtk_dai_dmic_hw_gain_get_priv(afe, id,
				MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP);
	if (!hw_gain_priv)
		return;

	val = DMIC_GAIN_CON0_SAMPLE_PER_STEP(hw_gain_priv->sample_per_step);

	regmap_update_bits(afe->regmap, reg->con0, msk, val);
}

static void mtk_dai_dmic_hw_gain_enable(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, bool enable)
{
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv;
	unsigned int channels = dai->channels;
	unsigned int begin = DMIC0;
	unsigned int end = 0;
	unsigned int id = 0;

	if (!channels)
		return;

	if (channels > 0)
		end = DMIC0;
	if (channels > 2)
		end = DMIC1;
	if (channels > 4)
		end = DMIC2;
	if (channels > 6)
		end = DMIC3;

	for (id = begin; id <= end; id++) {
		hw_gain_priv = mtk_dai_dmic_hw_gain_get_priv(afe, id,
					MTK_DAI_DMIC_HW_GAIN_EN);
		if (hw_gain_priv && hw_gain_priv->en) {
			if (enable) {
				mtk_dai_dmic_hw_gain_byass(afe, id, !enable);
				mtk_dai_dmic_hw_gain_on(afe, id, enable);
			} else {
				mtk_dai_dmic_hw_gain_on(afe, id, enable);
				mtk_dai_dmic_hw_gain_byass(afe, id, !enable);
			}
		}
	}
}

static const struct reg_sequence mtk_dai_dmic_iir_coeff_reg_defaults[] = {
	{ AFE_DMIC0_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC0_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC0_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC0_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC1_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC1_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC1_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC1_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC2_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC2_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC2_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC2_IIR_COEF_10_09, 0x0000C048 },
	{ AFE_DMIC3_IIR_COEF_02_01, 0x00000000 },
	{ AFE_DMIC3_IIR_COEF_04_03, 0x00003FB8 },
	{ AFE_DMIC3_IIR_COEF_06_05, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_08_07, 0x3FB80000 },
	{ AFE_DMIC3_IIR_COEF_10_09, 0x0000C048 },
};

static int mtk_dai_dmic_load_iir_coeff_table(struct mtk_base_afe *afe)
{
	return regmap_multi_reg_write(afe->regmap,
			mtk_dai_dmic_iir_coeff_reg_defaults,
			ARRAY_SIZE(mtk_dai_dmic_iir_coeff_reg_defaults));
}

static int mtk_dai_dmic_configure_array(struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv = NULL;
	unsigned int *dmic_src_sel = NULL;
	unsigned int mask =
			PWR2_TOP_CON_DMIC8_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC7_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC6_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC5_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC4_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC3_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC2_SRC_SEL_MASK |
			PWR2_TOP_CON_DMIC1_SRC_SEL_MASK;
	unsigned int val = 0;

	if (dai->id < 0)
		return -EINVAL;

	dmic_priv = afe_priv->dai_priv[dai->id];
	dmic_src_sel = dmic_priv->dmic_src_sel;
	val = PWR2_TOP_CON_DMIC8_SRC_SEL_VAL(dmic_src_sel[7]) |
	      PWR2_TOP_CON_DMIC7_SRC_SEL_VAL(dmic_src_sel[6]) |
	      PWR2_TOP_CON_DMIC6_SRC_SEL_VAL(dmic_src_sel[5]) |
	      PWR2_TOP_CON_DMIC5_SRC_SEL_VAL(dmic_src_sel[4]) |
	      PWR2_TOP_CON_DMIC4_SRC_SEL_VAL(dmic_src_sel[3]) |
	      PWR2_TOP_CON_DMIC3_SRC_SEL_VAL(dmic_src_sel[2]) |
	      PWR2_TOP_CON_DMIC2_SRC_SEL_VAL(dmic_src_sel[1]) |
	      PWR2_TOP_CON_DMIC1_SRC_SEL_VAL(dmic_src_sel[0]);

	regmap_update_bits(afe->regmap, PWR2_TOP_CON0, mask, val);

	return 0;
}

static int mtk_dai_dmic_configure(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv = NULL;
	unsigned int rate = dai->rate;
	unsigned int channels = dai->channels;
	unsigned int two_wire_mode = 0;
	unsigned int clk_phase_sel_ch1 = 0;
	unsigned int clk_phase_sel_ch2 = 0;
	unsigned int iir_on = 0;
	const struct mtk_dai_dmic_ctrl_reg *reg = NULL;
	unsigned int val = 0;
	unsigned int msk = 0;
	unsigned int ch_base;

	if (dai->id < 0)
		return -EINVAL;

	dmic_priv = afe_priv->dai_priv[dai->id];
	two_wire_mode = dmic_priv->two_wire_mode;
	clk_phase_sel_ch1 = dmic_priv->clk_phase_sel_ch[0];
	clk_phase_sel_ch2 = dmic_priv->clk_phase_sel_ch[1];
	iir_on = dmic_priv->iir_on;

	if (two_wire_mode)
		val |= AFE_DMIC_UL_SRC_CON0_UL_TWO_WIRE_MODE_CTL;
	else
		clk_phase_sel_ch2 = clk_phase_sel_ch1 + 4;

	msk |= AFE_DMIC_UL_SRC_CON0_UL_TWO_WIRE_MODE_CTL_MASK;

	val |= AFE_DMIC_UL_SRC_CON0_UL_PHASE_SEL_CH1(clk_phase_sel_ch1);
	val |= AFE_DMIC_UL_SRC_CON0_UL_PHASE_SEL_CH2(clk_phase_sel_ch2);
	msk |= AFE_DMIC_UL_SRC_CON0_UL_PHASE_SEL_MASK;

	mtk_dai_dmic_configure_array(dai);

	switch (rate) {
	case 96000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_96K;
		break;
	case 48000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_48K;
		break;
	case 32000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_32K;
		break;
	case 16000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_16K;
		break;
	case 8000:
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_8K;
		break;
	default:
		dev_dbg(afe->dev, "%s invalid rate %u, use 48000Hz\n", __func__, rate);
		val |= AFE_DMIC_UL_CON0_VOCIE_MODE_48K;
		break;
	}
	msk |= AFE_DMIC_UL_VOICE_MODE_MASK;

	if (iir_on) {
		mtk_dai_dmic_load_iir_coeff_table(afe);
		val |= AFE_DMIC_UL_SRC_CON0_UL_IIR_MODE_CTL(0);
		val |= AFE_DMIC_UL_SRC_CON0_UL_IIR_ON_TMP_CTL;
		msk |= AFE_DMIC_UL_SRC_CON0_UL_IIR_MODE_CTL_MASK;
		msk |= AFE_DMIC_UL_SRC_CON0_UL_IIR_ON_TMP_CTL_MASK;
	}

	ch_base = dmic_priv->dmic_clk_mono ? 1 : 2;
	if (channels > ch_base * 3) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[3]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base * 2) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[2]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > ch_base) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[1]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}
	if (channels > 0) {
		reg = get_dmic_ctrl_reg(dmic_priv->clk_index[0]);
		if (reg)
			regmap_update_bits(afe->regmap, reg->con0, msk, val);
	}

	dmic_priv->channels = channels;

	return 0;
}

static int mtk_dai_dmic_hw_gain_configure(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv;
	unsigned int channels = dai->channels;
	unsigned int begin = DMIC0;
	unsigned int end = 0;
	unsigned int id = 0;

	if (!channels) {
		dev_dbg(afe->dev, "%s invalid channels\n", __func__);
		return -EINVAL;
	}

	if (channels > 0)
		end = DMIC0;
	if (channels > 2)
		end = DMIC1;
	if (channels > 4)
		end = DMIC2;
	if (channels > 6)
		end = DMIC3;

	for (id = begin; id <= end; id++) {
		hw_gain_priv = mtk_dai_dmic_hw_gain_get_priv(afe, id,
					MTK_DAI_DMIC_HW_GAIN_EN);
		if (hw_gain_priv && hw_gain_priv->en) {
			mtk_dai_dmic_hw_gain_one_heart(afe, id);
			mtk_dai_dmic_hw_gain_target(afe, id);
			mtk_dai_dmic_hw_gain_current(afe, id);
			mtk_dai_dmic_hw_gain_up_step(afe, id);
			mtk_dai_dmic_hw_gain_down_step(afe, id);
			mtk_dai_dmic_hw_gain_sample_per_step(afe, id);
		}
	}

	return 0;
}

/* dai ops */
static int mtk_dai_dmic_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC1]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC2]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC3]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC4]);

	return 0;
}

static void mtk_dai_dmic_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	mtk_dai_dmic_disable(afe, dai);
	mtk_dai_dmic_hw_gain_enable(afe, dai, false);

	usleep_range(125, 126);

	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC4]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC3]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC2]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE_DMIC1]);
}

static int mtk_dai_dmic_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	ret = mtk_dai_dmic_hw_gain_configure(afe, substream, dai);
	if (ret)
		return ret;

	ret = mtk_dai_dmic_configure(afe, substream, dai);
	if (ret)
		return ret;

	mtk_dai_dmic_hw_gain_enable(afe, dai, true);
	mtk_dai_dmic_enable(afe, dai);

	return 0;
}

static const struct snd_soc_dai_ops mtk_dai_dmic_ops = {
	.startup	= mtk_dai_dmic_startup,
	.shutdown	= mtk_dai_dmic_shutdown,
	.prepare	= mtk_dai_dmic_prepare,
};

/* dai driver */
#define MTK_DMIC_RATES (SNDRV_PCM_RATE_8000 |\
		       SNDRV_PCM_RATE_16000 |\
		       SNDRV_PCM_RATE_32000 |\
		       SNDRV_PCM_RATE_48000 |\
		       SNDRV_PCM_RATE_96000)

#define MTK_DMIC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_dmic_driver[] = {
	{
		.name = "DMIC",
		.id = MT8188_AFE_IO_DMIC_IN,
		.capture = {
			.stream_name = "DMIC Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_DMIC_RATES,
			.formats = MTK_DMIC_FORMATS,
		},
		.ops = &mtk_dai_dmic_ops,
	},
};

static const struct snd_soc_dapm_widget mtk_dai_dmic_widgets[] = {
	SND_SOC_DAPM_MIXER("I004", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I005", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I006", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I007", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I008", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I009", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I010", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I011", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_INPUT("DMIC_INPUT"),
};

static const struct snd_soc_dapm_route mtk_dai_dmic_routes[] = {
	{"I004", NULL, "DMIC Capture"},
	{"I005", NULL, "DMIC Capture"},
	{"I006", NULL, "DMIC Capture"},
	{"I007", NULL, "DMIC Capture"},
	{"I008", NULL, "DMIC Capture"},
	{"I009", NULL, "DMIC Capture"},
	{"I010", NULL, "DMIC Capture"},
	{"I011", NULL, "DMIC Capture"},

	{"DMIC Capture", NULL, "DMIC_INPUT"},
};

static int mtk_dai_dmic_hw_gain_ctrl_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->id.device;
	unsigned int sub_id = kcontrol->id.subdevice;
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv =
		mtk_dai_dmic_hw_gain_get_priv(afe, id, sub_id);
	long val = 0;

	if (!hw_gain_priv)
		return -EINVAL;

	switch (sub_id) {
	case MTK_DAI_DMIC_HW_GAIN_EN:
		val = hw_gain_priv->en;
		break;
	case MTK_DAI_DMIC_HW_GAIN_TARGET:
		val = hw_gain_priv->target;
		break;
	case MTK_DAI_DMIC_HW_GAIN_CURRENT:
		val = hw_gain_priv->cur;
		break;
	case MTK_DAI_DMIC_HW_GAIN_UP_STEP:
		val = hw_gain_priv->up_step;
		break;
	case MTK_DAI_DMIC_HW_GAIN_DOWN_STEP:
		val = hw_gain_priv->down_step;
		break;
	case MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP:
		val = hw_gain_priv->sample_per_step;
		break;
	default:
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int mtk_dai_dmic_hw_gain_ctrl_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->id.device;
	unsigned int sub_id = kcontrol->id.subdevice;
	struct mtk_dai_dmic_hw_gain_priv *hw_gain_priv =
		mtk_dai_dmic_hw_gain_get_priv(afe, id, sub_id);
	long val = ucontrol->value.integer.value[0];
	unsigned int *cached = 0;

	if (!hw_gain_priv)
		return -EINVAL;

	switch (sub_id) {
	case MTK_DAI_DMIC_HW_GAIN_EN:
		cached = &hw_gain_priv->en;
		break;
	case MTK_DAI_DMIC_HW_GAIN_TARGET:
		cached = &hw_gain_priv->target;
		break;
	case MTK_DAI_DMIC_HW_GAIN_CURRENT:
		cached = &hw_gain_priv->cur;
		break;
	case MTK_DAI_DMIC_HW_GAIN_UP_STEP:
		cached = &hw_gain_priv->up_step;
		break;
	case MTK_DAI_DMIC_HW_GAIN_DOWN_STEP:
		cached = &hw_gain_priv->down_step;
		break;
	case MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP:
		cached = &hw_gain_priv->sample_per_step;
		break;
	default:
		return -EINVAL;
	}

	if (val == *cached)
		return 0;

	*cached = val;

	return 0;
}

static const struct snd_kcontrol_new mtk_dai_dmic_controls[] = {
	MT8188_SOC_SINGLE_EXT("dmic1_hw_gain_en",
			      SND_SOC_NOPM, 0, 1, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC0,
			      MTK_DAI_DMIC_HW_GAIN_EN),
	MT8188_SOC_SINGLE_EXT("dmic2_hw_gain_en",
			      SND_SOC_NOPM, 0, 1, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC1,
			      MTK_DAI_DMIC_HW_GAIN_EN),
	MT8188_SOC_SINGLE_EXT("dmic3_hw_gain_en",
			      SND_SOC_NOPM, 0, 1, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC2,
			      MTK_DAI_DMIC_HW_GAIN_EN),
	MT8188_SOC_SINGLE_EXT("dmic4_hw_gain_en",
			      SND_SOC_NOPM, 0, 1, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC3,
			      MTK_DAI_DMIC_HW_GAIN_EN),
	MT8188_SOC_SINGLE_EXT("dmic1_hw_gain_target",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC0,
			      MTK_DAI_DMIC_HW_GAIN_TARGET),
	MT8188_SOC_SINGLE_EXT("dmic2_hw_gain_target",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC1,
			      MTK_DAI_DMIC_HW_GAIN_TARGET),
	MT8188_SOC_SINGLE_EXT("dmic3_hw_gain_target",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC2,
			      MTK_DAI_DMIC_HW_GAIN_TARGET),
	MT8188_SOC_SINGLE_EXT("dmic4_hw_gain_target",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC3,
			      MTK_DAI_DMIC_HW_GAIN_TARGET),
	MT8188_SOC_SINGLE_EXT("dmic1_hw_gain_current",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC0,
			      MTK_DAI_DMIC_HW_GAIN_CURRENT),
	MT8188_SOC_SINGLE_EXT("dmic2_hw_gain_current",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC1,
			      MTK_DAI_DMIC_HW_GAIN_CURRENT),
	MT8188_SOC_SINGLE_EXT("dmic3_hw_gain_current",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC2,
			      MTK_DAI_DMIC_HW_GAIN_CURRENT),
	MT8188_SOC_SINGLE_EXT("dmic4_hw_gain_current",
			      SND_SOC_NOPM, 0, 0x0fffffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC3,
			      MTK_DAI_DMIC_HW_GAIN_CURRENT),
	MT8188_SOC_SINGLE_EXT("dmic1_hw_gain_up_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC0,
			      MTK_DAI_DMIC_HW_GAIN_UP_STEP),
	MT8188_SOC_SINGLE_EXT("dmic2_hw_gain_up_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC1,
			      MTK_DAI_DMIC_HW_GAIN_UP_STEP),
	MT8188_SOC_SINGLE_EXT("dmic3_hw_gain_up_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC2,
			      MTK_DAI_DMIC_HW_GAIN_UP_STEP),
	MT8188_SOC_SINGLE_EXT("dmic4_hw_gain_up_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC3,
			      MTK_DAI_DMIC_HW_GAIN_UP_STEP),
	MT8188_SOC_SINGLE_EXT("dmic1_hw_gain_down_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC0,
			      MTK_DAI_DMIC_HW_GAIN_DOWN_STEP),
	MT8188_SOC_SINGLE_EXT("dmic2_hw_gain_down_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC1,
			      MTK_DAI_DMIC_HW_GAIN_DOWN_STEP),
	MT8188_SOC_SINGLE_EXT("dmic3_hw_gain_down_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC2,
			      MTK_DAI_DMIC_HW_GAIN_DOWN_STEP),
	MT8188_SOC_SINGLE_EXT("dmic4_hw_gain_down_step",
			      SND_SOC_NOPM, 0, 0x000fffff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC3,
			      MTK_DAI_DMIC_HW_GAIN_DOWN_STEP),
	MT8188_SOC_SINGLE_EXT("dmic1_hw_gain_sample_per_step",
			      SND_SOC_NOPM, 0, 0x000000ff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC0,
			      MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP),
	MT8188_SOC_SINGLE_EXT("dmic2_hw_gain_sample_per_step",
			      SND_SOC_NOPM, 0, 0x000000ff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC1,
			      MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP),
	MT8188_SOC_SINGLE_EXT("dmic3_hw_gain_sample_per_step",
			      SND_SOC_NOPM, 0, 0x000000ff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC2,
			      MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP),
	MT8188_SOC_SINGLE_EXT("dmic4_hw_gain_sample_per_step",
			      SND_SOC_NOPM, 0, 0x000000ff, 0,
			      mtk_dai_dmic_hw_gain_ctrl_get,
			      mtk_dai_dmic_hw_gain_ctrl_put,
			      DMIC3,
			      MTK_DAI_DMIC_HW_GAIN_SAMPLE_PER_STEP),
};


static int init_dmic_priv_data(struct mtk_base_afe *afe)
{
	const struct device_node *of_node = afe->dev->of_node;
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_dmic_priv *dmic_priv;
	unsigned int dmic_gain_en[DMIC_NUM];
	int ret = 0, id;

	dmic_priv = devm_kzalloc(afe->dev, sizeof(struct mtk_dai_dmic_priv),
				 GFP_KERNEL);
	if (!dmic_priv)
		return -ENOMEM;

	dmic_priv->two_wire_mode = of_property_read_bool(of_node,
		"mediatek,dmic-two-wire-mode");

	ret = of_property_read_u32_array(of_node, "mediatek,dmic-clk-phases",
				&dmic_priv->clk_phase_sel_ch[0],
					 2);
	if ((ret != 0) && !dmic_priv->two_wire_mode) {
		dmic_priv->clk_phase_sel_ch[0] = 0;
		dmic_priv->clk_phase_sel_ch[1] = 4;
	}

	ret = of_property_read_u32_array(of_node, "mediatek,dmic-src-sels",
				&dmic_priv->dmic_src_sel[0], DMIC_MAX_CH);
	if (ret != 0) {
		if (dmic_priv->two_wire_mode) {
			dmic_priv->dmic_src_sel[0] = 0;
			dmic_priv->dmic_src_sel[1] = 1;
			dmic_priv->dmic_src_sel[2] = 2;
			dmic_priv->dmic_src_sel[3] = 3;
			dmic_priv->dmic_src_sel[4] = 4;
			dmic_priv->dmic_src_sel[5] = 5;
			dmic_priv->dmic_src_sel[6] = 6;
			dmic_priv->dmic_src_sel[7] = 7;
		} else {
			dmic_priv->dmic_src_sel[0] = 0;
			dmic_priv->dmic_src_sel[2] = 2;
			dmic_priv->dmic_src_sel[4] = 4;
			dmic_priv->dmic_src_sel[6] = 6;
		}
	}

	dmic_priv->iir_on = of_property_read_bool(of_node,
						  "mediatek,dmic-iir-on");

	dmic_priv->dmic_clk_mono = of_property_read_bool(of_node,
							 "mediatek,dmic-clk-mono");

	ret = of_property_read_u32_array(of_node, "mediatek,dmic-clk-index",
					 &dmic_priv->clk_index[0], DMIC_NUM);
	if (ret) {
		dmic_priv->clk_index[0] = DMIC0;
		dmic_priv->clk_index[1] = DMIC1;
		dmic_priv->clk_index[2] = DMIC2;
		dmic_priv->clk_index[3] = DMIC3;
	}

	ret = of_property_read_u32_array(of_node, "mediatek,dmic-gain-en",
					 &dmic_gain_en[0], DMIC_NUM);
	if (!ret) {
		for (id = DMIC0; id < DMIC_NUM; id++) {
			if (dmic_gain_en[id] == 0)
				continue;

			dmic_priv->hw_gain[id].en = 1;
			dmic_priv->hw_gain[id].up_step =
					DMIC_HW_GAIN_DEFAULT_UP_STEP;
			dmic_priv->hw_gain[id].sample_per_step =
					DMIC_HW_GAIN_DEFAULT_SAMPLE_PER_STEP;
			dmic_priv->hw_gain[id].target =
					DMIC_HW_GAIN_DEFAULT_TARGET;
			dmic_priv->hw_gain[id].cur =
					DMIC_HW_GAIN_DEFAULT_CURRENT;
		}
	}

	afe_priv->dai_priv[MT8188_AFE_IO_DMIC_IN] = dmic_priv;
	return 0;
}

int mt8188_dai_dmic_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_dmic_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_dmic_driver);

	dai->dapm_widgets = mtk_dai_dmic_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_dmic_widgets);
	dai->dapm_routes = mtk_dai_dmic_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_dmic_routes);
	dai->controls = mtk_dai_dmic_controls;
	dai->num_controls = ARRAY_SIZE(mtk_dai_dmic_controls);

	return init_dmic_priv_data(afe);
}

