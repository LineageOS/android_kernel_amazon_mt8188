// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_vdo1_comp.h"

int mtk_vdo1_sel_in(
	struct mtk_drm_crtc *mtk_crtc,
	enum mtk_ddp_comp_id curr,
	enum mtk_ddp_comp_id next,
	unsigned int *addr)
{
	int value = -1;
	void __iomem *reg = mtk_crtc->config_regs;

	if (mtk_crtc->dispsys_num > 1 &&
		mtk_crtc->mmsys_reg_data->dispsys_map &&
		mtk_crtc->mmsys_reg_data->dispsys_map[curr])
		reg = mtk_crtc->side_config_regs;

	DDPDBG("%s(): curr=%d, next=%d, reg=%p\n",
		__func__, curr, next, reg);

	if (curr == DDP_COMPONENT_PSEUDO_OVL &&
		next == DDP_COMPONENT_ETHDR) {
		DDPDBG("%s(): pseudo_ovl -> ethdr\n", __func__);
		writel_relaxed(VDO1_VPP_MERGE0_P0_SEL_IN_FROM_MDP_RDMA0,
			reg + VDO1_VPP_MERGE0_P0_SEL_IN);
		writel_relaxed(VDO1_VPP_MERGE0_P1_SEL_IN_FROM_MDP_RDMA1,
			reg + VDO1_VPP_MERGE0_P1_SEL_IN);
		writel_relaxed(VDO1_VPP_MERGE1_P0_SEL_IN_FROM_MDP_RDMA2,
			reg + VDO1_VPP_MERGE1_P0_SEL_IN);

		writel_relaxed(VDO1_RDMA0_SEL_IN_FROM_RDMA0_SOUT,
			reg + VDO1_RDMA0_SEL_IN);
		writel_relaxed(VDO1_RDMA1_SEL_IN_FROM_RDMA1_SOUT,
			reg + VDO1_RDMA1_SEL_IN);
		writel_relaxed(VDO1_RDMA2_SEL_IN_FROM_RDMA2_SOUT,
			reg + VDO1_RDMA2_SEL_IN);
		writel_relaxed(VDO1_RDMA3_SEL_IN_FROM_RDMA3_SOUT,
			reg + VDO1_RDMA3_SEL_IN);
		writel_relaxed(VDO1_RDMA4_SEL_IN_FROM_RDMA4_SOUT,
			reg + VDO1_RDMA4_SEL_IN);
		writel_relaxed(VDO1_RDMA5_SEL_IN_FROM_RDMA5_SOUT,
			reg + VDO1_RDMA5_SEL_IN);
		writel_relaxed(VDO1_RDMA6_SEL_IN_FROM_RDMA6_SOUT,
			reg + VDO1_RDMA6_SEL_IN);
		writel_relaxed(VDO1_RDMA7_SEL_IN_FROM_RDMA7_SOUT,
			reg + VDO1_RDMA7_SEL_IN);
	} else if (curr == DDP_COMPONENT_ETHDR &&
		next == DDP_COMPONENT_MERGE5) {
		DDPDBG("%s(): ethdr -> merge5\n", __func__);
		writel_relaxed(VDO1_MIXER_IN1_SEL_IN_FROM_HDR_VDO_FE0,
			reg + VDO1_MIXER_IN1_SEL_IN);
		writel_relaxed(VDO1_MIXER_IN2_SEL_IN_FROM_HDR_VDO_FE1,
			reg + VDO1_MIXER_IN2_SEL_IN);
		writel_relaxed(VDO1_MIXER_IN3_SEL_IN_FROM_HDR_GFX_FE0,
			reg + VDO1_MIXER_IN3_SEL_IN);
		writel_relaxed(VDO1_MIXER_IN4_SEL_IN_FROM_HDR_GFX_FE1,
			reg + VDO1_MIXER_IN4_SEL_IN);
		writel_relaxed(VDO1_MIXER_SOUT_SEL_IN_FROM_DISP_MIXER,
			reg + VDO1_MIXER_SOUT_SEL_IN);
		writel_relaxed(VDO1_MERGE4_ASYNC_SEL_IN_FROM_HDR_VDO_BE0,
			reg + VDO1_MERGE4_ASYNC_SEL_IN);
	} else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_DPI0) {
		DDPDBG("%s(): merge5 -> dpi0\n", __func__);
		*addr = VDO1_DISP_DPI0_SEL_IN;
		value = VDO1_DISP_DPI0_SEL_IN_FROM_VPP_MERGE4_MOUT;
	} else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_DPI1) {
		DDPDBG("%s(): merge5 -> dpi1\n", __func__);
		*addr = VDO1_DISP_DPI1_SEL_IN;
		value = VDO1_DISP_DPI1_SEL_IN_FROM_VPP_MERGE4_MOUT;
	} else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_DP_INTF1) {
		DDPDBG("%s(): merge5 -> dp_intf1\n", __func__);
		*addr = VDO1_DISP_DP_INTF0_SEL_IN;
		value = VDO1_DISP_DP_INTF0_SEL_IN_FROM_VPP_MERGE4_MOUT;
	}
