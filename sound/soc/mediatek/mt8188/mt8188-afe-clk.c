// SPDX-License-Identifier: GPL-2.0
/*
 * mt8188-afe-clk.c  --  Mediatek 8188 afe clock ctrl
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "mt8188-afe-common.h"
#include "mt8188-afe-clk.h"
#include "mt8188-reg.h"
#include "mt8188-audsys-clk.h"

static const char *aud_clks[MT8188_CLK_NUM] = {
	/* xtal */
	[MT8188_CLK_XTAL_26M] = "clk26m",
	/* pll */
	[MT8188_CLK_APMIXED_APLL1] = "apll1",
	[MT8188_CLK_APMIXED_APLL2] = "apll2",
	[MT8188_CLK_APMIXED_APLL3] = "apll3",
	[MT8188_CLK_APMIXED_APLL4] = "apll4",
	[MT8188_CLK_APMIXED_APLL5] = "apll5",
	/* divider */
	[MT8188_CLK_TOP_APLL1] = "apll1_ck",
	[MT8188_CLK_TOP_APLL1_D4] = "apll1_d4",
	[MT8188_CLK_TOP_APLL2] = "apll2_ck",
	[MT8188_CLK_TOP_APLL2_D4] = "apll2_d4",
	[MT8188_CLK_TOP_APLL3] = "apll3_ck",
	[MT8188_CLK_TOP_APLL3_D4] = "apll3_d4",
	[MT8188_CLK_TOP_APLL4] = "apll4_ck",
	[MT8188_CLK_TOP_APLL4_D4] = "apll4_d4",
	[MT8188_CLK_TOP_APLL5] = "apll5_ck",
	[MT8188_CLK_TOP_APLL5_D4] = "apll5_d4",
	[MT8188_CLK_TOP_APLL12_DIV0] = "apll12_div0",
	[MT8188_CLK_TOP_APLL12_DIV1] = "apll12_div1",
	[MT8188_CLK_TOP_APLL12_DIV2] = "apll12_div2",
	[MT8188_CLK_TOP_APLL12_DIV3] = "apll12_div3",
	[MT8188_CLK_TOP_APLL12_DIV4] = "apll12_div4",
	[MT8188_CLK_TOP_APLL12_DIV9] = "apll12_div9",
	[MT8188_CLK_TOP_MAINPLL_D4] = "mainpll_d4",
	[MT8188_CLK_TOP_MAINPLL_D7] = "mainpll_d7",
	[MT8188_CLK_TOP_MAINPLL_D7_D2] = "mainpll_d7_d2",
	/* mux */
	[MT8188_CLK_TOP_APLL1_SEL] = "apll1_sel",
	[MT8188_CLK_TOP_APLL2_SEL] = "apll2_sel",
	[MT8188_CLK_TOP_APLL3_SEL] = "apll3_sel",
	[MT8188_CLK_TOP_APLL4_SEL] = "apll4_sel",
	[MT8188_CLK_TOP_APLL5_SEL] = "apll5_sel",
	[MT8188_CLK_TOP_A1SYS_HP_SEL] = "a1sys_hp_sel",
	[MT8188_CLK_TOP_A2SYS_SEL] = "a2sys_sel",
	[MT8188_CLK_TOP_A3SYS_SEL] = "a3sys_sel",
	[MT8188_CLK_TOP_A4SYS_SEL] = "a4sys_sel",
	[MT8188_CLK_TOP_ASM_H_SEL] = "asm_h_sel",
	[MT8188_CLK_TOP_ASM_L_SEL] = "asm_l_sel",
	[MT8188_CLK_TOP_AUD_IEC_SEL] = "aud_iec_sel",
	[MT8188_CLK_TOP_AUD_INTBUS_SEL] = "aud_intbus_sel",
	[MT8188_CLK_TOP_AUDIO_H_SEL] = "audio_h_sel",
	[MT8188_CLK_TOP_AUDIO_LOCAL_BUS_SEL] = "audio_local_bus_sel",
	[MT8188_CLK_TOP_DPTX_M_SEL] = "dptx_m_sel",
	[MT8188_CLK_TOP_INTDIR_SEL] = "intdir_sel",
	[MT8188_CLK_TOP_I2SO1_M_SEL] = "i2so1_m_sel",
	[MT8188_CLK_TOP_I2SO2_M_SEL] = "i2so2_m_sel",
	[MT8188_CLK_TOP_I2SI1_M_SEL] = "i2si1_m_sel",
	[MT8188_CLK_TOP_I2SI2_M_SEL] = "i2si2_m_sel",
	/* clock gate */
	[MT8188_CLK_TOP_MPHONE_SLAVE_B] = "mphone_slave_b",
	[MT8188_CLK_INFRA_AO_AUDIO_26M_B] = "infra_ao_audio_26m_b",
	[MT8188_CLK_AUD_AFE] = "aud_afe",
	[MT8188_CLK_AUD_LRCK_CNT] = "aud_lrck_cnt",
	[MT8188_CLK_AUD_SPDIFIN_TUNER_APLL] = "aud_spdifin_tuner_apll",
	[MT8188_CLK_AUD_SPDIFIN_TUNER_DBG] = "aud_spdifin_tuner_dbg",
	[MT8188_CLK_AUD_UL_TML] = "aud_ul_tml",
	[MT8188_CLK_AUD_APLL1_TUNER] = "aud_apll1_tuner",
	[MT8188_CLK_AUD_APLL2_TUNER] = "aud_apll2_tuner",
	[MT8188_CLK_AUD_TOP0_SPDF] = "aud_top0_spdf",
	[MT8188_CLK_AUD_APLL] = "aud_apll",
	[MT8188_CLK_AUD_APLL2] = "aud_apll2",
	[MT8188_CLK_AUD_DAC] = "aud_dac",
	[MT8188_CLK_AUD_DAC_PREDIS] = "aud_dac_predis",
	[MT8188_CLK_AUD_TML] = "aud_tml",
	[MT8188_CLK_AUD_ADC] = "aud_adc",
	[MT8188_CLK_AUD_DAC_HIRES] = "aud_dac_hires",
	[MT8188_CLK_AUD_A1SYS_HP] = "aud_a1sys_hp",
	[MT8188_CLK_AUD_AFE_DMIC1] = "aud_afe_dmic1",
	[MT8188_CLK_AUD_AFE_DMIC2] = "aud_afe_dmic2",
	[MT8188_CLK_AUD_AFE_DMIC3] = "aud_afe_dmic3",
	[MT8188_CLK_AUD_AFE_DMIC4] = "aud_afe_dmic4",
	[MT8188_CLK_AUD_AFE_26M_DMIC_TM] = "aud_afe_26m_dmic_tm",
	[MT8188_CLK_AUD_UL_TML_HIRES] = "aud_ul_tml_hires",
	[MT8188_CLK_AUD_ADC_HIRES] = "aud_adc_hires",
	[MT8188_CLK_AUD_LINEIN_TUNER] = "aud_linein_tuner",
	[MT8188_CLK_AUD_EARC_TUNER] = "aud_earc_tuner",
	[MT8188_CLK_AUD_I2SIN] = "aud_i2sin",
	[MT8188_CLK_AUD_TDM_IN] = "aud_tdm_in",
	[MT8188_CLK_AUD_I2S_OUT] = "aud_i2s_out",
	[MT8188_CLK_AUD_TDM_OUT] = "aud_tdm_out",
	[MT8188_CLK_AUD_HDMI_OUT] = "aud_hdmi_out",
	[MT8188_CLK_AUD_ASRC11] = "aud_asrc11",
	[MT8188_CLK_AUD_ASRC12] = "aud_asrc12",
	[MT8188_CLK_AUD_MULTI_IN] = "aud_multi_in",
	[MT8188_CLK_AUD_INTDIR] = "aud_intdir",
	[MT8188_CLK_AUD_A1SYS] = "aud_a1sys",
	[MT8188_CLK_AUD_A2SYS] = "aud_a2sys",
	[MT8188_CLK_AUD_PCMIF] = "aud_pcmif",
	[MT8188_CLK_AUD_A3SYS] = "aud_a3sys",
	[MT8188_CLK_AUD_A4SYS] = "aud_a4sys",
	[MT8188_CLK_AUD_MEMIF_UL1] = "aud_memif_ul1",
	[MT8188_CLK_AUD_MEMIF_UL2] = "aud_memif_ul2",
	[MT8188_CLK_AUD_MEMIF_UL3] = "aud_memif_ul3",
	[MT8188_CLK_AUD_MEMIF_UL4] = "aud_memif_ul4",
	[MT8188_CLK_AUD_MEMIF_UL5] = "aud_memif_ul5",
	[MT8188_CLK_AUD_MEMIF_UL6] = "aud_memif_ul6",
	[MT8188_CLK_AUD_MEMIF_UL8] = "aud_memif_ul8",
	[MT8188_CLK_AUD_MEMIF_UL9] = "aud_memif_ul9",
	[MT8188_CLK_AUD_MEMIF_UL10] = "aud_memif_ul10",
	[MT8188_CLK_AUD_MEMIF_DL2] = "aud_memif_dl2",
	[MT8188_CLK_AUD_MEMIF_DL3] = "aud_memif_dl3",
	[MT8188_CLK_AUD_MEMIF_DL6] = "aud_memif_dl6",
	[MT8188_CLK_AUD_MEMIF_DL7] = "aud_memif_dl7",
	[MT8188_CLK_AUD_MEMIF_DL8] = "aud_memif_dl8",
	[MT8188_CLK_AUD_MEMIF_DL10] = "aud_memif_dl10",
	[MT8188_CLK_AUD_MEMIF_DL11] = "aud_memif_dl11",
	[MT8188_CLK_AUD_GASRC0] = "aud_gasrc0",
	[MT8188_CLK_AUD_GASRC1] = "aud_gasrc1",
	[MT8188_CLK_AUD_GASRC2] = "aud_gasrc2",
	[MT8188_CLK_AUD_GASRC3] = "aud_gasrc3",
	[MT8188_CLK_AUD_GASRC4] = "aud_gasrc4",
	[MT8188_CLK_AUD_GASRC5] = "aud_gasrc5",
	[MT8188_CLK_AUD_GASRC6] = "aud_gasrc6",
	[MT8188_CLK_AUD_GASRC7] = "aud_gasrc7",
	[MT8188_CLK_AUD_GASRC8] = "aud_gasrc8",
	[MT8188_CLK_AUD_GASRC9] = "aud_gasrc9",
	[MT8188_CLK_AUD_GASRC10] = "aud_gasrc10",
	[MT8188_CLK_AUD_GASRC11] = "aud_gasrc11",
};

