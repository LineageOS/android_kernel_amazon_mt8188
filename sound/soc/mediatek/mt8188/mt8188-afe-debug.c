// SPDX-License-Identifier: GPL-2.0
/*
 * Mediatek ALSA SoC AFE debug control for 8188
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/debugfs.h>
#include "mt8188-afe-common.h"
#include "mt8188-afe-clk.h"
#include "mt8188-afe-debug.h"
#include "mt8188-reg.h"

enum {
	MT8188_AFE_DEBUG_FS_ADDA,
	MT8188_AFE_DEBUG_FS_CLK,
	MT8188_AFE_DEBUG_FS_CLK_TUNER,
	MT8188_AFE_DEBUG_FS_DMIC,
	MT8188_AFE_DEBUG_FS_DPTX,
	MT8188_AFE_DEBUG_FS_ETDM,
	MT8188_AFE_DEBUG_FS_GASRC_0_7,
	MT8188_AFE_DEBUG_FS_GASRC_8_11,
	MT8188_AFE_DEBUG_FS_GASRC_TIMING,
	MT8188_AFE_DEBUG_FS_HW_GAIN,
	MT8188_AFE_DEBUG_FS_IRQ,
	MT8188_AFE_DEBUG_FS_LPBK_CFG,
	MT8188_AFE_DEBUG_FS_MEMIF,
	MT8188_AFE_DEBUG_FS_MPHONE_MULTI,
	MT8188_AFE_DEBUG_FS_PCMIF,
	MT8188_AFE_DEBUG_FS_SGEN,
	MT8188_AFE_DEBUG_FS_NUM,
};

static int mt8188_afe_dbg_ctrl_sgen_en_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int ret;

	unsigned int clk = MT8188_CLK_AUD_TML;

	if (ucontrol->value.integer.value[0])
		mt8188_afe_enable_clk(afe, afe_priv->clk[clk]);
	else
		mt8188_afe_disable_clk(afe, afe_priv->clk[clk]);

	ret = snd_soc_put_volsw(kcontrol, ucontrol);

	return ret;
}

static int mt8188_afe_dbg_ctrl_ulsgen_en_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int ret;

	if (ucontrol->value.integer.value[0]) {
		mt8188_afe_enable_clk(afe,
				      afe_priv->clk[MT8188_CLK_AUD_UL_TML]);
		mt8188_afe_enable_clk(afe,
				      afe_priv->clk[MT8188_CLK_AUD_UL_TML_HIRES]);
	} else {
		mt8188_afe_disable_clk(afe,
				       afe_priv->clk[MT8188_CLK_AUD_UL_TML]);
		mt8188_afe_disable_clk(afe,
				       afe_priv->clk[MT8188_CLK_AUD_UL_TML_HIRES]);
	}

	ret = snd_soc_put_volsw(kcontrol, ucontrol);

	return ret;
}

static int mt8188_afe_dbg_ctrl_dmic_sgen_en_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int ret;

	unsigned int clk = MT8188_CLK_AUD_AFE_26M_DMIC_TM;

	if (ucontrol->value.integer.value[0])
		mt8188_afe_enable_clk(afe, afe_priv->clk[clk]);
	else
		mt8188_afe_disable_clk(afe, afe_priv->clk[clk]);

	ret = snd_soc_put_volsw(kcontrol, ucontrol);

	return ret;
}

/* afe_sgen_mode_ch1/afe_sgen_mode_ch2 */
static const char * const mt8188_afe_sgen_mode_text[] = {
	"8k", "12k", "16k", "24k", "32k", "48k", "96k", "192k", "384k",
	"7k", "11k", "14k", "22k", "29k", "44k", "88k", "176k", "352k",
};

static const unsigned int mt8188_afe_sgen_mode_values[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
	16, 17, 18, 19, 20, 21, 22, 23, 24,
};

static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_afe_sgen_mode_ch1_enum,
			AFE_SINEGEN_CON1, 16, 0x1f,
			mt8188_afe_sgen_mode_text,
			mt8188_afe_sgen_mode_values);

static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_afe_sgen_mode_ch2_enum,
			AFE_SINEGEN_CON1, 21, 0x1f,
			mt8188_afe_sgen_mode_text,
			mt8188_afe_sgen_mode_values);


/* afe_sgen_c_inner_lpbk_mode */
static const char * const mt8188_afe_sgen_c_inner_lpbk_mode_text[] = {
	"cm0", "ul9_2ch", "cm1", "cm2", "ul2_2ch", "pcmtx", "ul5",
	"etdm_out3_ch01", "etdm_out3_ch23", "etdm_out3_ch45", "etdm_out3_ch67",
	"ul10_2ch", "ul3",  "ul4",
	"gasrc0_in", "gasrc1_in", "gasrc2_in", "gasrc3_in", "gasrc4_in",
	"gasrc5_in", "gasrc6_in", "gasrc7_in", "gasrc8_in", "gasrc9_in",
	"gasrc10_in", "gasrc11_in", "gasrc12_in", "gasrc13_in", "gasrc14_in",
	"gasrc15_in", "gasrc16_in", "gasrc17_in", "gasrc18_in", "gasrc19_in",
	"dl6", "pcmrx", "dmic1", "dmic2", "dmic3", "dmic4",
	"etdm_in2_ch01", "etdm_in2_ch23", "etdm_in2_ch45", "etdm_in2_ch78",
	"dl3", "i22_i23", "i24_i25", "i26_i27", "i28_i29", "i30_i31",
	"i32_i33", "i34_i35", "i36_i37", "i38_i39", "i40_i41", "i42_i43",
	"i44_i45", "i46_i47", "i48_i49", "i50_i51", "i52_i53", "i54_i55",
	"i56_i57", "i58_i59", "i60_i61", "i62_i63", "i64_i65", "i66_i67",
	"i68_i69", "dl2",
	"i72_i73", "i74_i75", "i76_i77", "i78_i79", "i80_i81", "i82_i83",
	"i84_i85", "i86_i87", "i88_i89", "i90_i91", "i92_i93", "i94_i95",
	"gasrc0_out", "gasrc1_out", "gasrc2_out", "gasrc3_out", "gasrc4_out",
	"gasrc5_out", "gasrc6_out", "gasrc7_out", "gasrc8_out", "gasrc9_out",
	"gasrc10_out", "gasrc11_out", "gasrc12_out", "gasrc13_out",
	"gasrc14_out", "gasrc15_out", "gasrc16_out", "gasrc17_out",
	"gasrc18_out", "gasrc19_out",
	"hw_gain1_out", "hw_gain2_out", "hw_gain1_in", "hw_gain2_in",
	"off",
};