#ifdef CONFIG_MTK_WFD_OVER_VDO1
	else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_MERGE0) {
		reg = mtk_crtc->config_regs;
		writel_relaxed(readl(reg + VDO0_SEL_VPP_MERGE0) |
			VDO0_SEL_VPP_MERGE0_I0_FROM_VDO1,
			reg + VDO0_SEL_VPP_MERGE0);
	} else if (curr == DDP_COMPONENT_MERGE0 &&
		next == DDP_COMPONENT_WDMA0) {
		writel_relaxed(VDO0_SIN_SEL_DISP_WDMA0_FROM_VPP_MERGE0_O0,
			reg + VDO0_SIN_SEL_DISP_WDMA0);
	} else if (curr == DDP_COMPONENT_MERGE0 &&
		next == DDP_COMPONENT_DPI1) {
		writel_relaxed(VDO0_SIN_SEL_VDO0A_VDO1_FROM_VPP_MERGE0,
			reg + VDO0_SIN_SEL_VDO0A_VDO1);
		reg = mtk_crtc->side_config_regs;
		writel_relaxed(VDO1_DISP_DPI1_SEL_IN_FROM_VDO0_MERGE_DL_ASYNC_MOUT,
			reg + VDO1_DISP_DPI1_SEL_IN);
	}
#endif
	else
		DDPDBG("%s(): unknown path\n", __func__);

	return value;
}

int mtk_vdo1_sout_sel(
	struct mtk_drm_crtc *mtk_crtc,
	enum mtk_ddp_comp_id curr,
	enum mtk_ddp_comp_id next)
{
	int value = -1;
	void __iomem *reg = mtk_crtc->config_regs;

	if (mtk_crtc->dispsys_num > 1 &&
		mtk_crtc->mmsys_reg_data->dispsys_map &&
		mtk_crtc->mmsys_reg_data->dispsys_map[curr])
		reg = mtk_crtc->side_config_regs;

	DDPDBG("%s(): curr=%d, next=%d, reg=%p\n",
		__func__, curr, next, reg);