static DEFINE_MUTEX(mutex_request_dram);

int mt8188_afe_get_mclk_source_clk_id(int sel)
{
	switch (sel) {
	case MT8188_MCK_SEL_26M:
		return MT8188_CLK_XTAL_26M;
	case MT8188_MCK_SEL_APLL1:
		return MT8188_CLK_TOP_APLL1;
	case MT8188_MCK_SEL_APLL2:
		return MT8188_CLK_TOP_APLL2;
	case MT8188_MCK_SEL_APLL3:
		return MT8188_CLK_TOP_APLL3;
	case MT8188_MCK_SEL_APLL4:
		return MT8188_CLK_TOP_APLL4;
	case MT8188_MCK_SEL_APLL5:
		return MT8188_CLK_TOP_APLL5;
	default:
		return -EINVAL;
	}
}

int mt8188_afe_get_mclk_source_rate(struct mtk_base_afe *afe, int apll)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int clk_id = mt8188_afe_get_mclk_source_clk_id(apll);

	if (clk_id < 0) {
		dev_dbg(afe->dev, "invalid clk id\n");
		return 0;
	}

	return clk_get_rate(afe_priv->clk[clk_id]);
}

int mt8188_afe_get_default_mclk_source_by_rate(int rate)
{
	return ((rate % 8000) == 0) ?
		MT8188_MCK_SEL_APLL1 : MT8188_MCK_SEL_APLL2;
}