static const unsigned int mt8188_afe_sgen_c_inner_lpbk_mode_values[] = {
	0x1, 0x11, 0x12, 0x13, 0x16, 0x17, 0x18,
	0x19, 0x1a, 0x1b, 0x1c,
	0x1d, 0x1e, 0x1f,
	0x25, 0x26, 0x27, 0x28, 0x29,
	0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
	0x2f, 0x30, 0x31, 0x32, 0x33,
	0x34, 0x35, 0x36, 0x37, 0x38,
	0x39, 0x3a, 0x3e, 0x3f, 0x40, 0x41,
	0x43, 0x44, 0x45, 0x46,
	0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
	0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52,
	0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
	0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
	0x5f, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
	0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
	0x6d, 0x6e, 0x6f, 0x70, 0x71,
	0x72, 0x73, 0x74, 0x75, 0x76,
	0x77, 0x78, 0x79, 0x7a,
	0x7b, 0x7c, 0x7d, 0x7e,
	0x7f, 0x80,
	0x81, 0x82, 0x83, 0x84,
	0xff,
};

static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_afe_sgen_c_inner_lpbk_mode_enum,
			AFE_SINEGEN_CON2, 24, 0xff,
			mt8188_afe_sgen_c_inner_lpbk_mode_text,
			mt8188_afe_sgen_c_inner_lpbk_mode_values);

// TODO: 96k for mt8188
/* dmic ul src sgen */
static const char * const mt8188_dmic_ul_src_sgen_mode_text[] = {
	"8k", "11k", "12k", "16k", "22k", "24k", "32k", "44k", "48k",
	"48k", "48k", "48k", "48k", "48k", "48k", "48k",
};

static const unsigned int mt8188_dmic_ul_src_sgen_values[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
	9, 10, 11, 12, 13, 14, 15,
};

/* dmic1_ul_src_sgen_mode_ch1/dmic1_ul_src_sgen_mode_ch2 */
static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic1_ul_src_sgen_mode_ch1_enum,
			AFE_DMIC0_UL_SRC_CON1, 0, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic1_ul_src_sgen_mode_ch2_enum,
			AFE_DMIC0_UL_SRC_CON1, 12, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

/* dmic2_ul_src_sgen_mode_ch1/dmic2_ul_src_sgen_mode_ch2 */
static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic2_ul_src_sgen_mode_ch1_enum,
			AFE_DMIC1_UL_SRC_CON1, 0, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic2_ul_src_sgen_mode_ch2_enum,
			AFE_DMIC1_UL_SRC_CON1, 12, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

/* dmic3_ul_src_sgen_mode_ch1/dmic3_ul_src_sgen_mode_ch2 */
static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic3_ul_src_sgen_mode_ch1_enum,
			AFE_DMIC2_UL_SRC_CON1, 0, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic3_ul_src_sgen_mode_ch2_enum,
			AFE_DMIC2_UL_SRC_CON1, 12, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

/* dmic4_ul_src_sgen_mode_ch1/dmic4_ul_src_sgen_mode_ch2 */
static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic4_ul_src_sgen_mode_ch1_enum,
			AFE_DMIC3_UL_SRC_CON1, 0, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

static SOC_VALUE_ENUM_SINGLE_DECL(mt8188_dmic4_ul_src_sgen_mode_ch2_enum,
			AFE_DMIC3_UL_SRC_CON1, 12, 0xf,
			mt8188_dmic_ul_src_sgen_mode_text,
			mt8188_dmic_ul_src_sgen_values);

