/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_VDO1_PATH_H__
#define __MTK_VDO1_PATH_H__

#include "../../mtk_drm_crtc.h"
#include "../../mtk_drm_ddp_comp.h"

static const struct mtk_addon_scenario_data mtk_ext_addon[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

// external display
static const enum mtk_ddp_comp_id mtk_ext_path[] = {
#ifdef CONFIG_MTK_WFD_OVER_VDO1
	DDP_COMPONENT_SUB1_VIRTUAL0
#else
	DDP_COMPONENT_PSEUDO_OVL,
	DDP_COMPONENT_ETHDR,
	DDP_COMPONENT_MERGE5,
#ifdef CONFIG_MTK_DPTX_SUPPORT
	DDP_COMPONENT_DP_INTF1,
#else
	DDP_COMPONENT_DPI1,
#endif
#endif
};

static const struct mtk_crtc_path_data mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mtk_ext_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mtk_ext_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mtk_ext_addon,
};

#ifdef CONFIG_MTK_WFD_OVER_VDO1
// virtual display
static const enum mtk_ddp_comp_id mtk_vrt_path[] = {
	DDP_COMPONENT_PSEUDO_OVL,
	DDP_COMPONENT_ETHDR,
	DDP_COMPONENT_MERGE5,
	DDP_COMPONENT_MERGE0,
	DDP_COMPONENT_WDMA0
};

static const struct mtk_crtc_path_data mtk_vrt_path_data = {
	.path[DDP_MAJOR][0] = mtk_vrt_path,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mtk_vrt_path),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mtk_ext_addon,
};

#endif

#endif // __MTK_VDO1_PATH_H__