int mt8188_afe_init_clock(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct arm_smccc_res smccc_res;
#endif
	int i;
	int ret = 0;

	ret = mt8188_audsys_clk_register(afe);
	if (ret) {
		dev_info(afe->dev, "register audsys clk fail %d\n", ret);
		return ret;
	}

	afe_priv->clk =
		devm_kcalloc(afe->dev, MT8188_CLK_NUM, sizeof(*afe_priv->clk),
			GFP_KERNEL);
	if (!afe_priv->clk)
		return -ENOMEM;

	for (i = 0; i < MT8188_CLK_NUM; i++) {
		afe_priv->clk[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clk[i])) {
			dev_dbg(afe->dev, "%s(), devm_clk_get %s fail, ret %ld\n",
				__func__, aud_clks[i],
				PTR_ERR(afe_priv->clk[i]));
			return PTR_ERR(afe_priv->clk[i]);
		}
	}

#ifndef CONFIG_FPGA_EARLY_PORTING
	/* init arm_smccc_smc call */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL, MTK_AUDIO_SMC_OP_INIT,
		      0, 0, 0, 0, 0, 0, &smccc_res);
#endif

	/* initial tuner */
	for (i = 0; i < MT8188_AUD_PLL_NUM; i++) {
		ret = mt8188_afe_init_apll_tuner(i);
		if (ret) {
			dev_info(afe->dev, "%s(), init apll_tuner%d failed",
				 __func__, (i + 1));
			return -EINVAL;
		}
	}

	return 0;
}

void mt8188_afe_deinit_clock(struct mtk_base_afe *afe)
{
	mt8188_audsys_clk_unregister(afe);
}