static const struct snd_kcontrol_new mt8188_afe_debug_controls[] = {
	SOC_SINGLE_EXT("afe_sgen_en",
		AFE_SINEGEN_CON0, 26, 0x1, 0,
		snd_soc_get_volsw,
		mt8188_afe_dbg_ctrl_sgen_en_put),
	SOC_SINGLE("afe_sgen_freq_div_ch1",
		AFE_SINEGEN_CON0, 0, 0x1f, 0),
	SOC_SINGLE("afe_sgen_freq_div_ch2",
		AFE_SINEGEN_CON0, 12, 0x1f, 0),
	SOC_SINGLE("afe_sgen_amp_div_ch1",
		AFE_SINEGEN_CON0, 5, 0x7, 0),
	SOC_SINGLE("afe_sgen_amp_div_ch2",
		AFE_SINEGEN_CON0, 17, 0x7, 0),
	SOC_SINGLE("afe_sgen_mute_ch1",
		AFE_SINEGEN_CON0, 24, 0x1, 0),
	SOC_SINGLE("afe_sgen_mute_ch2",
		AFE_SINEGEN_CON0, 25, 0x1, 0),
	SOC_ENUM("afe_sgen_mode_ch1",
		mt8188_afe_sgen_mode_ch1_enum),
	SOC_ENUM("afe_sgen_mode_ch2",
		mt8188_afe_sgen_mode_ch2_enum),
	SOC_ENUM("afe_sgen_c_inner_lpbk_mode",
		mt8188_afe_sgen_c_inner_lpbk_mode_enum),
	SOC_SINGLE("cm0_debug_mode",
		AFE_CM0_CON, 31, 0x1, 0),
	SOC_SINGLE("cm1_debug_mode",
		AFE_CM1_CON, 31, 0x1, 0),
	SOC_SINGLE("cm0_ul_sel",
		AFE_CM0_CON, 30, 0x1, 0),
	SOC_SINGLE("cm1_ul_sel",
		AFE_CM1_CON, 30, 0x1, 0),
	SOC_SINGLE("cm2_ul_sel",
		AFE_CM2_CON, 30, 0x1, 0),
	SOC_SINGLE("dl11_sgen_on",
		AFE_SINEGEN_CON1, 15, 0x1, 0),
	SOC_SINGLE("dmic1_sgen_on",
		AFE_SINEGEN_CON1, 0, 0x1, 0),
	SOC_SINGLE("dmic2_sgen_on",
		AFE_SINEGEN_CON1, 1, 0x1, 0),
	SOC_SINGLE("dmic3_sgen_on",
		AFE_SINEGEN_CON1, 2, 0x1, 0),
	SOC_SINGLE("dmic4_sgen_on",
		AFE_SINEGEN_CON1, 3, 0x1, 0),
	SOC_SINGLE_EXT("dmic1_ul_src_sgen_en",
		AFE_DMIC0_UL_SRC_CON1, 27, 0x1, 0,
		snd_soc_get_volsw,
		mt8188_afe_dbg_ctrl_dmic_sgen_en_put),
	SOC_SINGLE_EXT("dmic2_ul_src_sgen_en",
		AFE_DMIC1_UL_SRC_CON1, 27, 0x1, 0,
		snd_soc_get_volsw,
		mt8188_afe_dbg_ctrl_dmic_sgen_en_put),
	SOC_SINGLE_EXT("dmic3_ul_src_sgen_en",
		AFE_DMIC2_UL_SRC_CON1, 27, 0x1, 0,
		snd_soc_get_volsw,
		mt8188_afe_dbg_ctrl_dmic_sgen_en_put),
	SOC_SINGLE_EXT("dmic4_ul_src_sgen_en",
		AFE_DMIC3_UL_SRC_CON1, 27, 0x1, 0,
		snd_soc_get_volsw,
		mt8188_afe_dbg_ctrl_dmic_sgen_en_put),
	SOC_SINGLE("dmic1_ul_src_sgen_freq_div_ch1",
		AFE_DMIC0_UL_SRC_CON1, 4, 0x1f, 0),
	SOC_SINGLE("dmic1_ul_src_sgen_freq_div_ch2",
		AFE_DMIC0_UL_SRC_CON1, 16, 0x1f, 0),
	SOC_SINGLE("dmic2_ul_src_sgen_freq_div_ch1",
		AFE_DMIC1_UL_SRC_CON1, 4, 0x1f, 0),
	SOC_SINGLE("dmic2_ul_src_sgen_freq_div_ch2",
		AFE_DMIC1_UL_SRC_CON1, 16, 0x1f, 0),
	SOC_SINGLE("dmic3_ul_src_sgen_freq_div_ch1",
		AFE_DMIC2_UL_SRC_CON1, 4, 0x1f, 0),
	SOC_SINGLE("dmic3_ul_src_sgen_freq_div_ch2",
		AFE_DMIC2_UL_SRC_CON1, 16, 0x1f, 0),
	SOC_SINGLE("dmic4_ul_src_sgen_freq_div_ch1",
		AFE_DMIC3_UL_SRC_CON1, 4, 0x1f, 0),
	SOC_SINGLE("dmic4_ul_src_sgen_freq_div_ch2",
		AFE_DMIC3_UL_SRC_CON1, 16, 0x1f, 0),
	SOC_SINGLE("dmic1_ul_src_sgen_amp_div_ch1",
		AFE_DMIC0_UL_SRC_CON1, 9, 0x7, 0),
	SOC_SINGLE("dmic1_ul_src_sgen_amp_div_ch2",
		AFE_DMIC0_UL_SRC_CON1, 21, 0x7, 0),
	SOC_SINGLE("dmic2_ul_src_sgen_amp_div_ch1",
		AFE_DMIC1_UL_SRC_CON1, 9, 0x7, 0),
	SOC_SINGLE("dmic2_ul_src_sgen_amp_div_ch2",
		AFE_DMIC1_UL_SRC_CON1, 21, 0x7, 0),
	SOC_SINGLE("dmic3_ul_src_sgen_amp_div_ch1",
		AFE_DMIC2_UL_SRC_CON1, 9, 0x7, 0),
	SOC_SINGLE("dmic3_ul_src_sgen_amp_div_ch2",
		AFE_DMIC2_UL_SRC_CON1, 21, 0x7, 0),
	SOC_SINGLE("dmic4_ul_src_sgen_amp_div_ch1",
		AFE_DMIC3_UL_SRC_CON1, 9, 0x7, 0),
	SOC_SINGLE("dmic4_ul_src_sgen_amp_div_ch2",
		AFE_DMIC3_UL_SRC_CON1, 21, 0x7, 0),
	SOC_ENUM("dmic1_ul_src_sgen_mode_ch1",
		mt8188_dmic1_ul_src_sgen_mode_ch1_enum),
	SOC_ENUM("dmic1_ul_src_sgen_mode_ch2",
		mt8188_dmic1_ul_src_sgen_mode_ch2_enum),
	SOC_ENUM("dmic2_ul_src_sgen_mode_ch1",
		mt8188_dmic2_ul_src_sgen_mode_ch1_enum),
	SOC_ENUM("dmic2_ul_src_sgen_mode_ch2",
		mt8188_dmic2_ul_src_sgen_mode_ch2_enum),
	SOC_ENUM("dmic3_ul_src_sgen_mode_ch1",
		mt8188_dmic3_ul_src_sgen_mode_ch1_enum),
	SOC_ENUM("dmic3_ul_src_sgen_mode_ch2",
		mt8188_dmic3_ul_src_sgen_mode_ch2_enum),
	SOC_ENUM("dmic4_ul_src_sgen_mode_ch1",
		mt8188_dmic4_ul_src_sgen_mode_ch1_enum),
	SOC_ENUM("dmic4_ul_src_sgen_mode_ch2",
		mt8188_dmic4_ul_src_sgen_mode_ch2_enum),
	SOC_SINGLE("etdm_in2_sgen_on1",
		AFE_SINEGEN_CON1, 4, 0x1, 0),
	SOC_SINGLE("etdm_in2_sgen_on2",
		AFE_SINEGEN_CON1, 5, 0x1, 0),
	SOC_SINGLE("etdm_in2_sgen_on3",
		AFE_SINEGEN_CON1, 6, 0x1, 0),
	SOC_SINGLE("etdm_in2_sgen_on4",
		AFE_SINEGEN_CON1, 7, 0x1, 0),
	SOC_SINGLE("etdm_out1_use_sgen",
		ETDM_COWORK_CON3, 29, 0x1, 0),
	SOC_SINGLE("etdm_out2_use_sgen",
		ETDM_COWORK_CON3, 30, 0x1, 0),
	SOC_SINGLE("etdm_out3_use_sgen",
		ETDM_COWORK_CON3, 31, 0x1, 0),
	SOC_SINGLE("gasrc_sgen_on1",
		AFE_SINEGEN_CON1, 12, 0x1, 0),
	SOC_SINGLE("gasrc_sgen_on2",
		AFE_SINEGEN_CON1, 13, 0x1, 0),
	SOC_SINGLE("ul8_use_sgen",
		ASYS_TOP_CON, 8, 0x1, 0),
	SOC_SINGLE("asys_lp_mode",
		ASYS_TOP_CON, 3, 0x1, 0),
	SOC_SINGLE("mphone_multi_in1_sgen_en",
		AFE_MPHONE_MULTI_CON2, 18, 0x1, 0),
	SOC_SINGLE("mphone_multi_in1_sgen_freq_div_ch1",
		AFE_MPHONE_MULTI_CON2, 0, 0x1f, 0),
	SOC_SINGLE("mphone_multi_in1_sgen_freq_div_ch2",
		AFE_MPHONE_MULTI_CON2, 8, 0x1f, 0),
	SOC_SINGLE("mphone_multi_in1_sgen_amp_div_ch1",
		AFE_MPHONE_MULTI_CON2, 5, 0x7, 0),
	SOC_SINGLE("mphone_multi_in1_sgen_amp_div_ch2",
		AFE_MPHONE_MULTI_CON2, 13, 0x7, 0),
	SOC_SINGLE("mphone_multi_in1_sgen_mute_ch1",
		AFE_MPHONE_MULTI_CON2, 16, 0x1, 0),
	SOC_SINGLE("mphone_multi_in1_sgen_mute_ch2",
		AFE_MPHONE_MULTI_CON2, 17, 0x1, 0),
	SOC_SINGLE("mphone_multi_in2_sgen_en",
		AFE_MPHONE_MULTI2_CON2, 18, 0x1, 0),
	SOC_SINGLE("mphone_multi_in2_sgen_freq_div_ch1",
		AFE_MPHONE_MULTI2_CON2, 0, 0x1f, 0),
	SOC_SINGLE("mphone_multi_in2_sgen_freq_div_ch2",
		AFE_MPHONE_MULTI2_CON2, 8, 0x1f, 0),
	SOC_SINGLE("mphone_multi_in2_sgen_amp_div_ch1",
		AFE_MPHONE_MULTI2_CON2, 5, 0x7, 0),
	SOC_SINGLE("mphone_multi_in2_sgen_amp_div_ch2",
		AFE_MPHONE_MULTI2_CON2, 13, 0x7, 0),
	SOC_SINGLE("mphone_multi_in2_sgen_mute_ch1",
		AFE_MPHONE_MULTI2_CON2, 16, 0x1, 0),
	SOC_SINGLE("mphone_multi_in2_sgen_mute_ch2",
		AFE_MPHONE_MULTI2_CON2, 17, 0x1, 0),
	SOC_SINGLE_EXT("adda_ul_use_sgen",
		AFE_ADDA_UL_SRC_CON1, 27, 0x1, 0,
		snd_soc_get_volsw,
		mt8188_afe_dbg_ctrl_ulsgen_en_put),
	SOC_SINGLE("adda_ul_sgen_freq_div_ch1",
		AFE_ADDA_UL_SRC_CON1, 4, 0x1f, 0),
	SOC_SINGLE("adda_ul_sgen_freq_div_ch2",
		AFE_ADDA_UL_SRC_CON1, 16, 0x1f, 0),
	SOC_SINGLE("adda_ul_sgen_amp_div_ch1",
		AFE_ADDA_UL_SRC_CON1, 9, 0x7, 0),
	SOC_SINGLE("adda_ul_sgen_amp_div_ch2",
		AFE_ADDA_UL_SRC_CON1, 21, 0x7, 0),
	SOC_SINGLE_EXT("adda6_ul_use_sgen",
		AFE_ADDA6_UL_SRC_CON1, 27, 0x1, 0,
		snd_soc_get_volsw,
		mt8188_afe_dbg_ctrl_ulsgen_en_put),
	SOC_SINGLE("adda6_ul_sgen_freq_div_ch1",
		AFE_ADDA6_UL_SRC_CON1, 4, 0x1f, 0),
	SOC_SINGLE("adda6_ul_sgen_freq_div_ch2",
		AFE_ADDA6_UL_SRC_CON1, 16, 0x1f, 0),
	SOC_SINGLE("adda6_ul_sgen_amp_div_ch1",
		AFE_ADDA6_UL_SRC_CON1, 9, 0x7, 0),
	SOC_SINGLE("adda6_ul_sgen_amp_div_ch2",
		AFE_ADDA6_UL_SRC_CON1, 21, 0x7, 0),
};

