// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek ALSA SoC Audio DAI GASRC Control
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/regmap.h>
#include <sound/pcm_params.h>
#include "mt8188-afe-clk.h"
#include "mt8188-afe-common.h"
#include "mt8188-reg.h"

#define GASRC_TO_DAI_ID(x)    (x + MT8188_AFE_IO_GASRC_START)
#define DAI_TO_GASRC_ID(x)    (x - MT8188_AFE_IO_GASRC_START)
#define DAI_TO_GASRC_CG_ID(x) (DAI_TO_GASRC_ID(x) + MT8188_CLK_AUD_GASRC0)

struct mtk_dai_gasrc_priv {
	unsigned int in_asys_timing_sel;
	unsigned int out_asys_timing_sel;
	bool cali_tx;
	bool cali_rx;
	bool iir_on;
	bool duplex;
	bool op_freq_45m;
	unsigned int cali_cycles;
};

struct mtk_dai_gasrc_ctrl_reg {
	unsigned int con0;
	unsigned int con1;
	unsigned int con2;
	unsigned int con3;
	unsigned int con4;
	unsigned int con5;
	unsigned int con6;
	unsigned int con7;
	unsigned int con8;
	unsigned int con9;
	unsigned int con10;
	unsigned int con11;
};

static const struct mtk_dai_gasrc_ctrl_reg
	gasrc_ctrl_reg[MT8188_AFE_IO_GASRC_NUM] = {
	[0] = {
		.con0 = AFE_GASRC0_NEW_CON0,
		.con1 = AFE_GASRC0_NEW_CON1,
		.con2 = AFE_GASRC0_NEW_CON2,
		.con3 = AFE_GASRC0_NEW_CON3,
		.con4 = AFE_GASRC0_NEW_CON4,
		.con5 = AFE_GASRC0_NEW_CON5,
		.con6 = AFE_GASRC0_NEW_CON6,
		.con7 = AFE_GASRC0_NEW_CON7,
		.con8 = AFE_GASRC0_NEW_CON8,
		.con9 = AFE_GASRC0_NEW_CON9,
		.con10 = AFE_GASRC0_NEW_CON10,
		.con11 = AFE_GASRC0_NEW_CON11,
	},
	[1] = {
		.con0 = AFE_GASRC1_NEW_CON0,
		.con1 = AFE_GASRC1_NEW_CON1,
		.con2 = AFE_GASRC1_NEW_CON2,
		.con3 = AFE_GASRC1_NEW_CON3,
		.con4 = AFE_GASRC1_NEW_CON4,
		.con5 = AFE_GASRC1_NEW_CON5,
		.con6 = AFE_GASRC1_NEW_CON6,
		.con7 = AFE_GASRC1_NEW_CON7,
		.con8 = AFE_GASRC1_NEW_CON8,
		.con9 = AFE_GASRC1_NEW_CON9,
		.con10 = AFE_GASRC1_NEW_CON10,
		.con11 = AFE_GASRC1_NEW_CON11,
	},
	[2] = {
		.con0 = AFE_GASRC2_NEW_CON0,
		.con1 = AFE_GASRC2_NEW_CON1,
		.con2 = AFE_GASRC2_NEW_CON2,
		.con3 = AFE_GASRC2_NEW_CON3,
		.con4 = AFE_GASRC2_NEW_CON4,
		.con5 = AFE_GASRC2_NEW_CON5,
		.con6 = AFE_GASRC2_NEW_CON6,
		.con7 = AFE_GASRC2_NEW_CON7,
		.con8 = AFE_GASRC2_NEW_CON8,
		.con9 = AFE_GASRC2_NEW_CON9,
		.con10 = AFE_GASRC2_NEW_CON10,
		.con11 = AFE_GASRC2_NEW_CON11,
	},
	[3] = {
		.con0 = AFE_GASRC3_NEW_CON0,
		.con1 = AFE_GASRC3_NEW_CON1,
		.con2 = AFE_GASRC3_NEW_CON2,
		.con3 = AFE_GASRC3_NEW_CON3,
		.con4 = AFE_GASRC3_NEW_CON4,
		.con5 = AFE_GASRC3_NEW_CON5,
		.con6 = AFE_GASRC3_NEW_CON6,
		.con7 = AFE_GASRC3_NEW_CON7,
		.con8 = AFE_GASRC3_NEW_CON8,
		.con9 = AFE_GASRC3_NEW_CON9,
		.con10 = AFE_GASRC3_NEW_CON10,
		.con11 = AFE_GASRC3_NEW_CON11,
	},
	[4] = {
		.con0 = AFE_GASRC4_NEW_CON0,
		.con1 = AFE_GASRC4_NEW_CON1,
		.con2 = AFE_GASRC4_NEW_CON2,
		.con3 = AFE_GASRC4_NEW_CON3,
		.con4 = AFE_GASRC4_NEW_CON4,
		.con5 = AFE_GASRC4_NEW_CON5,
		.con6 = AFE_GASRC4_NEW_CON6,
		.con7 = AFE_GASRC4_NEW_CON7,
		.con8 = AFE_GASRC4_NEW_CON8,
		.con9 = AFE_GASRC4_NEW_CON9,
		.con10 = AFE_GASRC4_NEW_CON10,
		.con11 = AFE_GASRC4_NEW_CON11,
	},
	[5] = {
		.con0 = AFE_GASRC5_NEW_CON0,
		.con1 = AFE_GASRC5_NEW_CON1,
		.con2 = AFE_GASRC5_NEW_CON2,
		.con3 = AFE_GASRC5_NEW_CON3,
		.con4 = AFE_GASRC5_NEW_CON4,
		.con5 = AFE_GASRC5_NEW_CON5,
		.con6 = AFE_GASRC5_NEW_CON6,
		.con7 = AFE_GASRC5_NEW_CON7,
		.con8 = AFE_GASRC5_NEW_CON8,
		.con9 = AFE_GASRC5_NEW_CON9,
		.con10 = AFE_GASRC5_NEW_CON10,
		.con11 = AFE_GASRC5_NEW_CON11,
	},
	[6] = {
		.con0 = AFE_GASRC6_NEW_CON0,
		.con1 = AFE_GASRC6_NEW_CON1,
		.con2 = AFE_GASRC6_NEW_CON2,
		.con3 = AFE_GASRC6_NEW_CON3,
		.con4 = AFE_GASRC6_NEW_CON4,
		.con5 = AFE_GASRC6_NEW_CON5,
		.con6 = AFE_GASRC6_NEW_CON6,
		.con7 = AFE_GASRC6_NEW_CON7,
		.con8 = AFE_GASRC6_NEW_CON8,
		.con9 = AFE_GASRC6_NEW_CON9,
		.con10 = AFE_GASRC6_NEW_CON10,
		.con11 = AFE_GASRC6_NEW_CON11,
	},
	[7] = {
		.con0 = AFE_GASRC7_NEW_CON0,
		.con1 = AFE_GASRC7_NEW_CON1,
		.con2 = AFE_GASRC7_NEW_CON2,
		.con3 = AFE_GASRC7_NEW_CON3,
		.con4 = AFE_GASRC7_NEW_CON4,
		.con5 = AFE_GASRC7_NEW_CON5,
		.con6 = AFE_GASRC7_NEW_CON6,
		.con7 = AFE_GASRC7_NEW_CON7,
		.con8 = AFE_GASRC7_NEW_CON8,
		.con9 = AFE_GASRC7_NEW_CON9,
		.con10 = AFE_GASRC7_NEW_CON10,
		.con11 = AFE_GASRC7_NEW_CON11,
	},
	[8] = {
		.con0 = AFE_GASRC8_NEW_CON0,
		.con1 = AFE_GASRC8_NEW_CON1,
		.con2 = AFE_GASRC8_NEW_CON2,
		.con3 = AFE_GASRC8_NEW_CON3,
		.con4 = AFE_GASRC8_NEW_CON4,
		.con5 = AFE_GASRC8_NEW_CON5,
		.con6 = AFE_GASRC8_NEW_CON6,
		.con7 = AFE_GASRC8_NEW_CON7,
		.con8 = AFE_GASRC8_NEW_CON8,
		.con9 = AFE_GASRC8_NEW_CON9,
		.con10 = AFE_GASRC8_NEW_CON10,
		.con11 = AFE_GASRC8_NEW_CON11,
	},
	[9] = {
		.con0 = AFE_GASRC9_NEW_CON0,
		.con1 = AFE_GASRC9_NEW_CON1,
		.con2 = AFE_GASRC9_NEW_CON2,
		.con3 = AFE_GASRC9_NEW_CON3,
		.con4 = AFE_GASRC9_NEW_CON4,
		.con5 = AFE_GASRC9_NEW_CON5,
		.con6 = AFE_GASRC9_NEW_CON6,
		.con7 = AFE_GASRC9_NEW_CON7,
		.con8 = AFE_GASRC9_NEW_CON8,
		.con9 = AFE_GASRC9_NEW_CON9,
		.con10 = AFE_GASRC9_NEW_CON10,
		.con11 = AFE_GASRC9_NEW_CON11,
	},
	[10] = {
		.con0 = AFE_GASRC10_NEW_CON0,
		.con1 = AFE_GASRC10_NEW_CON1,
		.con2 = AFE_GASRC10_NEW_CON2,
		.con3 = AFE_GASRC10_NEW_CON3,
		.con4 = AFE_GASRC10_NEW_CON4,
		.con5 = AFE_GASRC10_NEW_CON5,
		.con6 = AFE_GASRC10_NEW_CON6,
		.con7 = AFE_GASRC10_NEW_CON7,
		.con8 = AFE_GASRC10_NEW_CON8,
		.con9 = AFE_GASRC10_NEW_CON9,
		.con10 = AFE_GASRC10_NEW_CON10,
		.con11 = AFE_GASRC10_NEW_CON11,
	},
	[11] = {
		.con0 = AFE_GASRC11_NEW_CON0,
		.con1 = AFE_GASRC11_NEW_CON1,
		.con2 = AFE_GASRC11_NEW_CON2,
		.con3 = AFE_GASRC11_NEW_CON3,
		.con4 = AFE_GASRC11_NEW_CON4,
		.con5 = AFE_GASRC11_NEW_CON5,
		.con6 = AFE_GASRC11_NEW_CON6,
		.con7 = AFE_GASRC11_NEW_CON7,
		.con8 = AFE_GASRC11_NEW_CON8,
		.con9 = AFE_GASRC11_NEW_CON9,
		.con10 = AFE_GASRC11_NEW_CON10,
		.con11 = AFE_GASRC11_NEW_CON11,
	},
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o096_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN96, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN96, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN96, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I022 Switch", AFE_CONN96, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I046 Switch", AFE_CONN96_1, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN96_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I072 Switch", AFE_CONN96_2, 8, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o097_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN97, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN97, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN97, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I023 Switch", AFE_CONN97, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I047 Switch", AFE_CONN97_1, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN97_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I073 Switch", AFE_CONN97_2, 9, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o098_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN98, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN98, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I014 Switch", AFE_CONN98, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN98, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I024 Switch", AFE_CONN98, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I048 Switch", AFE_CONN98_1, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN98_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I074 Switch", AFE_CONN98_2, 10, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o099_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN99, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN99, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I015 Switch", AFE_CONN99, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN99, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I025 Switch", AFE_CONN99, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I049 Switch", AFE_CONN99_1, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN99_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I075 Switch", AFE_CONN99_2, 11, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o100_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN100, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN100, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I016 Switch", AFE_CONN100, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN100, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I026 Switch", AFE_CONN100, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I050 Switch", AFE_CONN100_1, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN100_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I076 Switch", AFE_CONN100_2, 12, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o101_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN101, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN101, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I017 Switch", AFE_CONN101, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN101, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I027 Switch", AFE_CONN101, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I051 Switch", AFE_CONN101_1, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN101_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I077 Switch", AFE_CONN101_2, 13, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o102_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN102, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN102, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I018 Switch", AFE_CONN102, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN102, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I028 Switch", AFE_CONN102, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I052 Switch", AFE_CONN102_1, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN102_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I078 Switch", AFE_CONN102_2, 14, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o103_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN103, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN103, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I019 Switch", AFE_CONN103, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN103, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I029 Switch", AFE_CONN103, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I053 Switch", AFE_CONN103_1, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN103_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I079 Switch", AFE_CONN103_2, 15, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o104_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN104, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN104, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN104, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I030 Switch", AFE_CONN104, 30, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I054 Switch", AFE_CONN104_1, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN104_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I080 Switch", AFE_CONN104_2, 16, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o105_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN105, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN105, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN105, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I031 Switch", AFE_CONN105, 31, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I055 Switch", AFE_CONN105_1, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN105_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I081 Switch", AFE_CONN105_2, 17, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o106_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN106, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN106, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN106, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I032 Switch", AFE_CONN106_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I056 Switch", AFE_CONN106_1, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN106_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I082 Switch", AFE_CONN106_2, 18, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o107_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN107, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN107, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN107, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I033 Switch", AFE_CONN107_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I057 Switch", AFE_CONN107_1, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN107_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I083 Switch", AFE_CONN107_2, 19, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o108_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN108, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN108, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN108, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I034 Switch", AFE_CONN108_1, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I058 Switch", AFE_CONN108_1, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN108_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I084 Switch", AFE_CONN108_2, 20, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o109_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN109, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN109, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN109, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I035 Switch", AFE_CONN109_1, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I059 Switch", AFE_CONN109_1, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN109_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I085 Switch", AFE_CONN109_2, 21, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o110_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN110, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN110, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN110, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I036 Switch", AFE_CONN110_1, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I060 Switch", AFE_CONN110_1, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN110_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I086 Switch", AFE_CONN110_2, 22, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o111_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN111, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN111, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN111, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I037 Switch", AFE_CONN111_1, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I061 Switch", AFE_CONN111_1, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN111_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I087 Switch", AFE_CONN111_2, 23, 1, 0),
};
static const struct snd_kcontrol_new mtk_dai_gasrc_o112_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN112, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN112, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN112, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN112_2, 6, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o113_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN113, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN113, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN113, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN113_2, 7, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o114_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN114, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN114, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN114, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN114_2, 6, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o115_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN115, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN115, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN115, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN115_2, 7, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o116_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN116, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN116, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN116, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN116_2, 6, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o117_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN117, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN117, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN117, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN117_2, 7, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o118_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN118, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN118, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN118, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN118_2, 6, 1, 0),
};

static const struct snd_kcontrol_new mtk_dai_gasrc_o119_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN119, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN119, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN119, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN119_2, 7, 1, 0),
};