int mt8188_afe_enable_clk(struct mtk_base_afe *afe, struct clk *clk)
{
	int ret;

	if (clk) {
		ret = clk_prepare_enable(clk);
		if (ret) {
			dev_dbg(afe->dev, "%s(), failed to enable clk\n",
				__func__);
			return ret;
		}
	} else {
		dev_dbg(afe->dev, "NULL clk\n");
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mt8188_afe_enable_clk);

void mt8188_afe_disable_clk(struct mtk_base_afe *afe, struct clk *clk)
{
	if (clk)
		clk_disable_unprepare(clk);
	else
		dev_dbg(afe->dev, "NULL clk\n");
}
EXPORT_SYMBOL_GPL(mt8188_afe_disable_clk);

int mt8188_afe_prepare_clk(struct mtk_base_afe *afe, struct clk *clk)
{
	int ret;

	if (clk) {
		ret = clk_prepare(clk);
		if (ret) {
			dev_dbg(afe->dev, "%s(), failed to prepare clk\n",
				__func__);
			return ret;
		}
	} else {
		dev_dbg(afe->dev, "NULL clk\n");
	}
	return 0;
}

void mt8188_afe_unprepare_clk(struct mtk_base_afe *afe, struct clk *clk)
{
	if (clk)
		clk_unprepare(clk);
	else
		dev_dbg(afe->dev, "NULL clk\n");
}

int mt8188_afe_clk_enable(struct mtk_base_afe *afe, struct clk *clk)
{
	int ret;

	if (clk) {
		ret = clk_enable(clk);
		if (ret) {
			dev_dbg(afe->dev, "%s(), failed to clk enable\n",
				__func__);
			return ret;
		}
	} else {
		dev_dbg(afe->dev, "NULL clk\n");
	}
	return 0;
}

void mt8188_afe_clk_disable(struct mtk_base_afe *afe, struct clk *clk)
{
	if (clk)
		clk_disable(clk);
	else
		dev_dbg(afe->dev, "NULL clk\n");
}

int mt8188_afe_set_clk_rate(struct mtk_base_afe *afe, struct clk *clk,
	unsigned int rate)
{
	int ret;

	if (clk) {
		ret = clk_set_rate(clk, rate);
		if (ret) {
			dev_dbg(afe->dev, "%s(), failed to set clk rate\n",
				__func__);
			return ret;
		}
	}

	return 0;
}

int mt8188_afe_get_clk_parent(struct mtk_base_afe *afe, struct clk *clk,
	struct clk **parent)
{
	if (clk) {
		*parent = clk_get_parent(clk);
		return 0;
	}

	dev_info(afe->dev, "%s(), clk is NULL\n", __func__);

	return -EINVAL;
}

int mt8188_afe_set_clk_parent(struct mtk_base_afe *afe, struct clk *clk,
	struct clk *parent)
{
	int ret;

	if (clk && parent) {
		ret = clk_set_parent(clk, parent);
		if (ret) {
			dev_dbg(afe->dev, "%s(), failed to set clk parent\n",
				__func__);
			return ret;
		}
	}

	return 0;
}

static unsigned int get_top_cg_reg(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8188_TOP_CG_A1SYS_TIMING:
	case MT8188_TOP_CG_A2SYS_TIMING:
	case MT8188_TOP_CG_A3SYS_TIMING:
	case MT8188_TOP_CG_A4SYS_TIMING:
	case MT8188_TOP_CG_26M_TIMING:
		return ASYS_TOP_CON;
	case MT8188_TOP_CG_GASRC0:
	case MT8188_TOP_CG_GASRC1:
	case MT8188_TOP_CG_GASRC2:
	case MT8188_TOP_CG_GASRC3:
	case MT8188_TOP_CG_GASRC4:
	case MT8188_TOP_CG_GASRC5:
	case MT8188_TOP_CG_GASRC6:
	case MT8188_TOP_CG_GASRC7:
	case MT8188_TOP_CG_GASRC8:
	case MT8188_TOP_CG_GASRC9:
	case MT8188_TOP_CG_GASRC10:
	case MT8188_TOP_CG_GASRC11:
		return AUDIO_TOP_CON6;
	default:
		return 0;
	}
}

static unsigned int get_top_cg_mask(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8188_TOP_CG_A1SYS_TIMING:
		return ASYS_TOP_CON_A1SYS_TIMING_ON;
	case MT8188_TOP_CG_A2SYS_TIMING:
		return ASYS_TOP_CON_A2SYS_TIMING_ON;
	case MT8188_TOP_CG_A3SYS_TIMING:
		return ASYS_TOP_CON_A3SYS_TIMING_ON;
	case MT8188_TOP_CG_A4SYS_TIMING:
		return ASYS_TOP_CON_A4SYS_TIMING_ON;
	case MT8188_TOP_CG_26M_TIMING:
		return ASYS_TOP_CON_26M_TIMING_ON;
	case MT8188_TOP_CG_GASRC0:
		return AUDIO_TOP_CON6_PDN_GASRC0;
	case MT8188_TOP_CG_GASRC1:
		return AUDIO_TOP_CON6_PDN_GASRC1;
	case MT8188_TOP_CG_GASRC2:
		return AUDIO_TOP_CON6_PDN_GASRC2;
	case MT8188_TOP_CG_GASRC3:
		return AUDIO_TOP_CON6_PDN_GASRC3;
	case MT8188_TOP_CG_GASRC4:
		return AUDIO_TOP_CON6_PDN_GASRC4;
	case MT8188_TOP_CG_GASRC5:
		return AUDIO_TOP_CON6_PDN_GASRC5;
	case MT8188_TOP_CG_GASRC6:
		return AUDIO_TOP_CON6_PDN_GASRC6;
	case MT8188_TOP_CG_GASRC7:
		return AUDIO_TOP_CON6_PDN_GASRC7;
	case MT8188_TOP_CG_GASRC8:
		return AUDIO_TOP_CON6_PDN_GASRC8;
	case MT8188_TOP_CG_GASRC9:
		return AUDIO_TOP_CON6_PDN_GASRC9;
	case MT8188_TOP_CG_GASRC10:
		return AUDIO_TOP_CON6_PDN_GASRC10;
	case MT8188_TOP_CG_GASRC11:
		return AUDIO_TOP_CON6_PDN_GASRC11;
	default:
		return 0;
	}
}

static unsigned int get_top_cg_on_val(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8188_TOP_CG_A1SYS_TIMING:
	case MT8188_TOP_CG_A2SYS_TIMING:
	case MT8188_TOP_CG_A3SYS_TIMING:
	case MT8188_TOP_CG_A4SYS_TIMING:
	case MT8188_TOP_CG_26M_TIMING:
		return get_top_cg_mask(cg_type);
	default:
		return 0;
	}
}

static unsigned int get_top_cg_off_val(unsigned int cg_type)
{
	switch (cg_type) {
	case MT8188_TOP_CG_A1SYS_TIMING:
	case MT8188_TOP_CG_A2SYS_TIMING:
	case MT8188_TOP_CG_A3SYS_TIMING:
	case MT8188_TOP_CG_A4SYS_TIMING:
	case MT8188_TOP_CG_26M_TIMING:
		return 0;
	default:
		return get_top_cg_mask(cg_type);
	}
}

