/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt8188-afe-clk.h  --  Mediatek 8188 afe clock ctrl definition
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#ifndef _MT8188_AFE_CLK_H_
#define _MT8188_AFE_CLK_H_

enum {
	/* xtal */
	MT8188_CLK_XTAL_26M,
	/* pll */
	MT8188_CLK_APMIXED_APLL1,
	MT8188_CLK_APMIXED_APLL2,
	MT8188_CLK_APMIXED_APLL3,
	MT8188_CLK_APMIXED_APLL4,
	MT8188_CLK_APMIXED_APLL5,
	/* divider */
	MT8188_CLK_TOP_APLL1,
	MT8188_CLK_TOP_APLL1_D4,
	MT8188_CLK_TOP_APLL2,
	MT8188_CLK_TOP_APLL2_D4,
	MT8188_CLK_TOP_APLL3,
	MT8188_CLK_TOP_APLL3_D4,
	MT8188_CLK_TOP_APLL4,
	MT8188_CLK_TOP_APLL4_D4,
	MT8188_CLK_TOP_APLL5,
	MT8188_CLK_TOP_APLL5_D4,
	MT8188_CLK_TOP_APLL12_DIV0,
	MT8188_CLK_TOP_APLL12_DIV1,
	MT8188_CLK_TOP_APLL12_DIV2,
	MT8188_CLK_TOP_APLL12_DIV3,
	MT8188_CLK_TOP_APLL12_DIV4,
	MT8188_CLK_TOP_APLL12_DIV9,
	MT8188_CLK_TOP_MAINPLL_D4,
	MT8188_CLK_TOP_MAINPLL_D7,
	MT8188_CLK_TOP_MAINPLL_D7_D2,
	/* mux */
	MT8188_CLK_TOP_APLL1_SEL,
	MT8188_CLK_TOP_APLL2_SEL,
	MT8188_CLK_TOP_APLL3_SEL,
	MT8188_CLK_TOP_APLL4_SEL,
	MT8188_CLK_TOP_APLL5_SEL,
	MT8188_CLK_TOP_A1SYS_HP_SEL,
	MT8188_CLK_TOP_A2SYS_SEL,
	MT8188_CLK_TOP_A3SYS_SEL,
	MT8188_CLK_TOP_A4SYS_SEL,
	MT8188_CLK_TOP_ASM_H_SEL,
	MT8188_CLK_TOP_ASM_L_SEL,
	MT8188_CLK_TOP_AUD_IEC_SEL,
	MT8188_CLK_TOP_AUD_INTBUS_SEL,
	MT8188_CLK_TOP_AUDIO_H_SEL,
	MT8188_CLK_TOP_AUDIO_LOCAL_BUS_SEL,
	MT8188_CLK_TOP_DPTX_M_SEL,
	MT8188_CLK_TOP_INTDIR_SEL,
	MT8188_CLK_TOP_I2SO1_M_SEL,
	MT8188_CLK_TOP_I2SO2_M_SEL,
	MT8188_CLK_TOP_I2SI1_M_SEL,
	MT8188_CLK_TOP_I2SI2_M_SEL,
	/* clock gate */
	MT8188_CLK_TOP_MPHONE_SLAVE_B,
	// TODO: MT8188 audio 26m source moved to adsp, not from infra.
	MT8188_CLK_INFRA_AO_AUDIO_26M_B,
	MT8188_CLK_AUD_AFE,
	MT8188_CLK_AUD_LRCK_CNT,
	MT8188_CLK_AUD_SPDIFIN_TUNER_APLL,
	MT8188_CLK_AUD_SPDIFIN_TUNER_DBG,
	MT8188_CLK_AUD_UL_TML,
	MT8188_CLK_AUD_APLL1_TUNER,
	MT8188_CLK_AUD_APLL2_TUNER,
	MT8188_CLK_AUD_TOP0_SPDF,
	MT8188_CLK_AUD_APLL,
	MT8188_CLK_AUD_APLL2,
	MT8188_CLK_AUD_DAC,
	MT8188_CLK_AUD_DAC_PREDIS,
	MT8188_CLK_AUD_TML,
	MT8188_CLK_AUD_ADC,
	MT8188_CLK_AUD_DAC_HIRES,
	MT8188_CLK_AUD_A1SYS_HP,
	MT8188_CLK_AUD_AFE_DMIC1,
	MT8188_CLK_AUD_AFE_DMIC2,
	MT8188_CLK_AUD_AFE_DMIC3,
	MT8188_CLK_AUD_AFE_DMIC4,
	MT8188_CLK_AUD_AFE_26M_DMIC_TM,
	MT8188_CLK_AUD_UL_TML_HIRES,
	MT8188_CLK_AUD_ADC_HIRES,
	MT8188_CLK_AUD_LINEIN_TUNER,
	MT8188_CLK_AUD_EARC_TUNER,
	MT8188_CLK_AUD_I2SIN,
	MT8188_CLK_AUD_TDM_IN,
	MT8188_CLK_AUD_I2S_OUT,
	MT8188_CLK_AUD_TDM_OUT,
	MT8188_CLK_AUD_HDMI_OUT,
	MT8188_CLK_AUD_ASRC11,
	MT8188_CLK_AUD_ASRC12,
	MT8188_CLK_AUD_MULTI_IN,
	MT8188_CLK_AUD_INTDIR,
	MT8188_CLK_AUD_A1SYS,
	MT8188_CLK_AUD_A2SYS,
	MT8188_CLK_AUD_PCMIF,
	MT8188_CLK_AUD_A3SYS,
	MT8188_CLK_AUD_A4SYS,
	MT8188_CLK_AUD_MEMIF_UL1,
	MT8188_CLK_AUD_MEMIF_UL2,
	MT8188_CLK_AUD_MEMIF_UL3,
	MT8188_CLK_AUD_MEMIF_UL4,
	MT8188_CLK_AUD_MEMIF_UL5,
	MT8188_CLK_AUD_MEMIF_UL6,
	MT8188_CLK_AUD_MEMIF_UL8,
	MT8188_CLK_AUD_MEMIF_UL9,
	MT8188_CLK_AUD_MEMIF_UL10,
	MT8188_CLK_AUD_MEMIF_DL2,
	MT8188_CLK_AUD_MEMIF_DL3,
	MT8188_CLK_AUD_MEMIF_DL6,
	MT8188_CLK_AUD_MEMIF_DL7,
	MT8188_CLK_AUD_MEMIF_DL8,
	MT8188_CLK_AUD_MEMIF_DL10,
	MT8188_CLK_AUD_MEMIF_DL11,
	MT8188_CLK_AUD_GASRC0,
	MT8188_CLK_AUD_GASRC1,
	MT8188_CLK_AUD_GASRC2,
	MT8188_CLK_AUD_GASRC3,
	MT8188_CLK_AUD_GASRC4,
	MT8188_CLK_AUD_GASRC5,
	MT8188_CLK_AUD_GASRC6,
	MT8188_CLK_AUD_GASRC7,
	MT8188_CLK_AUD_GASRC8,
	MT8188_CLK_AUD_GASRC9,
	MT8188_CLK_AUD_GASRC10,
	MT8188_CLK_AUD_GASRC11,
	MT8188_CLK_NUM,
};