	if (curr == DDP_COMPONENT_PSEUDO_OVL &&
		next == DDP_COMPONENT_ETHDR) {
		DDPDBG("%s(): pseudo_ovl -> ethdr\n", __func__);
		writel_relaxed(VDO1_RDMA0_SOUT_SEL_TO_RDMA0_SEL,
			reg + VDO1_RDMA0_SOUT_SEL);
		writel_relaxed(VDO1_RDMA1_SOUT_SEL_TO_RDMA1_SEL,
			reg + VDO1_RDMA1_SOUT_SEL);
		writel_relaxed(VDO1_RDMA2_SOUT_SEL_TO_RDMA2_SEL,
			reg + VDO1_RDMA2_SOUT_SEL);
		writel_relaxed(VDO1_RDMA3_SOUT_SEL_TO_RDMA3_SEL,
			reg + VDO1_RDMA3_SOUT_SEL);
		writel_relaxed(VDO1_RDMA4_SOUT_SEL_TO_RDMA4_SEL,
			reg + VDO1_RDMA4_SOUT_SEL);
		writel_relaxed(VDO1_RDMA5_SOUT_SEL_TO_RDMA5_SEL,
			reg + VDO1_RDMA5_SOUT_SEL);
		writel_relaxed(VDO1_RDMA6_SOUT_SEL_TO_RDMA6_SEL,
			reg + VDO1_RDMA6_SOUT_SEL);
		writel_relaxed(VDO1_RDMA7_SOUT_SEL_TO_RDMA7_SEL,
			reg + VDO1_RDMA7_SOUT_SEL);

		writel_relaxed(VDO1_MERGE0_ASYNC_SOUT_SEL_TO_HDR_VDO_FE0,
			reg + VDO1_MERGE0_ASYNC_SOUT_SEL);
		writel_relaxed(VDO1_MERGE1_ASYNC_SOUT_SEL_HDR_VDO_FE1,
			reg + VDO1_MERGE1_ASYNC_SOUT_SEL);
		writel_relaxed(VDO1_MERGE2_ASYNC_SOUT_SEL_HDR_GFX_FE0,
			reg + VDO1_MERGE2_ASYNC_SOUT_SEL);
		writel_relaxed(VDO1_MERGE3_ASYNC_SOUT_SEL_HDR_GFX_FE1,
			reg + VDO1_MERGE3_ASYNC_SOUT_SEL);
	} else if (curr == DDP_COMPONENT_ETHDR &&
		next == DDP_COMPONENT_MERGE5) {
		DDPDBG("%s(): ethdr -> merge5\n", __func__);
		writel_relaxed(VDO1_MIXER_IN1_SOUT_SEL_TO_DISP_MIXER,
			reg + VDO1_MIXER_IN1_SOUT_SEL);
		writel_relaxed(VDO1_MIXER_IN2_SOUT_SEL_TO_DISP_MIXER,
			reg + VDO1_MIXER_IN2_SOUT_SEL);
		writel_relaxed(VDO1_MIXER_IN3_SOUT_SEL_TO_DISP_MIXER,
			reg + VDO1_MIXER_IN3_SOUT_SEL);
		writel_relaxed(VDO1_MIXER_IN4_SOUT_SEL_TO_DISP_MIXER,
			reg + VDO1_MIXER_IN4_SOUT_SEL);
		writel_relaxed(VDO1_MIXER_OUT_SOUT_SEL_TO_HDR_VDO_BE0,
			reg + VDO1_MIXER_OUT_SOUT_SEL);
	}
#ifdef CONFIG_MTK_WFD_OVER_VDO1
	else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_MERGE0) {
		reg = mtk_crtc->config_regs;
		writel_relaxed(VDO0_SOUT_SEL_VDO1_VDO0_TO_VPP_MERGE0,
			reg + VDO0_SOUT_SEL_VDO1_VDO0);
	} else if (curr == DDP_COMPONENT_MERGE0 &&
		next == DDP_COMPONENT_WDMA0) {
		writel_relaxed(readl(reg + VDO0_SEL_VPP_MERGE0) |
			VDO0_SEL_VPP_MERGE0_O0_TO_DISP_WDMA0,
			reg + VDO0_SEL_VPP_MERGE0);
	} else if (curr == DDP_COMPONENT_MERGE0 &&
		next == DDP_COMPONENT_DPI1) {
		writel_relaxed(readl(reg + VDO0_SEL_VPP_MERGE0) |
			VDO0_SEL_VPP_MERGE0_O0_TO_VDO1,
			reg + VDO0_SEL_VPP_MERGE0);
		reg = mtk_crtc->side_config_regs;
		writel_relaxed(VDO0_MERGE_DL_ASYNC_MOUT_EN_TO_DPI1_SEL,
			reg + VDO0_MERGE_DL_ASYNC_MOUT_EN);
	}
#endif
	else
		DDPDBG("%s(): unknown path\n", __func__);

	return value;
}

int mtk_vdo1_mout_en(
	enum mtk_ddp_comp_id curr,
	enum mtk_ddp_comp_id next,
	unsigned int *addr)
{
	int value = -1;

	DDPDBG("%s(): curr=%d, next=%d\n", __func__, curr, next);

	if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_DPI0) {
		DDPDBG("%s(): merge5 -> dpi0\n", __func__);
		*addr = VDO1_MERGE4_MOUT_EN;
		value = VDO1_MERGE4_MOUT_EN_TO_DPI0_SEL;
	} else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_DPI1) {
		*addr = VDO1_MERGE4_MOUT_EN;
#ifndef CONFIG_FPGA_EARLY_PORTING
		DDPDBG("%s(): merge5 -> dpi1\n", __func__);
		value = VDO1_MERGE4_MOUT_EN_TO_DPI1_SEL;
#else
		DDPDBG("%s(): merge5 -> dsi0 (debug only)\n", __func__);
		value = VDO1_MERGE4_MOUT_EN_TO_VDOSYS0;
#endif
	} else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_DP_INTF1) {
		DDPDBG("%s(): merge5 -> dp_intf1\n", __func__);
		*addr = VDO1_MERGE4_MOUT_EN;
		value = VDO1_MERGE4_MOUT_EN_TO_DP_INTF0_SEL;
	}
#ifdef CONFIG_MTK_WFD_OVER_VDO1
	else if (curr == DDP_COMPONENT_MERGE5 &&
		next == DDP_COMPONENT_MERGE0) {
		*addr = VDO1_MERGE4_MOUT_EN;
		value = VDO1_MERGE4_MOUT_EN_TO_VDOSYS0;
	}
#endif
	else
		DDPDBG("%s(): unknown path\n", __func__);

	return value;
}