int mt8188_afe_enable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	unsigned int reg = get_top_cg_reg(cg_type);
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned int val = get_top_cg_on_val(cg_type);
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->top_cg_ref_cnt[cg_type]++;
	if (afe_priv->top_cg_ref_cnt[cg_type] == 1)
		need_update = true;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, reg, mask, val);

	return 0;
}

int mt8188_afe_disable_top_cg(struct mtk_base_afe *afe, unsigned int cg_type)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	unsigned int reg = get_top_cg_reg(cg_type);
	unsigned int mask = get_top_cg_mask(cg_type);
	unsigned int val = get_top_cg_off_val(cg_type);
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->top_cg_ref_cnt[cg_type]--;
	if (afe_priv->top_cg_ref_cnt[cg_type] == 0)
		need_update = true;
	else if (afe_priv->top_cg_ref_cnt[cg_type] < 0)
		afe_priv->top_cg_ref_cnt[cg_type] = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, reg, mask, val);

	return 0;
}

int mt8188_afe_enable_reg_rw_clk(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	/* bus clock for DRAM access */
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUDIO_LOCAL_BUS_SEL]);

	/* audio 26m clock source */
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_INFRA_AO_AUDIO_26M_B]);

	/* bus clock for AFE internal SRAM access */
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUD_INTBUS_SEL]);

	/* AFE hw clock */
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_A1SYS_HP_SEL]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS_HP]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS]);

	return 0;
}

int mt8188_afe_disable_reg_rw_clk(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS_HP]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_A1SYS_HP_SEL]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUD_INTBUS_SEL]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_INFRA_AO_AUDIO_26M_B]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUDIO_LOCAL_BUS_SEL]);

	return 0;
}

int mt8188_afe_enable_afe_on(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->afe_on_ref_cnt++;
	if (afe_priv->afe_on_ref_cnt == 1)
		need_update = true;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x1);

	return 0;
}

int mt8188_afe_disable_afe_on(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	unsigned long flags;
	bool need_update = false;

	spin_lock_irqsave(&afe_priv->afe_ctrl_lock, flags);

	afe_priv->afe_on_ref_cnt--;
	if (afe_priv->afe_on_ref_cnt == 0)
		need_update = true;
	else if (afe_priv->afe_on_ref_cnt < 0)
		afe_priv->afe_on_ref_cnt = 0;

	spin_unlock_irqrestore(&afe_priv->afe_ctrl_lock, flags);

	if (need_update)
		regmap_update_bits(afe->regmap, AFE_DAC_CON0, 0x1, 0x0);

	return 0;
}

int mt8188_afe_enable_main_clock(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_A1SYS_HP_SEL]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUD_INTBUS_SEL]);

	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS_HP]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A2SYS]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A3SYS]);
	mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A4SYS]);

	mt8188_afe_enable_top_cg(afe, MT8188_TOP_CG_A1SYS_TIMING);
	mt8188_afe_enable_top_cg(afe, MT8188_TOP_CG_A2SYS_TIMING);
	mt8188_afe_enable_top_cg(afe, MT8188_TOP_CG_A3SYS_TIMING);
	mt8188_afe_enable_top_cg(afe, MT8188_TOP_CG_A4SYS_TIMING);
	mt8188_afe_enable_top_cg(afe, MT8188_TOP_CG_26M_TIMING);

	mt8188_afe_enable_afe_on(afe);

	return 0;
}

int mt8188_afe_disable_main_clock(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	mt8188_afe_disable_afe_on(afe);

	mt8188_afe_disable_top_cg(afe, MT8188_TOP_CG_26M_TIMING);
	mt8188_afe_disable_top_cg(afe, MT8188_TOP_CG_A4SYS_TIMING);
	mt8188_afe_disable_top_cg(afe, MT8188_TOP_CG_A3SYS_TIMING);
	mt8188_afe_disable_top_cg(afe, MT8188_TOP_CG_A2SYS_TIMING);
	mt8188_afe_disable_top_cg(afe, MT8188_TOP_CG_A1SYS_TIMING);

	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A4SYS]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A3SYS]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A2SYS]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_AFE]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_A1SYS_HP]);

	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_AUD_INTBUS_SEL]);
	mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_TOP_A1SYS_HP_SEL]);

	return 0;
}

struct mt8188_afe_tuner_cfg {
	unsigned int id;
	int apll_div_reg;
	unsigned int apll_div_shift;
	unsigned int apll_div_maskbit;
	unsigned int apll_div_default;
	int ref_ck_sel_reg;
	unsigned int ref_ck_sel_shift;
	unsigned int ref_ck_sel_maskbit;
	unsigned int ref_ck_sel_default;
	int tuner_en_reg;
	unsigned int tuner_en_shift;
	unsigned int tuner_en_maskbit;
	int upper_bound_reg;
	unsigned int upper_bound_shift;
	unsigned int upper_bound_maskbit;
	unsigned int upper_bound_default;
	spinlock_t ctrl_lock;
	int ref_cnt;
};