enum {
	MT8188_AUD_PLL1,
	MT8188_AUD_PLL2,
	MT8188_AUD_PLL3,
	MT8188_AUD_PLL4,
	MT8188_AUD_PLL5,
	MT8188_AUD_PLL_NUM,
};

enum {
	MT8188_MCK_SEL_26M,
	MT8188_MCK_SEL_APLL1,
	MT8188_MCK_SEL_APLL2,
	MT8188_MCK_SEL_APLL3,
	MT8188_MCK_SEL_APLL4,
	MT8188_MCK_SEL_APLL5,
	MT8188_MCK_SEL_NUM,
};

struct mtk_base_afe;
struct snd_soc_component;

int mt8188_afe_get_mclk_source_clk_id(int sel);
int mt8188_afe_get_mclk_source_rate(struct mtk_base_afe *afe, int apll);
int mt8188_afe_get_default_mclk_source_by_rate(int rate);
int mt8188_afe_init_clock(struct mtk_base_afe *afe);
void mt8188_afe_deinit_clock(struct mtk_base_afe *afe);
int mt8188_afe_enable_clk(struct mtk_base_afe *afe, struct clk *clk);
void mt8188_afe_disable_clk(struct mtk_base_afe *afe, struct clk *clk);
int mt8188_afe_prepare_clk(struct mtk_base_afe *afe, struct clk *clk);
void mt8188_afe_unprepare_clk(struct mtk_base_afe *afe, struct clk *clk);
int mt8188_afe_clk_enable(struct mtk_base_afe *afe, struct clk *clk);
void mt8188_afe_clk_disable(struct mtk_base_afe *afe, struct clk *clk);
int mt8188_afe_set_clk_rate(struct mtk_base_afe *afe, struct clk *clk,
			    unsigned int rate);
int mt8188_afe_get_clk_parent(struct mtk_base_afe *afe, struct clk *clk,
			      struct clk **parent);
int mt8188_afe_set_clk_parent(struct mtk_base_afe *afe, struct clk *clk,
			      struct clk *parent);
int mt8188_afe_enable_main_clock(struct mtk_base_afe *afe);
int mt8188_afe_disable_main_clock(struct mtk_base_afe *afe);
int mt8188_afe_enable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type);
int mt8188_afe_disable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type);
int mt8188_afe_enable_reg_rw_clk(struct mtk_base_afe *afe);
int mt8188_afe_disable_reg_rw_clk(struct mtk_base_afe *afe);
int mt8188_afe_enable_afe_on(struct mtk_base_afe *afe);
int mt8188_afe_disable_afe_on(struct mtk_base_afe *afe);
int mt8188_afe_init_apll_tuner(unsigned int id);
int mt8188_afe_enable_apll_tuner(struct mtk_base_afe *afe, unsigned int id);
int mt8188_afe_disable_apll_tuner(struct mtk_base_afe *afe, unsigned int id);
bool mt8188_afe_is_apll_tuner_enabled(struct mtk_base_afe *afe, unsigned int id);
int mt8188_afe_dram_request(struct device *dev);
int mt8188_afe_dram_release(struct device *dev);
int mt8188_afe_add_clk_controls(struct snd_soc_component *component);
#endif