int mt8188_afe_add_debug_controls(struct snd_soc_component *component)
{
	return snd_soc_add_component_controls(component,
					      mt8188_afe_debug_controls,
					      ARRAY_SIZE(mt8188_afe_debug_controls));
}

#ifdef CONFIG_DEBUG_FS
struct mt8188_afe_dump_reg_attr {
	uint32_t offset;
	char *name;
};

struct mt8188_afe_debug_fs {
	char *fs_name;
	const struct file_operations *fops;
	const struct mt8188_afe_dump_reg_attr *attr;
	unsigned int attr_nums;
};

#define DUMP_REG_ENTRY(reg) {reg, #reg}

static const struct mt8188_afe_dump_reg_attr adda_regs[] = {
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SRC2_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_ADDA6_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_ADDA6_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_ADDA_DL_SDM_DCCOMP_CON),
	DUMP_REG_ENTRY(AFE_ADDA_UL_DL_CON0),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_CFG0),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_RX_CFG2),
	DUMP_REG_ENTRY(AFE_ADDA6_MTKAIF_CFG0),
	DUMP_REG_ENTRY(AFE_ADDA6_MTKAIF_RX_CFG2),
	DUMP_REG_ENTRY(AFE_ADDA_MTKAIF_SYNCWORD_CFG),
	DUMP_REG_ENTRY(AFE_AUD_PAD_TOP),
};

static const struct mt8188_afe_dump_reg_attr clk_regs[] = {
	DUMP_REG_ENTRY(AUDIO_TOP_CON0),
	DUMP_REG_ENTRY(AUDIO_TOP_CON1),
	DUMP_REG_ENTRY(AUDIO_TOP_CON2),
	DUMP_REG_ENTRY(AUDIO_TOP_CON3),
	DUMP_REG_ENTRY(AUDIO_TOP_CON4),
	DUMP_REG_ENTRY(AUDIO_TOP_CON5),
	DUMP_REG_ENTRY(AUDIO_TOP_CON6),
	DUMP_REG_ENTRY(ASYS_TOP_CON),
};