static struct mt8188_afe_tuner_cfg
	mt8188_afe_tuner_cfgs[MT8188_AUD_PLL_NUM] = {
	[MT8188_AUD_PLL1] = {
		.id = MT8188_AUD_PLL1,
		.apll_div_reg = AFE_APLL_TUNER_CFG,
		.apll_div_shift = 4,
		.apll_div_maskbit = 0xf,
		.apll_div_default = 0x7,
		.ref_ck_sel_reg = AFE_APLL_TUNER_CFG,
		.ref_ck_sel_shift = 1,
		.ref_ck_sel_maskbit = 0x3,
		.ref_ck_sel_default = 0x2,
		.tuner_en_reg = AFE_APLL_TUNER_CFG,
		.tuner_en_shift = 0,
		.tuner_en_maskbit = 0x1,
		.upper_bound_reg = AFE_APLL_TUNER_CFG,
		.upper_bound_shift = 8,
		.upper_bound_maskbit = 0xff,
		.upper_bound_default = 0x3,
	},
	[MT8188_AUD_PLL2] = {
		.id = MT8188_AUD_PLL2,
		.apll_div_reg = AFE_APLL_TUNER_CFG1,
		.apll_div_shift = 4,
		.apll_div_maskbit = 0xf,
		.apll_div_default = 0x7,
		.ref_ck_sel_reg = AFE_APLL_TUNER_CFG1,
		.ref_ck_sel_shift = 1,
		.ref_ck_sel_maskbit = 0x3,
		.ref_ck_sel_default = 0x1,
		.tuner_en_reg = AFE_APLL_TUNER_CFG1,
		.tuner_en_shift = 0,
		.tuner_en_maskbit = 0x1,
		.upper_bound_reg = AFE_APLL_TUNER_CFG1,
		.upper_bound_shift = 8,
		.upper_bound_maskbit = 0xff,
		.upper_bound_default = 0x3,
	},
	[MT8188_AUD_PLL3] = {
		.id = MT8188_AUD_PLL3,
		.apll_div_reg = AFE_EARC_APLL_TUNER_CFG,
		.apll_div_shift = 4,
		.apll_div_maskbit = 0x3f,
		.apll_div_default = 0x3,
		.ref_ck_sel_reg = AFE_EARC_APLL_TUNER_CFG,
		.ref_ck_sel_shift = 24,
		.ref_ck_sel_maskbit = 0x3,
		.ref_ck_sel_default = 0x0,
		.tuner_en_reg = AFE_EARC_APLL_TUNER_CFG,
		.tuner_en_shift = 0,
		.tuner_en_maskbit = 0x1,
		.upper_bound_reg = AFE_EARC_APLL_TUNER_CFG,
		.upper_bound_shift = 12,
		.upper_bound_maskbit = 0xff,
		.upper_bound_default = 0x4,
	},
	[MT8188_AUD_PLL4] = {
		.id = MT8188_AUD_PLL4,
		.apll_div_reg = AFE_SPDIFIN_APLL_TUNER_CFG,
		.apll_div_shift = 4,
		.apll_div_maskbit = 0x3f,
		.apll_div_default = 0x7,
		.ref_ck_sel_reg = AFE_SPDIFIN_APLL_TUNER_CFG1,
		.ref_ck_sel_shift = 8,
		.ref_ck_sel_maskbit = 0x1,
		.ref_ck_sel_default = 0,
		.tuner_en_reg = AFE_SPDIFIN_APLL_TUNER_CFG,
		.tuner_en_shift = 0,
		.tuner_en_maskbit = 0x1,
		.upper_bound_reg = AFE_SPDIFIN_APLL_TUNER_CFG,
		.upper_bound_shift = 12,
		.upper_bound_maskbit = 0xff,
		.upper_bound_default = 0x4,
	},
	[MT8188_AUD_PLL5] = {
		.id = MT8188_AUD_PLL5,
		.apll_div_reg = AFE_LINEIN_APLL_TUNER_CFG,
		.apll_div_shift = 4,
		.apll_div_maskbit = 0x3f,
		.apll_div_default = 0x3,
		.ref_ck_sel_reg = AFE_LINEIN_APLL_TUNER_CFG,
		.ref_ck_sel_shift = 24,
		.ref_ck_sel_maskbit = 0x1,
		.ref_ck_sel_default = 0,
		.tuner_en_reg = AFE_LINEIN_APLL_TUNER_CFG,
		.tuner_en_shift = 0,
		.tuner_en_maskbit = 0x1,
		.upper_bound_reg = AFE_LINEIN_APLL_TUNER_CFG,
		.upper_bound_shift = 12,
		.upper_bound_maskbit = 0xff,
		.upper_bound_default = 0x4,
	},
};

static struct mt8188_afe_tuner_cfg *mt8188_afe_found_apll_tuner(unsigned int id)
{
	if (id >= MT8188_AUD_PLL_NUM)
		return NULL;

	return &mt8188_afe_tuner_cfgs[id];
}

int mt8188_afe_init_apll_tuner(unsigned int id)
{
	struct mt8188_afe_tuner_cfg *cfg = mt8188_afe_found_apll_tuner(id);

	if (!cfg)
		return -EINVAL;

	cfg->ref_cnt = 0;
	spin_lock_init(&cfg->ctrl_lock);

	return 0;
}