static const char * const mtk_dai_gasrc_in_out_mux_text[] = {
	"normal", "one_heart",
};

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc0_output_mux_enum,
	AFE_GASRC0_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc0_output_mux =
	SOC_DAPM_ENUM("GASRC0 Sink", mtk_dai_gasrc0_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc1_output_mux_enum,
	AFE_GASRC1_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc1_output_mux =
	SOC_DAPM_ENUM("GASRC1 Sink", mtk_dai_gasrc1_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc2_output_mux_enum,
	AFE_GASRC2_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc2_output_mux =
	SOC_DAPM_ENUM("GASRC2 Sink", mtk_dai_gasrc2_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc3_output_mux_enum,
	AFE_GASRC3_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc3_output_mux =
	SOC_DAPM_ENUM("GASRC3 Sink", mtk_dai_gasrc3_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc4_output_mux_enum,
	AFE_GASRC4_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc4_output_mux =
	SOC_DAPM_ENUM("GASRC4 Sink", mtk_dai_gasrc4_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc5_output_mux_enum,
	AFE_GASRC5_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc5_output_mux =
	SOC_DAPM_ENUM("GASRC5 Sink", mtk_dai_gasrc5_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc6_output_mux_enum,
	AFE_GASRC6_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc6_output_mux =
	SOC_DAPM_ENUM("GASRC6 Sink", mtk_dai_gasrc6_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc7_output_mux_enum,
	AFE_GASRC7_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc7_output_mux =
	SOC_DAPM_ENUM("GASRC7 Sink", mtk_dai_gasrc7_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc8_output_mux_enum,
	AFE_GASRC8_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc8_output_mux =
	SOC_DAPM_ENUM("GASRC8 Sink", mtk_dai_gasrc8_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc9_output_mux_enum,
	AFE_GASRC9_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc9_output_mux =
	SOC_DAPM_ENUM("GASRC9 Sink", mtk_dai_gasrc9_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc10_output_mux_enum,
	AFE_GASRC10_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc10_output_mux =
	SOC_DAPM_ENUM("GASRC10 Sink", mtk_dai_gasrc10_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc11_output_mux_enum,
	AFE_GASRC11_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc11_output_mux =
	SOC_DAPM_ENUM("GASRC11 Sink", mtk_dai_gasrc11_output_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc0_input_mux_enum,
	AFE_GASRC0_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc0_input_mux =
	SOC_DAPM_ENUM("GASRC0 Source", mtk_dai_gasrc0_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc1_input_mux_enum,
	AFE_GASRC1_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc1_input_mux =
	SOC_DAPM_ENUM("GASRC1 Source", mtk_dai_gasrc1_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc2_input_mux_enum,
	AFE_GASRC2_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc2_input_mux =
	SOC_DAPM_ENUM("GASRC2 Source", mtk_dai_gasrc2_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc3_input_mux_enum,
	AFE_GASRC3_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc3_input_mux =
	SOC_DAPM_ENUM("GASRC3 Source", mtk_dai_gasrc3_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc4_input_mux_enum,
	AFE_GASRC4_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc4_input_mux =
	SOC_DAPM_ENUM("GASRC4 Source", mtk_dai_gasrc4_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc5_input_mux_enum,
	AFE_GASRC5_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc5_input_mux =
	SOC_DAPM_ENUM("GASRC5 Source", mtk_dai_gasrc5_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc6_input_mux_enum,
	AFE_GASRC6_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc6_input_mux =
	SOC_DAPM_ENUM("GASRC6 Source", mtk_dai_gasrc6_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc7_input_mux_enum,
	AFE_GASRC7_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc7_input_mux =
	SOC_DAPM_ENUM("GASRC7 Source", mtk_dai_gasrc7_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc8_input_mux_enum,
	AFE_GASRC8_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc8_input_mux =
	SOC_DAPM_ENUM("GASRC8 Source", mtk_dai_gasrc8_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc9_input_mux_enum,
	AFE_GASRC9_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc9_input_mux =
	SOC_DAPM_ENUM("GASRC9 Source", mtk_dai_gasrc9_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc10_input_mux_enum,
	AFE_GASRC10_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc10_input_mux =
	SOC_DAPM_ENUM("GASRC10 Source", mtk_dai_gasrc10_input_mux_enum);

static SOC_ENUM_SINGLE_DECL(mtk_dai_gasrc11_input_mux_enum,
	AFE_GASRC11_NEW_CON0, 31, mtk_dai_gasrc_in_out_mux_text);

static const struct snd_kcontrol_new mtk_dai_gasrc11_input_mux =
	SOC_DAPM_ENUM("GASRC11 Source", mtk_dai_gasrc11_input_mux_enum);

static const struct snd_soc_dapm_widget mtk_dai_gasrc_widgets[] = {
	SND_SOC_DAPM_MIXER("I096", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I097", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I098", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I099", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I100", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I101", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I102", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I103", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I104", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I105", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I106", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I107", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I108", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I109", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I110", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I111", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I112", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I113", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I114", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I115", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I116", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I117", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I118", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I119", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("O096", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o096_mix, ARRAY_SIZE(mtk_dai_gasrc_o096_mix)),
	SND_SOC_DAPM_MIXER("O097", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o097_mix, ARRAY_SIZE(mtk_dai_gasrc_o097_mix)),
	SND_SOC_DAPM_MIXER("O098", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o098_mix, ARRAY_SIZE(mtk_dai_gasrc_o098_mix)),
	SND_SOC_DAPM_MIXER("O099", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o099_mix, ARRAY_SIZE(mtk_dai_gasrc_o099_mix)),
	SND_SOC_DAPM_MIXER("O100", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o100_mix, ARRAY_SIZE(mtk_dai_gasrc_o100_mix)),
	SND_SOC_DAPM_MIXER("O101", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o101_mix, ARRAY_SIZE(mtk_dai_gasrc_o101_mix)),
	SND_SOC_DAPM_MIXER("O102", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o102_mix, ARRAY_SIZE(mtk_dai_gasrc_o102_mix)),
	SND_SOC_DAPM_MIXER("O103", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o103_mix, ARRAY_SIZE(mtk_dai_gasrc_o103_mix)),
	SND_SOC_DAPM_MIXER("O104", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o104_mix, ARRAY_SIZE(mtk_dai_gasrc_o104_mix)),
	SND_SOC_DAPM_MIXER("O105", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o105_mix, ARRAY_SIZE(mtk_dai_gasrc_o105_mix)),
	SND_SOC_DAPM_MIXER("O106", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o106_mix, ARRAY_SIZE(mtk_dai_gasrc_o106_mix)),
	SND_SOC_DAPM_MIXER("O107", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o107_mix, ARRAY_SIZE(mtk_dai_gasrc_o107_mix)),
	SND_SOC_DAPM_MIXER("O108", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o108_mix, ARRAY_SIZE(mtk_dai_gasrc_o108_mix)),
	SND_SOC_DAPM_MIXER("O109", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o109_mix, ARRAY_SIZE(mtk_dai_gasrc_o109_mix)),
	SND_SOC_DAPM_MIXER("O110", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o110_mix, ARRAY_SIZE(mtk_dai_gasrc_o110_mix)),
	SND_SOC_DAPM_MIXER("O111", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o111_mix, ARRAY_SIZE(mtk_dai_gasrc_o111_mix)),
	SND_SOC_DAPM_MIXER("O112", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o112_mix, ARRAY_SIZE(mtk_dai_gasrc_o112_mix)),
	SND_SOC_DAPM_MIXER("O113", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o113_mix, ARRAY_SIZE(mtk_dai_gasrc_o113_mix)),
	SND_SOC_DAPM_MIXER("O114", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o114_mix, ARRAY_SIZE(mtk_dai_gasrc_o114_mix)),
	SND_SOC_DAPM_MIXER("O115", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o115_mix, ARRAY_SIZE(mtk_dai_gasrc_o115_mix)),
	SND_SOC_DAPM_MIXER("O116", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o116_mix, ARRAY_SIZE(mtk_dai_gasrc_o116_mix)),
	SND_SOC_DAPM_MIXER("O117", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o117_mix, ARRAY_SIZE(mtk_dai_gasrc_o117_mix)),
	SND_SOC_DAPM_MIXER("O118", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o118_mix, ARRAY_SIZE(mtk_dai_gasrc_o118_mix)),
	SND_SOC_DAPM_MIXER("O119", SND_SOC_NOPM, 0, 0,
		mtk_dai_gasrc_o119_mix, ARRAY_SIZE(mtk_dai_gasrc_o119_mix)),

	SND_SOC_DAPM_MUX("GASRC0 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc0_output_mux),
	SND_SOC_DAPM_MUX("GASRC1 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc1_output_mux),
	SND_SOC_DAPM_MUX("GASRC2 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc2_output_mux),
	SND_SOC_DAPM_MUX("GASRC3 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc3_output_mux),
	SND_SOC_DAPM_MUX("GASRC4 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc4_output_mux),
	SND_SOC_DAPM_MUX("GASRC5 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc5_output_mux),
	SND_SOC_DAPM_MUX("GASRC6 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc6_output_mux),
	SND_SOC_DAPM_MUX("GASRC7 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc7_output_mux),
	SND_SOC_DAPM_MUX("GASRC8 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc8_output_mux),
	SND_SOC_DAPM_MUX("GASRC9 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc9_output_mux),
	SND_SOC_DAPM_MUX("GASRC10 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc10_output_mux),
	SND_SOC_DAPM_MUX("GASRC11 Output Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc11_output_mux),

	SND_SOC_DAPM_MUX("GASRC0 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc0_input_mux),
	SND_SOC_DAPM_MUX("GASRC1 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc1_input_mux),
	SND_SOC_DAPM_MUX("GASRC2 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc2_input_mux),
	SND_SOC_DAPM_MUX("GASRC3 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc3_input_mux),
	SND_SOC_DAPM_MUX("GASRC4 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc4_input_mux),
	SND_SOC_DAPM_MUX("GASRC5 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc5_input_mux),
	SND_SOC_DAPM_MUX("GASRC6 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc6_input_mux),
	SND_SOC_DAPM_MUX("GASRC7 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc7_input_mux),
	SND_SOC_DAPM_MUX("GASRC8 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc8_input_mux),
	SND_SOC_DAPM_MUX("GASRC9 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc9_input_mux),
	SND_SOC_DAPM_MUX("GASRC10 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc10_input_mux),
	SND_SOC_DAPM_MUX("GASRC11 Input Mux", SND_SOC_NOPM, 0, 0,
		&mtk_dai_gasrc11_input_mux),
};

static const struct snd_soc_dapm_route mtk_dai_gasrc_routes[] = {
	{"GASRC0 Output Mux", "normal", "GASRC0 Playback"},
	{"GASRC0 Output Mux", "one_heart", "GASRC0 Playback"},
	{"GASRC0 Output Mux", "normal", "GASRC0 Capture"},
	{"GASRC0 Output Mux", "one_heart", "GASRC0 Capture"},
	{"GASRC1 Output Mux", "normal", "GASRC1 Playback"},
	{"GASRC1 Output Mux", "one_heart", "GASRC1 Playback"},
	{"GASRC1 Output Mux", "normal", "GASRC1 Capture"},
	{"GASRC1 Output Mux", "one_heart", "GASRC1 Capture"},
	{"GASRC2 Output Mux", "normal", "GASRC2 Playback"},
	{"GASRC2 Output Mux", "one_heart", "GASRC2 Playback"},
	{"GASRC2 Output Mux", "normal", "GASRC2 Capture"},
	{"GASRC2 Output Mux", "one_heart", "GASRC2 Capture"},
	{"GASRC3 Output Mux", "normal", "GASRC3 Playback"},
	{"GASRC3 Output Mux", "one_heart", "GASRC3 Playback"},
	{"GASRC3 Output Mux", "normal", "GASRC3 Capture"},
	{"GASRC3 Output Mux", "one_heart", "GASRC3 Capture"},
	{"GASRC4 Output Mux", "normal", "GASRC4 Playback"},
	{"GASRC4 Output Mux", "one_heart", "GASRC4 Playback"},
	{"GASRC4 Output Mux", "normal", "GASRC4 Capture"},
	{"GASRC4 Output Mux", "one_heart", "GASRC4 Capture"},
	{"GASRC5 Output Mux", "normal", "GASRC5 Playback"},
	{"GASRC5 Output Mux", "one_heart", "GASRC5 Playback"},
	{"GASRC5 Output Mux", "normal", "GASRC5 Capture"},
	{"GASRC5 Output Mux", "one_heart", "GASRC5 Capture"},
	{"GASRC6 Output Mux", "normal", "GASRC6 Playback"},
	{"GASRC6 Output Mux", "one_heart", "GASRC6 Playback"},
	{"GASRC6 Output Mux", "normal", "GASRC6 Capture"},
	{"GASRC6 Output Mux", "one_heart", "GASRC6 Capture"},
	{"GASRC7 Output Mux", "normal", "GASRC7 Playback"},
	{"GASRC7 Output Mux", "one_heart", "GASRC7 Playback"},
	{"GASRC7 Output Mux", "normal", "GASRC7 Capture"},
	{"GASRC7 Output Mux", "one_heart", "GASRC7 Capture"},
	{"GASRC8 Output Mux", "normal", "GASRC8 Playback"},
	{"GASRC8 Output Mux", "one_heart", "GASRC8 Playback"},
	{"GASRC8 Output Mux", "normal", "GASRC8 Capture"},
	{"GASRC8 Output Mux", "one_heart", "GASRC8 Capture"},
	{"GASRC9 Output Mux", "normal", "GASRC9 Playback"},
	{"GASRC9 Output Mux", "one_heart", "GASRC9 Playback"},
	{"GASRC9 Output Mux", "normal", "GASRC9 Capture"},
	{"GASRC9 Output Mux", "one_heart", "GASRC9 Capture"},
	{"GASRC10 Output Mux", "normal", "GASRC10 Playback"},
	{"GASRC10 Output Mux", "one_heart", "GASRC10 Playback"},
	{"GASRC10 Output Mux", "normal", "GASRC10 Capture"},
	{"GASRC10 Output Mux", "one_heart", "GASRC10 Capture"},
	{"GASRC11 Output Mux", "normal", "GASRC11 Playback"},
	{"GASRC11 Output Mux", "one_heart", "GASRC11 Playback"},
	{"GASRC11 Output Mux", "normal", "GASRC11 Capture"},
	{"GASRC11 Output Mux", "one_heart", "GASRC11 Capture"},

	{"I096", NULL, "GASRC0 Output Mux"},
	{"I097", NULL, "GASRC0 Output Mux"},
	{"I098", NULL, "GASRC1 Output Mux"},
	{"I099", NULL, "GASRC1 Output Mux"},
	{"I100", NULL, "GASRC2 Output Mux"},
	{"I101", NULL, "GASRC2 Output Mux"},
	{"I102", NULL, "GASRC3 Output Mux"},
	{"I103", NULL, "GASRC3 Output Mux"},
	{"I104", NULL, "GASRC4 Output Mux"},
	{"I105", NULL, "GASRC4 Output Mux"},
	{"I106", NULL, "GASRC5 Output Mux"},
	{"I107", NULL, "GASRC5 Output Mux"},
	{"I108", NULL, "GASRC6 Output Mux"},
	{"I109", NULL, "GASRC6 Output Mux"},
	{"I110", NULL, "GASRC7 Output Mux"},
	{"I111", NULL, "GASRC7 Output Mux"},
	{"I112", NULL, "GASRC8 Output Mux"},
	{"I113", NULL, "GASRC8 Output Mux"},
	{"I114", NULL, "GASRC9 Output Mux"},
	{"I115", NULL, "GASRC9 Output Mux"},
	{"I116", NULL, "GASRC10 Output Mux"},
	{"I117", NULL, "GASRC10 Output Mux"},
	{"I118", NULL, "GASRC11 Output Mux"},
	{"I119", NULL, "GASRC11 Output Mux"},

	{"GASRC0 Input Mux", "normal", "O096"},
	{"GASRC0 Input Mux", "normal", "O097"},
	{"GASRC1 Input Mux", "normal", "O098"},
	{"GASRC1 Input Mux", "normal", "O099"},
	{"GASRC2 Input Mux", "normal", "O100"},
	{"GASRC2 Input Mux", "normal", "O101"},
	{"GASRC3 Input Mux", "normal", "O102"},
	{"GASRC3 Input Mux", "normal", "O103"},
	{"GASRC4 Input Mux", "normal", "O104"},
	{"GASRC4 Input Mux", "normal", "O105"},
	{"GASRC5 Input Mux", "normal", "O106"},
	{"GASRC5 Input Mux", "normal", "O107"},
	{"GASRC6 Input Mux", "normal", "O108"},
	{"GASRC6 Input Mux", "normal", "O109"},
	{"GASRC7 Input Mux", "normal", "O110"},
	{"GASRC7 Input Mux", "normal", "O111"},
	{"GASRC8 Input Mux", "normal", "O112"},
	{"GASRC8 Input Mux", "normal", "O113"},
	{"GASRC9 Input Mux", "normal", "O114"},
	{"GASRC9 Input Mux", "normal", "O115"},
	{"GASRC10 Input Mux", "normal", "O116"},
	{"GASRC10 Input Mux", "normal", "O117"},
	{"GASRC11 Input Mux", "normal", "O118"},
	{"GASRC11 Input Mux", "normal", "O119"},

	{"GASRC0 Input Mux", "one_heart", "O096"},
	{"GASRC0 Input Mux", "one_heart", "O097"},
	{"GASRC1 Input Mux", "one_heart", "O098"},
	{"GASRC1 Input Mux", "one_heart", "O099"},
	{"GASRC2 Input Mux", "one_heart", "O100"},
	{"GASRC2 Input Mux", "one_heart", "O101"},
	{"GASRC3 Input Mux", "one_heart", "O102"},
	{"GASRC3 Input Mux", "one_heart", "O103"},
	{"GASRC4 Input Mux", "one_heart", "O104"},
	{"GASRC4 Input Mux", "one_heart", "O105"},
	{"GASRC5 Input Mux", "one_heart", "O106"},
	{"GASRC5 Input Mux", "one_heart", "O107"},
	{"GASRC6 Input Mux", "one_heart", "O108"},
	{"GASRC6 Input Mux", "one_heart", "O109"},
	{"GASRC7 Input Mux", "one_heart", "O110"},
	{"GASRC7 Input Mux", "one_heart", "O111"},
	{"GASRC8 Input Mux", "one_heart", "O112"},
	{"GASRC8 Input Mux", "one_heart", "O113"},
	{"GASRC9 Input Mux", "one_heart", "O114"},
	{"GASRC9 Input Mux", "one_heart", "O115"},
	{"GASRC10 Input Mux", "one_heart", "O116"},
	{"GASRC10 Input Mux", "one_heart", "O117"},
	{"GASRC11 Input Mux", "one_heart", "O118"},
	{"GASRC11 Input Mux", "one_heart", "O119"},

	{"GASRC0 Playback", NULL, "GASRC0 Input Mux"},
	{"GASRC0 Capture", NULL, "GASRC0 Input Mux"},
	{"GASRC1 Playback", NULL, "GASRC1 Input Mux"},
	{"GASRC1 Capture", NULL, "GASRC1 Input Mux"},
	{"GASRC2 Playback", NULL, "GASRC2 Input Mux"},
	{"GASRC2 Capture", NULL, "GASRC2 Input Mux"},
	{"GASRC3 Playback", NULL, "GASRC3 Input Mux"},
	{"GASRC3 Capture", NULL, "GASRC3 Input Mux"},
	{"GASRC4 Playback", NULL, "GASRC4 Input Mux"},
	{"GASRC4 Capture", NULL, "GASRC4 Input Mux"},
	{"GASRC5 Playback", NULL, "GASRC5 Input Mux"},
	{"GASRC5 Capture", NULL, "GASRC5 Input Mux"},
	{"GASRC6 Playback", NULL, "GASRC6 Input Mux"},
	{"GASRC6 Capture", NULL, "GASRC6 Input Mux"},
	{"GASRC7 Playback", NULL, "GASRC7 Input Mux"},
	{"GASRC7 Capture", NULL, "GASRC7 Input Mux"},
	{"GASRC8 Playback", NULL, "GASRC8 Input Mux"},
	{"GASRC8 Capture", NULL, "GASRC8 Input Mux"},
	{"GASRC9 Playback", NULL, "GASRC9 Input Mux"},
	{"GASRC9 Capture", NULL, "GASRC9 Input Mux"},
	{"GASRC10 Playback", NULL, "GASRC10 Input Mux"},
	{"GASRC10 Capture", NULL, "GASRC10 Input Mux"},
	{"GASRC11 Playback", NULL, "GASRC11 Input Mux"},
	{"GASRC11 Capture", NULL, "GASRC11 Input Mux"},

	{"O096", "I000 Switch", "I000"},
	{"O097", "I001 Switch", "I001"},
	{"O098", "I000 Switch", "I000"},
	{"O099", "I001 Switch", "I001"},
	{"O100", "I000 Switch", "I000"},
	{"O101", "I001 Switch", "I001"},
	{"O102", "I000 Switch", "I000"},
	{"O103", "I001 Switch", "I001"},
	{"O104", "I000 Switch", "I000"},
	{"O105", "I001 Switch", "I001"},
	{"O106", "I000 Switch", "I000"},
	{"O107", "I001 Switch", "I001"},
	{"O108", "I000 Switch", "I000"},
	{"O109", "I001 Switch", "I001"},
	{"O110", "I000 Switch", "I000"},
	{"O111", "I001 Switch", "I001"},
	{"O112", "I000 Switch", "I000"},
	{"O113", "I001 Switch", "I001"},
	{"O114", "I000 Switch", "I000"},
	{"O115", "I001 Switch", "I001"},
	{"O116", "I000 Switch", "I000"},
	{"O117", "I001 Switch", "I001"},
	{"O118", "I000 Switch", "I000"},
	{"O119", "I001 Switch", "I001"},

	{"O096", "I012 Switch", "I012"},
	{"O097", "I013 Switch", "I013"},
	{"O098", "I012 Switch", "I012"},
	{"O099", "I013 Switch", "I013"},
	{"O100", "I012 Switch", "I012"},
	{"O101", "I013 Switch", "I013"},
	{"O102", "I012 Switch", "I012"},
	{"O103", "I013 Switch", "I013"},
	{"O104", "I012 Switch", "I012"},
	{"O105", "I013 Switch", "I013"},
	{"O106", "I012 Switch", "I012"},
	{"O107", "I013 Switch", "I013"},
	{"O108", "I012 Switch", "I012"},
	{"O109", "I013 Switch", "I013"},
	{"O110", "I012 Switch", "I012"},
	{"O111", "I013 Switch", "I013"},
	{"O112", "I012 Switch", "I012"},
	{"O113", "I013 Switch", "I013"},
	{"O114", "I012 Switch", "I012"},
	{"O115", "I013 Switch", "I013"},
	{"O116", "I012 Switch", "I012"},
	{"O117", "I013 Switch", "I013"},
	{"O118", "I012 Switch", "I012"},
	{"O119", "I013 Switch", "I013"},

	{"O098", "I014 Switch", "I014"},
	{"O099", "I015 Switch", "I015"},
	{"O100", "I016 Switch", "I016"},
	{"O101", "I017 Switch", "I017"},
	{"O102", "I018 Switch", "I018"},
	{"O103", "I019 Switch", "I019"},

	{"O096", "I020 Switch", "I020"},
	{"O097", "I021 Switch", "I021"},
	{"O098", "I020 Switch", "I020"},
	{"O099", "I021 Switch", "I021"},
	{"O100", "I020 Switch", "I020"},
	{"O101", "I021 Switch", "I021"},
	{"O102", "I020 Switch", "I020"},
	{"O103", "I021 Switch", "I021"},
	{"O104", "I020 Switch", "I020"},
	{"O105", "I021 Switch", "I021"},
	{"O106", "I020 Switch", "I020"},
	{"O107", "I021 Switch", "I021"},
	{"O108", "I020 Switch", "I020"},
	{"O109", "I021 Switch", "I021"},
	{"O110", "I020 Switch", "I020"},
	{"O111", "I021 Switch", "I021"},
	{"O112", "I020 Switch", "I020"},
	{"O113", "I021 Switch", "I021"},
	{"O114", "I020 Switch", "I020"},
	{"O115", "I021 Switch", "I021"},
	{"O116", "I020 Switch", "I020"},
	{"O117", "I021 Switch", "I021"},
	{"O118", "I020 Switch", "I020"},
	{"O119", "I021 Switch", "I021"},

	{"O096", "I022 Switch", "I022"},
	{"O097", "I023 Switch", "I023"},
	{"O098", "I024 Switch", "I024"},
	{"O099", "I025 Switch", "I025"},
	{"O100", "I026 Switch", "I026"},
	{"O101", "I027 Switch", "I027"},
	{"O102", "I028 Switch", "I028"},
	{"O103", "I029 Switch", "I029"},
	{"O104", "I030 Switch", "I030"},
	{"O105", "I031 Switch", "I031"},
	{"O106", "I032 Switch", "I032"},
	{"O107", "I033 Switch", "I033"},
	{"O108", "I034 Switch", "I034"},
	{"O109", "I035 Switch", "I035"},
	{"O110", "I036 Switch", "I036"},
	{"O111", "I037 Switch", "I037"},

	{"O096", "I046 Switch", "I046"},
	{"O097", "I047 Switch", "I047"},
	{"O098", "I048 Switch", "I048"},
	{"O099", "I049 Switch", "I049"},
	{"O100", "I050 Switch", "I050"},
	{"O101", "I051 Switch", "I051"},
	{"O102", "I052 Switch", "I052"},
	{"O103", "I053 Switch", "I053"},
	{"O104", "I054 Switch", "I054"},
	{"O105", "I055 Switch", "I055"},
	{"O106", "I056 Switch", "I056"},
	{"O107", "I057 Switch", "I057"},
	{"O108", "I058 Switch", "I058"},
	{"O109", "I059 Switch", "I059"},
	{"O110", "I060 Switch", "I060"},
	{"O111", "I061 Switch", "I061"},

	{"O096", "I070 Switch", "I070"},
	{"O097", "I071 Switch", "I071"},
	{"O098", "I070 Switch", "I070"},
	{"O099", "I071 Switch", "I071"},
	{"O100", "I070 Switch", "I070"},
	{"O101", "I071 Switch", "I071"},
	{"O102", "I070 Switch", "I070"},
	{"O103", "I071 Switch", "I071"},
	{"O104", "I070 Switch", "I070"},
	{"O105", "I071 Switch", "I071"},
	{"O106", "I070 Switch", "I070"},
	{"O107", "I071 Switch", "I071"},
	{"O108", "I070 Switch", "I070"},
	{"O109", "I071 Switch", "I071"},
	{"O110", "I070 Switch", "I070"},
	{"O111", "I071 Switch", "I071"},
	{"O112", "I070 Switch", "I070"},
	{"O113", "I071 Switch", "I071"},
	{"O114", "I070 Switch", "I070"},
	{"O115", "I071 Switch", "I071"},
	{"O116", "I070 Switch", "I070"},
	{"O117", "I071 Switch", "I071"},
	{"O118", "I070 Switch", "I070"},
	{"O119", "I071 Switch", "I071"},

	{"O096", "I072 Switch", "I072"},
	{"O097", "I073 Switch", "I073"},
	{"O098", "I074 Switch", "I074"},
	{"O099", "I075 Switch", "I075"},
	{"O100", "I076 Switch", "I076"},
	{"O101", "I077 Switch", "I077"},
	{"O102", "I078 Switch", "I078"},
	{"O103", "I079 Switch", "I079"},
	{"O104", "I080 Switch", "I080"},
	{"O105", "I081 Switch", "I081"},
	{"O106", "I082 Switch", "I082"},
	{"O107", "I083 Switch", "I083"},
	{"O108", "I084 Switch", "I084"},
	{"O109", "I085 Switch", "I085"},
	{"O110", "I086 Switch", "I086"},
	{"O111", "I087 Switch", "I087"},
};

static bool is_valid_gasrc_dai(int dai_id)
{
	if (dai_id < MT8188_AFE_IO_GASRC_START ||
	    dai_id >= MT8188_AFE_IO_GASRC_END)
		return false;
	else
		return true;
}

static void mt8188_afe_reset_gasrc(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai)
{
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con5;
	val = AFE_GASRC_NEW_CON5_SOFT_RESET;

	regmap_update_bits(afe->regmap, ctrl_reg, val, val);
	regmap_update_bits(afe->regmap, ctrl_reg, val, 0);
}

static void mt8188_afe_clear_gasrc(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai)
{
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;
	val = AFE_GASRC_NEW_CON0_CHSET_STR_CLR;

	regmap_update_bits(afe->regmap, ctrl_reg, val, val);
}

static struct snd_soc_dai *mt8188_get_be_cpu_dai(
	struct snd_pcm_substream *be_substream,
	const char *dai_name)
{
	struct snd_soc_dpcm *dpcm_be;
	struct snd_soc_dpcm *dpcm_fe;
	struct snd_soc_pcm_runtime *be = be_substream->private_data;
	struct snd_soc_pcm_runtime *fe;
	unsigned int stream = be_substream->stream;

	if (!dai_name)
		return NULL;

	list_for_each_entry(dpcm_fe, &be->dpcm[stream].fe_clients, list_fe) {
		fe = dpcm_fe->fe;
		list_for_each_entry(dpcm_be,
			&fe->dpcm[stream].be_clients, list_be) {
			if (!strcmp(dpcm_be->be->dais[0]->name, dai_name))
				return dpcm_be->be->dais[0];
		}
	}

	return NULL;
}

static unsigned int mt8188_get_be_dai_fmt(
	struct snd_pcm_substream *be_substream,
	const char *dai_name)
{
	struct snd_soc_dpcm *dpcm_be;
	struct snd_soc_dpcm *dpcm_fe;
	struct snd_soc_pcm_runtime *be = be_substream->private_data;
	struct snd_soc_pcm_runtime *fe;
	unsigned int stream = be_substream->stream;

	if (!dai_name)
		return 0;

	list_for_each_entry(dpcm_fe, &be->dpcm[stream].fe_clients, list_fe) {
		fe = dpcm_fe->fe;
		list_for_each_entry(dpcm_be,
			&fe->dpcm[stream].be_clients, list_be) {
			if (!strcmp(dpcm_be->be->dais[0]->name, dai_name))
				return dpcm_be->be->dai_link->dai_fmt;
		}
	}

	return 0;
}

static bool mt8188_is_be_dai_slave(
	struct snd_pcm_substream *be_substream,
	const char *dai_name)
{
	unsigned int dai_fmt = mt8188_get_be_dai_fmt(be_substream, dai_name);

	if (dai_fmt & SND_SOC_DAIFMT_CBM_CFM)
		return true;

	else
		return false;
}


static bool mt8188_be_cpu_dai_matched(struct snd_soc_dapm_widget *w,
	unsigned int dai_id, int stream)
{
	struct snd_soc_dapm_path *path;
	struct snd_soc_dai *dai;
	bool ret = false;
	bool playback = (stream == SNDRV_PCM_STREAM_PLAYBACK) ? true : false;

	if (!w)
		return false;

	if (w->is_ep) {
		if ((w->id != snd_soc_dapm_dai_in) &&
		    (w->id != snd_soc_dapm_dai_out))
			return false;
	}

	if (playback) {
		snd_soc_dapm_widget_for_each_source_path(w, path) {
			if (path && path->source) {
				if (!path->connect)
					continue;

				dai = path->source->priv;
				if (dai && dai_id == dai->id)
					return true;

				ret = mt8188_be_cpu_dai_matched(path->source,
						dai_id, stream);
				if (ret)
					return ret;
			}
		}
	} else {
		snd_soc_dapm_widget_for_each_sink_path(w, path) {
			if (path && path->sink) {
				if (!path->connect)
					continue;

				dai = path->sink->priv;
				if (dai && dai_id == dai->id)
					return true;

				ret = mt8188_be_cpu_dai_matched(path->sink,
						dai_id, stream);
				if (ret)
					return ret;
			}
		}
	}

	return false;
}

static bool mt8188_be_cpu_dai_connected(
	struct snd_pcm_substream *be_substream,
	const char *dai_name, int stream, unsigned int dai_id)
{
	struct snd_soc_dai *dai =
		mt8188_get_be_cpu_dai(be_substream, dai_name);
	struct snd_soc_dapm_widget *w = NULL;

	if (dai == NULL)
		return false;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		w = dai->playback_widget;
	else if (stream == SNDRV_PCM_STREAM_CAPTURE)
		w = dai->capture_widget;

	return mt8188_be_cpu_dai_matched(w, dai_id, stream);
}

static bool mt8188_get_be_cpu_dai_rate(struct snd_pcm_substream *be_substream,
	const char *dai_name, int stream,
	unsigned int dai_id, unsigned int *rate)
{
	struct snd_soc_dai *dai = mt8188_get_be_cpu_dai(be_substream, dai_name);
	bool ret = false;

	if (!rate || !dai)
		return ret;

	ret = mt8188_be_cpu_dai_connected(be_substream,
			dai_name, stream, dai_id);
	if (!ret)
		return false;

	*rate = dai->rate;

	return ret;
}

static int mt8188_afe_gasrc_get_input_rate(
	struct snd_pcm_substream *substream,
	unsigned int dai_id, unsigned int rate)
{
	static const char * const source_be[] = {
		"UL_SRC1",
		"UL_SRC2",
		"DMIC",
		"ETDM1_IN",
		"ETDM2_IN",
	};
	unsigned int input_rate = 0;
	bool ret = false;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(source_be); i++) {
		ret = mt8188_get_be_cpu_dai_rate(substream, source_be[i],
			SNDRV_PCM_STREAM_CAPTURE, dai_id, &input_rate);
		if (ret)
			return input_rate;
	}

	return rate;
}

static int mt8188_afe_gasrc_get_output_rate(
	struct snd_pcm_substream *substream,
	unsigned int dai_id, unsigned int rate)
{
	static const char * const sink_be[] = {
		"DL_SRC",
		"ETDM1_OUT",
		"ETDM2_OUT",
	};
	unsigned int output_rate = 0;
	bool ret = false;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(sink_be); i++) {
		ret = mt8188_get_be_cpu_dai_rate(substream, sink_be[i],
			SNDRV_PCM_STREAM_PLAYBACK, dai_id, &output_rate);
		if (ret)
			return output_rate;
	}

	return rate;
}

struct mt8188_afe_gasrc_in_out_fs {
	const char *name;
	int timing;
};

static const struct mt8188_afe_gasrc_in_out_fs mt8188_afe_gasrc_input_fs[] = {
	{ .name = "UL_SRC1", .timing = -1, },
	{ .name = "UL_SRC2", .timing = -1, },
	{ .name = "DMIC", .timing = -1, },
	{ .name = "ETDM1_IN", .timing = MT8188_ETDM_IN1_1X_EN, },
	{ .name = "ETDM2_IN", .timing = MT8188_ETDM_IN2_1X_EN, },
};

static int mt8188_afe_gasrc_get_input_fs(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai, unsigned int rate)
{
	const struct mt8188_afe_gasrc_in_out_fs *input_fs = NULL;
	unsigned int dai_id = dai->id;
	int be_rate = 0;
	int fs = 0;
	bool con  = false;
	bool slv = false;
	int i = 0;

	fs = mt8188_afe_fs_timing(rate);

	for (i = 0; i < ARRAY_SIZE(mt8188_afe_gasrc_input_fs); i++) {
		input_fs = &mt8188_afe_gasrc_input_fs[i];
		con = mt8188_be_cpu_dai_connected(substream,
				input_fs->name,
				SNDRV_PCM_STREAM_CAPTURE, dai_id);
		if (con) {
			/* handle slave backend */
			slv = mt8188_is_be_dai_slave(substream,
					input_fs->name);
			if (slv) {
				fs = input_fs->timing;
				if (fs >= 0)
					break;
			}

			be_rate = mt8188_afe_gasrc_get_input_rate(substream,
				dai_id, rate);
			fs = mt8188_afe_fs_timing(be_rate);
			break;
		}
	}

	return fs;
}

static const struct mt8188_afe_gasrc_in_out_fs mt8188_afe_gasrc_output_fs[] = {
	{ .name = "DL_SRC", .timing = -1, },
	{ .name = "ETDM1_OUT", .timing = -1, },
	{ .name = "ETDM2_OUT", .timing = MT8188_ETDM_OUT2_1X_EN, },
};

static int mt8188_afe_gasrc_get_output_fs(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai, unsigned int rate)
{
	const struct mt8188_afe_gasrc_in_out_fs *output_fs = NULL;
	unsigned int dai_id = dai->id;
	int be_rate = 0;
	int fs = 0;
	bool con  = false;
	bool slv = false;
	int i = 0;

	fs = mt8188_afe_fs_timing(rate);

	for (i = 0; i < ARRAY_SIZE(mt8188_afe_gasrc_output_fs); i++) {
		output_fs = &mt8188_afe_gasrc_output_fs[i];
		con = mt8188_be_cpu_dai_connected(substream,
				output_fs->name,
				SNDRV_PCM_STREAM_PLAYBACK, dai_id);
		if (con) {
			/* handle slave backend */
			slv = mt8188_is_be_dai_slave(substream,
					output_fs->name);
			if (slv) {
				fs = output_fs->timing;
				if (fs >= 0)
					break;
			}

			be_rate = mt8188_afe_gasrc_get_output_rate(substream,
				dai_id, rate);
			fs = mt8188_afe_fs_timing(be_rate);
			break;
		}
	}

	return fs;
}

static void mt8188_afe_gasrc_set_input_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	switch (dai->id) {
	case MT8188_AFE_IO_GASRC0:
		ctrl_reg = GASRC_TIMING_CON0;
		mask = GASRC_TIMING_CON0_GASRC0_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC0_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC1:
		ctrl_reg = GASRC_TIMING_CON0;
		mask = GASRC_TIMING_CON0_GASRC1_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC1_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC2:
		ctrl_reg = GASRC_TIMING_CON0;
		mask = GASRC_TIMING_CON0_GASRC2_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC2_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC3:
		ctrl_reg = GASRC_TIMING_CON0;
		mask = GASRC_TIMING_CON0_GASRC3_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC3_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC4:
		ctrl_reg = GASRC_TIMING_CON0;
		mask = GASRC_TIMING_CON0_GASRC4_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC4_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC5:
		ctrl_reg = GASRC_TIMING_CON0;
		mask = GASRC_TIMING_CON0_GASRC5_IN_MODE_MASK;
		val = GASRC_TIMING_CON0_GASRC5_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC6:
		ctrl_reg = GASRC_TIMING_CON1;
		mask = GASRC_TIMING_CON1_GASRC6_IN_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC6_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC7:
		ctrl_reg = GASRC_TIMING_CON1;
		mask = GASRC_TIMING_CON1_GASRC7_IN_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC7_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC8:
		ctrl_reg = GASRC_TIMING_CON1;
		mask = GASRC_TIMING_CON1_GASRC8_IN_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC8_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC9:
		ctrl_reg = GASRC_TIMING_CON1;
		mask = GASRC_TIMING_CON1_GASRC9_IN_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC9_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC10:
		ctrl_reg = GASRC_TIMING_CON1;
		mask = GASRC_TIMING_CON1_GASRC10_IN_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC10_IN_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC11:
		ctrl_reg = GASRC_TIMING_CON1;
		mask = GASRC_TIMING_CON1_GASRC11_IN_MODE_MASK;
		val = GASRC_TIMING_CON1_GASRC11_IN_MODE(fs_timing);
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, ctrl_reg, mask, val);
}

static void mt8188_afe_gasrc_set_output_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;
	unsigned int mask = 0;

	switch (dai->id) {
	case MT8188_AFE_IO_GASRC0:
		ctrl_reg = GASRC_TIMING_CON4;
		mask = GASRC_TIMING_CON4_GASRC0_OUT_MODE_MASK;
		val = GASRC_TIMING_CON4_GASRC0_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC1:
		ctrl_reg = GASRC_TIMING_CON4;
		mask = GASRC_TIMING_CON4_GASRC1_OUT_MODE_MASK;
		val = GASRC_TIMING_CON4_GASRC1_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC2:
		ctrl_reg = GASRC_TIMING_CON4;
		mask = GASRC_TIMING_CON4_GASRC2_OUT_MODE_MASK;
		val = GASRC_TIMING_CON4_GASRC2_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC3:
		ctrl_reg = GASRC_TIMING_CON4;
		mask = GASRC_TIMING_CON4_GASRC3_OUT_MODE_MASK;
		val = GASRC_TIMING_CON4_GASRC3_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC4:
		ctrl_reg = GASRC_TIMING_CON4;
		mask = GASRC_TIMING_CON4_GASRC4_OUT_MODE_MASK;
		val = GASRC_TIMING_CON4_GASRC4_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC5:
		ctrl_reg = GASRC_TIMING_CON4;
		mask = GASRC_TIMING_CON4_GASRC5_OUT_MODE_MASK;
		val = GASRC_TIMING_CON4_GASRC5_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC6:
		ctrl_reg = GASRC_TIMING_CON5;
		mask = GASRC_TIMING_CON5_GASRC6_OUT_MODE_MASK;
		val = GASRC_TIMING_CON5_GASRC6_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC7:
		ctrl_reg = GASRC_TIMING_CON5;
		mask = GASRC_TIMING_CON5_GASRC7_OUT_MODE_MASK;
		val = GASRC_TIMING_CON5_GASRC7_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC8:
		ctrl_reg = GASRC_TIMING_CON5;
		mask = GASRC_TIMING_CON5_GASRC8_OUT_MODE_MASK;
		val = GASRC_TIMING_CON5_GASRC8_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC9:
		ctrl_reg = GASRC_TIMING_CON5;
		mask = GASRC_TIMING_CON5_GASRC9_OUT_MODE_MASK;
		val = GASRC_TIMING_CON5_GASRC9_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC10:
		ctrl_reg = GASRC_TIMING_CON5;
		mask = GASRC_TIMING_CON5_GASRC10_OUT_MODE_MASK;
		val = GASRC_TIMING_CON5_GASRC10_OUT_MODE(fs_timing);
		break;
	case MT8188_AFE_IO_GASRC11:
		ctrl_reg = GASRC_TIMING_CON5;
		mask = GASRC_TIMING_CON5_GASRC11_OUT_MODE_MASK;
		val = GASRC_TIMING_CON5_GASRC11_OUT_MODE(fs_timing);
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, ctrl_reg, mask, val);
}

struct mt8188_afe_gasrc_freq {
	unsigned int rate;
	unsigned int freq_val;
};

static const struct mt8188_afe_gasrc_freq
	mt8188_afe_gasrc_freq_palette_49m_45m_64_cycles[] = {
	{ .rate = 8000, .freq_val = 0x050000 },
	{ .rate = 12000, .freq_val = 0x078000 },
	{ .rate = 16000, .freq_val = 0x0A0000 },
	{ .rate = 24000, .freq_val = 0x0F0000 },
	{ .rate = 32000, .freq_val = 0x140000 },
	{ .rate = 48000, .freq_val = 0x1E0000 },
	{ .rate = 96000, .freq_val = 0x3C0000 },
	{ .rate = 192000, .freq_val = 0x780000 },
	{ .rate = 384000, .freq_val = 0xF00000 },
	{ .rate = 7350, .freq_val = 0x049800 },
	{ .rate = 11025, .freq_val = 0x06E400 },
	{ .rate = 14700, .freq_val = 0x093000 },
	{ .rate = 22050, .freq_val = 0x0DC800 },
	{ .rate = 29400, .freq_val = 0x126000 },
	{ .rate = 44100, .freq_val = 0x1B9000 },
	{ .rate = 88200, .freq_val = 0x372000 },
	{ .rate = 176400, .freq_val = 0x6E4000 },
	{ .rate = 352800, .freq_val = 0xDC8000 },
};

static const struct mt8188_afe_gasrc_freq
	mt8188_afe_gasrc_period_palette_49m_64_cycles[] = {
	{ .rate = 8000, .freq_val = 0x060000 },
	{ .rate = 12000, .freq_val = 0x040000 },
	{ .rate = 16000, .freq_val = 0x030000 },
	{ .rate = 24000, .freq_val = 0x020000 },
	{ .rate = 32000, .freq_val = 0x018000 },
	{ .rate = 48000, .freq_val = 0x010000 },
	{ .rate = 96000, .freq_val = 0x008000 },
	{ .rate = 192000, .freq_val = 0x004000 },
	{ .rate = 384000, .freq_val = 0x002000 },
	{ .rate = 7350, .freq_val = 0x0687D8 },
	{ .rate = 11025, .freq_val = 0x045A90 },
	{ .rate = 14700, .freq_val = 0x0343EC },
	{ .rate = 22050, .freq_val = 0x022D48 },
	{ .rate = 29400, .freq_val = 0x01A1F6 },
	{ .rate = 44100, .freq_val = 0x0116A4 },
	{ .rate = 88200, .freq_val = 0x008B52 },
	{ .rate = 176400, .freq_val = 0x0045A9 },
	{ .rate = 352800, .freq_val = 0x0022D4 },
};

static const struct mt8188_afe_gasrc_freq
	mt8188_afe_gasrc_period_palette_45m_64_cycles[] = {
	{ .rate = 8000, .freq_val = 0x058332 },
	{ .rate = 12000, .freq_val = 0x03ACCC },
	{ .rate = 16000, .freq_val = 0x02C199 },
	{ .rate = 24000, .freq_val = 0x01D666 },
	{ .rate = 32000, .freq_val = 0x0160CC },
	{ .rate = 48000, .freq_val = 0x00EB33 },
	{ .rate = 96000, .freq_val = 0x007599 },
	{ .rate = 192000, .freq_val = 0x003ACD },
	{ .rate = 384000, .freq_val = 0x001D66 },
	{ .rate = 7350, .freq_val = 0x060000 },
	{ .rate = 11025, .freq_val = 0x040000 },
	{ .rate = 14700, .freq_val = 0x030000 },
	{ .rate = 22050, .freq_val = 0x020000 },
	{ .rate = 29400, .freq_val = 0x018000 },
	{ .rate = 44100, .freq_val = 0x010000 },
	{ .rate = 88200, .freq_val = 0x008000 },
	{ .rate = 176400, .freq_val = 0x004000 },
	{ .rate = 352800, .freq_val = 0x002000 },
};

static int mt8188_afe_gasrc_get_freq_val(unsigned int rate,
	unsigned int cali_cycles)
{
	int i;
	const struct mt8188_afe_gasrc_freq *freq_palette = NULL;
	int tbl_size = 0;

	freq_palette = mt8188_afe_gasrc_freq_palette_49m_45m_64_cycles;
	tbl_size = ARRAY_SIZE(mt8188_afe_gasrc_freq_palette_49m_45m_64_cycles);

	for (i = 0; i < tbl_size; i++)
		if (freq_palette[i].rate == rate)
			return freq_palette[i].freq_val;

	return -EINVAL;
}

static int mt8188_afe_gasrc_get_period_val(unsigned int rate,
	bool op_freq_45m, unsigned int cali_cycles)
{
	int i;
	const struct mt8188_afe_gasrc_freq *period_palette = NULL;
	int tbl_size = 0;

	if (op_freq_45m) {
		period_palette =
			mt8188_afe_gasrc_period_palette_45m_64_cycles;
		tbl_size = ARRAY_SIZE(
			mt8188_afe_gasrc_period_palette_45m_64_cycles);
	} else {
		period_palette =
			mt8188_afe_gasrc_period_palette_49m_64_cycles;
		tbl_size = ARRAY_SIZE(
			mt8188_afe_gasrc_period_palette_49m_64_cycles);
	}

	for (i = 0; i < tbl_size; i++) {
		if (period_palette[i].rate == rate)
			return period_palette[i].freq_val;
	}

	return -EINVAL;
}

static void mt8188_afe_gasrc_set_rx_mode_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int input_rate, int output_rate)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_gasrc_priv *gasrc_priv;
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	unsigned int cali_cycles = 64;
	unsigned int ctrl_reg = 0;
	int val = 0;

	if (!is_valid_gasrc_dai(dai->id)) {
		dev_info(dai->dev, "%s invalid dai->id [%d]\n",
			__func__, dai->id);
		return;
	}
	gasrc_priv = afe_priv->dai_priv[dai->id];

	cali_cycles = gasrc_priv->cali_cycles;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_OFS_SEL_MASK,
		AFE_GASRC_NEW_CON0_CHSET0_OFS_SEL_RX);
	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_IFS_SEL_MASK,
		AFE_GASRC_NEW_CON0_CHSET0_IFS_SEL_RX);

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con2;
	val = mt8188_afe_gasrc_get_freq_val(output_rate, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con3;
	val = mt8188_afe_gasrc_get_freq_val(input_rate, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);
}

static void mt8188_afe_gasrc_set_tx_mode_fs(struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int input_rate, int output_rate)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_gasrc_priv *gasrc_priv;
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	bool gasrc_op_freq_45m = false;
	unsigned int cali_cycles = 64;
	unsigned int ctrl_reg = 0;
	int val = 0;

	if (!is_valid_gasrc_dai(dai->id)) {
		dev_info(dai->dev, "%s invalid dai->id [%d]\n",
			__func__, dai->id);
		return;
	}
	gasrc_priv = afe_priv->dai_priv[dai->id];

	gasrc_op_freq_45m = gasrc_priv->op_freq_45m;
	cali_cycles = gasrc_priv->cali_cycles;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_OFS_SEL_MASK,
		AFE_GASRC_NEW_CON0_CHSET0_OFS_SEL_TX);
	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_IFS_SEL_MASK,
		AFE_GASRC_NEW_CON0_CHSET0_IFS_SEL_TX);

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con4;
	val = mt8188_afe_gasrc_get_period_val(output_rate,
		gasrc_op_freq_45m, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con1;
	val = mt8188_afe_gasrc_get_period_val(input_rate,
		gasrc_op_freq_45m, cali_cycles);
	if (val > 0)
		regmap_write(afe->regmap, ctrl_reg, val);
}

struct mt8188_afe_gasrc_cali_lrck_sel {
	int fs_timing;
	unsigned int lrck_sel_val;
	unsigned int sig_mux_sel;
};

enum {
	MT8188_AFE_GASRC_CALI_LRCK_SEL_ETDM_IN1 = 1,
	MT8188_AFE_GASRC_CALI_LRCK_SEL_ETDM_IN2 = 3,
	MT8188_AFE_GASRC_CALI_LRCK_SEL_ETDM_OUT2 = 5,
};

enum {
	MT8188_AFE_GASRC_CALI_SIG_MUX_SEL0,
	MT8188_AFE_GASRC_CALI_SIG_MUX_SEL1,
};

static const struct mt8188_afe_gasrc_cali_lrck_sel
	mt8188_afe_gasrc_cali_lrck_sels[] = {
	{
		.fs_timing = MT8188_ETDM_IN1_1X_EN,
		.lrck_sel_val = MT8188_AFE_GASRC_CALI_LRCK_SEL_ETDM_IN1,
		.sig_mux_sel = MT8188_AFE_GASRC_CALI_SIG_MUX_SEL1,
	},
	{
		.fs_timing = MT8188_ETDM_IN2_1X_EN,
		.lrck_sel_val = MT8188_AFE_GASRC_CALI_LRCK_SEL_ETDM_IN2,
		.sig_mux_sel = MT8188_AFE_GASRC_CALI_SIG_MUX_SEL1,
	},
	{
		.fs_timing = MT8188_ETDM_OUT2_1X_EN,
		.lrck_sel_val = MT8188_AFE_GASRC_CALI_LRCK_SEL_ETDM_OUT2,
		.sig_mux_sel = MT8188_AFE_GASRC_CALI_SIG_MUX_SEL1,
	},
};

static int mt8188_afe_gasrc_get_cali_lrck_sel_val(int fs_timing)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8188_afe_gasrc_cali_lrck_sels); i++)
		if (mt8188_afe_gasrc_cali_lrck_sels[i].fs_timing == fs_timing)
			return mt8188_afe_gasrc_cali_lrck_sels[i].lrck_sel_val;

	return -EINVAL;
}

static int mt8188_afe_gasrc_get_cali_sig_mux_sel_val(int fs_timing)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8188_afe_gasrc_cali_lrck_sels); i++)
		if (mt8188_afe_gasrc_cali_lrck_sels[i].fs_timing == fs_timing)
			return mt8188_afe_gasrc_cali_lrck_sels[i].sig_mux_sel;

	return -EINVAL;
}

static void mt8188_afe_gasrc_sel_lrck(
	struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, int fs_timing)
{
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	unsigned int ctrl_reg = 0;
	unsigned int mask = 0;
	int val = 0;

	/* cali lrck sel */
	val = mt8188_afe_gasrc_get_cali_lrck_sel_val(fs_timing);
	if (val < 0)
		return;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con5;
	mask = AFE_GASRC_NEW_CON5_CALI_LRCK_SEL_MASK;
	val = AFE_GASRC_NEW_CON5_CALI_LRCK_SEL(val);

	regmap_update_bits(afe->regmap, ctrl_reg, mask, val);

	/* cali sig sel */
	val = mt8188_afe_gasrc_get_cali_sig_mux_sel_val(fs_timing);
	if (val < 0)
		return;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con6;
	mask = AFE_GASRC_NEW_CON6_CALI_SIG_MUX_SEL_MASK;
	val = AFE_GASRC_NEW_CON6_CALI_SIG_MUX_SEL(val);

	regmap_update_bits(afe->regmap, ctrl_reg, mask, val);
}

static void mt8188_afe_gasrc_sel_cali_clk(
	struct mtk_base_afe *afe,
	struct snd_soc_dai *dai, bool gasrc_op_freq_45m)
{
	unsigned int ctrl_reg = 0;
	unsigned int mask = 0;
	unsigned int val = 0;

	val = gasrc_op_freq_45m;

	switch (dai->id) {
	case MT8188_AFE_IO_GASRC0:
		ctrl_reg = PWR1_ASM_CON1;
		mask = PWR1_ASM_CON1_GASRC0_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC0_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC1:
		ctrl_reg = PWR1_ASM_CON1;
		mask = PWR1_ASM_CON1_GASRC1_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC1_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC2:
		ctrl_reg = PWR1_ASM_CON1;
		mask = PWR1_ASM_CON1_GASRC2_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC2_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC3:
		ctrl_reg = PWR1_ASM_CON1;
		mask = PWR1_ASM_CON1_GASRC3_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON1_GASRC3_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC4:
		ctrl_reg = PWR1_ASM_CON2;
		mask = PWR1_ASM_CON2_GASRC4_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON2_GASRC4_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC5:
		ctrl_reg = PWR1_ASM_CON2;
		mask = PWR1_ASM_CON2_GASRC5_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON2_GASRC5_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC6:
		ctrl_reg = PWR1_ASM_CON2;
		mask = PWR1_ASM_CON2_GASRC6_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON2_GASRC6_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC7:
		ctrl_reg = PWR1_ASM_CON2;
		mask = PWR1_ASM_CON2_GASRC7_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON2_GASRC7_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC8:
		ctrl_reg = PWR1_ASM_CON2;
		mask = PWR1_ASM_CON2_GASRC8_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON2_GASRC8_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC9:
		ctrl_reg = PWR1_ASM_CON2;
		mask = PWR1_ASM_CON2_GASRC9_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON2_GASRC9_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC10:
		ctrl_reg = PWR1_ASM_CON3;
		mask = PWR1_ASM_CON3_GASRC10_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON3_GASRC10_CALI_CK_SEL(val);
		break;
	case MT8188_AFE_IO_GASRC11:
		ctrl_reg = PWR1_ASM_CON3;
		mask = PWR1_ASM_CON3_GASRC11_CALI_CK_SEL_MASK;
		val = PWR1_ASM_CON3_GASRC11_CALI_CK_SEL(val);
		break;
	default:
		return;
	}

	regmap_update_bits(afe->regmap, ctrl_reg, mask, val);
}

static bool mt8188_afe_gasrc_is_tx_tracking(int fs_timing)
{
	if (fs_timing == MT8188_ETDM_OUT2_1X_EN)
		return true;
	else
		return false;
}

static bool mt8188_afe_gasrc_is_rx_tracking(int fs_timing)
{
	if ((fs_timing == MT8188_ETDM_IN1_1X_EN) ||
		(fs_timing == MT8188_ETDM_IN2_1X_EN))
		return true;
	else
		return false;
}

static void mt8188_afe_gasrc_enable_iir(struct mtk_base_afe *afe,
	unsigned int gasrc_idx)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;

	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_IIR_STAGE_MASK,
		AFE_GASRC_NEW_CON0_CHSET0_IIR_STAGE(8));

	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_CLR_IIR_HISTORY,
		AFE_GASRC_NEW_CON0_CHSET0_CLR_IIR_HISTORY);

	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_IIR_EN,
		AFE_GASRC_NEW_CON0_CHSET0_IIR_EN);
}

static void mt8188_afe_gasrc_disable_iir(struct mtk_base_afe *afe,
	unsigned int gasrc_idx)
{
	unsigned int ctrl_reg = 0;

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;

	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_CHSET0_IIR_EN, 0);
}

enum afe_gasrc_iir_coeff_table_id {
	MT8188_AFE_GASRC_IIR_COEFF_384_to_352 = 0,
	MT8188_AFE_GASRC_IIR_COEFF_256_to_192,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_256,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_256,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	MT8188_AFE_GASRC_IIR_COEFF_256_to_96,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_128,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_128,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_96,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_88,
	MT8188_AFE_GASRC_IIR_COEFF_256_to_48,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_64,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_64,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_48,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_48,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_44,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_32,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_32,
	MT8188_AFE_GASRC_IIR_COEFF_352_to_24,
	MT8188_AFE_GASRC_IIR_COEFF_384_to_24,
	MT8188_AFE_GASRC_IIR_TABLES,
};

struct mt8188_afe_gasrc_iir_coeff_table_id {
	int input_rate;
	int output_rate;
	int table_id;
};

static const struct mt8188_afe_gasrc_iir_coeff_table_id
	mt8188_afe_gasrc_iir_coeff_table_ids[] = {
	{
		.input_rate = 8000,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 12000,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 12000,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 16000,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 16000,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 16000,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 16000,
		.output_rate = 14700,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 24000,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 24000,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 24000,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 24000,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 24000,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 32000,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 32000,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_256_to_96,
	},
	{
		.input_rate = 32000,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 32000,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 32000,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 32000,
		.output_rate = 14700,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 32000,
		.output_rate = 29400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 48000,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 48000,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 48000,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 48000,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 48000,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 48000,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 48000,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 48000,
		.output_rate = 44100,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 96000,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 96000,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 96000,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 96000,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 96000,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 96000,
		.output_rate = 48000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 96000,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_44,
	},
	{
		.input_rate = 96000,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 96000,
		.output_rate = 44100,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 96000,
		.output_rate = 88200,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 192000,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 192000,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 192000,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 192000,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 192000,
		.output_rate = 48000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 192000,
		.output_rate = 96000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 192000,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_44,
	},
	{
		.input_rate = 192000,
		.output_rate = 44100,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 192000,
		.output_rate = 88200,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 192000,
		.output_rate = 176400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 384000,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 384000,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 384000,
		.output_rate = 48000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 384000,
		.output_rate = 96000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 384000,
		.output_rate = 192000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 384000,
		.output_rate = 44100,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_44,
	},
	{
		.input_rate = 384000,
		.output_rate = 88200,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_88,
	},
	{
		.input_rate = 384000,
		.output_rate = 176400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_176,
	},
	{
		.input_rate = 384000,
		.output_rate = 352800,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_352,
	},
	{
		.input_rate = 11025,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_256,
	},
	{
		.input_rate = 11025,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 14700,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 14700,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 14700,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 22050,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_128,
	},
	{
		.input_rate = 22050,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 22050,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_256,
	},
	{
		.input_rate = 22050,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 22050,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 22050,
		.output_rate = 14700,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 29400,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 29400,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 29400,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 29400,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_256_to_96,
	},
	{
		.input_rate = 29400,
		.output_rate = 14700,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 29400,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_256_to_192,
	},
	{
		.input_rate = 44100,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_64,
	},
	{
		.input_rate = 44100,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 44100,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_128,
	},
	{
		.input_rate = 44100,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 44100,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_256,
	},
	{
		.input_rate = 44100,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 44100,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 44100,
		.output_rate = 14700,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 44100,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 44100,
		.output_rate = 29400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_256,
	},
	{
		.input_rate = 88200,
		.output_rate = 8000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_32,
	},
	{
		.input_rate = 88200,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_48,
	},
	{
		.input_rate = 88200,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_64,
	},
	{
		.input_rate = 88200,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 88200,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_128,
	},
	{
		.input_rate = 88200,
		.output_rate = 48000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 88200,
		.output_rate = 7350,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 88200,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 88200,
		.output_rate = 14700,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 88200,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 88200,
		.output_rate = 29400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_128,
	},
	{
		.input_rate = 88200,
		.output_rate = 44100,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 176400,
		.output_rate = 12000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_24,
	},
	{
		.input_rate = 176400,
		.output_rate = 16000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_32,
	},
	{
		.input_rate = 176400,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_48,
	},
	{
		.input_rate = 176400,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_64,
	},
	{
		.input_rate = 176400,
		.output_rate = 48000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 176400,
		.output_rate = 96000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 176400,
		.output_rate = 11025,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 176400,
		.output_rate = 14700,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 176400,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 176400,
		.output_rate = 29400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_64,
	},
	{
		.input_rate = 176400,
		.output_rate = 44100,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 176400,
		.output_rate = 88200,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
	{
		.input_rate = 352800,
		.output_rate = 24000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_24,
	},
	{
		.input_rate = 352800,
		.output_rate = 32000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_32,
	},
	{
		.input_rate = 352800,
		.output_rate = 48000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_48,
	},
	{
		.input_rate = 352800,
		.output_rate = 96000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_96,
	},
	{
		.input_rate = 352800,
		.output_rate = 192000,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_352_to_192,
	},
	{
		.input_rate = 352800,
		.output_rate = 22050,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_24,
	},
	{
		.input_rate = 352800,
		.output_rate = 29400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_32,
	},
	{
		.input_rate = 352800,
		.output_rate = 44100,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_48,
	},
	{
		.input_rate = 352800,
		.output_rate = 88200,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_96,
	},
	{
		.input_rate = 352800,
		.output_rate = 176400,
		.table_id = MT8188_AFE_GASRC_IIR_COEFF_384_to_192,
	},
};

#define IIR_NUMS (48)

static const unsigned int
	mt8188_afe_gasrc_iir_coeffs[MT8188_AFE_GASRC_IIR_TABLES][IIR_NUMS] = {
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_352] = {
		0x10bea3af, 0x2007e9be, 0x10bea3af,
		0xe2821372, 0xf0848d58, 0x00000003,
		0x08f9d435, 0x113d1a1f, 0x08f9d435,
		0xe31a73c5, 0xf1030af1, 0x00000003,
		0x09dd37b9, 0x13106967, 0x09dd37b9,
		0xe41398e1, 0xf1c98ae5, 0x00000003,
		0x0b55c74b, 0x16182d46, 0x0b55c74b,
		0xe5bce8cb, 0xf316f594, 0x00000003,
		0x0e02cb05, 0x1b950f07, 0x0e02cb05,
		0xf44d829a, 0xfaa9876b, 0x00000004,
		0x13e0e18e, 0x277f6d77, 0x13e0e18e,
		0xf695efae, 0xfc700da4, 0x00000004,
		0x0db3df0d, 0x1b6240b3, 0x0db3df0d,
		0xf201ce8e, 0xfca24567, 0x00000003,
		0x06b31e0f, 0x0cca96d1, 0x06b31e0f,
		0xc43a9021, 0xe051c370, 0x00000002,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_256_to_192] = {
		0x0de3c667, 0x137bf0e3, 0x0de3c667,
		0xd9575388, 0xe0d4770d, 0x00000002,
		0x0e54ed46, 0x1474090f, 0x0e54ed46,
		0xdb1c8213, 0xe2a7b6b7, 0x00000002,
		0x0d58713b, 0x13bde364, 0x05d8713b,
		0xde0a3770, 0xe5183cde, 0x00000002,
		0x0bdcfce3, 0x128ef355, 0x0bdcfce3,
		0xe2be28af, 0xe8affd19, 0x00000002,
		0x139091b3, 0x20f20a8e, 0x139091b3,
		0xe9ed58af, 0xedff795d, 0x00000002,
		0x0e68e9cd, 0x1a4cb00b, 0x0e68e9cd,
		0xf3ba2b24, 0xf5275137, 0x00000002,
		0x13079595, 0x251713f9, 0x13079595,
		0xf78c204d, 0xf227616a, 0x00000000,
		0x00000000, 0x2111eb8f, 0x2111eb8f,
		0x0014ac5b, 0x00000000, 0x00000006,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_256] = {
		0x0db45c84, 0x1113e68a, 0x0db45c84,
		0xdf58fbd3, 0xe0e51ba2, 0x00000002,
		0x0e0c4d8f, 0x11eaf5ef, 0x0e0c4d8f,
		0xe11e9264, 0xe2da4b80, 0x00000002,
		0x0cf2558c, 0x1154c11a, 0x0cf2558c,
		0xe41c6288, 0xe570c517, 0x00000002,
		0x0b5132d7, 0x10545ecd, 0x0b5132d7,
		0xe8e2e944, 0xe92f8dc6, 0x00000002,
		0x1234ffbb, 0x1cfba5c7, 0x1234ffbb,
		0xf00653e0, 0xee9406e3, 0x00000002,
		0x0cfd073a, 0x170277ad, 0x0cfd073a,
		0xf96e16e7, 0xf59562f9, 0x00000002,
		0x08506c2b, 0x1011cd72, 0x08506c2b,
		0x164a9eae, 0xe4203311, 0xffffffff,
		0x00000000, 0x3d58af1e, 0x3d58af1e,
		0x001bee13, 0x00000000, 0x00000007,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_256] = {
		0x0eca2fa9, 0x0f2b0cd3, 0x0eca2fa9,
		0xf50313ef, 0xf15857a7, 0x00000003,
		0x0ee239a9, 0x1045115c, 0x0ee239a9,
		0xec9f2976, 0xe5090807, 0x00000002,
		0x0ec57a45, 0x11d000f7, 0x0ec57a45,
		0xf0bb67bb, 0xe84c86de, 0x00000002,
		0x0e85ba7e, 0x13ee7e9a, 0x0e85ba7e,
		0xf6c74ebb, 0xecdba82c, 0x00000002,
		0x1cba1ac9, 0x2da90ada, 0x1cba1ac9,
		0xfecba589, 0xf2c756e1, 0x00000002,
		0x0f79dec4, 0x1c27f5e0, 0x0f79dec4,
		0x03c44399, 0xfc96c6aa, 0x00000003,
		0x1104a702, 0x21a72c89, 0x1104a702,
		0x1b6a6fb8, 0xfb5ee0f2, 0x00000001,
		0x0622fc30, 0x061a0c67, 0x0622fc30,
		0xe88911f2, 0xe0da327a, 0x00000002,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_192] = {
		0x1c012b9a, 0x09302bd9, 0x1c012b9a,
		0x0056c6d0, 0xe2b7f35c, 0x00000002,
		0x1b60cee5, 0x0b59639b, 0x1b60cee5,
		0x045dc965, 0xca2264a0, 0x00000001,
		0x19ec96ad, 0x0eb20aa9, 0x19ec96ad,
		0x0a6789cd, 0xd08944ba, 0x00000001,
		0x17c243aa, 0x1347e7fc, 0x17c243aa,
		0x131e03a8, 0xd9241dd4, 0x00000001,
		0x1563b168, 0x1904032f, 0x1563b168,
		0x0f0d206b, 0xf1d7f8e1, 0x00000002,
		0x14cd0206, 0x2169e2af, 0x14cd0206,
		0x14a5d991, 0xf7279caf, 0x00000002,
		0x0aac4c7f, 0x14cb084b, 0x0aac4c7f,
		0x30bc41c6, 0xf5565720, 0x00000001,
		0x0cea20d5, 0x03bc5f00, 0x0cea20d5,
		0xfeec800a, 0xc1b99664, 0x00000001,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_192] = {
		0x1bd356f3, 0x012e014f, 0x1bd356f3,
		0x081be0a6, 0xe28e2407, 0x00000002,
		0x0d7d8ee8, 0x01b9274d, 0x0d7d8ee8,
		0x09857a7b, 0xe4cae309, 0x00000002,
		0x0c999cbe, 0x038e89c5, 0x0c999cbe,
		0x0beae5bc, 0xe7ded2a4, 0x00000002,
		0x0b4b6e2c, 0x061cd206, 0x0b4b6e2c,
		0x0f6a2551, 0xec069422, 0x00000002,
		0x13ad5974, 0x129397e7, 0x13ad5974,
		0x13d3c166, 0xf11cacb8, 0x00000002,
		0x126195d4, 0x1b259a6c, 0x126195d4,
		0x184cdd94, 0xf634a151, 0x00000002,
		0x092aa1ea, 0x11add077, 0x092aa1ea,
		0x3682199e, 0xf31b28fc, 0x00000001,
		0x0e09b91b, 0x0010b76f, 0x0e09b91b,
		0x0f0e2575, 0xc19d364a, 0x00000001,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_176] = {
		0x1b4feb25, 0xfa1874df, 0x1b4feb25,
		0x0fc84364, 0xe27e7427, 0x00000002,
		0x0d22ad1f, 0xfe465ea8, 0x0d22ad1f,
		0x10d89ab2, 0xe4aa760e, 0x00000002,
		0x0c17b497, 0x004c9a14, 0x0c17b497,
		0x12ba36ef, 0xe7a11513, 0x00000002,
		0x0a968b87, 0x031b65c2, 0x0a968b87,
		0x157c39d1, 0xeb9561ce, 0x00000002,
		0x11cea26a, 0x0d025bcc, 0x11cea26a,
		0x18ef4a32, 0xf05a2342, 0x00000002,
		0x0fe5d188, 0x156af55c, 0x0fe5d188,
		0x1c6234df, 0xf50cd288, 0x00000002,
		0x07a1ea25, 0x0e900dd7, 0x07a1ea25,
		0x3d441ae6, 0xf0314c15, 0x00000001,
		0x0dd3517a, 0xfc7f1621, 0x0dd3517a,
		0x1ee4972a, 0xc193ad77, 0x00000001,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_256_to_96] = {
		0x0bad1c6d, 0xf7125e39, 0x0bad1c6d,
		0x200d2195, 0xe0e69a20, 0x00000002,
		0x0b7cc85d, 0xf7b2aa2b, 0x0b7cc85d,
		0x1fd4a137, 0xe2d2e8fc, 0x00000002,
		0x09ad4898, 0xf9f3edb1, 0x09ad4898,
		0x202ffee3, 0xe533035b, 0x00000002,
		0x073ebe31, 0xfcd552f2, 0x073ebe31,
		0x2110eb62, 0xe84975f6, 0x00000002,
		0x092af7cc, 0xff2b1fc9, 0x092af7cc,
		0x2262052a, 0xec1ceb75, 0x00000002,
		0x09655d3e, 0x04f0939d, 0x09655d3e,
		0x47cf219d, 0xe075904a, 0x00000001,
		0x021b3ca5, 0x03057f44, 0x021b3ca5,
		0x4a5c8f68, 0xe72b7f7b, 0x00000001,
		0x00000000, 0x389ecf53, 0x358ecf53,
		0x04b60049, 0x00000000, 0x00000004,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_128] = {
		0x0c4deacd, 0xf5b3be35, 0x0c4deacd,
		0x20349d1f, 0xe0b9a80d, 0x00000002,
		0x0c5dbbaa, 0xf6157998, 0x0c5dbbaa,
		0x200c143d, 0xe25209ea, 0x00000002,
		0x0a9de1bd, 0xf85ee460, 0x0a9de1bd,
		0x206099de, 0xe46a166c, 0x00000002,
		0x081f9a34, 0xfb7ffe47, 0x081f9a34,
		0x212dd0f7, 0xe753c9ab, 0x00000002,
		0x0a6f9ddb, 0xfd863e9e, 0x0a6f9ddb,
		0x226bd8a2, 0xeb2ead0b, 0x00000002,
		0x05497d0e, 0x01ebd7f0, 0x05497d0e,
		0x23eba2f6, 0xef958aff, 0x00000002,
		0x008e7c5f, 0x00be6aad, 0x008e7c5f,
		0x4a74b30a, 0xe6b0319a, 0x00000001,
		0x00000000, 0x38f3c5aa, 0x38f3c5aa,
		0x012e1306, 0x00000000, 0x00000006,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_128] = {
		0x08cc3b9a, 0xf7322a4e, 0x08cc3b9a,
		0x22659b56, 0xe04529b8, 0x00000002,
		0x0dfec98d, 0xf246d874, 0x0dfec98d,
		0x228c0369, 0xe0eb257c, 0x00000002,
		0x0c9132f7, 0xf4503094, 0x0c9132f7,
		0x232d11ae, 0xe1edc14e, 0x00000002,
		0x0a24dd72, 0xf7ab8c8b, 0x0a24dd72,
		0x24759a82, 0xe39fd6a1, 0x00000002,
		0x0d7b5edf, 0xf813b4f0, 0x0d7b5edf,
		0x269f376b, 0xe65655e7, 0x00000002,
		0x065de739, 0xff903f27, 0x065de739,
		0x29a35e05, 0xea109462, 0x00000002,
		0x01d526a4, 0x01dd9358, 0x01d526a4,
		0x2cac1287, 0xedcc0cda, 0x00000002,
		0x00000000, 0x1dfc1f40, 0x1dfc1f40,
		0x05c0aa28, 0x00000000, 0x00000004,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_96] = {
		0x0ba3aaf1, 0xf0c12941, 0x0ba3aaf1,
		0x2d8fe4ae, 0xe097f1ad, 0x00000002,
		0x0be92064, 0xf0b1f1a9, 0x0be92064,
		0x2d119d04, 0xe1e5fe1b, 0x00000002,
		0x0a1220de, 0xf3a9aff8, 0x0a1220de,
		0x2ccb18cb, 0xe39903cf, 0x00000002,
		0x07794a30, 0xf7c2c155, 0x07794a30,
		0x2ca647c8, 0xe5ef0ccd, 0x00000002,
		0x0910b1c4, 0xf84c9886, 0x0910b1c4,
		0x2c963877, 0xe8fbcb7a, 0x00000002,
		0x041d6154, 0xfec82c8a, 0x041d6154,
		0x2c926893, 0xec6aa839, 0x00000002,
		0x005b2676, 0x0050bb1f, 0x005b2676,
		0x5927e9f4, 0xde9fd5bc, 0x00000001,
		0x00000000, 0x2b1e5dc1, 0x2b1e5dc1,
		0x0164aa09, 0x00000000, 0x00000006,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_96] = {
		0x0481f41d, 0xf9c1b194, 0x0481f41d,
		0x31c66864, 0xe0581a1d, 0x00000002,
		0x0a3e5a4c, 0xf216665d, 0x0a3e5a4c,
		0x31c3de69, 0xe115ebae, 0x00000002,
		0x0855f15c, 0xf5369aef, 0x0855f15c,
		0x323c17ad, 0xe1feed04, 0x00000002,
		0x05caeeeb, 0xf940c54b, 0x05caeeeb,
		0x33295d2b, 0xe3295c94, 0x00000002,
		0x0651a46a, 0xfa4d6542, 0x0651a46a,
		0x3479d138, 0xe49580b2, 0x00000002,
		0x025e0ccb, 0xff36a412, 0x025e0ccb,
		0x35f517d7, 0xe6182a82, 0x00000002,
		0x0085eff3, 0x0074e0ca, 0x0085eff3,
		0x372ef0de, 0xe7504e71, 0x00000002,
		0x00000000, 0x29b76685, 0x29b76685,
		0x0deab1c3, 0x00000000, 0x00000003,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_88] = {
		0x0c95e01f, 0xed56f8fc, 0x0c95e01f,
		0x191b8467, 0xf0c99b0e, 0x00000003,
		0x0bbee41a, 0xef0e8160, 0x0bbee41a,
		0x31c02b41, 0xe2ef4cd9, 0x00000002,
		0x0a2d258f, 0xf2225b96, 0x0a2d258f,
		0x314c8bd2, 0xe4c10e08, 0x00000002,
		0x07f9e42a, 0xf668315f, 0x07f9e42a,
		0x30cf47d4, 0xe71e3418, 0x00000002,
		0x0afd6fa9, 0xf68f867d, 0x0afd6fa9,
		0x3049674d, 0xe9e0cf4b, 0x00000002,
		0x06ebc830, 0xffaa9acd, 0x06ebc830,
		0x2fcee1bf, 0xec81ee52, 0x00000002,
		0x010de038, 0x01a27806, 0x010de038,
		0x2f82d453, 0xee2ade9b, 0x00000002,
		0x064f0462, 0xf68a0d30, 0x064f0462,
		0x32c81742, 0xe07f3a37, 0x00000002,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_256_to_48] = {
		0x02b72fb4, 0xfb7c5152, 0x02b72fb4,
		0x374ab8ef, 0xe039095c, 0x00000002,
		0x05ca62de, 0xf673171b, 0x05ca62de,
		0x1b94186a, 0xf05c2de7, 0x00000003,
		0x09a9656a, 0xf05ffe29, 0x09a9656a,
		0x37394e81, 0xe1611f87, 0x00000002,
		0x06e86c29, 0xf54bf713, 0x06e86c29,
		0x37797f41, 0xe24ce1f6, 0x00000002,
		0x07a6b7c2, 0xf5491ea7, 0x07a6b7c2,
		0x37e40444, 0xe3856d91, 0x00000002,
		0x02bf8a3e, 0xfd2f5fa6, 0x02bf8a3e,
		0x38673190, 0xe4ea5a4d, 0x00000002,
		0x007e1bd5, 0x000e76ca, 0x007e1bd5,
		0x38da5414, 0xe61afd77, 0x00000002,
		0x00000000, 0x2038247b, 0x2038247b,
		0x07212644, 0x00000000, 0x00000004,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_64] = {
		0x05c89f29, 0xf6443184, 0x05c89f29,
		0x1bbe0f00, 0xf034bf19, 0x00000003,
		0x05e47be3, 0xf6284bfe, 0x05e47be3,
		0x1b73d610, 0xf0a9a268, 0x00000003,
		0x09eb6c29, 0xefbc8df5, 0x09eb6c29,
		0x365264ff, 0xe286ce76, 0x00000002,
		0x0741f28e, 0xf492d155, 0x0741f28e,
		0x35a08621, 0xe4320cfe, 0x00000002,
		0x087cdc22, 0xf3daa1c7, 0x087cdc22,
		0x34c55ef0, 0xe6664705, 0x00000002,
		0x038022af, 0xfc43da62, 0x038022af,
		0x33d2b188, 0xe8e92eb8, 0x00000002,
		0x001de8ed, 0x0001bd74, 0x001de8ed,
		0x33061aa8, 0xeb0d6ae7, 0x00000002,
		0x00000000, 0x3abd8743, 0x3abd8743,
		0x032b3f7f, 0x00000000, 0x00000005,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_64] = {
		0x05690759, 0xf69bdff3, 0x05690759,
		0x392fbdf5, 0xe032c3cc, 0x00000002,
		0x05c3ff7a, 0xf60d6b05, 0x05c3ff7a,
		0x1c831a72, 0xf052119a, 0x00000003,
		0x0999efb9, 0xefae71b0, 0x0999efb9,
		0x3900fd02, 0xe13a60b9, 0x00000002,
		0x06d5aa46, 0xf4c1d0ea, 0x06d5aa46,
		0x39199f34, 0xe20c15e1, 0x00000002,
		0x077f7d1d, 0xf49411e4, 0x077f7d1d,
		0x394b3591, 0xe321be50, 0x00000002,
		0x02a14b6b, 0xfcd3c8a5, 0x02a14b6b,
		0x398b4c12, 0xe45e5473, 0x00000002,
		0x00702155, 0xffef326c, 0x00702155,
		0x39c46c90, 0xe56c1e59, 0x00000002,
		0x00000000, 0x1c69d66c, 0x1c69d66c,
		0x0e76f270, 0x00000000, 0x00000003,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_48] = {
		0x05be8a21, 0xf589fb98, 0x05be8a21,
		0x1d8de063, 0xf026c3d8, 0x00000003,
		0x05ee4f4f, 0xf53df2e5, 0x05ee4f4f,
		0x1d4d87e2, 0xf07d5518, 0x00000003,
		0x0a015402, 0xee079bc7, 0x0a015402,
		0x3a0a0c2b, 0xe1e16c40, 0x00000002,
		0x07512c6a, 0xf322f651, 0x07512c6a,
		0x394e82c2, 0xe326def2, 0x00000002,
		0x087a5316, 0xf1d3ba1f, 0x087a5316,
		0x385bbd4a, 0xe4dbe26b, 0x00000002,
		0x035bd161, 0xfb2b7588, 0x035bd161,
		0x37464782, 0xe6d6a034, 0x00000002,
		0x00186dd8, 0xfff28830, 0x00186dd8,
		0x365746b9, 0xe88d9a4a, 0x00000002,
		0x00000000, 0x2cd02ed1, 0x2cd02ed1,
		0x035f6308, 0x00000000, 0x00000005,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_48] = {
		0x0c68c88c, 0xe9266466, 0x0c68c88c,
		0x1db3d4c3, 0xf0739c07, 0x00000003,
		0x05c69407, 0xf571a70a, 0x05c69407,
		0x1d6f1d3b, 0xf0d89718, 0x00000003,
		0x09e8d133, 0xee2a68df, 0x09e8d133,
		0x3a32d61b, 0xe2c2246a, 0x00000002,
		0x079233b7, 0xf2d17252, 0x079233b7,
		0x3959a2c3, 0xe4295381, 0x00000002,
		0x09c2822e, 0xf0613d7b, 0x09c2822e,
		0x385c3c48, 0xe5d3476b, 0x00000002,
		0x050e0b2c, 0xfa200d5d, 0x050e0b2c,
		0x37688f21, 0xe76fc030, 0x00000002,
		0x006ddb6e, 0x00523f01, 0x006ddb6e,
		0x36cd234d, 0xe8779510, 0x00000002,
		0x0635039f, 0xf488f773, 0x0635039f,
		0x3be42508, 0xe0488e99, 0x00000002,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_44] = {
		0x0c670696, 0xe8dc1ef2, 0x0c670696,
		0x1e05c266, 0xf06a9f0d, 0x00000003,
		0x05c60160, 0xf54b9f4a, 0x05c60160,
		0x1dc3811d, 0xf0c7e4db, 0x00000003,
		0x09e74455, 0xeddfc92a, 0x09e74455,
		0x3adfddda, 0xe28c4ae3, 0x00000002,
		0x078ea9ae, 0xf28c3ba7, 0x078ea9ae,
		0x3a0a98e8, 0xe3d93541, 0x00000002,
		0x09b32647, 0xefe954c5, 0x09b32647,
		0x3910a244, 0xe564f781, 0x00000002,
		0x04f0e9e4, 0xf9b7e8d5, 0x04f0e9e4,
		0x381f6928, 0xe6e5316c, 0x00000002,
		0x006303ee, 0x003ae836, 0x006303ee,
		0x37852c0e, 0xe7db78c1, 0x00000002,
		0x06337ac0, 0xf46665c5, 0x06337ac0,
		0x3c818406, 0xe042df81, 0x00000002,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_32] = {
		0x07d25973, 0xf0fd68ae, 0x07d25973,
		0x3dd9d640, 0xe02aaf11, 0x00000002,
		0x05a0521d, 0xf5390cc4, 0x05a0521d,
		0x1ec7dff7, 0xf044be0d, 0x00000003,
		0x04a961e1, 0xf71c730b, 0x04a961e1,
		0x1e9edeee, 0xf082b378, 0x00000003,
		0x06974728, 0xf38e3bf1, 0x06974728,
		0x3cd69b60, 0xe1afd01c, 0x00000002,
		0x072d4553, 0xf2c1e0e2, 0x072d4553,
		0x3c54fdc3, 0xe28e96b6, 0x00000002,
		0x02802de3, 0xfbb07dd5, 0x02802de3,
		0x3bc4f40f, 0xe38a3256, 0x00000002,
		0x000ce31b, 0xfff0d7a8, 0x000ce31b,
		0x3b4bbb40, 0xe45f55d6, 0x00000002,
		0x00000000, 0x1ea1b887, 0x1ea1b887,
		0x03b1b27d, 0x00000000, 0x00000005,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_32] = {
		0x0c5074a7, 0xe83ee090, 0x0c5074a7,
		0x1edf8fe7, 0xf04ec5d0, 0x00000003,
		0x05bbb01f, 0xf4fa20a7, 0x05bbb01f,
		0x1ea87e16, 0xf093b881, 0x00000003,
		0x04e8e57f, 0xf69fc31d, 0x04e8e57f,
		0x1e614210, 0xf0f1139e, 0x00000003,
		0x07756686, 0xf1f67c0b, 0x07756686,
		0x3c0a3b55, 0xe2d8c5a6, 0x00000002,
		0x097212e8, 0xeede0608, 0x097212e8,
		0x3b305555, 0xe3ff02e3, 0x00000002,
		0x0495d6c0, 0xf8bf1399, 0x0495d6c0,
		0x3a5c93a1, 0xe51e0d14, 0x00000002,
		0x00458b2d, 0xfffdc761, 0x00458b2d,
		0x39d4793b, 0xe5d6d407, 0x00000002,
		0x0609587b, 0xf456ed0f, 0x0609587b,
		0x3e1d20e1, 0xe0315c96, 0x00000002,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_352_to_24] = {
		0x062002ee, 0xf4075ac9, 0x062002ee,
		0x1f577599, 0xf0166280, 0x00000003,
		0x05cdb68c, 0xf4ab2e81, 0x05cdb68c,
		0x1f2a7a17, 0xf0484eb7, 0x00000003,
		0x04e3078b, 0xf67b954a, 0x04e3078b,
		0x1ef25b71, 0xf08a5bcf, 0x00000003,
		0x071fc81e, 0xf23391f6, 0x071fc81e,
		0x3d4bc51b, 0xe1cdf67e, 0x00000002,
		0x08359c1c, 0xf04d3910, 0x08359c1c,
		0x3c80bf1e, 0xe2c6cf99, 0x00000002,
		0x0331888d, 0xfa1ebde6, 0x0331888d,
		0x3b94c153, 0xe3e96fad, 0x00000002,
		0x00143063, 0xffe1d1af, 0x00143063,
		0x3ac672e3, 0xe4e7f96f, 0x00000002,
		0x00000000, 0x2d7cf831, 0x2d7cf831,
		0x074e3a4f, 0x00000000, 0x00000004,
	},
	[MT8188_AFE_GASRC_IIR_COEFF_384_to_24] = {
		0x0c513993, 0xe7dbde26, 0x0c513993,
		0x1f4e3b98, 0xf03b6bee, 0x00000003,
		0x05bd9980, 0xf4c4fb19, 0x05bd9980,
		0x1f21aa2b, 0xf06fa0e5, 0x00000003,
		0x04eb9c21, 0xf6692328, 0x04eb9c21,
		0x1ee6fb2f, 0xf0b6982c, 0x00000003,
		0x07795c9e, 0xf18d56cf, 0x07795c9e,
		0x3d345c1a, 0xe229a2a1, 0x00000002,
		0x096d3d11, 0xee265518, 0x096d3d11,
		0x3c7d096a, 0xe30bee74, 0x00000002,
		0x0478f0db, 0xf8270d5a, 0x0478f0db,
		0x3bc96998, 0xe3ea3cf8, 0x00000002,
		0x0037d4b8, 0xffdedcf0, 0x0037d4b8,
		0x3b553ec9, 0xe47a2910, 0x00000002,
		0x0607e296, 0xf42bc1d7, 0x0607e296,
		0x3ee67cb9, 0xe0252e31, 0x00000002,
	},
};

static bool mt8188_afe_gasrc_found_iir_coeff_table_id(int input_rate,
	int output_rate, int *table_id)
{
	int i;
	const struct mt8188_afe_gasrc_iir_coeff_table_id *table =
		mt8188_afe_gasrc_iir_coeff_table_ids;

	if (!table_id)
		return false;

	/* no need to apply iir for up-sample */
	if (input_rate <= output_rate)
		return false;

	for (i = 0; i < ARRAY_SIZE(mt8188_afe_gasrc_iir_coeff_table_ids); i++) {
		if ((table[i].input_rate == input_rate) &&
			(table[i].output_rate == output_rate)) {
			*table_id = table[i].table_id;
			return true;
		}
	}

	return false;
}

static bool mt8188_afe_gasrc_fill_iir_coeff_table(struct mtk_base_afe *afe,
	unsigned int gasrc_idx, int table_id)
{
	const unsigned int *table;
	unsigned int ctrl_reg;
	int i;

	if ((table_id < 0) ||
		(table_id >= MT8188_AFE_GASRC_IIR_TABLES))
		return false;

	dev_dbg(afe->dev, "%s [%u] table_id %d\n",
		__func__, gasrc_idx, table_id);

	table = &mt8188_afe_gasrc_iir_coeffs[table_id][0];

	/* enable access for iir sram */
	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_COEFF_SRAM_CTRL,
		AFE_GASRC_NEW_CON0_COEFF_SRAM_CTRL);

	/* fill coeffs from addr 0 */
	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con11;
	regmap_write(afe->regmap, ctrl_reg, 0);

	/* fill all coeffs */
	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con10;
	for (i = 0; i < IIR_NUMS; i++)
		regmap_write(afe->regmap, ctrl_reg, table[i]);

	/* disable access for iir sram */
	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;
	regmap_update_bits(afe->regmap, ctrl_reg,
		AFE_GASRC_NEW_CON0_COEFF_SRAM_CTRL, 0);

	return true;
}

static bool mt8188_afe_load_gasrc_iir_coeff_table(struct mtk_base_afe *afe,
	unsigned int gasrc_idx, int input_rate, int output_rate)
{
	int table_id;

	if (mt8188_afe_gasrc_found_iir_coeff_table_id(input_rate,
			output_rate, &table_id)) {
		return mt8188_afe_gasrc_fill_iir_coeff_table(afe,
			gasrc_idx, table_id);
	}

	return false;
}

static int mt8188_afe_configure_gasrc(struct mtk_base_afe *afe,
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_gasrc_priv *gasrc_priv;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	unsigned int stream = substream->stream;
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	int input_fs = 0, output_fs = 0;
	int input_rate = 0, output_rate = 0;
	unsigned int val = 0;
	unsigned int mask = 0;
	struct snd_soc_pcm_runtime *be = substream->private_data;
	struct snd_pcm_substream *paired_substream = NULL;
	bool duplex = false;
	bool *gasrc_op_freq_45m = NULL;
	unsigned int *cali_cycles = NULL;

	if (!is_valid_gasrc_dai(dai->id)) {
		dev_info(dai->dev, "%s invalid dai->id [%d]\n",
			__func__, dai->id);
		return -EINVAL;
	}
	gasrc_priv = afe_priv->dai_priv[dai->id];

	duplex = gasrc_priv->duplex;
	gasrc_op_freq_45m = &(gasrc_priv->op_freq_45m);
	cali_cycles = &(gasrc_priv->cali_cycles);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		input_fs = mt8188_afe_fs_timing(runtime->rate);
		input_rate = runtime->rate;
		*gasrc_op_freq_45m = (input_rate % 8000);

		if (duplex) {
			paired_substream = snd_soc_dpcm_get_substream(be,
				SNDRV_PCM_STREAM_CAPTURE);
			output_fs = mt8188_afe_gasrc_get_output_fs(
				paired_substream, dai,
				paired_substream->runtime->rate);
			output_rate = mt8188_afe_gasrc_get_output_rate(
				paired_substream, dai->id,
				paired_substream->runtime->rate);
		} else {
			output_fs = mt8188_afe_gasrc_get_output_fs(
				substream, dai, runtime->rate);
			output_rate = mt8188_afe_gasrc_get_output_rate(
				substream, dai->id, runtime->rate);
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		output_fs = mt8188_afe_fs_timing(runtime->rate);
		output_rate = runtime->rate;
		*gasrc_op_freq_45m = (output_rate % 8000);

		if (duplex) {
			paired_substream =
				snd_soc_dpcm_get_substream(be,
					SNDRV_PCM_STREAM_PLAYBACK);
			input_fs = mt8188_afe_gasrc_get_input_fs(
				paired_substream, dai,
				paired_substream->runtime->rate);
			input_rate = mt8188_afe_gasrc_get_input_rate(
				paired_substream, dai->id,
				paired_substream->runtime->rate);
		} else {
			input_fs = mt8188_afe_gasrc_get_input_fs(
				substream, dai, runtime->rate);
			input_rate = mt8188_afe_gasrc_get_input_rate(
				substream, dai->id, runtime->rate);
		}
	}

	dev_dbg(dai->dev,
		"%s [%d] '%s' in fs-rate 0x%x-%d out fs-rate 0x%x-%d 45m %d\n",
		__func__, gasrc_idx, snd_pcm_stream_str(substream),
		input_fs, input_rate, output_fs, output_rate,
		*gasrc_op_freq_45m);

	if (mt8188_afe_load_gasrc_iir_coeff_table(afe, gasrc_idx,
			input_rate, output_rate)) {
		mt8188_afe_gasrc_enable_iir(afe, gasrc_idx);
		gasrc_priv->iir_on = true;
	} else {
		mt8188_afe_gasrc_disable_iir(afe, gasrc_idx);
		gasrc_priv->iir_on = false;
	}

	mt8188_afe_gasrc_set_input_fs(afe, dai, input_fs);
	mt8188_afe_gasrc_set_output_fs(afe, dai, output_fs);

	gasrc_priv->cali_tx = false;
	gasrc_priv->cali_rx = false;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mt8188_afe_gasrc_is_tx_tracking(output_fs))
			gasrc_priv->cali_tx = true;
		else
			gasrc_priv->cali_tx = false;
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (mt8188_afe_gasrc_is_rx_tracking(input_fs))
			gasrc_priv->cali_rx = true;
		else
			gasrc_priv->cali_rx = false;

		gasrc_priv->cali_tx = false;
	}

	dev_dbg(dai->dev, "%s [%d] '%s' cali_tx %d, cali_rx %d\n",
		__func__, gasrc_idx, snd_pcm_stream_str(substream),
		gasrc_priv->cali_tx, gasrc_priv->cali_rx);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mt8188_afe_gasrc_set_tx_mode_fs(afe,
			dai, input_rate, output_rate);
		if (gasrc_priv->cali_tx) {
			mt8188_afe_gasrc_sel_cali_clk(afe,
				dai, *gasrc_op_freq_45m);
			mt8188_afe_gasrc_sel_lrck(afe, dai, output_fs);
		}
	} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		mt8188_afe_gasrc_set_rx_mode_fs(afe,
			dai, input_rate, output_rate);
		if (gasrc_priv->cali_rx) {
			mt8188_afe_gasrc_sel_cali_clk(afe,
				dai, *gasrc_op_freq_45m);
			mt8188_afe_gasrc_sel_lrck(afe, dai, input_fs);
		}
	}

	if (gasrc_priv->cali_tx || gasrc_priv->cali_rx) {
		val = (*gasrc_op_freq_45m) ?
		      AFE_GASRC_NEW_CON7_FREQ_CALC_DENOMINATOR_45M :
		      AFE_GASRC_NEW_CON7_FREQ_CALC_DENOMINATOR_49M;
		mask = AFE_GASRC_NEW_CON7_FREQ_CALC_DENOMINATOR_MASK;
		regmap_update_bits(afe->regmap, gasrc_ctrl_reg[gasrc_idx].con7,
			mask, val);

		val = AFE_GASRC_NEW_CON6_FREQ_CALI_CYCLE(*cali_cycles) |
		      AFE_GASRC_NEW_CON6_COMP_FREQ_RES_EN |
		      AFE_GASRC_NEW_CON6_FREQ_CALI_BP_DGL |
		      AFE_GASRC_NEW_CON6_CALI_USE_FREQ_OUT |
		      AFE_GASRC_NEW_CON6_FREQ_CALI_AUTO_RESTART;
		mask = AFE_GASRC_NEW_CON6_FREQ_CALI_CYCLE_MASK |
		       AFE_GASRC_NEW_CON6_COMP_FREQ_RES_EN |
		       AFE_GASRC_NEW_CON6_FREQ_CALI_BP_DGL |
		       AFE_GASRC_NEW_CON6_CALI_USE_FREQ_OUT |
		       AFE_GASRC_NEW_CON6_FREQ_CALI_AUTO_RESTART;

		regmap_update_bits(afe->regmap, gasrc_ctrl_reg[gasrc_idx].con6,
			mask, val);
	}

	return 0;
}

static int mt8188_afe_enable_gasrc(struct snd_soc_dai *dai,
	unsigned int stream)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_gasrc_priv *gasrc_priv;
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s [%d]\n", __func__, gasrc_idx);

	if (!is_valid_gasrc_dai(dai->id)) {
		dev_info(dai->dev, "%s invalid dai->id [%d]\n",
			__func__, dai->id);
		return -EINVAL;
	}
	gasrc_priv = afe_priv->dai_priv[dai->id];

	if (gasrc_priv->cali_tx || gasrc_priv->cali_rx) {
		ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con6;
		val = AFE_GASRC_NEW_CON6_CALI_EN;
		regmap_update_bits(afe->regmap, ctrl_reg, val, val);

		val = AFE_GASRC_NEW_CON6_AUTO_TUNE_FREQ2 |
		      AFE_GASRC_NEW_CON6_AUTO_TUNE_FREQ3;
		regmap_update_bits(afe->regmap, ctrl_reg, val, val);
	}

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;

	val = AFE_GASRC_NEW_CON0_ASM_ON;
	regmap_update_bits(afe->regmap, ctrl_reg, val, val);

	return ret;
}

static int mt8188_afe_disable_gasrc(struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_gasrc_priv *gasrc_priv;
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);
	unsigned int ctrl_reg = 0;
	unsigned int val = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s [%d]\n", __func__, gasrc_idx);

	if (!is_valid_gasrc_dai(dai->id)) {
		dev_info(dai->dev, "%s invalid dai->id [%d]\n",
			__func__, dai->id);
		return -EINVAL;
	}
	gasrc_priv = afe_priv->dai_priv[dai->id];

	ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con0;
	val = AFE_GASRC_NEW_CON0_ASM_ON;
	regmap_update_bits(afe->regmap, ctrl_reg, val, 0);

	if (gasrc_priv->cali_tx || gasrc_priv->cali_rx) {
		ctrl_reg = gasrc_ctrl_reg[gasrc_idx].con6;
		val = AFE_GASRC_NEW_CON6_CALI_EN;
		regmap_update_bits(afe->regmap, ctrl_reg, val, 0);
	}

	if (gasrc_priv->iir_on)
		mt8188_afe_gasrc_disable_iir(afe, gasrc_idx);

	return ret;
}

static int mtk_dai_gasrc_get_cg_id_by_dai_id(struct snd_soc_dai *dai)
{
	return DAI_TO_GASRC_CG_ID(dai->id);
}

/* dai ops */
static int mtk_dai_gasrc_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int cg_id = mtk_dai_gasrc_get_cg_id_by_dai_id(dai);
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);

	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_ASM_L_SEL]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_ASM_H_SEL]);

	if (cg_id >= 0)
		mt8188_afe_enable_clk(afe, afe_priv->clk[cg_id]);

	// TODO: CCF enable fail?
	mt8188_afe_enable_top_cg(afe, MT8188_TOP_CG_GASRC0 + gasrc_idx);

	return 0;
}

static void mtk_dai_gasrc_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int cg_id = mtk_dai_gasrc_get_cg_id_by_dai_id(dai);
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);

	if (cg_id >= 0)
		mt8188_afe_disable_clk(afe, afe_priv->clk[cg_id]);

	// TODO: CCF enable fail?
	mt8188_afe_disable_top_cg(afe, MT8188_TOP_CG_GASRC0 + gasrc_idx);

	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_ASM_H_SEL]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_ASM_L_SEL]);
}

static int mtk_dai_gasrc_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_gasrc_priv *gasrc_priv;
	unsigned int gasrc_idx = DAI_TO_GASRC_ID(dai->id);

	if (!is_valid_gasrc_dai(dai->id)) {
		dev_info(dai->dev, "%s invalid dai->id [%d]\n",
			__func__, dai->id);
		return -EINVAL;
	}
	gasrc_priv = afe_priv->dai_priv[dai->id];

	gasrc_priv->duplex = false;
	gasrc_priv->op_freq_45m = false;
	gasrc_priv->cali_cycles = 64;

	if (dai->stream_active[SNDRV_PCM_STREAM_CAPTURE] &&
	    dai->stream_active[SNDRV_PCM_STREAM_PLAYBACK])
		gasrc_priv->duplex = true;

	dev_dbg(dai->dev, "%s [%d] '%s' duplex %d\n", __func__,
		gasrc_idx, snd_pcm_stream_str(substream),
		gasrc_priv->duplex);

	if (gasrc_priv->duplex)
		mt8188_afe_disable_gasrc(dai);

	mt8188_afe_reset_gasrc(afe, dai);
	mt8188_afe_clear_gasrc(afe, dai);
	mt8188_afe_configure_gasrc(afe, substream, dai);

	return 0;
}

static int mtk_dai_gasrc_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	dev_dbg(dai->dev, "%s [%d] '%s' cmd %d\n", __func__,
		DAI_TO_GASRC_ID(dai->id),
		snd_pcm_stream_str(substream), cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = mt8188_afe_enable_gasrc(dai, substream->stream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = mt8188_afe_disable_gasrc(dai);
		mt8188_afe_reset_gasrc(afe, dai);
		mt8188_afe_clear_gasrc(afe, dai);
		break;
	default:
		break;
	}

	return ret;
}

static const struct snd_soc_dai_ops mtk_dai_gasrc_ops = {
	.startup	= mtk_dai_gasrc_startup,
	.shutdown	= mtk_dai_gasrc_shutdown,
	.prepare	= mtk_dai_gasrc_prepare,
	.trigger	= mtk_dai_gasrc_trigger,
};

/* dai driver */
#define MTK_GASRC_RATES (SNDRV_PCM_RATE_8000_48000 |\
			 SNDRV_PCM_RATE_88200 |\
			 SNDRV_PCM_RATE_96000 |\
			 SNDRV_PCM_RATE_176400 |\
			 SNDRV_PCM_RATE_192000 |\
			 SNDRV_PCM_RATE_352800 |\
			 SNDRV_PCM_RATE_384000)

#define MTK_GASRC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			   SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mtk_dai_gasrc_driver[] = {
	{
		.name = "GASRC0",
		.id = MT8188_AFE_IO_GASRC0,
		.playback = {
			.stream_name = "GASRC0 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC0 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC1",
		.id = MT8188_AFE_IO_GASRC1,
		.playback = {
			.stream_name = "GASRC1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC2",
		.id = MT8188_AFE_IO_GASRC2,
		.playback = {
			.stream_name = "GASRC2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC3",
		.id = MT8188_AFE_IO_GASRC3,
		.playback = {
			.stream_name = "GASRC3 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC3 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC4",
		.id = MT8188_AFE_IO_GASRC4,
		.playback = {
			.stream_name = "GASRC4 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC4 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC5",
		.id = MT8188_AFE_IO_GASRC5,
		.playback = {
			.stream_name = "GASRC5 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC5 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC6",
		.id = MT8188_AFE_IO_GASRC6,
		.playback = {
			.stream_name = "GASRC6 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC6 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC7",
		.id = MT8188_AFE_IO_GASRC7,
		.playback = {
			.stream_name = "GASRC7 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC7 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC8",
		.id = MT8188_AFE_IO_GASRC8,
		.playback = {
			.stream_name = "GASRC8 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC8 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC9",
		.id = MT8188_AFE_IO_GASRC9,
		.playback = {
			.stream_name = "GASRC9 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC9 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC10",
		.id = MT8188_AFE_IO_GASRC10,
		.playback = {
			.stream_name = "GASRC10 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC10 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
	{
		.name = "GASRC11",
		.id = MT8188_AFE_IO_GASRC11,
		.playback = {
			.stream_name = "GASRC11 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.capture = {
			.stream_name = "GASRC11 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_GASRC_RATES,
			.formats = MTK_GASRC_FORMATS,
		},
		.ops = &mtk_dai_gasrc_ops,
	},
};

static const char * const mtk_dai_gasrc_1x_en_sel_text[] = {
	"a1sys_a2sys", "a3sys", "a4sys",
};

static const unsigned int mtk_dai_gasrc_1x_en_sel_values[] = {
	0, 1, 2,
};

static int mtk_dai_gasrc_in_1x_en_sel_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	unsigned int dai_id = kcontrol->id.device;
	long val = ucontrol->value.integer.value[0];
	int ret = 0;
	struct mtk_dai_gasrc_priv *gasrc_priv = afe_priv->dai_priv[dai_id];

	if (val == gasrc_priv->in_asys_timing_sel)
		return 0;

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);

	gasrc_priv->in_asys_timing_sel = val;

	return ret;
}

static int mtk_dai_gasrc_out_1x_en_sel_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	unsigned int dai_id = kcontrol->id.device;
	long val = ucontrol->value.integer.value[0];
	int ret = 0;
	struct mtk_dai_gasrc_priv *gasrc_priv = afe_priv->dai_priv[dai_id];

	if (val == gasrc_priv->out_asys_timing_sel)
		return 0;

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);

	gasrc_priv->out_asys_timing_sel = val;

	return ret;
}

static SOC_VALUE_ENUM_SINGLE_DECL(gasrc0_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 0, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc1_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 2, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc2_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 4, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc3_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 6, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc4_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 8, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc5_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 10, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc6_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 12, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc7_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 14, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc8_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 16, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc9_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 18, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc10_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 20, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc11_in_1x_en_sel_enum,
			A3_A4_TIMING_SEL2, 22, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);

static SOC_VALUE_ENUM_SINGLE_DECL(gasrc0_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 0, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc1_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 2, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc2_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 4, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc3_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 6, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc4_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 8, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc5_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 10, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc6_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 12, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc7_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 14, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc8_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 16, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc9_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 18, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc10_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 20, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(gasrc11_out_1x_en_sel_enum,
			A3_A4_TIMING_SEL4, 22, 0x3,
			mtk_dai_gasrc_1x_en_sel_text,
			mtk_dai_gasrc_1x_en_sel_values);

static const struct snd_kcontrol_new mtk_dai_gasrc_controls[] = {
	MT8188_SOC_ENUM_EXT("gasrc0_in_1x_en_sel",
		gasrc0_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC0),
	MT8188_SOC_ENUM_EXT("gasrc1_in_1x_en_sel",
		gasrc1_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC1),
	MT8188_SOC_ENUM_EXT("gasrc2_in_1x_en_sel",
		gasrc2_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC2),
	MT8188_SOC_ENUM_EXT("gasrc3_in_1x_en_sel",
		gasrc3_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC3),
	MT8188_SOC_ENUM_EXT("gasrc4_in_1x_en_sel",
		gasrc4_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC4),
	MT8188_SOC_ENUM_EXT("gasrc5_in_1x_en_sel",
		gasrc5_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC5),
	MT8188_SOC_ENUM_EXT("gasrc6_in_1x_en_sel",
		gasrc6_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC6),
	MT8188_SOC_ENUM_EXT("gasrc7_in_1x_en_sel",
		gasrc7_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC7),
	MT8188_SOC_ENUM_EXT("gasrc8_in_1x_en_sel",
		gasrc8_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC8),
	MT8188_SOC_ENUM_EXT("gasrc9_in_1x_en_sel",
		gasrc9_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC9),
	MT8188_SOC_ENUM_EXT("gasrc10_in_1x_en_sel",
		gasrc10_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC10),
	MT8188_SOC_ENUM_EXT("gasrc11_in_1x_en_sel",
		gasrc11_in_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_in_1x_en_sel_put,
		MT8188_AFE_IO_GASRC11),

	MT8188_SOC_ENUM_EXT("gasrc0_out_1x_en_sel",
		gasrc0_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC0),
	MT8188_SOC_ENUM_EXT("gasrc1_out_1x_en_sel",
		gasrc1_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC1),
	MT8188_SOC_ENUM_EXT("gasrc2_out_1x_en_sel",
		gasrc2_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC2),
	MT8188_SOC_ENUM_EXT("gasrc3_out_1x_en_sel",
		gasrc3_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC3),
	MT8188_SOC_ENUM_EXT("gasrc4_out_1x_en_sel",
		gasrc4_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC4),
	MT8188_SOC_ENUM_EXT("gasrc5_out_1x_en_sel",
		gasrc5_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC5),
	MT8188_SOC_ENUM_EXT("gasrc6_out_1x_en_sel",
		gasrc6_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC6),
	MT8188_SOC_ENUM_EXT("gasrc7_out_1x_en_sel",
		gasrc7_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC7),
	MT8188_SOC_ENUM_EXT("gasrc8_out_1x_en_sel",
		gasrc8_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC8),
	MT8188_SOC_ENUM_EXT("gasrc9_out_1x_en_sel",
		gasrc9_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC9),
	MT8188_SOC_ENUM_EXT("gasrc10_out_1x_en_sel",
		gasrc10_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC10),
	MT8188_SOC_ENUM_EXT("gasrc11_out_1x_en_sel",
		gasrc11_out_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mtk_dai_gasrc_out_1x_en_sel_put,
		MT8188_AFE_IO_GASRC11),
};

static int init_gasrc_priv_data(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_gasrc_priv *gasrc_priv;
	int i;

	for (i = MT8188_AFE_IO_GASRC_START; i < MT8188_AFE_IO_GASRC_END; i++) {
		gasrc_priv = devm_kzalloc(afe->dev,
					 sizeof(struct mtk_dai_gasrc_priv),
					 GFP_KERNEL);
		if (!gasrc_priv)
			return -ENOMEM;

		afe_priv->dai_priv[i] = gasrc_priv;
	}
	return 0;
}

int mt8188_dai_gasrc_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mtk_dai_gasrc_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mtk_dai_gasrc_driver);

	dai->dapm_widgets = mtk_dai_gasrc_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mtk_dai_gasrc_widgets);
	dai->dapm_routes = mtk_dai_gasrc_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mtk_dai_gasrc_routes);
	dai->controls = mtk_dai_gasrc_controls;
	dai->num_controls = ARRAY_SIZE(mtk_dai_gasrc_controls);

	return init_gasrc_priv_data(afe);
}