static const struct mt8188_afe_dump_reg_attr clk_tuner_regs[] = {
	DUMP_REG_ENTRY(AFE_APLL_TUNER_CFG),
	DUMP_REG_ENTRY(AFE_APLL_TUNER_CFG1),
	DUMP_REG_ENTRY(AFE_EARC_APLL_TUNER_CFG),
	DUMP_REG_ENTRY(AFE_SPDIFIN_APLL_TUNER_CFG),
	DUMP_REG_ENTRY(AFE_SPDIFIN_APLL_TUNER_CFG1),
	DUMP_REG_ENTRY(AFE_LINEIN_APLL_TUNER_CFG),
};

static const struct mt8188_afe_dump_reg_attr dmic_regs[] = {
	DUMP_REG_ENTRY(AFE_DMIC0_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC0_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC0_UL_SRC_MON0),
	DUMP_REG_ENTRY(AFE_DMIC0_UL_SRC_MON1),
	DUMP_REG_ENTRY(AFE_DMIC1_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC1_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC1_UL_SRC_MON0),
	DUMP_REG_ENTRY(AFE_DMIC1_UL_SRC_MON1),
	DUMP_REG_ENTRY(AFE_DMIC2_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC2_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC2_UL_SRC_MON0),
	DUMP_REG_ENTRY(AFE_DMIC2_UL_SRC_MON1),
	DUMP_REG_ENTRY(AFE_DMIC3_UL_SRC_CON0),
	DUMP_REG_ENTRY(AFE_DMIC3_UL_SRC_CON1),
	DUMP_REG_ENTRY(AFE_DMIC3_UL_SRC_MON0),
	DUMP_REG_ENTRY(AFE_DMIC3_UL_SRC_MON1),
	DUMP_REG_ENTRY(DMIC_BYPASS_HW_GAIN),
	DUMP_REG_ENTRY(DMIC_GAIN1_CON0),
	DUMP_REG_ENTRY(DMIC_GAIN1_CON1),
	DUMP_REG_ENTRY(DMIC_GAIN1_CON2),
	DUMP_REG_ENTRY(DMIC_GAIN1_CON3),
	DUMP_REG_ENTRY(DMIC_GAIN1_CUR),
	DUMP_REG_ENTRY(DMIC_GAIN2_CON0),
	DUMP_REG_ENTRY(DMIC_GAIN2_CON1),
	DUMP_REG_ENTRY(DMIC_GAIN2_CON2),
	DUMP_REG_ENTRY(DMIC_GAIN2_CON3),
	DUMP_REG_ENTRY(DMIC_GAIN2_CUR),
	DUMP_REG_ENTRY(DMIC_GAIN3_CON0),
	DUMP_REG_ENTRY(DMIC_GAIN3_CON1),
	DUMP_REG_ENTRY(DMIC_GAIN3_CON2),
	DUMP_REG_ENTRY(DMIC_GAIN3_CON3),
	DUMP_REG_ENTRY(DMIC_GAIN3_CUR),
	DUMP_REG_ENTRY(DMIC_GAIN4_CON0),
	DUMP_REG_ENTRY(DMIC_GAIN4_CON1),
	DUMP_REG_ENTRY(DMIC_GAIN4_CON2),
	DUMP_REG_ENTRY(DMIC_GAIN4_CON3),
	DUMP_REG_ENTRY(DMIC_GAIN4_CUR),
	DUMP_REG_ENTRY(PWR2_TOP_CON0),
	DUMP_REG_ENTRY(PWR2_TOP_CON1),
};

static const struct mt8188_afe_dump_reg_attr dptx_regs[] = {
	DUMP_REG_ENTRY(AFE_DPTX_CON),
	DUMP_REG_ENTRY(AFE_DPTX_MON),
};

static const struct mt8188_afe_dump_reg_attr etdm_regs[] = {
	DUMP_REG_ENTRY(ETDM_IN1_AFIFO_CON),
	DUMP_REG_ENTRY(ETDM_IN2_AFIFO_CON),
	DUMP_REG_ENTRY(ETDM_IN1_MONITOR),
	DUMP_REG_ENTRY(ETDM_IN2_MONITOR),
	DUMP_REG_ENTRY(ETDM_OUT1_MONITOR),
	DUMP_REG_ENTRY(ETDM_OUT2_MONITOR),
	DUMP_REG_ENTRY(ETDM_OUT3_MONITOR),
	DUMP_REG_ENTRY(ETDM_COWORK_CON0),
	DUMP_REG_ENTRY(ETDM_COWORK_CON1),
	DUMP_REG_ENTRY(ETDM_COWORK_CON2),
	DUMP_REG_ENTRY(ETDM_COWORK_CON3),
	DUMP_REG_ENTRY(ETDM_IN1_CON0),
	DUMP_REG_ENTRY(ETDM_IN1_CON1),
	DUMP_REG_ENTRY(ETDM_IN1_CON2),
	DUMP_REG_ENTRY(ETDM_IN1_CON3),
	DUMP_REG_ENTRY(ETDM_IN1_CON4),
	DUMP_REG_ENTRY(ETDM_IN1_CON5),
	DUMP_REG_ENTRY(ETDM_IN1_CON6),
	DUMP_REG_ENTRY(ETDM_IN1_CON7),
	DUMP_REG_ENTRY(ETDM_IN2_CON0),
	DUMP_REG_ENTRY(ETDM_IN2_CON1),
	DUMP_REG_ENTRY(ETDM_IN2_CON2),
	DUMP_REG_ENTRY(ETDM_IN2_CON3),
	DUMP_REG_ENTRY(ETDM_IN2_CON4),
	DUMP_REG_ENTRY(ETDM_IN2_CON5),
	DUMP_REG_ENTRY(ETDM_IN2_CON6),
	DUMP_REG_ENTRY(ETDM_IN2_CON7),
	DUMP_REG_ENTRY(ETDM_OUT1_CON0),
	DUMP_REG_ENTRY(ETDM_OUT1_CON1),
	DUMP_REG_ENTRY(ETDM_OUT1_CON2),
	DUMP_REG_ENTRY(ETDM_OUT1_CON3),
	DUMP_REG_ENTRY(ETDM_OUT1_CON4),
	DUMP_REG_ENTRY(ETDM_OUT1_CON5),
	DUMP_REG_ENTRY(ETDM_OUT1_CON6),
	DUMP_REG_ENTRY(ETDM_OUT1_CON7),
	DUMP_REG_ENTRY(ETDM_OUT1_CON8),
	DUMP_REG_ENTRY(ETDM_OUT2_CON0),
	DUMP_REG_ENTRY(ETDM_OUT2_CON1),
	DUMP_REG_ENTRY(ETDM_OUT2_CON2),
	DUMP_REG_ENTRY(ETDM_OUT2_CON3),
	DUMP_REG_ENTRY(ETDM_OUT2_CON4),
	DUMP_REG_ENTRY(ETDM_OUT2_CON5),
	DUMP_REG_ENTRY(ETDM_OUT2_CON6),
	DUMP_REG_ENTRY(ETDM_OUT2_CON7),
	DUMP_REG_ENTRY(ETDM_OUT2_CON8),
	DUMP_REG_ENTRY(ETDM_OUT3_CON0),
	DUMP_REG_ENTRY(ETDM_OUT3_CON1),
	DUMP_REG_ENTRY(ETDM_OUT3_CON2),
	DUMP_REG_ENTRY(ETDM_OUT3_CON3),
	DUMP_REG_ENTRY(ETDM_OUT3_CON4),
	DUMP_REG_ENTRY(ETDM_OUT3_CON5),
	DUMP_REG_ENTRY(ETDM_OUT3_CON6),
	DUMP_REG_ENTRY(ETDM_OUT3_CON7),
	DUMP_REG_ENTRY(ETDM_OUT3_CON8),
	DUMP_REG_ENTRY(AFE_TDMOUT_CONN0),
};