static int mt8188_afe_setup_apll_tuner(struct mtk_base_afe *afe, unsigned int id)
{
	const struct mt8188_afe_tuner_cfg *cfg = mt8188_afe_found_apll_tuner(id);

	if (!cfg)
		return -EINVAL;

	regmap_update_bits(afe->regmap,
			   cfg->apll_div_reg,
			   cfg->apll_div_maskbit << cfg->apll_div_shift,
			   cfg->apll_div_default << cfg->apll_div_shift);

	regmap_update_bits(afe->regmap,
			   cfg->ref_ck_sel_reg,
			   cfg->ref_ck_sel_maskbit << cfg->ref_ck_sel_shift,
			   cfg->ref_ck_sel_default << cfg->ref_ck_sel_shift);

	regmap_update_bits(afe->regmap,
			   cfg->upper_bound_reg,
			   cfg->upper_bound_maskbit << cfg->upper_bound_shift,
			   cfg->upper_bound_default << cfg->upper_bound_shift);

	return 0;
}

static int mt8188_afe_get_tuner_clk_id(unsigned int id)
{
	int clk_id = -1;

	switch (id) {
	case MT8188_AUD_PLL1:
		clk_id = MT8188_CLK_APMIXED_APLL1;
		break;
	case MT8188_AUD_PLL2:
		clk_id = MT8188_CLK_APMIXED_APLL2;
		break;
	case MT8188_AUD_PLL3:
		clk_id = MT8188_CLK_APMIXED_APLL3;
		break;
	case MT8188_AUD_PLL4:
		clk_id = MT8188_CLK_APMIXED_APLL4;
		break;
	case MT8188_AUD_PLL5:
		clk_id = MT8188_CLK_APMIXED_APLL5;
		break;
	default:
		break;
	}

	return clk_id;
}

static int mt8188_afe_enable_tuner_clk(struct mtk_base_afe *afe,
	unsigned int id)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int clk_id = mt8188_afe_get_tuner_clk_id(id);
	int ret = 0;

	if (clk_id >= 0)
		mt8188_afe_enable_clk(afe, afe_priv->clk[clk_id]);

	switch (id) {
	case MT8188_AUD_PLL1:
		mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL]);
		mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL1_TUNER]);
		break;
	case MT8188_AUD_PLL2:
		mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL2]);
		mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL2_TUNER]);
		break;
	case MT8188_AUD_PLL3:
		mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_EARC_TUNER]);
		break;
	case MT8188_AUD_PLL4:
		mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_SPDIFIN_TUNER_APLL]);
		break;
	case MT8188_AUD_PLL5:
		mt8188_afe_enable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_LINEIN_TUNER]);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int mt8188_afe_disable_tuner_clk(struct mtk_base_afe *afe,
	unsigned int id)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int clk_id = mt8188_afe_get_tuner_clk_id(id);
	int ret = 0;

	switch (id) {
	case MT8188_AUD_PLL1:
		mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL1_TUNER]);
		mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL]);
		break;
	case MT8188_AUD_PLL2:
		mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL2_TUNER]);
		mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_APLL2]);
		break;
	case MT8188_AUD_PLL3:
		mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_EARC_TUNER]);
		break;
	case MT8188_AUD_PLL4:
		mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_SPDIFIN_TUNER_APLL]);
		break;
	case MT8188_AUD_PLL5:
		mt8188_afe_disable_clk(afe, afe_priv->clk[MT8188_CLK_AUD_LINEIN_TUNER]);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (clk_id >= 0)
		mt8188_afe_disable_clk(afe, afe_priv->clk[clk_id]);

	return ret;
}

int mt8188_afe_enable_apll_tuner(struct mtk_base_afe *afe, unsigned int id)
{
	struct mt8188_afe_tuner_cfg *cfg = mt8188_afe_found_apll_tuner(id);
	bool need_update = false;
	unsigned long flags;
	int ret = 0;

	if (!cfg)
		return -EINVAL;

	ret = mt8188_afe_setup_apll_tuner(afe, id);
	if (ret)
		return ret;

	ret = mt8188_afe_enable_tuner_clk(afe, id);
	if (ret)
		return ret;

	spin_lock_irqsave(&cfg->ctrl_lock, flags);

	cfg->ref_cnt++;
	if (cfg->ref_cnt == 1)
		need_update = true;

	spin_unlock_irqrestore(&cfg->ctrl_lock, flags);

	if (need_update) {
		regmap_update_bits(afe->regmap,
				   cfg->tuner_en_reg,
				   cfg->tuner_en_maskbit << cfg->tuner_en_shift,
				   1 << cfg->tuner_en_shift);
	}

	return ret;
}

int mt8188_afe_disable_apll_tuner(struct mtk_base_afe *afe, unsigned int id)
{
	struct mt8188_afe_tuner_cfg *cfg = mt8188_afe_found_apll_tuner(id);
	bool need_update = false;
	unsigned long flags;
	int ret = 0;

	if (!cfg)
		return -EINVAL;

	spin_lock_irqsave(&cfg->ctrl_lock, flags);

	cfg->ref_cnt--;
	if (cfg->ref_cnt == 0)
		need_update = true;
	else if (cfg->ref_cnt < 0)
		cfg->ref_cnt = 0;

	spin_unlock_irqrestore(&cfg->ctrl_lock, flags);

	if (need_update) {
		regmap_update_bits(afe->regmap,
				   cfg->tuner_en_reg,
				   cfg->tuner_en_maskbit << cfg->tuner_en_shift,
				   0 << cfg->tuner_en_shift);
	}

	ret = mt8188_afe_disable_tuner_clk(afe, id);
	if (ret)
		return ret;

	return ret;
}