static const struct mt8188_afe_dump_reg_attr gasrc_0_7_regs[] = {
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC0_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC1_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC2_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC3_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC4_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC5_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC6_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC7_NEW_CON11),
};

static const struct mt8188_afe_dump_reg_attr gasrc_8_11_regs[] = {
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC8_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC9_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC10_NEW_CON11),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON0),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON1),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON2),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON3),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON4),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON5),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON6),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON7),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON8),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON9),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON10),
	DUMP_REG_ENTRY(AFE_GASRC11_NEW_CON11),
};

static const struct mt8188_afe_dump_reg_attr gasrc_timing_regs[] = {
	DUMP_REG_ENTRY(PWR1_ASM_CON1),
	DUMP_REG_ENTRY(PWR1_ASM_CON2),
	DUMP_REG_ENTRY(PWR1_ASM_CON3),
	DUMP_REG_ENTRY(GASRC_TIMING_CON0),
	DUMP_REG_ENTRY(GASRC_TIMING_CON1),
	DUMP_REG_ENTRY(GASRC_TIMING_CON2),
	DUMP_REG_ENTRY(GASRC_TIMING_CON3),
	DUMP_REG_ENTRY(GASRC_TIMING_CON4),
	DUMP_REG_ENTRY(GASRC_TIMING_CON5),
};

static const struct mt8188_afe_dump_reg_attr hw_gain_regs[] = {
	DUMP_REG_ENTRY(AFE_GAIN1_CUR),
	DUMP_REG_ENTRY(AFE_GAIN1_CON0),
	DUMP_REG_ENTRY(AFE_GAIN1_CON1),
	DUMP_REG_ENTRY(AFE_GAIN1_CON2),
	DUMP_REG_ENTRY(AFE_GAIN1_CON3),
	DUMP_REG_ENTRY(AFE_GAIN2_CUR),
	DUMP_REG_ENTRY(AFE_GAIN2_CON0),
	DUMP_REG_ENTRY(AFE_GAIN2_CON1),
	DUMP_REG_ENTRY(AFE_GAIN2_CON2),
	DUMP_REG_ENTRY(AFE_GAIN2_CON3),
};

static const struct mt8188_afe_dump_reg_attr irq_regs[] = {
	DUMP_REG_ENTRY(ASYS_IRQ1_CON),
	DUMP_REG_ENTRY(ASYS_IRQ2_CON),
	DUMP_REG_ENTRY(ASYS_IRQ3_CON),
	DUMP_REG_ENTRY(ASYS_IRQ4_CON),
	DUMP_REG_ENTRY(ASYS_IRQ5_CON),
	DUMP_REG_ENTRY(ASYS_IRQ6_CON),
	DUMP_REG_ENTRY(ASYS_IRQ7_CON),
	DUMP_REG_ENTRY(ASYS_IRQ8_CON),
	DUMP_REG_ENTRY(ASYS_IRQ9_CON),
	DUMP_REG_ENTRY(ASYS_IRQ10_CON),
	DUMP_REG_ENTRY(ASYS_IRQ11_CON),
	DUMP_REG_ENTRY(ASYS_IRQ12_CON),
	DUMP_REG_ENTRY(ASYS_IRQ13_CON),
	DUMP_REG_ENTRY(ASYS_IRQ14_CON),
	DUMP_REG_ENTRY(ASYS_IRQ15_CON),
	DUMP_REG_ENTRY(ASYS_IRQ16_CON),
	DUMP_REG_ENTRY(AFE_IRQ_MASK),
	DUMP_REG_ENTRY(AFE_IRQ_STATUS),
};

static const struct mt8188_afe_dump_reg_attr lpbk_cfg_regs[] = {
	DUMP_REG_ENTRY(AFE_LOOPBACK_CFG0),
	DUMP_REG_ENTRY(AFE_LOOPBACK_CFG1),
	DUMP_REG_ENTRY(AFE_LOOPBACK_CFG2),
};

static const struct mt8188_afe_dump_reg_attr memif_regs[] = {
	DUMP_REG_ENTRY(AFE_DAC_CON0),
	DUMP_REG_ENTRY(AFE_DAC_CON2),
	DUMP_REG_ENTRY(AFE_DL2_BASE),
	DUMP_REG_ENTRY(AFE_DL2_CUR),
	DUMP_REG_ENTRY(AFE_DL2_END),
	DUMP_REG_ENTRY(AFE_DL2_CON0),
	DUMP_REG_ENTRY(AFE_DL3_BASE),
	DUMP_REG_ENTRY(AFE_DL3_CUR),
	DUMP_REG_ENTRY(AFE_DL3_END),
	DUMP_REG_ENTRY(AFE_DL3_CON0),
	DUMP_REG_ENTRY(AFE_DL6_BASE),
	DUMP_REG_ENTRY(AFE_DL6_CUR),
	DUMP_REG_ENTRY(AFE_DL6_END),
	DUMP_REG_ENTRY(AFE_DL6_CON0),
	DUMP_REG_ENTRY(AFE_DL7_BASE),
	DUMP_REG_ENTRY(AFE_DL7_CUR),
	DUMP_REG_ENTRY(AFE_DL7_END),
	DUMP_REG_ENTRY(AFE_DL7_CON0),
	DUMP_REG_ENTRY(AFE_DL8_BASE),
	DUMP_REG_ENTRY(AFE_DL8_CUR),
	DUMP_REG_ENTRY(AFE_DL8_END),
	DUMP_REG_ENTRY(AFE_DL8_CON0),
	DUMP_REG_ENTRY(AFE_DL10_BASE),
	DUMP_REG_ENTRY(AFE_DL10_CUR),
	DUMP_REG_ENTRY(AFE_DL10_END),
	DUMP_REG_ENTRY(AFE_DL10_CON0),
	DUMP_REG_ENTRY(AFE_DL11_BASE),
	DUMP_REG_ENTRY(AFE_DL11_CUR),
	DUMP_REG_ENTRY(AFE_DL11_END),
	DUMP_REG_ENTRY(AFE_DL11_CON0),
	DUMP_REG_ENTRY(AFE_UL1_BASE),
	DUMP_REG_ENTRY(AFE_UL1_CUR),
	DUMP_REG_ENTRY(AFE_UL1_END),
	DUMP_REG_ENTRY(AFE_UL1_CON0),
	DUMP_REG_ENTRY(AFE_UL2_BASE),
	DUMP_REG_ENTRY(AFE_UL2_CUR),
	DUMP_REG_ENTRY(AFE_UL2_END),
	DUMP_REG_ENTRY(AFE_UL2_CON0),
	DUMP_REG_ENTRY(AFE_UL3_BASE),
	DUMP_REG_ENTRY(AFE_UL3_CUR),
	DUMP_REG_ENTRY(AFE_UL3_END),
	DUMP_REG_ENTRY(AFE_UL3_CON0),
	DUMP_REG_ENTRY(AFE_UL4_BASE),
	DUMP_REG_ENTRY(AFE_UL4_CUR),
	DUMP_REG_ENTRY(AFE_UL4_END),
	DUMP_REG_ENTRY(AFE_UL4_CON0),
	DUMP_REG_ENTRY(AFE_UL5_BASE),
	DUMP_REG_ENTRY(AFE_UL5_CUR),
	DUMP_REG_ENTRY(AFE_UL5_END),
	DUMP_REG_ENTRY(AFE_UL5_CON0),
	DUMP_REG_ENTRY(AFE_UL6_BASE),
	DUMP_REG_ENTRY(AFE_UL6_CUR),
	DUMP_REG_ENTRY(AFE_UL6_END),
	DUMP_REG_ENTRY(AFE_UL6_CON0),
	DUMP_REG_ENTRY(AFE_UL8_BASE),
	DUMP_REG_ENTRY(AFE_UL8_CUR),
	DUMP_REG_ENTRY(AFE_UL8_END),
	DUMP_REG_ENTRY(AFE_UL8_CON0),
	DUMP_REG_ENTRY(AFE_UL9_BASE),
	DUMP_REG_ENTRY(AFE_UL9_CUR),
	DUMP_REG_ENTRY(AFE_UL9_END),
	DUMP_REG_ENTRY(AFE_UL9_CON0),
	DUMP_REG_ENTRY(AFE_UL10_BASE),
	DUMP_REG_ENTRY(AFE_UL10_CUR),
	DUMP_REG_ENTRY(AFE_UL10_END),
	DUMP_REG_ENTRY(AFE_UL10_CON0),
	DUMP_REG_ENTRY(AFE_BUS_MON1),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON0),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON1),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON2),
	DUMP_REG_ENTRY(AFE_MEMIF_AGENT_FS_CON3),
	DUMP_REG_ENTRY(AFE_CM0_CON),
	DUMP_REG_ENTRY(AFE_CM1_CON),
	DUMP_REG_ENTRY(AFE_CM2_CON),
	DUMP_REG_ENTRY(AFE_CM0_MON),
	DUMP_REG_ENTRY(AFE_CM1_MON),
	DUMP_REG_ENTRY(AFE_CM2_MON),
};

static const struct mt8188_afe_dump_reg_attr mphone_multi_regs[] = {
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_CON0),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_CON1),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI_CON2),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI2_CON0),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI2_CON1),
	DUMP_REG_ENTRY(AFE_MPHONE_MULTI2_CON2),
};

static const struct mt8188_afe_dump_reg_attr pcmif_regs[] = {
	DUMP_REG_ENTRY(PCM_INTF_CON1),
	DUMP_REG_ENTRY(PCM_INTF_CON2),
};

static const struct mt8188_afe_dump_reg_attr sgen_regs[] = {
	DUMP_REG_ENTRY(AFE_SINEGEN_CON0),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON1),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON2),
	DUMP_REG_ENTRY(AFE_SINEGEN_CON3),
};

static ssize_t mt8188_afe_dump_registers(char __user *user_buf,
	size_t count, loff_t *pos,
	struct mtk_base_afe *afe,
	const struct mt8188_afe_dump_reg_attr *regs,
	size_t regs_len)
{
	ssize_t ret, i;
	char *buf;
	unsigned int reg_value = 0;
	int n = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < regs_len; i++) {
		if (regmap_read(afe->regmap, regs[i].offset, &reg_value))
			n += scnprintf(buf + n, count - n, "%s[0x%x] = N/A\n",
				       regs[i].name, regs[i].offset);
		else
			n += scnprintf(buf + n, count - n, "%s[0x%x] = 0x%x\n",
				       regs[i].name, regs[i].offset, reg_value);
	}

	ret = simple_read_from_buffer(user_buf, count, pos, buf, n);
	kfree(buf);

	return ret;
}

static const struct mt8188_afe_dump_reg_attr *
	mt8188_afe_debug_found_regs(const char *str, int *num_regs);

static ssize_t mt8188_afe_debug_read_file(struct file *file,
	char __user *user_buf, size_t count, loff_t *pos)
{
	struct snd_soc_component *component = file->private_data;
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	const struct mt8188_afe_dump_reg_attr *regs = NULL;
	unsigned int num_regs;

	if (*pos < 0 || !count)
		return -EINVAL;

	regs = mt8188_afe_debug_found_regs(file->f_path.dentry->d_iname,
					   &num_regs);

	if (!regs || !num_regs)
		return -EINVAL;

	return mt8188_afe_dump_registers(user_buf, count, pos, afe,
					 regs, num_regs);
}