bool mt8188_afe_is_apll_tuner_enabled(struct mtk_base_afe *afe, unsigned int id)
{
	struct mt8188_afe_tuner_cfg *cfg = mt8188_afe_found_apll_tuner(id);
	unsigned int value = 0;

	if (!cfg)
		return false;

	regmap_read(afe->regmap, cfg->tuner_en_reg, &value);

	return !!(value & (cfg->tuner_en_maskbit << cfg->tuner_en_shift));
}

int mt8188_afe_dram_request(struct device *dev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct arm_smccc_res res;

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		__func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);

	/* use arm_smccc_smc to notify SPM */
	if (afe_priv->dram_resource_counter == 0)
		arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
			      MTK_AUDIO_SMC_OP_DRAM_REQUEST,
			      0, 0, 0, 0, 0, 0, &res);

	afe_priv->dram_resource_counter++;
	mutex_unlock(&mutex_request_dram);
#endif
	return 0;
}

int mt8188_afe_dram_release(struct device *dev)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct arm_smccc_res res;

	dev_dbg(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
	afe_priv->dram_resource_counter--;

	/* use arm_smccc_smc to notify SPM */
	if (afe_priv->dram_resource_counter == 0)
		arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
			      MTK_AUDIO_SMC_OP_DRAM_RELEASE,
			      0, 0, 0, 0, 0, 0, &res);

	if (afe_priv->dram_resource_counter < 0) {
		dev_dbg(dev, "%s(), dram_resource_counter %d\n",
			 __func__, afe_priv->dram_resource_counter);
		afe_priv->dram_resource_counter = 0;
	}
	mutex_unlock(&mutex_request_dram);
#endif
	return 0;
}

static const char * const mt8188_apll_tuner_en_text[] = {
	"Off",
	"On",
};

static SOC_ENUM_SINGLE_EXT_DECL(mt8188_apll1_tuner_en_enum,
				mt8188_apll_tuner_en_text);
static SOC_ENUM_SINGLE_EXT_DECL(mt8188_apll2_tuner_en_enum,
				mt8188_apll_tuner_en_text);
static SOC_ENUM_SINGLE_EXT_DECL(mt8188_apll3_tuner_en_enum,
				mt8188_apll_tuner_en_text);
static SOC_ENUM_SINGLE_EXT_DECL(mt8188_apll4_tuner_en_enum,
				mt8188_apll_tuner_en_text);
static SOC_ENUM_SINGLE_EXT_DECL(mt8188_apll5_tuner_en_enum,
				mt8188_apll_tuner_en_text);

static int mt8188_apll_tuner_en_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int id = kcontrol->id.device;

	ucontrol->value.enumerated.item[0] =
		mt8188_afe_is_apll_tuner_enabled(afe, id);

	return 0;
}

static int mt8188_apll_tuner_en_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	unsigned int en = ucontrol->value.enumerated.item[0];
	unsigned int id = kcontrol->id.device;

	if (en >= e->items)
		return -EINVAL;

	if (en)
		mt8188_afe_enable_apll_tuner(afe, id);
	else
		mt8188_afe_disable_apll_tuner(afe, id);

	return 0;
}

static const struct snd_kcontrol_new mt8188_afe_clk_controls[] = {
	MT8188_SOC_ENUM_EXT("APLL1_Tuner_Enable",
			    mt8188_apll1_tuner_en_enum,
			    mt8188_apll_tuner_en_get,
			    mt8188_apll_tuner_en_put,
			    MT8188_AUD_PLL1),
	MT8188_SOC_ENUM_EXT("APLL2_Tuner_Enable",
			    mt8188_apll2_tuner_en_enum,
			    mt8188_apll_tuner_en_get,
			    mt8188_apll_tuner_en_put,
			    MT8188_AUD_PLL2),
	MT8188_SOC_ENUM_EXT("APLL3_Tuner_Enable",
			    mt8188_apll3_tuner_en_enum,
			    mt8188_apll_tuner_en_get,
			    mt8188_apll_tuner_en_put,
			    MT8188_AUD_PLL3),
	MT8188_SOC_ENUM_EXT("APLL4_Tuner_Enable",
			    mt8188_apll4_tuner_en_enum,
			    mt8188_apll_tuner_en_get,
			    mt8188_apll_tuner_en_put,
			    MT8188_AUD_PLL4),
	MT8188_SOC_ENUM_EXT("APLL5_Tuner_Enable",
			    mt8188_apll5_tuner_en_enum,
			    mt8188_apll_tuner_en_get,
			    mt8188_apll_tuner_en_put,
			    MT8188_AUD_PLL5),
};

int mt8188_afe_add_clk_controls(struct snd_soc_component *component)
{
	return snd_soc_add_component_controls(component,
					      mt8188_afe_clk_controls,
					      ARRAY_SIZE(mt8188_afe_clk_controls));
}