static ssize_t mt8188_afe_debug_write_file(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[64];
	size_t buf_size;
	char *start = buf;
	char *reg_str;
	char *value_str;
	static const char delim[] = " ,";
	unsigned long reg, value;
	struct snd_soc_component *component = file->private_data;
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = 0;

	reg_str = strsep(&start, delim);
	if (!reg_str || !strlen(reg_str))
		return -EINVAL;

	value_str = strsep(&start, delim);
	if (!value_str || !strlen(value_str))
		return -EINVAL;

	if (kstrtoul(reg_str, 16, &reg))
		return -EINVAL;

	if (kstrtoul(value_str, 16, &value))
		return -EINVAL;

	regmap_write(afe->regmap, reg, value);

	return buf_size;
}

static const struct file_operations mt8188_afe_debug_fops = {
	.open = simple_open,
	.read = mt8188_afe_debug_read_file,
	.write = mt8188_afe_debug_write_file,
	.llseek = default_llseek,
};

static const struct mt8188_afe_debug_fs
	afe_debug_fs[MT8188_AFE_DEBUG_FS_NUM] = {
	[MT8188_AFE_DEBUG_FS_ADDA] = {
		.fs_name = "mt8188_afe_adda_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = adda_regs,
		.attr_nums = ARRAY_SIZE(adda_regs),
	},
	[MT8188_AFE_DEBUG_FS_CLK] = {
		.fs_name = "mt8188_afe_clk_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = clk_regs,
		.attr_nums = ARRAY_SIZE(clk_regs),
	},
	[MT8188_AFE_DEBUG_FS_CLK_TUNER] = {
		.fs_name = "mt8188_afe_clk_tuner_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = clk_tuner_regs,
		.attr_nums = ARRAY_SIZE(clk_tuner_regs),
	},
	[MT8188_AFE_DEBUG_FS_DMIC] = {
		.fs_name = "mt8188_afe_dmic_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = dmic_regs,
		.attr_nums = ARRAY_SIZE(dmic_regs),
	},
	[MT8188_AFE_DEBUG_FS_DPTX] = {
		.fs_name = "mt8188_afe_dptx_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = dptx_regs,
		.attr_nums = ARRAY_SIZE(dptx_regs),
	},
	[MT8188_AFE_DEBUG_FS_ETDM] = {
		.fs_name = "mt8188_afe_etdm_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = etdm_regs,
		.attr_nums = ARRAY_SIZE(etdm_regs),
	},
	[MT8188_AFE_DEBUG_FS_GASRC_0_7] = {
		.fs_name = "mt8188_afe_gasrc_0_7_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = gasrc_0_7_regs,
		.attr_nums = ARRAY_SIZE(gasrc_0_7_regs),
	},
	[MT8188_AFE_DEBUG_FS_GASRC_8_11] = {
		.fs_name = "mt8188_afe_gasrc_8_11_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = gasrc_8_11_regs,
		.attr_nums = ARRAY_SIZE(gasrc_8_11_regs),
	},
	[MT8188_AFE_DEBUG_FS_GASRC_TIMING] = {
		.fs_name = "mt8188_afe_gasrc_timing_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = gasrc_timing_regs,
		.attr_nums = ARRAY_SIZE(gasrc_timing_regs),
	},
	[MT8188_AFE_DEBUG_FS_HW_GAIN] = {
		.fs_name = "mt8188_afe_hw_gain_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = hw_gain_regs,
		.attr_nums = ARRAY_SIZE(hw_gain_regs),
	},
	[MT8188_AFE_DEBUG_FS_IRQ] = {
		.fs_name = "mt8188_afe_irq_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = irq_regs,
		.attr_nums = ARRAY_SIZE(irq_regs),
	},
	[MT8188_AFE_DEBUG_FS_LPBK_CFG] = {
		.fs_name = "mt8188_afe_lpbk_cfg_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = lpbk_cfg_regs,
		.attr_nums = ARRAY_SIZE(lpbk_cfg_regs),
	},
	[MT8188_AFE_DEBUG_FS_MEMIF] = {
		.fs_name = "mt8188_afe_memif_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = memif_regs,
		.attr_nums = ARRAY_SIZE(memif_regs),
	},
	[MT8188_AFE_DEBUG_FS_MPHONE_MULTI] = {
		.fs_name = "mt8188_afe_mphone_multi_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = mphone_multi_regs,
		.attr_nums = ARRAY_SIZE(mphone_multi_regs),
	},
	[MT8188_AFE_DEBUG_FS_PCMIF] = {
		.fs_name = "mt8188_afe_pcmif_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = pcmif_regs,
		.attr_nums = ARRAY_SIZE(pcmif_regs),
	},
	[MT8188_AFE_DEBUG_FS_SGEN] = {
		.fs_name = "mt8188_afe_sgen_regs",
		.fops = &mt8188_afe_debug_fops,
		.attr = sgen_regs,
		.attr_nums = ARRAY_SIZE(sgen_regs),
	},
};

static const struct mt8188_afe_dump_reg_attr *
	mt8188_afe_debug_found_regs(const char *str, int *num_regs)
{
	const struct mt8188_afe_dump_reg_attr *attr = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(afe_debug_fs); i++) {
		if (!strncmp(str, afe_debug_fs[i].fs_name, DNAME_INLINE_LEN)) {
			attr = afe_debug_fs[i].attr;
			*num_regs = afe_debug_fs[i].attr_nums;
			break;
		}
	}

	return attr;
}
#endif

int mt8188_afe_debugfs_init(struct snd_soc_component *component)
{
#ifdef CONFIG_DEBUG_FS
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int i;

	afe_priv->debugfs_dentry =
		devm_kcalloc(afe->dev, MT8188_AFE_DEBUG_FS_NUM,
			sizeof(*afe_priv->debugfs_dentry), GFP_KERNEL);
	if (!afe_priv->debugfs_dentry)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(afe_debug_fs); i++) {
		afe_priv->debugfs_dentry[i] =
			debugfs_create_file(afe_debug_fs[i].fs_name,
				0644, NULL, component, afe_debug_fs[i].fops);
		if (!afe_priv->debugfs_dentry[i])
			dev_info(afe->dev, "%s create %s debugfs failed\n",
				__func__, afe_debug_fs[i].fs_name);
	}
#endif
	return 0;
}

void mt8188_afe_debugfs_exit(struct snd_soc_component *component)
{
#ifdef CONFIG_DEBUG_FS
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int i;

	for (i = 0; i < ARRAY_SIZE(afe_debug_fs); i++)
		debugfs_remove(afe_priv->debugfs_dentry[i]);
#endif
}
