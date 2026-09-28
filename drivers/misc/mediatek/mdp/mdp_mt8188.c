// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/highmem.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>

#include "cmdq_reg.h"
#include "mdp_common.h"
#include "mdp_larb.h"
#include "mtk-interconnect.h"

#include "mdp_debug.h"
#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif
#include <linux/slab.h>
#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
#include "mtk_iommu_ext.h"
#elif IS_ENABLED(CONFIG_MTK_IOMMU)
#include "memory/mt8188-larb-port.h"
#elif IS_ENABLED(CONFIG_MTK_M4U)
#include "m4u.h"
#endif

#include "mdp_engine_mt8188.h"
#include "mdp_base_mt8188.h"

#include<linux/clk.h>

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include <cmdq-sec-iwc-common.h>
#endif

/* support RDMA prebuilt access */
int gCmdqRdmaPrebuiltSupport;
/* support register MSB */
int gMdpRegMSBSupport;

/* mdp */
static struct icc_path *path_mdp_rdma0[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_rdma2[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_rdma3[MDP_TOTAL_THREAD];

static struct icc_path *path_mdp_wrot0[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_wrot2[MDP_TOTAL_THREAD];
static struct icc_path *path_mdp_wrot3[MDP_TOTAL_THREAD];

#include "cmdq_device.h"
struct CmdqMdpModuleBaseVA {
	long MMSYS2_CONFIG;
	long MDP_RDMA0;
	long MDP_RDMA2;
	long MDP_RDMA3;
	long MDP_FG0;
	long MDP_FG2;
	long MDP_FG3;
	long MDP_HDR0;
	long MDP_HDR2;
	long MDP_HDR3;
	long MDP_AAL0;
	long MDP_AAL2;
	long MDP_AAL3;
	long MDP_RSZ0;
	long MDP_RSZ2;
	long MDP_RSZ3;
	long MDP_MERGE2;
	long MDP_MERGE3;
	long MDP_TDSHP0;
	long MDP_TDSHP2;
	long MDP_TDSHP3;
	long MDP_COLOR0;
	long MDP_COLOR2;
	long MDP_COLOR3;
	long MDP_OVL0;
	long MDP_PAD0;
	long MDP_PAD2;
	long MDP_PAD3;
	long MDP_WROT0;
	long MDP_WROT2;
	long MDP_WROT3;
	long MM_MUTEX;
	long MM_MUTEX2;
};
static struct CmdqMdpModuleBaseVA gCmdqMdpModuleBaseVA;

struct mdp_base_pa {
	u32 aal0;
	u32 aal2;
	u32 aal3;
	u32 hdr0;
	u32 hdr2;
	u32 hdr3;
};
static struct mdp_base_pa mdp_module_pa;

struct CmdqMdpModuleClock {
	/* VPPSYS0 CONFIG */
	struct clk *clk_MDP_FG0;              /* cg0[1] */
	struct clk *clk_MDP_PAD0;             /* cg0[7] */
	struct clk *clk_WARP0_ASYNC_TX;       /* cg0[10] */
	struct clk *clk_MDP_MUTEX0;           /* cg0[13] */
	struct clk *clk_VPP02VPP1_RELAY;      /* cg0[14] */
	struct clk *clk_VPP12VPP0_ASYNC;      /* cg0[15] */
	struct clk *clk_MMSYSRAM_TOP;		  /* cg0[16]  sysram clock */
	struct clk *clk_MDP_AAL0;             /* cg0[17] */
	struct clk *clk_MDP_RSZ0;             /* cg0[18] */

	struct clk *clk_MDP_RDMA0;            /* cg1[12] */
	struct clk *clk_MDP_WROT0;            /* cg1[13] */
	struct clk *clk_MDP_HDR0;             /* cg1[24] */
	struct clk *clk_MDP_TDSHP0;           /* cg1[25] */
	struct clk *clk_MDP_COLOR0;           /* cg1[26] */
	struct clk *clk_MDP_OVL0;             /* cg1[27] */

	struct clk *clk_WARP0_RELAY;          /* cg2[0] */
	struct clk *clk_WARP0_MDP_DL_ASYNC;   /* cg2[1] */

	/* VPPSYS1 CONFIG */
	struct clk *clk_MDP_WROT2;            /* cg0[4] */
	struct clk *clk_MDP_PAD2;             /* cg0[5] */
	struct clk *clk_MDP_WROT3;            /* cg0[6] */
	struct clk *clk_MDP_PAD3;             /* cg0[7] */
	struct clk *clk_MDP_RDMA2;            /* cg0[10] */
	struct clk *clk_MDP_FG2;              /* cg0[11] */
	struct clk *clk_MDP_RDMA3;            /* cg0[12] */
	struct clk *clk_MDP_FG3;              /* cg0[13] */
	struct clk *clk_MDP_RSZ2;			  /* cg0[20] */
	struct clk *clk_MDP_MERGE2;           /* cg0[21] */
	struct clk *clk_MDP_TDSHP2;           /* cg0[22] */
	struct clk *clk_MDP_COLOR2;           /* cg0[23] */
	struct clk *clk_MDP_RSZ3;			  /* cg0[24] */
	struct clk *clk_MDP_MERGE3;			  /* cg0[25] */
	struct clk *clk_MDP_TDSHP3;           /* cg0[26] */
	struct clk *clk_MDP_COLOR3;           /* cg0[27] */

	struct clk *clk_MDP_HDR2;             /* cg1[2] */
	struct clk *clk_MDP_AAL2;             /* cg1[3] */
	struct clk *clk_MDP_HDR3;             /* cg1[4] */
	struct clk *clk_MDP_AAL3;             /* cg1[5] */
	struct clk *clk_DISP_MUTEX;           /* cg1[7] */
	struct clk *clk_VPP0_DL_ASYNC; /* cg1[10] vpp02vpp1 cross clk */
	struct clk *clk_VPP0_DL1_RELAY; /* cg1[11] cross clk always on*/

	/* TOPCKGEN CONFIG */
	struct clk *clk_TOP_CFG_VPP0;		  /* cg1[0] */
	struct clk *clk_TOP_CFG_VPP1;		  /* cg1[1] */
	struct clk *clk_TOP_CFG_26M_VPP0;	  /* cg1[5] */
	struct clk *clk_TOP_CFG_26M_VPP1;	  /* cg1[6] */
};
static struct CmdqMdpModuleClock gCmdqMdpModuleClock;
#define IMP_ENABLE_MDP_HW_CLOCK(FN_NAME, HW_NAME)	\
uint32_t cmdq_mdp_enable_clock_##FN_NAME(bool enable)	\
{		\
	return cmdq_dev_enable_device_clock(enable,	\
		gCmdqMdpModuleClock.clk_##HW_NAME, #HW_NAME "-clk");	\
}
#define IMP_MDP_HW_CLOCK_IS_ENABLE(FN_NAME, HW_NAME)	\
bool cmdq_mdp_clock_is_enable_##FN_NAME(void)	\
{		\
	return cmdq_dev_device_clock_is_enable(		\
		gCmdqMdpModuleClock.clk_##HW_NAME);	\
}

IMP_ENABLE_MDP_HW_CLOCK(MDP_FG0, MDP_FG0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_PAD0, MDP_PAD0);
IMP_ENABLE_MDP_HW_CLOCK(WARP0_ASYNC_TX, WARP0_ASYNC_TX);
IMP_ENABLE_MDP_HW_CLOCK(MDP_MUTEX0, MDP_MUTEX0);
IMP_ENABLE_MDP_HW_CLOCK(VPP02VPP1_RELAY, VPP02VPP1_RELAY);
IMP_ENABLE_MDP_HW_CLOCK(VPP12VPP0_ASYNC, VPP12VPP0_ASYNC);
IMP_ENABLE_MDP_HW_CLOCK(MMSYSRAM_TOP, MMSYSRAM_TOP);
IMP_ENABLE_MDP_HW_CLOCK(MDP_AAL0, MDP_AAL0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ0, MDP_RSZ0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA0, MDP_RDMA0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT0, MDP_WROT0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_HDR0, MDP_HDR0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_TDSHP0, MDP_TDSHP0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_COLOR0, MDP_COLOR0);
IMP_ENABLE_MDP_HW_CLOCK(MDP_OVL0, MDP_OVL0);
IMP_ENABLE_MDP_HW_CLOCK(WARP0_RELAY, WARP0_RELAY);
IMP_ENABLE_MDP_HW_CLOCK(WARP0_MDP_DL_ASYNC, WARP0_MDP_DL_ASYNC);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT2, MDP_WROT2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_PAD2, MDP_PAD2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_WROT3, MDP_WROT3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_PAD3, MDP_PAD3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA2, MDP_RDMA2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_FG2, MDP_FG2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RDMA3, MDP_RDMA3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_FG3, MDP_FG3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ2, MDP_RSZ2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_MERGE2, MDP_MERGE2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_TDSHP2, MDP_TDSHP2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_COLOR2, MDP_COLOR2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_RSZ3, MDP_RSZ3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_MERGE3, MDP_MERGE3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_TDSHP3, MDP_TDSHP3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_COLOR3, MDP_COLOR3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_HDR2, MDP_HDR2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_AAL2, MDP_AAL2);
IMP_ENABLE_MDP_HW_CLOCK(MDP_HDR3, MDP_HDR3);
IMP_ENABLE_MDP_HW_CLOCK(MDP_AAL3, MDP_AAL3);
IMP_ENABLE_MDP_HW_CLOCK(DISP_MUTEX, DISP_MUTEX);
IMP_ENABLE_MDP_HW_CLOCK(VPP0_DL_ASYNC, VPP0_DL_ASYNC);
IMP_ENABLE_MDP_HW_CLOCK(VPP0_DL1_RELAY, VPP0_DL1_RELAY);
IMP_ENABLE_MDP_HW_CLOCK(TOP_CFG_VPP0, TOP_CFG_VPP0);
IMP_ENABLE_MDP_HW_CLOCK(TOP_CFG_26M_VPP0, TOP_CFG_26M_VPP0);
IMP_ENABLE_MDP_HW_CLOCK(TOP_CFG_VPP1, TOP_CFG_VPP1);
IMP_ENABLE_MDP_HW_CLOCK(TOP_CFG_26M_VPP1, TOP_CFG_26M_VPP1);



IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_FG0, MDP_FG0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_PAD0, MDP_PAD0);
IMP_MDP_HW_CLOCK_IS_ENABLE(WARP0_ASYNC_TX, WARP0_ASYNC_TX);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_MUTEX0, MDP_MUTEX0);
IMP_MDP_HW_CLOCK_IS_ENABLE(VPP02VPP1_RELAY, VPP02VPP1_RELAY);
IMP_MDP_HW_CLOCK_IS_ENABLE(VPP12VPP0_ASYNC, VPP12VPP0_ASYNC);
IMP_MDP_HW_CLOCK_IS_ENABLE(MMSYSRAM_TOP, MMSYSRAM_TOP);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_AAL0, MDP_AAL0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ0, MDP_RSZ0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA0, MDP_RDMA0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT0, MDP_WROT0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_HDR0, MDP_HDR0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_TDSHP0, MDP_TDSHP0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_COLOR0, MDP_COLOR0);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_OVL0, MDP_OVL0);
IMP_MDP_HW_CLOCK_IS_ENABLE(WARP0_RELAY, WARP0_RELAY);
IMP_MDP_HW_CLOCK_IS_ENABLE(WARP0_MDP_DL_ASYNC, WARP0_MDP_DL_ASYNC);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT2, MDP_WROT2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_PAD2, MDP_PAD2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_WROT3, MDP_WROT3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_PAD3, MDP_PAD3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA2, MDP_RDMA2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_FG2, MDP_FG2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RDMA3, MDP_RDMA3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_FG3, MDP_FG3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ2, MDP_RSZ2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_MERGE2, MDP_MERGE2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_TDSHP2, MDP_TDSHP2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_COLOR2, MDP_COLOR2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_RSZ3, MDP_RSZ3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_MERGE3, MDP_MERGE3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_TDSHP3, MDP_TDSHP3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_COLOR3, MDP_COLOR3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_HDR2, MDP_HDR2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_AAL2, MDP_AAL2);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_HDR3, MDP_HDR3);
IMP_MDP_HW_CLOCK_IS_ENABLE(MDP_AAL3, MDP_AAL3);
IMP_MDP_HW_CLOCK_IS_ENABLE(DISP_MUTEX, DISP_MUTEX);
IMP_MDP_HW_CLOCK_IS_ENABLE(VPP0_DL_ASYNC, VPP0_DL_ASYNC);
IMP_MDP_HW_CLOCK_IS_ENABLE(VPP0_DL1_RELAY, VPP0_DL1_RELAY);
IMP_MDP_HW_CLOCK_IS_ENABLE(TOP_CFG_VPP0, TOP_CFG_VPP0);
IMP_MDP_HW_CLOCK_IS_ENABLE(TOP_CFG_26M_VPP0, TOP_CFG_26M_VPP0);
IMP_MDP_HW_CLOCK_IS_ENABLE(TOP_CFG_VPP1, TOP_CFG_VPP1);
IMP_MDP_HW_CLOCK_IS_ENABLE(TOP_CFG_26M_VPP1, TOP_CFG_26M_VPP1);

#undef IMP_ENABLE_MDP_HW_CLOCK
#undef IMP_MDP_HW_CLOCK_IS_ENABLE

static const uint64_t gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = {
	CMDQ_ENG_ISP_GROUP_BITS,
	CMDQ_ENG_MDP_GROUP_BITS,
	CMDQ_ENG_DPE_GROUP_BITS,
	CMDQ_ENG_RSC_GROUP_BITS,
	CMDQ_ENG_GEPF_GROUP_BITS,
	CMDQ_ENG_WPE_GROUP_BITS,
	CMDQ_ENG_EAF_GROUP_BITS,
	CMDQ_ENG_OWE_GROUP_BITS,
	CMDQ_ENG_MFB_GROUP_BITS,
	CMDQ_ENG_FDVT_GROUP_BITS,
};

long cmdq_mdp_get_module_base_VA_MMSYS2_CONFIG(void)
{
	return gCmdqMdpModuleBaseVA.MMSYS2_CONFIG;
}

long cmdq_mdp_get_module_base_VA_MDP_RDMA0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA0;
}

long cmdq_mdp_get_module_base_VA_MDP_RDMA2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA2;
}

long cmdq_mdp_get_module_base_VA_MDP_RDMA3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RDMA3;
}

long cmdq_mdp_get_module_base_VA_MDP_FG0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_FG0;
}

long cmdq_mdp_get_module_base_VA_MDP_FG2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_FG2;
}

long cmdq_mdp_get_module_base_VA_MDP_FG3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_FG3;
}

long cmdq_mdp_get_module_base_VA_MDP_HDR0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_HDR0;
}

long cmdq_mdp_get_module_base_VA_MDP_HDR2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_HDR2;
}

long cmdq_mdp_get_module_base_VA_MDP_HDR3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_HDR3;
}

long cmdq_mdp_get_module_base_VA_MDP_AAL0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_AAL0;
}

long cmdq_mdp_get_module_base_VA_MDP_AAL2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_AAL2;
}

long cmdq_mdp_get_module_base_VA_MDP_AAL3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_AAL3;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ0;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ2;
}

long cmdq_mdp_get_module_base_VA_MDP_RSZ3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_RSZ3;
}

long cmdq_mdp_get_module_base_VA_MDP_MERGE2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_MERGE2;
}

long cmdq_mdp_get_module_base_VA_MDP_MERGE3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_MERGE3;
}

long cmdq_mdp_get_module_base_VA_MDP_TDSHP0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_TDSHP0;
}

long cmdq_mdp_get_module_base_VA_MDP_TDSHP2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_TDSHP2;
}

long cmdq_mdp_get_module_base_VA_MDP_TDSHP3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_TDSHP3;
}

long cmdq_mdp_get_module_base_VA_MDP_COLOR0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_COLOR0;
}

long cmdq_mdp_get_module_base_VA_MDP_COLOR2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_COLOR2;
}

long cmdq_mdp_get_module_base_VA_MDP_COLOR3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_COLOR3;
}

long cmdq_mdp_get_module_base_VA_MDP_OVL0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_OVL0;
}

long cmdq_mdp_get_module_base_VA_MDP_PAD0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_PAD0;
}

long cmdq_mdp_get_module_base_VA_MDP_PAD2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_PAD2;
}

long cmdq_mdp_get_module_base_VA_MDP_PAD3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_PAD3;
}

long cmdq_mdp_get_module_base_VA_MDP_WROT0(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT0;
}

long cmdq_mdp_get_module_base_VA_MDP_WROT2(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT2;
}

long cmdq_mdp_get_module_base_VA_MDP_WROT3(void)
{
	return gCmdqMdpModuleBaseVA.MDP_WROT3;
}

long cmdq_mdp_get_module_base_VA_MM_MUTEX(void)
{
	return gCmdqMdpModuleBaseVA.MM_MUTEX;
}

long cmdq_mdp_get_module_base_VA_MM_MUTEX2(void)
{
	return gCmdqMdpModuleBaseVA.MM_MUTEX2;
}

#define MMSYS_CONFIG_BASE	cmdq_mdp_get_module_base_VA_MMSYS_CONFIG()
#define MMSYS2_CONFIG_BASE  cmdq_mdp_get_module_base_VA_MMSYS2_CONFIG()
#define MDP_RDMA0_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA0()
#define MDP_RDMA2_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA2()
#define MDP_RDMA3_BASE		cmdq_mdp_get_module_base_VA_MDP_RDMA3()
#define MDP_FG0_BASE		cmdq_mdp_get_module_base_VA_MDP_FG0()
#define MDP_FG2_BASE		cmdq_mdp_get_module_base_VA_MDP_FG2()
#define MDP_FG3_BASE		cmdq_mdp_get_module_base_VA_MDP_FG3()
#define MDP_HDR0_BASE		cmdq_mdp_get_module_base_VA_MDP_HDR0()
#define MDP_HDR2_BASE		cmdq_mdp_get_module_base_VA_MDP_HDR2()
#define MDP_HDR3_BASE		cmdq_mdp_get_module_base_VA_MDP_HDR3()
#define MDP_AAL0_BASE		cmdq_mdp_get_module_base_VA_MDP_AAL0()
#define MDP_AAL2_BASE		cmdq_mdp_get_module_base_VA_MDP_AAL2()
#define MDP_AAL3_BASE		cmdq_mdp_get_module_base_VA_MDP_AAL3()
#define MDP_RSZ0_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ0()
#define MDP_RSZ2_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ2()
#define MDP_RSZ3_BASE		cmdq_mdp_get_module_base_VA_MDP_RSZ3()
#define MDP_MERGE2_BASE		cmdq_mdp_get_module_base_VA_MDP_MERGE2()
#define MDP_MERGE3_BASE		cmdq_mdp_get_module_base_VA_MDP_MERGE3()
#define MDP_TDSHP0_BASE		cmdq_mdp_get_module_base_VA_MDP_TDSHP0()
#define MDP_TDSHP2_BASE		cmdq_mdp_get_module_base_VA_MDP_TDSHP2()
#define MDP_TDSHP3_BASE		cmdq_mdp_get_module_base_VA_MDP_TDSHP3()
#define MDP_COLOR0_BASE		cmdq_mdp_get_module_base_VA_MDP_COLOR0()
#define MDP_COLOR2_BASE		cmdq_mdp_get_module_base_VA_MDP_COLOR2()
#define MDP_COLOR3_BASE		cmdq_mdp_get_module_base_VA_MDP_COLOR3()
#define MDP_OVL0_BASE		cmdq_mdp_get_module_base_VA_MDP_OVL0()
#define MDP_PAD0_BASE		cmdq_mdp_get_module_base_VA_MDP_PAD0()
#define MDP_PAD2_BASE		cmdq_mdp_get_module_base_VA_MDP_PAD2()
#define MDP_PAD3_BASE		cmdq_mdp_get_module_base_VA_MDP_PAD3()
#define MDP_WROT0_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT0()
#define MDP_WROT2_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT2()
#define MDP_WROT3_BASE		cmdq_mdp_get_module_base_VA_MDP_WROT3()
#define MM_MUTEX_BASE		cmdq_mdp_get_module_base_VA_MM_MUTEX()
#define MM_MUTEX2_BASE		cmdq_mdp_get_module_base_VA_MM_MUTEX2()

struct RegDef {
	int offset;
	const char *name;
};

void cmdq_mdp_dump_mmsys_config(const struct cmdqRecStruct *handle)
{
	int i = 0;
	uint32_t value = 0;

	static const struct RegDef vppsys0reg[] = {
		{0x020,	"VPPSYS0_CG_CON0"},
		{0x024,	"VPPSYS0_CG_SET0"},
		{0x028,	"VPPSYS0_CG_CLR0"},
		{0x02c,	"VPPSYS0_CG_CON1"},
		{0x030,	"VPPSYS0_CG_SET1"},
		{0x034,	"VPPSYS0_CG_CLR1"},
		{0x038,	"VPPSYS0_CG_CON2"},
		{0x03c,	"VPPSYS0_CG_SET2"},
		{0x040,	"VPPSYS0_CG_CLR2"},
		{0x044,	"VPPSYS0_CG_CON3"},
		{0x048,	"VPPSYS0_CG_SET3"},
		{0x04c,	"VPPSYS0_CG_CLR3"},
		{0x0c0,	"VPPSYS0_ASYNC_CFG0_WD"},
		{0x0c4,	"VPPSYS0_ASYNC_CFG0_OUT_RD"},
		{0x0c8,	"VPPSYS0_ASYNC_CFG0_IN_RD"},
		{0x0e4,	"VPPSYS0_RELAY0_CFG_WD"},
		{0x0e8,	"VPPSYS0_RELAY0_CNT_RD"},
		{0x104,	"VPPSYS0_DEBUG_OUT_SEL"},
		{0x108,	"VPPSYS0_SMI_BIST"},
		{0x12c,	"VPPSYS0_SMI_LARB4_GREQ"},
		{0x14c,	"VPPSYS0_BUF_UNDERRUN"},
		{0x150,	"VPPSYS0_BUF_UNDERRUN_ID0"},
		{0x154,	"VPPSYS0_BUF_UNDERRUN_ID1"},
		{0x158,	"VPPSYS0_BUF_UNDERRUN_ID2"},
		{0x15c,	"VPPSYS0_BUF_UNDERRUN_ID3"},
		{0x160,	"VPPSYS0_PWR_METER_CTL0"},
		{0x164,	"VPPSYS0_PWR_METER_CTL1"},
		{0x170,	"VPPSYS0_DL_VALID0"},
		{0x174,	"VPPSYS0_DL_READY0"},
		{0x178,	"VPPSYS0_DL_VALID1"},
		{0x17c,	"VPPSYS0_DL_READY1"},
		{0x184,	"VPPSYS0_MOUT_MASK"},
		{0xf04,	"VPPSYS0_PQ_SEL_IN"},
		{0xf08,	"VPPSYS0_VPP1_SEL_IN"},
		{0xf0c,	"VPPSYS0_HDR_SEL_IN"},
		{0xf10,	"VPPSYS0_TCC_SEL_IN"},
		{0xf14,	"VPPSYS0_WROT_SEL_IN"},
		{0xf18,	"VPPSYS0_AAL_SEL_IN"},
		{0xf1c,	"VPPSYS0_MDP_RDMA_SOUT_SEL_IN"},
		{0xf20,	"VPPSYS0_WARP0_SOUT_SEL_IN"},
		{0xf24,	"VPPSYS0_WARP1_SOUT_SEL_IN"},
		{0xf28,	"VPPSYS0_PQ_SOUT_SEL_IN"},
		{0xf2c,	"VPPSYS0_PADDING_SOUT_SEL_IN"},
		{0xf30,	"VPPSYS0_TCC_SOUT_SEL_IN"},
		{0xf34,	"VPPSYS0_VPP1_IN_SOUT_SEL_IN"},
		{0xf38,	"VPPSYS0_STITCH_MOUT_EN"},
		{0xf3c,	"VPPSYS0_WARP0_MOUT_EN"},
		{0xf40,	"VPPSYS0_WARP1_MOUT_EN"},
		{0xf44,	"VPPSYS0_FG_MOUT_EN"},
		{0xf48,	"VPPSYS0_RSZ_SEL_IN"},
		{0xf4C,	"VPPSYS0_RSZ_OUT_SEL_IN"},
		{0xf50,	"VPPSYS0_RSZ_IN_SOUT_SEL_IN"},
		{0xf54,	"VPPSYS0_RSZ_SOUT_SEL_IN"},
		{0xf58,	"VPPSYS0_DISP_RDMA_SOUT_SEL_IN"},
		{0xf5C,	"VPPSYS0_DISP_WDMA_SEL_IN"},

		{0x01c, "VPPSYS0_MISC"}
	};

	static const struct RegDef vppsys1reg[] = {
		{0x100,	"VPPSYS1_CG_CON0"},
		{0x104,	"VPPSYS1_CG_SET0"},
		{0x108,	"VPPSYS1_CG_CLR0"},
		{0x110,	"VPPSYS1_CG_CON1"},
		{0x114,	"VPPSYS1_CG_SET1"},
		{0x118,	"VPPSYS1_CG_CLR1"},
		{0x120,	"VPPSYS1_CG_CON2"},
		{0x124,	"VPPSYS1_CG_SET2"},
		{0x128,	"VPPSYS1_CG_CLR2"},
		{0x130,	"VPPSYS1_CG_CON3"},
		{0x134,	"VPPSYS1_CG_SET3"},
		{0x138,	"VPPSYS1_CG_CLR3"},
		{0x140,	"VPPSYS1_CG_CON4"},
		{0x144,	"VPPSYS1_CG_SET4"},
		{0x148,	"VPPSYS1_CG_CLR4"},
		{0x8dc,	"VPPSYS1_SMI_LARB_GREQ"},
		{0x900,	"VPPSYS1_PWR_METER_CTL0"},
		{0x904,	"VPPSYS1_PWR_METER_CTL1"},
		{0x920,	"VPP0_DL_IRELAY_WR"},
		{0x924,	"VPP0_DL_ORELAY_0_WR"},
		{0x928,	"VDO0_DL_ORELAY_0_WR"},
		{0x92c,	"VDO0_DL_ORELAY_1_WR"},
		{0x930,	"VDO1_DL_ORELAY_0_WR"},
		{0x934,	"VDO1_DL_ORELAY_1_WR"},
		{0x938,	"VPP0_DL_IRELAY_RD"},
		{0x93c,	"VPP0_DL_ORELAY_0_RD"},
		{0x940,	"VDO0_DL_ORELAY_0_RD"},
		{0x944,	"VDO0_DL_ORELAY_1_RD"},
		{0x948,	"VDO1_DL_ORELAY_0_RD"},
		{0x94c,	"VDO1_DL_ORELAY_1_RD"},
		{0x950,	"VPP_DL_ASYNC_CFG_RD0"},
		{0x954,	"VPP_DL_ASYNC_CFG_RD1"},
		{0xe00,	"VPPSYS1_BUF_UNDERRUN"},
		{0xe04,	"VPPSYS1_BUF_UNDERRUN_ID0"},
		{0xe08,	"VPPSYS1_BUF_UNDERRUN_ID1"},
		{0xe0c,	"VPPSYS1_BUF_UNDERRUN_ID2"},
		{0xe10,	"VPPSYS1_BUF_UNDERRUN_ID3"},
		{0xfd0,	"VPPSYS1_MOUT_MASK0"},
		{0xfd4,	"VPPSYS1_MOUT_MASK1"},
		{0xfd8,	"VPPSYS1_MOUT_MASK2"},
		{0xfe0,	"VPPSYS1_DL_VALID0"},
		{0xfe4,	"VPPSYS1_DL_VALID1"},
		{0xfe8,	"VPPSYS1_DL_VALID2"},
		{0xfec,	"VPPSYS1_DL_VALID3"},
		{0xff0,	"VPPSYS1_DL_READY0"},
		{0xff4,	"VPPSYS1_DL_READY1"},
		{0xff8,	"VPPSYS1_DL_READY2"},
		{0xffc,	"VPPSYS1_DL_READY3"},
		{0xf10,	"VPP_SPLIT_OUT0_SOUT_SEL"},
		{0xf14,	"VPP_SPLIT_OUT1_SOUT_SEL"},
		{0xf18,	"SVPP1_MDP_RDMA_SOUT_SEL"},
		{0xf1c,	"SVPP1_SRC_SEL_IN"},
		{0xf20,	"SVPP1_SRC_SEL_SOUT_SEL"},
		{0xf24,	"SVPP1_HDR_SRC_SEL_IN"},
		{0xf28,	"SVPP1_PATH_SOUT_SEL"},
		{0xf2c,	"SVPP1_WROT_SRC_SEL_IN"},
		{0xf30,	"SVPP1_TCC_SEL_IN"},
		{0xf34,	"SVPP1_TCC_SOUT_SEL"},
		{0xf38,	"SVPP2_SRC_SEL_IN"},
		{0xf3c,	"SVPP2_COLOR_SOUT_SEL"},
		{0xf40,	"SVPP2_WROT_SRC_SEL_IN"},
		{0xf44,	"SVPP2_RSZ_MERGE_IN_SEL_IN"},
		{0xf48,	"SVPP2_BUF_BF_RSZ_SWITCH"},
		{0xf4c,	"SVPP2_MDP_HDR_MOUT_EN"},
		{0xf50,	"SVPP2_SRC_SEL_MOUT_EN"},
		{0xf54,	"SVPP1_MDP_AAL_SEL_IN"},
		{0xf60,	"SVPP3_MDP_RDMA_SOUT_SEL"},
		{0xf64, "SVPP3_SRC_SEL_IN"},
		{0xf68, "SVPP3_COLOR_SOUT_SEL"},
		{0xf6c, "SVPP3_WROT_SRC_SEL_IN"},
		{0xf70, "SVPP3_RSZ_MERGE_IN_SEL_IN"},
		{0xf74, "SVPP3_BUF_BF_RSZ_SWITCH"},
		{0xf78, "SVPP3_MDP_HDR_MOUT_EN"},
		{0xf7c, "SVPP3_SRC_SEL_MOUT_EN"},
		{0xf80, "VPP0_DL1_SRC_SEL_IN"},
		{0xf8c, "VPP0_SRC_SOUT_SEL"},
		{0xf90, "SVPP2_MDP_RDMA_SOUT_SEL"},
		{0x0f0, "VPPSYS1_MISC"},
		{0x0f4, "VPPSYS1_MODULE_DBG"}
	};

	if (!MMSYS_CONFIG_BASE) {
		CMDQ_ERR("vppsys0 not porting\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(vppsys0reg); i++) {
		value = CMDQ_REG_GET32(MMSYS_CONFIG_BASE +
			vppsys0reg[i].offset);
		CMDQ_ERR("0x%04x %s: 0x%08x\n",
			vppsys0reg[i].offset,
			vppsys0reg[i].name,
			value);
	}

	if (!MMSYS2_CONFIG_BASE) {
		CMDQ_ERR("vppsys1 not porting\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(vppsys1reg); i++) {
		value = CMDQ_REG_GET32(MMSYS2_CONFIG_BASE +
			vppsys1reg[i].offset);
		CMDQ_ERR("0x%04x %s: 0x%08x\n",
			vppsys1reg[i].offset,
			vppsys1reg[i].name,
			value);
	}

	/*MMSYS_MUTEX MOD*/
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x030);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX0_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x034);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX0_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x050);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX1_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x054);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX1_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x070);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX2_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x074);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX2_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x090);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX3_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x094);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX3_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0B0);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX4_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0B4);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX4_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0D0);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX5_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0D4);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX5_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0F0);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX6_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x0F4);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX6_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x110);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX7_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX_BASE + 0x114);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS0_MUTEX7_MOD1", value);

	/*DISP_MUTEX MOD*/
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x030);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX0_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x034);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX0_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x050);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX1_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x054);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX1_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x070);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX2_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x074);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX2_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x090);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX3_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x094);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX3_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x0B0);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX4_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x0B4);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX4_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x0D0);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX5_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x0D4);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX5_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x0F0);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX6_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x0F4);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX6_MOD1", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x110);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX7_MOD0", value);
	value = CMDQ_REG_GET32(MM_MUTEX2_BASE + 0x114);
	CMDQ_ERR("%s: 0x%08x\n", "VPPSYS1_MUTEX7_MOD1", value);
}

int32_t cmdq_mdp_reset_with_mmsys(const uint64_t engineToResetAgain)
{
	long VPPSYS0_SW0_RST_B_REG = MMSYS_CONFIG_BASE + (0x1f0);
	long VPPSYS0_SW1_RST_B_REG = MMSYS_CONFIG_BASE + (0x1f4);
	long VPPSYS1_SW0_RST_B_REG = MMSYS2_CONFIG_BASE + (0x700);
	long VPPSYS1_SW1_RST_B_REG = MMSYS2_CONFIG_BASE + (0x704);
	int i = 0;
	uint64_t reset_bits0 = 0ULL;
	uint64_t reset_bits1 = 0ULL;
	int engineResetBit[60] = {
		-1,			/* bit  0 : RESERVED */
		CMDQ_ENG_MDP_FG0,	/* bit  1 : MDP_FG0 */
		-1,
		-1,			/* bit  3 : RESERVED  */
		-1,			/* bit  4 : RESERVED */
		-1,			/* bit  5 : RESERVED */
		-1,			/* bit  6 : RESERVED */
		CMDQ_ENG_MDP_PAD0,	/* bit  7 : MDP_PAD0 */
		-1,
		-1,			/* bit  9 : RESERVED  */
		CMDQ_ENG_MDP_CAMIN,	/* bit 10 : WARP0_ASYNC_TX */
		-1,
		-1,			/* bit 12 : RESERVED */
		-1,			/* bit 13 : MUTEX */
		CMDQ_ENG_MDP_VPP0_SOUT,	/* bit 14 : VPP02VPP1_RELAY */
		CMDQ_ENG_MDP_VPP1_SOUT,	/* bit 15 : VPP12VPP0_ASYNC */
		-1,			/* bit 16 : MMSYSRAM_TOP */
		CMDQ_ENG_MDP_AAL0,	/* bit 17 : MDP_AAL0 */
		CMDQ_ENG_MDP_RSZ0,	/* bit 18 : MDP_RSZ0  */
		-1,			/* bit 19 : RESERVED  */
		-1,			/* bit 20 : RESERVED */
		-1,			/* bit 21 : RESERVED */
		-1,			/* bit 22 : RESERVED */
		-1,			/* bit 23 : RESERVED */
		-1,			/* bit 24 : RESERVED */
		-1,			/* bit 25 : RESERVED  */
		-1,			/* bit 26 : RESERVED */
		-1,			/* bit 27 : RESERVED */
		-1,			/* bit 28 : RESERVED */
		-1,			/* bit 29 : RESERVED */
		-1,			/* bit 30 : RESERVED */
		-1,			/* bit 31 : RESERVED */
		-1,			/* bit 32 : RESERVED */
		-1,			/* bit 33 : RESERVED */
		-1,			/* bit 34 : RESERVED */
		-1,			/* bit 35 : RESERVED */
		-1,			/* bit 36 : RESERVED */
		-1,			/* bit 37 : RESERVED */
		-1,			/* bit 38 : RESERVED */
		-1,			/* bit 39 : RESERVED */
		-1,			/* bit 40 : RESERVED */
		-1,			/* bit 41 : RESERVED */
		-1,			/* bit 42 : RESERVED */
		-1,			/* bit 43 : RESERVED */
		CMDQ_ENG_MDP_RDMA0,	/* bit 44 : MDP_RDMA0 */
		CMDQ_ENG_MDP_WROT0,	/* bit 45 : MDP_WROT0 */
		-1,			/* bit 46 : RESERVED */
		-1,			/* bit 47 : RESERVED */
		-1,			/* bit 48 : SMI */
		-1,			/* bit 49 : SMI */
		-1,			/* bit 50 : SMI */
		-1,			/* bit 51 : SMI */
		-1,			/* bit 52 : SMI */
		-1,			/* bit 53 : SMI */
		-1,			/* bit 54 : SMI */
		-1,			/* bit 55 : FAKE_ENG*/
		CMDQ_ENG_MDP_HDR0,	/* bit 56 : MDP_HDR0 */
		CMDQ_ENG_MDP_TDSHP0,	/* bit 57 : MDP_TDSHP0 */
		CMDQ_ENG_MDP_COLOR0,	/* bit 58 : MDP_COLOR0 */
		CMDQ_ENG_MDP_OVL0,	/* bit 59 : MDP_OVL0 */
	};
	int engineResetVppsys1Bit[49] = {
		-1,	/* bit  0 : MDP_OVL1 */
		-1,	/* bit  1 : MDP_TCC1 */
		-1,	/* bit  2 : MDP_WROT1 */
		-1,	/* bit  3 : MDP_PAD1 */
		CMDQ_ENG_MDP_WROT2,	/* bit  4 : MDP_WROT2 */
		CMDQ_ENG_MDP_PAD2,	/* bit  5 : MDP_PAD2 */
		CMDQ_ENG_MDP_WROT3,	/* bit  6 : MDP_WROT3 */
		CMDQ_ENG_MDP_PAD3,	/* bit  7 : MDP_PAD3 */
		-1,	/* bit  8 : MDP_RDMA1 */
		-1,	/* bit  9 : MDP_FG1 */
		CMDQ_ENG_MDP_RDMA2,	/* bit 10 : MDP_RDMA2 */
		CMDQ_ENG_MDP_FG2,	/* bit 11 : MDP_FG2 */
		CMDQ_ENG_MDP_RDMA3,	/* bit 12 : MDP_RDMA3 */
		CMDQ_ENG_MDP_FG3,	/* bit 13 : MDP_FG3 */
		-1,		/* bit 14 : MDP_SPLIT */
		-1,			/* bit 15 : SVPP2_VDO0_DL_RELAY */
		-1,	/* bit 16 : MDP_TDSHP1 */
		-1,	/* bit 17 : MDP_COLOR1 */
		-1,			/* bit 18 : SVPP3_VDO1_DL_RELAY  */
		CMDQ_ENG_MDP_RSZ2,	/* bit 19 : MDP_MERGE2 */
		CMDQ_ENG_MDP_COLOR2,	/* bit 20 : MDP_COLOR2 */
		-1,			/* bit 21 : VPPSYS1_GALS */
		CMDQ_ENG_MDP_RSZ3,	/* bit 22 : MDP_MERGE3 */
		CMDQ_ENG_MDP_COLOR3,	/* bit 23 : MDP_COLOR3 */
		-1,			/* bit 24 : VPPSYS1_LARB*/
		-1,	/* bit 25 : MDP_RSZ1 */
		-1,	/* bit 26 : MDP_HDR1 */
		-1,	/* bit 27 : MDP_AAL1 */
		CMDQ_ENG_MDP_RSZ2,	/* bit 28 : MDP_RSZ2 */
		CMDQ_ENG_MDP_HDR2,	/* bit 29 : MDP_HDR2 */
		CMDQ_ENG_MDP_AAL2,	/* bit 30 : MDP_AAL2 */
		-1,			/* bit 31 : FAKE_ENG*/
		-1,			/* bit 32 : RESERVED*/
		CMDQ_ENG_MDP_HDR3,	/* bit  0 : MDP_HDR3 */
		CMDQ_ENG_MDP_AAL3,	/* bit  1 : MDP_AAL3 */
		-1,			/* bit  2 : SVPP2_VDO1_DL_RELAY */
		-1,			/* bit  3 : FAKE_ENG*/
		CMDQ_ENG_MDP_RSZ2,	/* bit  4 : MDP_RSZ2 */
		CMDQ_ENG_MDP_RSZ3,	/* bit  5 : MDP_RSZ3 */
		-1,			/* bit  6 : SVPP3_VDO0_DL_RELAY */
		-1,			/* bit  7 : DISP_MUTEX */
		CMDQ_ENG_MDP_TDSHP2,	/* bit  8 : MDP_TDSHP2 */
		CMDQ_ENG_MDP_TDSHP3,	/* bit  9 : MDP_TDSHP3 */
		-1,			/* bit 10 : VPP0_DL1_RELAY */
		-1,			/* bit 11 : HDMI_META */
		-1,		/* bit 12 : VPP_SPLIT_HDMI */
		-1,			/* bit 13 : DGI_IN */
		-1,			/* bit 14 : DGI_OUT */
		-1,		/* bit 15 : VPP_SPLIT_DGI */
	};

	for (i = 0; i < 32; ++i) {
		if (engineResetBit[i] < 0)
			continue;

		if (engineToResetAgain & (1LL << engineResetBit[i]))
			reset_bits0 |= (1 << i);
	}
	for (i = 32; i < 60; ++i) {
		if (engineResetBit[i] < 0)
			continue;

		if (engineToResetAgain & (1LL << engineResetBit[i]))
			reset_bits1 |= (1ULL << i);
	}

	if (reset_bits0 != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits0 = ~reset_bits0;

		CMDQ_REG_SET32(VPPSYS0_SW0_RST_B_REG, reset_bits0);
		CMDQ_REG_SET32(VPPSYS0_SW0_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}
	if (reset_bits1 != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits1 = ~reset_bits1;

		CMDQ_REG_SET32(VPPSYS0_SW1_RST_B_REG, reset_bits1);
		CMDQ_REG_SET32(VPPSYS0_SW1_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}

	reset_bits0 = 0ULL;
	reset_bits1 = 0ULL;

	for (i = 0; i < 32; ++i) {
		if (engineResetVppsys1Bit[i] < 0)
			continue;

		if (engineToResetAgain & (1LL << engineResetVppsys1Bit[i]))
			reset_bits0 |= (1 << i);
	}
	for (i = 32; i < 49; ++i) {
		if (engineResetVppsys1Bit[i] < 0)
			continue;

		if (engineToResetAgain & (1LL << engineResetVppsys1Bit[i]))
			reset_bits1 |= (1ULL << (i - 32));
	}

	if (reset_bits0 != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits0 = ~reset_bits0;

		CMDQ_REG_SET32(VPPSYS1_SW0_RST_B_REG, reset_bits0);
		CMDQ_REG_SET32(VPPSYS1_SW0_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}
	if (reset_bits1 != 0) {
		/* 0: reset */
		/* 1: not reset */
		/* so we need to reverse the bits */
		reset_bits1 = ~reset_bits1;

		CMDQ_REG_SET32(VPPSYS1_SW1_RST_B_REG, reset_bits1);
		CMDQ_REG_SET32(VPPSYS1_SW1_RST_B_REG, ~0);
		/* This takes effect immediately, no need to poll state */
	}

	return 0;
}

int cmdq_TranslationFault_callback(
	int port, unsigned int mva, void *data)
{
	char dispatchModel[MDP_DISPATCH_KEY_STR_LEN] = "MDP";

	CMDQ_ERR("======== [Translation] Dump Begin ========\n");
	CMDQ_ERR("[Translation]fault call port=%d, mva=0x%x", port, mva);

	cmdq_core_dump_tasks_info();

	switch (port) {
	case M4U_PORT_L4_MDP_RDMA:
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");
		break;
	case M4U_PORT_L5_SVPP2_MDP_RDMA:
		cmdq_mdp_dump_rdma(MDP_RDMA2_BASE, "RDMA2");
		break;
	case M4U_PORT_L6_SVPP3_MDP_RDMA:
		cmdq_mdp_dump_rdma(MDP_RDMA3_BASE, "RDMA3");
		break;
	case M4U_PORT_L4_MDP_WROT:
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");
		break;
	case M4U_PORT_L5_SVPP2_MDP_WROT:
		cmdq_mdp_dump_rot(MDP_WROT2_BASE, "WROT2");
		break;
	case M4U_PORT_L6_SVPP3_MDP_WROT:
		cmdq_mdp_dump_rot(MDP_WROT3_BASE, "WROT3");
		break;
	default:
		CMDQ_ERR("[MDP M4U]fault callback function");
		break;
	}

	CMDQ_ERR("======== [Translation] Frame Information Begin ========\n");
	/* find dispatch module and assign dispatch key */
	cmdq_mdp_check_TF_address(mva, dispatchModel);
	memcpy(data, dispatchModel, sizeof(dispatchModel));
	CMDQ_ERR("======== [Translation] Frame Information End ========\n");
	CMDQ_ERR("======== [Translation] Dump End ========\n");

	return 0;
}

void cmdq_mdp_init_module_base_VA(void)
{
	/* dts node: "mediatek,mdp" */
	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));

	gCmdqMdpModuleBaseVA.MMSYS2_CONFIG =
		cmdq_dev_alloc_reference_VA_by_name("mmsys2_config");
	gCmdqMdpModuleBaseVA.MDP_RDMA0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma0");
	gCmdqMdpModuleBaseVA.MDP_RDMA2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma2");
	gCmdqMdpModuleBaseVA.MDP_RDMA3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rdma3");
	gCmdqMdpModuleBaseVA.MDP_FG0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_fg0");
	gCmdqMdpModuleBaseVA.MDP_FG2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_fg2");
	gCmdqMdpModuleBaseVA.MDP_FG3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_fg3");
	gCmdqMdpModuleBaseVA.MDP_HDR0 =
		cmdq_dev_alloc_reference_by_name("mdp_hdr0",
		&mdp_module_pa.hdr0);
	gCmdqMdpModuleBaseVA.MDP_HDR2 =
		cmdq_dev_alloc_reference_by_name("mdp_hdr2",
		&mdp_module_pa.hdr2);
	gCmdqMdpModuleBaseVA.MDP_HDR3 =
		cmdq_dev_alloc_reference_by_name("mdp_hdr3",
		&mdp_module_pa.hdr3);
	gCmdqMdpModuleBaseVA.MDP_AAL0 =
		cmdq_dev_alloc_reference_by_name("mdp_aal0",
		&mdp_module_pa.aal0);
	gCmdqMdpModuleBaseVA.MDP_AAL2 =
		cmdq_dev_alloc_reference_by_name("mdp_aal2",
		&mdp_module_pa.aal2);
	gCmdqMdpModuleBaseVA.MDP_AAL3 =
		cmdq_dev_alloc_reference_by_name("mdp_aal3",
		&mdp_module_pa.aal3);
	gCmdqMdpModuleBaseVA.MDP_RSZ0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz0");
	gCmdqMdpModuleBaseVA.MDP_RSZ2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz2");
	gCmdqMdpModuleBaseVA.MDP_RSZ3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_rsz3");
	gCmdqMdpModuleBaseVA.MDP_MERGE2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_merge2");
	gCmdqMdpModuleBaseVA.MDP_MERGE3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_merge3");
	gCmdqMdpModuleBaseVA.MDP_TDSHP0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_tdshp0");
	gCmdqMdpModuleBaseVA.MDP_TDSHP2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_tdshp2");
	gCmdqMdpModuleBaseVA.MDP_TDSHP3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_tdshp3");
	gCmdqMdpModuleBaseVA.MDP_COLOR0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_color0");
	gCmdqMdpModuleBaseVA.MDP_COLOR2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_color2");
	gCmdqMdpModuleBaseVA.MDP_COLOR3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_color3");
	gCmdqMdpModuleBaseVA.MDP_OVL0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_ovl0");
	gCmdqMdpModuleBaseVA.MDP_PAD0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_pad0");
	gCmdqMdpModuleBaseVA.MDP_PAD2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_pad2");
	gCmdqMdpModuleBaseVA.MDP_PAD3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_pad3");
	gCmdqMdpModuleBaseVA.MDP_WROT0 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot0");
	gCmdqMdpModuleBaseVA.MDP_WROT2 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot2");
	gCmdqMdpModuleBaseVA.MDP_WROT3 =
		cmdq_dev_alloc_reference_VA_by_name("mdp_wrot3");
	gCmdqMdpModuleBaseVA.MM_MUTEX =
		cmdq_dev_alloc_reference_VA_by_name("mm_mutex");
	gCmdqMdpModuleBaseVA.MM_MUTEX2 =
		cmdq_dev_alloc_reference_VA_by_name("mm_mutex2");
}

void cmdq_mdp_deinit_module_base_VA(void)
{
	cmdq_dev_free_module_base_VA(
			cmdq_mdp_get_module_base_VA_MMSYS2_CONFIG());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RDMA3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_FG0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_FG2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_FG3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_HDR0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_HDR2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_HDR3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_AAL0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_AAL2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_AAL3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_RSZ3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_TDSHP0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_TDSHP2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_TDSHP3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_COLOR0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_COLOR2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_COLOR3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_OVL0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_PAD0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_PAD2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_PAD3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT0());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT2());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MDP_WROT3());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MM_MUTEX());
	cmdq_dev_free_module_base_VA(cmdq_mdp_get_module_base_VA_MM_MUTEX2());

	memset(&gCmdqMdpModuleBaseVA, 0, sizeof(struct CmdqMdpModuleBaseVA));
}

bool cmdq_mdp_clock_is_on(u32 engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_CAMIN:
		return cmdq_mdp_clock_is_enable_WARP0_ASYNC_TX() &&
			cmdq_mdp_clock_is_enable_WARP0_RELAY() &&
			cmdq_mdp_clock_is_enable_WARP0_MDP_DL_ASYNC();
	case CMDQ_ENG_MDP_RDMA0:
		return cmdq_mdp_clock_is_enable_MDP_RDMA0();
	case CMDQ_ENG_MDP_RDMA2:
		return cmdq_mdp_clock_is_enable_MDP_RDMA2();
	case CMDQ_ENG_MDP_RDMA3:
		return cmdq_mdp_clock_is_enable_MDP_RDMA3();
	case CMDQ_ENG_MDP_FG0:
		return cmdq_mdp_clock_is_enable_MDP_FG0();
	case CMDQ_ENG_MDP_FG2:
		return cmdq_mdp_clock_is_enable_MDP_FG2();
	case CMDQ_ENG_MDP_FG3:
		return cmdq_mdp_clock_is_enable_MDP_FG3();
	case CMDQ_ENG_MDP_HDR0:
		return cmdq_mdp_clock_is_enable_MDP_HDR0();
	case CMDQ_ENG_MDP_HDR2:
		return cmdq_mdp_clock_is_enable_MDP_HDR2();
	case CMDQ_ENG_MDP_HDR3:
		return cmdq_mdp_clock_is_enable_MDP_HDR3();
	case CMDQ_ENG_MDP_AAL0:
		return cmdq_mdp_clock_is_enable_MDP_AAL0();
	case CMDQ_ENG_MDP_AAL2:
		return cmdq_mdp_clock_is_enable_MDP_AAL2();
	case CMDQ_ENG_MDP_AAL3:
		return cmdq_mdp_clock_is_enable_MDP_AAL3();
	case CMDQ_ENG_MDP_RSZ0:
		return cmdq_mdp_clock_is_enable_MDP_RSZ0();
	case CMDQ_ENG_MDP_RSZ2:
		return cmdq_mdp_clock_is_enable_MDP_RSZ2();
	case CMDQ_ENG_MDP_RSZ3:
		return cmdq_mdp_clock_is_enable_MDP_RSZ3();
	case CMDQ_ENG_MDP_TDSHP0:
		return cmdq_mdp_clock_is_enable_MDP_TDSHP0();
	case CMDQ_ENG_MDP_TDSHP2:
		return cmdq_mdp_clock_is_enable_MDP_TDSHP2();
	case CMDQ_ENG_MDP_TDSHP3:
		return cmdq_mdp_clock_is_enable_MDP_TDSHP3();
	case CMDQ_ENG_MDP_COLOR0:
		return cmdq_mdp_clock_is_enable_MDP_COLOR0();
	case CMDQ_ENG_MDP_COLOR2:
		return cmdq_mdp_clock_is_enable_MDP_COLOR2();
	case CMDQ_ENG_MDP_COLOR3:
		return cmdq_mdp_clock_is_enable_MDP_COLOR3();
	case CMDQ_ENG_MDP_OVL0:
		return cmdq_mdp_clock_is_enable_MDP_OVL0();
	case CMDQ_ENG_MDP_PAD0:
		return cmdq_mdp_clock_is_enable_MDP_PAD0();
	case CMDQ_ENG_MDP_PAD2:
		return cmdq_mdp_clock_is_enable_MDP_PAD2();
	case CMDQ_ENG_MDP_PAD3:
		return cmdq_mdp_clock_is_enable_MDP_PAD3();
	case CMDQ_ENG_MDP_WROT0:
		return cmdq_mdp_clock_is_enable_MDP_WROT0();
	case CMDQ_ENG_MDP_WROT2:
		return cmdq_mdp_clock_is_enable_MDP_WROT2();
	case CMDQ_ENG_MDP_WROT3:
		return cmdq_mdp_clock_is_enable_MDP_WROT3();
	case CMDQ_ENG_MDP_VPP0_SOUT:
		return (cmdq_mdp_clock_is_enable_VPP02VPP1_RELAY() &&
			cmdq_mdp_clock_is_enable_VPP0_DL_ASYNC());
		break;
	case CMDQ_ENG_MDP_VPP1_SOUT:
		return cmdq_mdp_clock_is_enable_VPP0_DL1_RELAY() &&
			cmdq_mdp_clock_is_enable_VPP12VPP0_ASYNC();
	default:
		CMDQ_ERR("try to query unknown mdp engine[%d] clock\n", engine);
		return false;
	}
}

void cmdq_mdp_enable_clock(bool enable, u32 engine)
{
	switch (engine) {
	case CMDQ_ENG_MDP_CAMIN:
		cmdq_mdp_enable_clock_WARP0_ASYNC_TX(enable);
		cmdq_mdp_enable_clock_WARP0_RELAY(enable);
		cmdq_mdp_enable_clock_WARP0_MDP_DL_ASYNC(enable);
		break;
	case CMDQ_ENG_MDP_RDMA0:
		cmdq_mdp_enable_clock_MDP_RDMA0(enable);
		break;
	case CMDQ_ENG_MDP_RDMA2:
		cmdq_mdp_enable_clock_MDP_RDMA2(enable);
		break;
	case CMDQ_ENG_MDP_RDMA3:
		cmdq_mdp_enable_clock_MDP_RDMA3(enable);
		break;
	case CMDQ_ENG_MDP_FG0:
		cmdq_mdp_enable_clock_MDP_FG0(enable);
		break;
	case CMDQ_ENG_MDP_FG2:
		cmdq_mdp_enable_clock_MDP_FG2(enable);
		break;
	case CMDQ_ENG_MDP_FG3:
		cmdq_mdp_enable_clock_MDP_FG3(enable);
		break;
	case CMDQ_ENG_MDP_HDR0:
		cmdq_mdp_enable_clock_MDP_HDR0(enable);
		break;
	case CMDQ_ENG_MDP_HDR2:
		cmdq_mdp_enable_clock_MDP_HDR2(enable);
		break;
	case CMDQ_ENG_MDP_HDR3:
		cmdq_mdp_enable_clock_MDP_HDR3(enable);
		break;
	case CMDQ_ENG_MDP_AAL0:
		cmdq_mdp_enable_clock_MDP_AAL0(enable);
		break;
	case CMDQ_ENG_MDP_AAL2:
		cmdq_mdp_enable_clock_MDP_AAL2(enable);
		break;
	case CMDQ_ENG_MDP_AAL3:
		cmdq_mdp_enable_clock_MDP_AAL3(enable);
		break;
	case CMDQ_ENG_MDP_RSZ0:
		cmdq_mdp_enable_clock_MDP_RSZ0(enable);
		break;
	case CMDQ_ENG_MDP_RSZ2:
		cmdq_mdp_enable_clock_MDP_RSZ2(enable);
		cmdq_mdp_enable_clock_MDP_MERGE2(enable);
		break;
	case CMDQ_ENG_MDP_RSZ3:
		cmdq_mdp_enable_clock_MDP_RSZ3(enable);
		cmdq_mdp_enable_clock_MDP_MERGE3(enable);
		break;
	case CMDQ_ENG_MDP_TDSHP0:
		cmdq_mdp_enable_clock_MDP_TDSHP0(enable);
		break;
	case CMDQ_ENG_MDP_TDSHP2:
		cmdq_mdp_enable_clock_MDP_TDSHP2(enable);
		break;
	case CMDQ_ENG_MDP_TDSHP3:
		cmdq_mdp_enable_clock_MDP_TDSHP3(enable);
		break;
	case CMDQ_ENG_MDP_COLOR0:
		cmdq_mdp_enable_clock_MDP_COLOR0(enable);
		break;
	case CMDQ_ENG_MDP_COLOR2:
		cmdq_mdp_enable_clock_MDP_COLOR2(enable);
		break;
	case CMDQ_ENG_MDP_COLOR3:
		cmdq_mdp_enable_clock_MDP_COLOR3(enable);
		break;
	case CMDQ_ENG_MDP_OVL0:
		cmdq_mdp_enable_clock_MDP_OVL0(enable);
		break;
	case CMDQ_ENG_MDP_PAD0:
		cmdq_mdp_enable_clock_MDP_PAD0(enable);
		break;
	case CMDQ_ENG_MDP_PAD2:
		cmdq_mdp_enable_clock_MDP_PAD2(enable);
		break;
	case CMDQ_ENG_MDP_PAD3:
		cmdq_mdp_enable_clock_MDP_PAD3(enable);
		break;
	case CMDQ_ENG_MDP_WROT0:
		cmdq_mdp_enable_clock_MDP_WROT0(enable);
		break;
	case CMDQ_ENG_MDP_WROT2:
		cmdq_mdp_enable_clock_MDP_WROT2(enable);
		break;
	case CMDQ_ENG_MDP_WROT3:
		cmdq_mdp_enable_clock_MDP_WROT3(enable);
		break;
	/* vpp02vpp1 */
	case CMDQ_ENG_MDP_VPP0_SOUT:
		cmdq_mdp_enable_clock_VPP02VPP1_RELAY(enable);
		cmdq_mdp_enable_clock_VPP0_DL_ASYNC(enable);
		break;
	/* vpp12vpp0 */
	case CMDQ_ENG_MDP_VPP1_SOUT:
		cmdq_mdp_enable_clock_VPP0_DL1_RELAY(enable);
		cmdq_mdp_enable_clock_VPP12VPP0_ASYNC(enable);
		break;
	default:
		CMDQ_ERR("try to enable unknown mdp engine[%d] clock\n",
			engine);
		break;
	}
}

/* Common Clock Framework */
void cmdq_mdp_init_module_clk(void)
{
	/* vppsys0 */
	cmdq_dev_get_module_clock_by_name("mdp_fg0", "MDP_FG0",
	    &gCmdqMdpModuleClock.clk_MDP_FG0);
	cmdq_dev_get_module_clock_by_name("mdp_pad0", "MDP_PAD0",
		&gCmdqMdpModuleClock.clk_MDP_PAD0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "WARP0_ASYNC_TX",
		&gCmdqMdpModuleClock.clk_WARP0_ASYNC_TX);
	cmdq_dev_get_module_clock_by_name("mm_mutex", "MDP_MUTEX0",
		&gCmdqMdpModuleClock.clk_MDP_MUTEX0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "VPP02VPP1_RELAY",
		&gCmdqMdpModuleClock.clk_VPP02VPP1_RELAY);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "VPP12VPP0_ASYNC",
		&gCmdqMdpModuleClock.clk_VPP12VPP0_ASYNC);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "MMSYSRAM_TOP",
		&gCmdqMdpModuleClock.clk_MMSYSRAM_TOP);
	cmdq_dev_get_module_clock_by_name("mdp_aal0", "MDP_AAL0",
		&gCmdqMdpModuleClock.clk_MDP_AAL0);
	cmdq_dev_get_module_clock_by_name("mdp_rsz0", "MDP_RSZ0",
		&gCmdqMdpModuleClock.clk_MDP_RSZ0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "MDP_RDMA0",
		&gCmdqMdpModuleClock.clk_MDP_RDMA0);
	cmdq_dev_get_module_clock_by_name("mdp_wrot0", "MDP_WROT0",
		&gCmdqMdpModuleClock.clk_MDP_WROT0);
	cmdq_dev_get_module_clock_by_name("mdp_hdr0", "MDP_HDR0",
		&gCmdqMdpModuleClock.clk_MDP_HDR0);
	cmdq_dev_get_module_clock_by_name("mdp_tdshp0", "MDP_TDSHP0",
		&gCmdqMdpModuleClock.clk_MDP_TDSHP0);
	cmdq_dev_get_module_clock_by_name("mdp_color0", "MDP_COLOR0",
		&gCmdqMdpModuleClock.clk_MDP_COLOR0);
	cmdq_dev_get_module_clock_by_name("mdp_ovl0", "MDP_OVL0",
		&gCmdqMdpModuleClock.clk_MDP_OVL0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "WARP0_RELAY",
		&gCmdqMdpModuleClock.clk_WARP0_RELAY);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "WARP0_MDP_DL_ASYNC",
		&gCmdqMdpModuleClock.clk_WARP0_MDP_DL_ASYNC);

	/* vppsys1 */
	cmdq_dev_get_module_clock_by_name("mdp_wrot2", "MDP_WROT2",
		&gCmdqMdpModuleClock.clk_MDP_WROT2);
	cmdq_dev_get_module_clock_by_name("mdp_pad2", "MDP_PAD2",
		&gCmdqMdpModuleClock.clk_MDP_PAD2);
	cmdq_dev_get_module_clock_by_name("mdp_wrot3", "MDP_WROT3",
		&gCmdqMdpModuleClock.clk_MDP_WROT3);
	cmdq_dev_get_module_clock_by_name("mdp_pad3", "MDP_PAD3",
		&gCmdqMdpModuleClock.clk_MDP_PAD3);
	cmdq_dev_get_module_clock_by_name("mdp_rdma2", "MDP_RDMA2",
		&gCmdqMdpModuleClock.clk_MDP_RDMA2);
	cmdq_dev_get_module_clock_by_name("mdp_fg2", "MDP_FG2",
		&gCmdqMdpModuleClock.clk_MDP_FG2);
	cmdq_dev_get_module_clock_by_name("mdp_rdma3", "MDP_RDMA3",
		&gCmdqMdpModuleClock.clk_MDP_RDMA3);
	cmdq_dev_get_module_clock_by_name("mdp_fg3", "MDP_FG3",
		&gCmdqMdpModuleClock.clk_MDP_FG3);
	cmdq_dev_get_module_clock_by_name("mdp_rsz2", "MDP_RSZ2",
		&gCmdqMdpModuleClock.clk_MDP_RSZ2);
	cmdq_dev_get_module_clock_by_name("mdp_merge2", "MDP_MERGE2",
		&gCmdqMdpModuleClock.clk_MDP_MERGE2);
	cmdq_dev_get_module_clock_by_name("mdp_tdshp2", "MDP_TDSHP2",
		&gCmdqMdpModuleClock.clk_MDP_TDSHP2);
	cmdq_dev_get_module_clock_by_name("mdp_color2", "MDP_COLOR2",
		&gCmdqMdpModuleClock.clk_MDP_COLOR2);
	cmdq_dev_get_module_clock_by_name("mdp_rsz3", "MDP_RSZ3",
			&gCmdqMdpModuleClock.clk_MDP_RSZ3);
	cmdq_dev_get_module_clock_by_name("mdp_merge3", "MDP_MERGE3",
			&gCmdqMdpModuleClock.clk_MDP_MERGE3);
	cmdq_dev_get_module_clock_by_name("mdp_tdshp3", "MDP_TDSHP3",
				&gCmdqMdpModuleClock.clk_MDP_TDSHP3);
	cmdq_dev_get_module_clock_by_name("mdp_color3", "MDP_COLOR3",
			&gCmdqMdpModuleClock.clk_MDP_COLOR3);
	cmdq_dev_get_module_clock_by_name("mdp_hdr2", "MDP_HDR2",
		&gCmdqMdpModuleClock.clk_MDP_HDR2);
	cmdq_dev_get_module_clock_by_name("mdp_aal2", "MDP_AAL2",
		&gCmdqMdpModuleClock.clk_MDP_AAL2);
	cmdq_dev_get_module_clock_by_name("mdp_hdr3", "MDP_HDR3",
		&gCmdqMdpModuleClock.clk_MDP_HDR3);
	cmdq_dev_get_module_clock_by_name("mdp_aal3", "MDP_AAL3",
		&gCmdqMdpModuleClock.clk_MDP_AAL3);
	cmdq_dev_get_module_clock_by_name("mm_mutex2", "DISP_MUTEX",
		&gCmdqMdpModuleClock.clk_DISP_MUTEX);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0",
		"VPP0_DL_ASYNC",
		&gCmdqMdpModuleClock.clk_VPP0_DL_ASYNC);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "VPP0_DL1_RELAY",
		&gCmdqMdpModuleClock.clk_VPP0_DL1_RELAY);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "TOP_CFG_VPP0",
		&gCmdqMdpModuleClock.clk_TOP_CFG_VPP0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "TOP_CFG_26M_VPP0",
		&gCmdqMdpModuleClock.clk_TOP_CFG_26M_VPP0);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "TOP_CFG_VPP1",
		&gCmdqMdpModuleClock.clk_TOP_CFG_VPP1);
	cmdq_dev_get_module_clock_by_name("mdp_rdma0", "TOP_CFG_26M_VPP1",
		&gCmdqMdpModuleClock.clk_TOP_CFG_26M_VPP1);
}

/* MDP engine dump */
void cmdq_mdp_dump_rsz(const unsigned long base, const char *label)
{

	uint32_t value[45] = { 0 };
	uint32_t request[4] = { 0 };
	uint32_t state = 0;

	value[0] = CMDQ_REG_GET32(base + 0x000);  /* RSZ_ENABLE */
	value[1] = CMDQ_REG_GET32(base + 0x004);  /* RSZ_CONTROL_1 */
	value[2] = CMDQ_REG_GET32(base + 0x008);  /* RSZ_CONTROL_2 */
	value[3] = CMDQ_REG_GET32(base + 0x010);  /* RSZ_INPUT_IMAGE */
	value[4] = CMDQ_REG_GET32(base + 0x014);  /* RSZ_OUTPUT_IMAGE */
	/* RSZ_HORIZONTAL_COEFF_STEP */
	value[5] = CMDQ_REG_GET32(base + 0x018);
	/* RSZ_VERTICAL_COEFF_STEP */
	value[6] = CMDQ_REG_GET32(base + 0x01C);
	CMDQ_REG_SET32(base + 0x044, 0x00000001);
	value[7] = CMDQ_REG_GET32(base + 0x048);  /* RSZ_DEBUG_1 */
	CMDQ_REG_SET32(base + 0x044, 0x00000002);
	value[8] = CMDQ_REG_GET32(base + 0x048);  /* RSZ_DEBUG_2 */
	CMDQ_REG_SET32(base + 0x044, 0x00000003);
	value[9] = CMDQ_REG_GET32(base + 0x048);  /* RSZ_DEBUG_3 */
	CMDQ_REG_SET32(base + 0x044, 0x00000004);
	value[10] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_4 */
	CMDQ_REG_SET32(base + 0x044, 0x00000005);
	value[11] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_5 */
	CMDQ_REG_SET32(base + 0x044, 0x00000006);
	value[12] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_6 */
	CMDQ_REG_SET32(base + 0x044, 0x00000007);
	value[13] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_7 */
	CMDQ_REG_SET32(base + 0x044, 0x00000008);
	value[14] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_8 */
	CMDQ_REG_SET32(base + 0x044, 0x00000009);
	value[15] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_9 */
	CMDQ_REG_SET32(base + 0x044, 0x0000000A);
	value[16] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_10 */
	CMDQ_REG_SET32(base + 0x044, 0x0000000B);
	value[17] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_11 */
	CMDQ_REG_SET32(base + 0x044, 0x0000000C);
	value[18] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_12 */
	CMDQ_REG_SET32(base + 0x044, 0x0000000D);
	value[19] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_13 */
	CMDQ_REG_SET32(base + 0x044, 0x0000000E);
	value[20] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_14 */
	CMDQ_REG_SET32(base + 0x044, 0x0000000F);
	value[21] = CMDQ_REG_GET32(base + 0x048); /* RSZ_DEBUG_15 */
	value[22] = CMDQ_REG_GET32(base + 0x100); /* PAT1_GEN_SET */
	value[23] = CMDQ_REG_GET32(base + 0x200); /* PAT2_GEN_SET */
	value[24] = CMDQ_REG_GET32(base + 0x00C); /* RSZ_INT_FLAG */
	/* RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET */
	value[25] = CMDQ_REG_GET32(base + 0x020);
	/* RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET */
	value[26] = CMDQ_REG_GET32(base + 0x024);
	/* RSZ_LUMA_VERTICAL_INTEGER_OFFSET */
	value[27] = CMDQ_REG_GET32(base + 0x028);
	/* RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET */
	value[28] = CMDQ_REG_GET32(base + 0x02C);
	/* RSZ_CHROMA_HORIZONTAL_INTEGER_OFFSET */
	value[29] = CMDQ_REG_GET32(base + 0x030);
	/* RSZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET  */
	value[30] = CMDQ_REG_GET32(base + 0x034);
	value[31] = CMDQ_REG_GET32(base + 0x040); /* RSZ_RSV */
	value[32] = CMDQ_REG_GET32(base + 0x04C); /* RSZ_TAP_ADAPT */
	value[33] = CMDQ_REG_GET32(base + 0x050); /* RSZ_IBSE_SOFTCLIP */
	value[34] = CMDQ_REG_GET32(base + 0x22C); /* RSZ_ETC_CONTROL */
	value[35] = CMDQ_REG_GET32(base + 0x230); /* RSZ_ETC_SWITCH_MAX_MIN_1 */
	value[36] = CMDQ_REG_GET32(base + 0x234); /* RSZ_ETC_SWITCH_MAX_MIN_2 */
	value[37] = CMDQ_REG_GET32(base + 0x238); /* RSZ_ETC_RING_CONTROL */
	/* RSZ_ETC_RING_CONTROL_GAINCONTROL_1	*/
	value[38] = CMDQ_REG_GET32(base + 0x23C);
	/* RSZ_ETC_RING_CONTROL_GAINCONTROL_2	*/
	value[39] = CMDQ_REG_GET32(base + 0x240);
	/* RSZ_ETC_RING_CONTROL_GAINCONTROL_3	*/
	value[40] = CMDQ_REG_GET32(base + 0x244);
	/* RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_1 */
	value[41] = CMDQ_REG_GET32(base + 0x248);
	/* RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_2 */
	value[42] = CMDQ_REG_GET32(base + 0x24C);
	/* RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_3 */
	value[43] = CMDQ_REG_GET32(base + 0x250);
	value[44] = CMDQ_REG_GET32(base + 0x254); /* RSZ_ETC_BLEND */

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"RSZ_ENABLE: 0x%08x, RSZ_CONTROL_1: 0x%08x, RSZ_CONTROL_2: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"RSZ_INPUT_IMAGE: 0x%08x, RSZ_OUTPUT_IMAGE: 0x%08x\n",
		value[3],  value[4]);
	CMDQ_ERR(
		"RSZ_HORIZONTAL_COEFF_STEP: 0x%08x, RSZ_VERTICAL_COEFF_STEP: 0x%08x\n",
		value[5], value[6]);
	CMDQ_ERR(
		"RSZ_DEBUG_1: 0x%08x, RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
		value[7], value[8], value[9]);
	CMDQ_ERR(
		"RSZ_DEBUG_4: 0x%08x, RSZ_DEBUG_5: 0x%08x, RSZ_DEBUG_6: 0x%08x\n",
		value[10], value[11], value[12]);
	CMDQ_ERR(
		"RSZ_DEBUG_7: 0x%08x, RSZ_DEBUG_8: 0x%08x, RSZ_DEBUG_9: 0x%08x\n",
		value[13], value[14], value[15]);
	CMDQ_ERR(
		"RSZ_DEBUG_10: 0x%08x, RSZ_DEBUG_11: 0x%08x, RSZ_DEBUG_12: 0x%08x\n",
		value[16], value[17], value[18]);
	CMDQ_ERR(
		"RSZ_DEBUG_13: 0x%08x, RSZ_DEBUG_14: 0x%08x, RSZ_DEBUG_15: 0x%08x\n",
		value[19], value[20], value[21]);
	CMDQ_ERR(
		"PAT1_GEN_SET: 0x%08x, PAT2_GEN_SET: 0x%08x\n",
		value[22], value[23]);
	CMDQ_ERR(
		"RSZ_INT_FLAG: 0x%08x, RSZ_LUMA_HORIZONTAL_INTEGER_OFFSET: 0x%08x, RSZ_LUMA_HORIZONTAL_SUBPIXEL_OFFSET: 0x%08x\n",
		value[24], value[25], value[26]);
	CMDQ_ERR(
		"RSZ_LUMA_VERTICAL_INTEGER_OFFSET: 0x%08x, RSZ_LUMA_VERTICAL_SUBPIXEL_OFFSET: 0x%08x, RSZ_CHROMA_HORIZONTAL_INTEGER_OFFSET: 0x%08x\n",
		value[27], value[28], value[29]);
	CMDQ_ERR(
		"RSZ_CHROMA_HORIZONTAL_SUBPIXEL_OFFSET: 0x%08x, RSZ_RSV: 0x%08x, RSZ_TAP_ADAPT: 0x%08x\n",
		value[30], value[31], value[32]);
	CMDQ_ERR(
		"RSZ_IBSE_SOFTCLIP: 0x%08x, RSZ_ETC_CONTROL: 0x%08x, RSZ_ETC_SWITCH_MAX_MIN_1: 0x%08x\n",
		value[33], value[34], value[35]);
	CMDQ_ERR(
		"RSZ_ETC_SWITCH_MAX_MIN_2: 0x%08x, RSZ_ETC_RING_CONTROL: 0x%08x, RSZ_ETC_RING_CONTROL_GAINCONTROL_1: 0x%08x\n",
		value[36], value[37], value[38]);
	CMDQ_ERR(
		"RSZ_ETC_RING_CONTROL_GAINCONTROL_2: 0x%08x, RSZ_ETC_RING_CONTROL_GAINCONTROL_3: 0x%08x, RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_1: 0x%08x\n",
		value[39], value[40], value[41]);
	CMDQ_ERR(
		"RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_2: 0x%08x, RSZ_ETC_SIMILARITY_PROTECTION_GAINCONTROL_3: 0x%08x, RSZ_ETC_BLEND: 0x%08x\n",
		value[42], value[43], value[44]);
	/* parse state */
	/* .valid=1/request=1: upstream module sends data */
	/* .ready=1: downstream module receives data */
	state = value[8] & 0xF;
	request[0] = state & (0x1);	/* out valid */
	request[1] = (state & (0x1 << 1)) >> 1;	/* out ready */
	request[2] = (state & (0x1 << 2)) >> 2;	/* in valid */
	request[3] = (state & (0x1 << 3)) >> 3;	/* in ready */
	CMDQ_ERR("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)\n",
		request[3], request[2], request[1], request[0],
		cmdq_mdp_get_rsz_state(state));
}

void cmdq_mdp_dump_merge(const unsigned long base, const char *label)
{
	u32 value[12] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x000);
	value[1] = CMDQ_REG_GET32(base + 0x010);
	value[2] = CMDQ_REG_GET32(base + 0x020);
	value[3] = CMDQ_REG_GET32(base + 0x040);
	value[4] = CMDQ_REG_GET32(base + 0x070);
	value[5] = CMDQ_REG_GET32(base + 0x074);
	value[6] = CMDQ_REG_GET32(base + 0x100);
	value[7] = CMDQ_REG_GET32(base + 0x104);
	value[8] = CMDQ_REG_GET32(base + 0x108);
	value[9] = CMDQ_REG_GET32(base + 0x120);
	value[10] = CMDQ_REG_GET32(base + 0x124);
	value[11] = CMDQ_REG_GET32(base + 0x128);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"MERGE_ENABLE: 0x%08x\n",
		 value[0]);
	CMDQ_ERR(
		"MERGE_CFG_0: 0x%08x, MERGE_CFG_4: 0x%08x\n",
		value[1], value[2]);
	CMDQ_ERR(
		"MERGE_CFG_12: 0x%08x\n",
		 value[3]);
	CMDQ_ERR(
		"MERGE_CFG_24: 0x%08x, MERGE_CFG_25: 0x%08x\n",
		value[4], value[5]);
	CMDQ_ERR(
		"MERGE_MON_0: 0x%08x, MERGE_MON_1: 0x%08x, MERGE_MON_2: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR(
		"MERGE_MON_4: 0x%08x, MERGE_MON_5: 0x%08x, MERGE_MON_6: 0x%08x\n",
		value[9], value[10], value[11]);
}

void cmdq_mdp_dump_tdshp(const unsigned long base, const char *label)
{
	uint32_t value[10] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x114);
	value[1] = CMDQ_REG_GET32(base + 0x11C);
	value[2] = CMDQ_REG_GET32(base + 0x104);
	value[3] = CMDQ_REG_GET32(base + 0x108);
	value[4] = CMDQ_REG_GET32(base + 0x10C);
	value[5] = CMDQ_REG_GET32(base + 0x110);
	value[6] = CMDQ_REG_GET32(base + 0x120);
	value[7] = CMDQ_REG_GET32(base + 0x124);
	value[8] = CMDQ_REG_GET32(base + 0x128);
	value[9] = CMDQ_REG_GET32(base + 0x12C);
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("TDSHP INPUT_CNT: 0x%08x, OUTPUT_CNT: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR("TDSHP INTEN: 0x%08x, INTSTA: 0x%08x, STATUS: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("TDSHP CFG: 0x%08x, IN_SIZE: 0x%08x, OUT_SIZE: 0x%08x\n",
		value[5], value[6], value[8]);
	CMDQ_ERR("TDSHP OUTPUT_OFFSET: 0x%08x, BLANK_WIDTH: 0x%08x\n",
		value[7], value[9]);
}
void cmdq_mdp_dump_aal(const unsigned long base, const char *label)
{
	uint32_t value[9] = { 0 };
	u32 sram_cfg, sram_addr, sram_status;

	value[0] = CMDQ_REG_GET32(base + 0x00C);    /* MDP_AAL_INTSTA       */
	value[1] = CMDQ_REG_GET32(base + 0x010);    /* MDP_AAL_STATUS       */
	value[2] = CMDQ_REG_GET32(base + 0x024);    /* MDP_AAL_INPUT_COUNT  */
	value[3] = CMDQ_REG_GET32(base + 0x028);    /* MDP_AAL_OUTPUT_COUNT */
	value[4] = CMDQ_REG_GET32(base + 0x030);    /* MDP_AAL_SIZE         */
	value[5] = CMDQ_REG_GET32(base + 0x034);    /* MDP_AAL_OUTPUT_SIZE  */
	value[6] = CMDQ_REG_GET32(base + 0x038);    /* MDP_AAL_OUTPUT_OFFSET*/
	value[7] = CMDQ_REG_GET32(base + 0x4EC);    /* MDP_AAL_TILE_00      */
	value[8] = CMDQ_REG_GET32(base + 0x4F0);    /* MDP_AAL_TILE_01      */

	sram_cfg = CMDQ_REG_GET32(base + 0x0C4);
	sram_addr = CMDQ_REG_GET32(base + 0x0D4);
	sram_status = CMDQ_REG_GET32(base + 0x0C8);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("AAL_INTSTA: 0x%08x, AAL_STATUS: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR(
		"AAL_INPUT_COUNT: 0x%08x, AAL_OUTPUT_COUNT: 0x%08x, AAL_SIZE: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("AAL_OUTPUT_SIZE: 0x%08x, AAL_OUTPUT_OFFSET: 0x%08x\n",
		value[5], value[6]);
	CMDQ_ERR("AAL_TILE_00: 0x%08x, AAL_TILE_01: 0x%08x\n",
		value[7], value[8]);
	CMDQ_ERR("MDP_AAL_SRAM_CFG:%#x MDP_AAL_SRAM_RW_IF_2:%#x MDP_AAL_SRAM_STATUS:%#x\n",
		sram_cfg, sram_addr, sram_status);
}
void cmdq_mdp_dump_hdr(const unsigned long base, const char *label)
{
	uint32_t value[15] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x000);    /* MDP_HDR_TOP            */
	value[1] = CMDQ_REG_GET32(base + 0x004);    /* MDP_HDR_RELAY          */
	value[2] = CMDQ_REG_GET32(base + 0x00C);    /* MDP_HDR_INTSTA         */
	value[3] = CMDQ_REG_GET32(base + 0x010);    /* MDP_HDR_ENGSTA         */
	value[4] = CMDQ_REG_GET32(base + 0x020);    /* MDP_HDR_HIST_CTRL_0    */
	value[5] = CMDQ_REG_GET32(base + 0x024);    /* MDP_HDR_HIST_CTRL_1    */
	value[6] = CMDQ_REG_GET32(base + 0x014);    /* MDP_HDR_SIZE_0         */
	value[7] = CMDQ_REG_GET32(base + 0x018);    /* MDP_HDR_SIZE_1         */
	value[8] = CMDQ_REG_GET32(base + 0x01C);    /* MDP_HDR_SIZE_2         */
	value[9] = CMDQ_REG_GET32(base + 0x0EC);    /* MDP_HDR_CURSOR_CTRL    */
	value[10] = CMDQ_REG_GET32(base + 0x0F0);   /* MDP_HDR_CURSOR_POS     */
	value[11] = CMDQ_REG_GET32(base + 0x0F4);   /* MDP_HDR_CURSOR_COLOR   */
	value[12] = CMDQ_REG_GET32(base + 0x0F8);   /* MDP_HDR_TILE_POS       */
	value[13] = CMDQ_REG_GET32(base + 0x0FC);   /* MDP_HDR_CURSOR_BUF0    */
	value[14] = CMDQ_REG_GET32(base + 0x100);   /* MDP_HDR_CURSOR_BUF1    */
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("HDR_TOP: 0x%08x, HDR_RELAY: 0x%08x, HDR_INTSTA: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"HDR_ENGSTA: 0x%08x, HDR_HIST_CTRL0: 0x%08x, HDR_HIST_CTRL1: 0x%08x\n",
		value[3], value[4], value[5]);
	CMDQ_ERR("HDR_SIZE_0: 0x%08x, HDR_SIZE_1: 0x%08x, HDR_SIZE_2: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR(
		"HDR_CURSOR_CTRL: 0x%08x, HDR_CURSOR_POS: 0x%08x, HDR_CURSOR_COLOR: 0x%08x\n",
		value[9], value[10], value[11]);
	CMDQ_ERR(
		"HDR_TILE_POS: 0x%08x, HDR_CURSOR_BUF0: 0x%08x, HDR_CURSOR_BUF1: 0x%08x\n",
		value[12], value[13], value[14]);
}
void cmdq_mdp_dump_tcc(const unsigned long base, const char *label)
{
	uint32_t value[8] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x00000000);	/* MDP_TCC_CTRL	      */
	value[1] = CMDQ_REG_GET32(base + 0x00000520);	/* MDP_TCC_DEBUG      */
	value[2] = CMDQ_REG_GET32(base + 0x00000524);   /* MDP_TCC_INTEN      */
	value[3] = CMDQ_REG_GET32(base + 0x00000528);	/* MDP_TCC_INTST      */
	value[4] = CMDQ_REG_GET32(base + 0x0000052C);	/* MDP_TCC_ST         */
	value[5] = CMDQ_REG_GET32(base + 0x00000530);	/* MDP_TCC_CROP_X     */
	value[6] = CMDQ_REG_GET32(base + 0x00000534);	/* MDP_TCC_CROP_Y     */
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x300, 0x060B);   /* SS */
	value[7] = CMDQ_REG_GET32(
			MMSYS_CONFIG_BASE + 0x048); /* MDPSYS_MODULE_DBG   */

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"MDP_TCC_CTRL: 0x%08x, MDP_TCC_DEBUG: 0x%08x, MDP_TCC_INTEN: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR("MDP_TCC_INTST: 0x%08x, MDP_TCC_ST: 0x%08x\n",
		value[3], value[4]);
	CMDQ_ERR("MDP_TCC_CROP_X: 0x%08x, MDP_TCC_CROP_Y: 0x%08x\n",
		value[5], value[6]);
	CMDQ_ERR("MDP_TCC_DEBUG: 0x%08x\n", value[7]);
}

struct reg_dump
{
	char *regName;
	int offset;
};

void cmdq_mdp_dump_fg(const unsigned long base, const char *label)
{
	struct reg_dump regs[] = {
		{"MDP_FG_TRIGGER", 0x000},
		{"MDP_FG_STATUS", 0x004},
		{"MDP_FG_LUMA_TBL_BASE", 0x008},
		{"MDP_FG_CB_TBL_BASE", 0x00C},
		{"MDP_FG_CR_TBL_BASE", 0x010},
		{"MDP_FG_LUT_BASE", 0x014},
		{"MDP_FG_FG_CTRL_0", 0x020},
		{"MDP_FG_FG_CK_EN", 0x024},
		{"MDP_FG_BACK_DOOR_0", 0x02C},
		{"MDP_FG_PIC_INFO_0", 0x400},
		{"MDP_FG_PIC_INFO_1", 0x404},
		{"MDP_FG_PPS_0", 0x408},
		{"MDP_FG_PPS_1", 0x40C},
		{"MDP_FG_PPS_2", 0x410},
		{"MDP_FG_PPS_3", 0x414},
		{"MDP_FG_TILE_INFO_0", 0x418},
		{"MDP_FG_TILE_INFO_1", 0x41C},
		{"MDP_FG_DEBUG_0", 0x500},
		{"MDP_FG_DEBUG_1", 0x504},
		{"MDP_FG_DEBUG_2", 0x508},
		{"MDP_FG_DEBUG_3", 0x50C},
		{"MDP_FG_DEBUG_4", 0x510},
		{"MDP_FG_DEBUG_5", 0x514},
		{"MDP_FG_DEBUG_6", 0x518},
	};
	int i = 0;
	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);

	for (i = 0; i < ARRAY_SIZE(regs); i++)
		CMDQ_ERR("[0x%03X]%s:\t0x%08x\n",
			regs[i].offset,
			regs[i].regName,
			CMDQ_REG_GET32(base + regs[i].offset));
}
int32_t cmdqMdpClockOn(uint64_t engineFlag)
{
	CMDQ_MSG("Enable MDP(0x%llx) clock begin\n", engineFlag);

#ifdef CMDQ_PWR_AWARE
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_CAMIN);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RDMA3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_FG0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_FG2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_FG3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_HDR0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_HDR2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_HDR3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_AAL0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_AAL2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_AAL3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_RSZ3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TDSHP3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_COLOR0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_COLOR2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_COLOR3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_OVL0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_PAD0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_PAD2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_PAD3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT0);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT2);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_WROT3);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_PQ0_SOUT);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TO_WARP0MOUT);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TO_SVPP2MOUT);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_TO_SVPP3MOUT);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_VPP0_SOUT);
	cmdq_mdp_enable(engineFlag, CMDQ_ENG_MDP_VPP1_SOUT);
#else
	CMDQ_MSG("Force MDP clock all on\n");

	/* enable all bits in MMSYS_CG_CLR0 and MMSYS_CG_CLR1 */
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x28, 0xFFFFFFFF);
	CMDQ_REG_SET32(MMSYS_CONFIG_BASE + 0x34, 0xFFFFFFFF);

	CMDQ_REG_SET32(MMSYS2_CONFIG_BASE + 0x108, 0xFFFFFFFF);
	CMDQ_REG_SET32(MMSYS2_CONFIG_BASE + 0x118, 0xFFFFFFFF);
#endif /* #ifdef CMDQ_PWR_AWARE */

	CMDQ_MSG("Enable MDP(0x%llx) clock end\n", engineFlag);

	return 0;
}

struct MODULE_BASE {
	uint64_t engine;
	/* considering 64 bit kernel, use long type to store base addr */
	long base;
	const char *name;
};

static uint32_t cmdq_mdp_qos_translate_port(uint32_t engineId)
{
	switch (engineId) {
	case CMDQ_ENG_MDP_RDMA0:
		return mdp_engine_port[ENGBASE_MDP_RDMA0];
	case CMDQ_ENG_MDP_RDMA2:
		return mdp_engine_port[ENGBASE_MDP_RDMA2];
	case CMDQ_ENG_MDP_RDMA3:
		return mdp_engine_port[ENGBASE_MDP_RDMA3];
	case CMDQ_ENG_MDP_WROT0:
		return mdp_engine_port[ENGBASE_MDP_WROT0];
	case CMDQ_ENG_MDP_WROT2:
		return mdp_engine_port[ENGBASE_MDP_WROT2];
	case CMDQ_ENG_MDP_WROT3:
		return mdp_engine_port[ENGBASE_MDP_WROT3];
	}

	if (engineId != CMDQ_ENG_MDP_CAMIN)
		CMDQ_ERR("pmqos invalid engineId %d\n", engineId);
	return 0;
}

#define MDP_ICC_GET(port) do { \
	path_##port[thread_id] = of_mtk_icc_get(dev, #port);		\
	if (!path_##port[thread_id])					\
		CMDQ_ERR("%s port:%s icc fail\n", __func__, #port);	\
} while (0)

static void mdp_qos_init(struct platform_device *pdev, u32 thread_id)
{
	struct device *dev = &pdev->dev;

	CMDQ_MSG("%s thread %u\n", __func__, thread_id);

	MDP_ICC_GET(mdp_rdma0);
	MDP_ICC_GET(mdp_rdma2);
	MDP_ICC_GET(mdp_rdma3);

	MDP_ICC_GET(mdp_wrot0);
	MDP_ICC_GET(mdp_wrot2);
	MDP_ICC_GET(mdp_wrot3);
}

static void *mdp_qos_get_path(u32 thread_id, u32 port)
{
	if (!port)
		return NULL;

	switch (port) {
	/* mdp part */
	case M4U_PORT_L4_MDP_RDMA:
		return path_mdp_rdma0[thread_id];
	case M4U_PORT_L5_SVPP2_MDP_RDMA:
		return path_mdp_rdma2[thread_id];
	case M4U_PORT_L6_SVPP3_MDP_RDMA:
		return path_mdp_rdma3[thread_id];

	case M4U_PORT_L4_MDP_WROT:
		return path_mdp_wrot0[thread_id];
	case M4U_PORT_L5_SVPP2_MDP_WROT:
		return path_mdp_wrot2[thread_id];
	case M4U_PORT_L6_SVPP3_MDP_WROT:
		return path_mdp_wrot3[thread_id];
	}

	CMDQ_ERR("%s pmqos invalid port %d\n", __func__, port);
	return NULL;
}

static void mdp_qos_clear_all(u32 thread_id)
{
	mtk_icc_set_bw(path_mdp_rdma0[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_rdma2[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_rdma3[thread_id], 0, 0);

	mtk_icc_set_bw(path_mdp_wrot0[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_wrot2[thread_id], 0, 0);
	mtk_icc_set_bw(path_mdp_wrot3[thread_id], 0, 0);
}

static u32 mdp_get_group_max(void)
{
	return CMDQ_MAX_GROUP_COUNT;
}

static u32 mdp_get_group_isp_plat(void)
{
	return CMDQ_GROUP_ISP;
}

static u32 mdp_get_group_mdp(void)
{
	return CMDQ_GROUP_MDP;
}

static u32 mdp_get_group_wpe_plat(void)
{
	return CMDQ_GROUP_WPE;
}

static const char **const mdp_get_engine_group_name(void)
{
	static const char *const engineGroupName[] = {
		CMDQ_FOREACH_GROUP(GENERATE_STRING)
	};

	return (const char **const)engineGroupName;
}

#define DEFINE_MODULE(eng, base) {eng, base, #eng}

int32_t cmdqMdpDumpInfo(uint64_t engineFlag, int logLevel)
{
	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0))
		cmdq_mdp_dump_rdma(MDP_RDMA0_BASE, "RDMA0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA2))
		cmdq_mdp_dump_rdma(MDP_RDMA2_BASE, "RDMA2");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA3))
		cmdq_mdp_dump_rdma(MDP_RDMA3_BASE, "RDMA3");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG0))
		cmdq_mdp_dump_fg(MDP_FG0_BASE, "FG0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG2))
		cmdq_mdp_dump_fg(MDP_FG2_BASE, "FG2");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG3))
		cmdq_mdp_dump_fg(MDP_FG3_BASE, "FG3");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0))
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ0_BASE, "RSZ0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ2)) {
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ2_BASE, "RSZ2");
		cmdq_mdp_get_func()->mdpDumpMerge(MDP_MERGE2_BASE, "MERGE2");
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ3)) {
		cmdq_mdp_get_func()->mdpDumpRsz(MDP_RSZ3_BASE, "RSZ3");
		cmdq_mdp_get_func()->mdpDumpMerge(MDP_MERGE3_BASE, "MERGE3");
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_TDSHP0_BASE, "TDSHP0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP2))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_TDSHP2_BASE, "TDSHP2");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP3))
		cmdq_mdp_get_func()->mdpDumpTdshp(MDP_TDSHP3_BASE, "TDSHP3");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0))
		cmdq_mdp_dump_color(MDP_COLOR0_BASE, "COLOR0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR2))
		cmdq_mdp_dump_color(MDP_COLOR2_BASE, "COLOR2");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR3))
		cmdq_mdp_dump_color(MDP_COLOR3_BASE, "COLOR3");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0))
		cmdq_mdp_dump_rot(MDP_WROT0_BASE, "WROT0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT2))
		cmdq_mdp_dump_rot(MDP_WROT2_BASE, "WROT2");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT3))
		cmdq_mdp_dump_rot(MDP_WROT3_BASE, "WROT3");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR0))
		cmdq_mdp_dump_hdr(MDP_HDR0_BASE, "HDR0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR2))
		cmdq_mdp_dump_hdr(MDP_HDR2_BASE, "HDR2");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR3))
		cmdq_mdp_dump_hdr(MDP_HDR3_BASE, "HDR3");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL0))
		cmdq_mdp_dump_aal(MDP_AAL0_BASE, "AAL0");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL2))
		cmdq_mdp_dump_aal(MDP_AAL2_BASE, "AAL2");

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL3))
		cmdq_mdp_dump_aal(MDP_AAL3_BASE, "AAL3");

	/* verbose case, dump entire 1KB HW register region */
	/* for each enabled HW module. */
	if (logLevel >= 1) {
		int inner = 0;

		const struct MODULE_BASE bases[] = {
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA2, MDP_RDMA2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RDMA3, MDP_RDMA3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ0, MDP_RSZ0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ2, MDP_RSZ2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ3, MDP_RSZ3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ2, MDP_MERGE2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_RSZ3, MDP_MERGE3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP2, MDP_TDSHP2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_TDSHP3, MDP_TDSHP3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR0, MDP_COLOR0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR2, MDP_COLOR2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_COLOR3, MDP_COLOR3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_FG0, MDP_FG0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_FG2, MDP_FG2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_FG3, MDP_FG3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_HDR0, MDP_HDR0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_HDR2, MDP_HDR2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_HDR3, MDP_HDR3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT0, MDP_WROT0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT2, MDP_WROT2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_WROT3, MDP_WROT3_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_AAL0, MDP_AAL0_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_AAL2, MDP_AAL2_BASE),
			DEFINE_MODULE(CMDQ_ENG_MDP_AAL3, MDP_AAL3_BASE),
		};

		for (inner = 0; inner < ARRAY_SIZE(bases); ++inner) {
			if (engineFlag & (1LL << bases[inner].engine)) {
				CMDQ_ERR(
					"========= [CMDQ] %s dump base 0x%lx ========\n",
					bases[inner].name, bases[inner].base);
				print_hex_dump(KERN_ERR, "",
					DUMP_PREFIX_ADDRESS, 32, 4,
					(void *)bases[inner].base, 1024,
					false);
			}
		}
	}

	return 0;
}

enum VPPSYS0_MOUT_BITS {
	MOUT_BITS_STITCH = 0,  /* bit  0: STITCH multiple outupt reset */
	MOUT_BITS_WARP0  = 1,  /* bit  1: WARP0 multiple outupt reset */
	MOUT_BITS_WARP1  = 2,  /* bit  2: WARP1 multiple outupt reset */
	MOUT_BITS_FG0    = 3,  /* bit  3: FG0 multiple outupt reset */
};

#ifdef TODO
enum VPPSYS1_MOUT_BITS {
};
#endif

int32_t cmdqMdpResetEng(uint64_t engineFlag)
{
#ifndef CMDQ_PWR_AWARE
	return 0;
#else
	int status = 0;
	int64_t engineToResetAgain = 0LL;
	uint32_t mout_bits_old = 0L;
	uint32_t mout_bits = 0L;

	long MMSYS_MOUT_RST_REG = MMSYS_CONFIG_BASE + (0xF00);

	CMDQ_PROF_START(0, "MDP_Rst");
	CMDQ_VERBOSE("Reset MDP(0x%llx) begin\n", engineFlag);

	/* After resetting each component, */
	/* we need also reset corresponding MOUT config. */
	mout_bits_old = CMDQ_REG_GET32(MMSYS_MOUT_RST_REG);
	mout_bits = 0;

	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		mout_bits |= (1 << MOUT_BITS_WARP0);
		cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG0)) {
		mout_bits |= (1 << MOUT_BITS_FG0);
		engineToResetAgain |= (1LL << CMDQ_ENG_MDP_FG0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_RDMA0,
			MDP_RDMA0_BASE + 0x8, MDP_RDMA0_BASE + 0x408,
			0x7FF00, 0x100, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA2)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_RDMA2,
			MDP_RDMA2_BASE + 0x8, MDP_RDMA2_BASE + 0x408,
			0x7FF00, 0x100, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA2);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA3)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_RDMA3,
			MDP_RDMA3_BASE + 0x8, MDP_RDMA3_BASE + 0x408,
			0x7FF00, 0x100, false);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_RDMA3);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP2)) {
			CMDQ_REG_SET32(MDP_TDSHP2_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP2_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP2_BASE + 0x100, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP3)) {
			CMDQ_REG_SET32(MDP_TDSHP3_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP3_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP3_BASE + 0x100, 0x0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0x010, MDP_WROT0_BASE + 0x014,
			0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT0);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT2)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT2,
			MDP_WROT2_BASE + 0x010, MDP_WROT2_BASE + 0x014,
			0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT2);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT3)) {
		status = cmdq_mdp_loop_reset(CMDQ_ENG_MDP_WROT3,
			MDP_WROT3_BASE + 0x010, MDP_WROT3_BASE + 0x014,
			0x1, 0x1, true);
		if (status != 0)
			engineToResetAgain |= (1LL << CMDQ_ENG_MDP_WROT3);
	}

	// TODO: AAL HDR TCC

	/*
	 * when MDP engines fail to reset,
	 * 1. print SMI debug log
	 * 2. try resetting from MMSYS to restore system state
	 * 3. report to QA by raising AEE warning
	 * this reset will reset all registers to power on state.
	 * but DpFramework always reconfigures register values,
	 * so there is no need to backup registers.
	 */
	if (engineToResetAgain != 0) {
		/* check SMI state immediately */
		/* if (1 == is_smi_larb_busy(0)) */
		/* { */
		/* smi_hanging_debug(5); */
		/* } */

		CMDQ_ERR(
			"Reset failed MDP engines(0x%llx), reset again with MMSYS_SW0_RST_B\n",
			 engineToResetAgain);

		cmdq_mdp_reset_with_mmsys(engineToResetAgain);
		cmdqMdpDumpInfo(engineToResetAgain, 1);

		/* finally, raise AEE warning to report normal reset fail. */
		/* we hope that reset MMSYS. */
		CMDQ_AEE("MDP", "Disable 0x%llx engine failed\n",
			engineToResetAgain);

		status = -EFAULT;
	}
	/* MOUT configuration reset */
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old | mout_bits));
	CMDQ_REG_SET32(MMSYS_MOUT_RST_REG, (mout_bits_old & (~mout_bits)));

	CMDQ_MSG("Reset MDP(0x%llx) end\n", engineFlag);
	CMDQ_PROF_END(0, "MDP_Rst");

	return status;

#endif				/* #ifdef CMDQ_PWR_AWARE */

}

int32_t cmdqMdpClockOff(uint64_t engineFlag)
{
	CMDQ_MSG("Disable MDP(0x%llx) clock begin\n", engineFlag);

#ifdef CMDQ_PWR_AWARE
	if (engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_CAMIN)) {
			cmdq_mdp_reset_with_mmsys((1LL << CMDQ_ENG_MDP_CAMIN));
			CMDQ_MSG("Disable WPEI clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_CAMIN);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_RDMA0, MDP_RDMA0_BASE + 0x008,
			MDP_RDMA0_BASE + 0x408, 0x7FF00, 0x100, false);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA2)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_RDMA2, MDP_RDMA2_BASE + 0x008,
			MDP_RDMA2_BASE + 0x408, 0x7FF00, 0x100, false);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RDMA3)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_RDMA3, MDP_RDMA3_BASE + 0x008,
			MDP_RDMA3_BASE + 0x408, 0x7FF00, 0x100, false);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_FG0)) {
			CMDQ_MSG("Disable MDP_FG0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_FG0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_FG2)) {
			CMDQ_MSG("Disable MDP_FG2 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_FG2);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_FG3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_FG3)) {
			CMDQ_MSG("Disable MDP_FG3 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_FG3);
		}
	}

	/* HDR */
	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_HDR0)) {
			CMDQ_MSG("Disable MDP_HDR0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_HDR0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_HDR2)) {
			CMDQ_MSG("Disable MDP_HDR2 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_HDR2);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_HDR3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_HDR3)) {
			CMDQ_MSG("Disable MDP_HDR3 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_HDR3);
		}
	}

	/* AAL */
	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_AAL0)) {
			CMDQ_MSG("Disable MDP_AAL0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_AAL0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_AAL2)) {
			CMDQ_MSG("Disable MDP_AAL2 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_AAL2);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_AAL3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_AAL3)) {
			CMDQ_MSG("Disable MDP_AAL3 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_AAL3);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ0)) {
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ0_BASE, 0x0);

			CMDQ_MSG("Disable MDP_RSZ0 clock\n");

			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ2)) {
			CMDQ_REG_SET32(MDP_RSZ2_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ2_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ2_BASE, 0x0);

			CMDQ_MSG("Disable MDP_RSZ2 clock\n");

			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ2);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_RSZ3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_RSZ3)) {
			CMDQ_REG_SET32(MDP_RSZ3_BASE, 0x0);
			CMDQ_REG_SET32(MDP_RSZ3_BASE, 0x10000);
			CMDQ_REG_SET32(MDP_RSZ3_BASE, 0x0);

			CMDQ_MSG("Disable MDP_RSZ3 clock\n");

			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_RSZ3);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP0)) {
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP0_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP2)) {
			CMDQ_REG_SET32(MDP_TDSHP2_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP2_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP2_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP2 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP2);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_TDSHP3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_TDSHP3)) {
			CMDQ_REG_SET32(MDP_TDSHP3_BASE + 0x100, 0x0);
			CMDQ_REG_SET32(MDP_TDSHP3_BASE + 0x100, 0x2);
			CMDQ_REG_SET32(MDP_TDSHP3_BASE + 0x100, 0x0);
			CMDQ_MSG("Disable MDP_TDSHP3 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_TDSHP3);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR0)) {
			CMDQ_MSG("Disable MDP_COLOR0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR2)) {
			CMDQ_MSG("Disable MDP_COLOR2 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR2);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_COLOR3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_COLOR3)) {
			CMDQ_MSG("Disable MDP_COLOR3 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_COLOR3);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_OVL0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_OVL0)) {
			CMDQ_MSG("Disable MDP_OVL0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_OVL0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_PAD0)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_PAD0)) {
			CMDQ_MSG("Disable MDP_PAD0 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_PAD0);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_PAD2)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_PAD2)) {
			CMDQ_MSG("Disable MDP_PAD2 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_PAD2);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_PAD3)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_PAD3)) {
			CMDQ_MSG("Disable MDP_PAD3 clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_PAD3);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT0)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT0,
			MDP_WROT0_BASE + 0X010, MDP_WROT0_BASE + 0X014,
			0x1, 0x1, true);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT2)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT2,
			MDP_WROT2_BASE + 0X010, MDP_WROT2_BASE + 0X014,
			0x1, 0x1, true);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_WROT3)) {
		cmdq_mdp_loop_off(CMDQ_ENG_MDP_WROT3,
			MDP_WROT3_BASE + 0X010, MDP_WROT3_BASE + 0X014,
			0x1, 0x1, true);
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_VPP0_SOUT)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_VPP0_SOUT)) {
			CMDQ_MSG("Disable MDP_VPP0_SOUT clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_VPP0_SOUT);
		}
	}

	if (engineFlag & (1LL << CMDQ_ENG_MDP_VPP1_SOUT)) {
		if (cmdq_mdp_get_func()->mdpClockIsOn(CMDQ_ENG_MDP_VPP1_SOUT)) {
			CMDQ_MSG("Disable MDP_VPP1_SOUT clock\n");
			cmdq_mdp_get_func()->enableMdpClock(false,
				CMDQ_ENG_MDP_VPP1_SOUT);
		}
	}

#endif /* #ifdef CMDQ_PWR_AWARE */

	CMDQ_MSG("Disable MDP(0x%llx) clock end\n", engineFlag);

	return 0;
}


void cmdqMdpInitialSetting(struct platform_device *pdev)
{
#ifdef FPGA_TODO	/* harry TODO: need to enable */
	char *data = kzalloc(MDP_DISPATCH_KEY_STR_LEN, GFP_KERNEL);

	/* Register ION Translation Fault function */
	mtk_iommu_register_fault_callback(M4U_PORT_L4_MDP_RDMA,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_L5_SVPP1_MDP_RDMA,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_L5_SVPP2_MDP_RDMA,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_L6_SVPP3_MDP_RDMA,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_L4_MDP_WROT,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_L5_SVPP1_MDP_WROT,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_L5_SVPP2_MDP_WROT,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
	mtk_iommu_register_fault_callback(M4U_PORT_L6_SVPP3_MDP_WROT,
		(mtk_iommu_fault_callback_t)cmdq_TranslationFault_callback,
		(void *)data);
#endif
}

uint32_t cmdq_mdp_rdma_get_reg_offset_src_addr(void)
{
	return 0xF00;
}

uint32_t cmdq_mdp_wrot_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

uint32_t cmdq_mdp_wdma_get_reg_offset_dst_addr(void)
{
	return 0xF00;
}

const char *cmdq_mdp_parse_handle_error_module(
	const struct cmdqRecStruct *handle)
{
	const char *module = NULL;

	if ((handle->engineFlag & CMDQ_ENG_WPE_GROUP_BITS)
		== handle->engineFlag)
		module = "WPE_ONLY";

	/* for secure path, use HW flag is sufficient */
	do {
		if (module != NULL)
			break;

		if (!handle->secData.is_secure) {
			/* normal path,
			 * need parse current running instruciton
			 * for more detail
			 */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(handle->engineFlag)) {
			module = "MDP";
			break;
		}

		module = "CMDQ";
	} while (0);

	return module;
}

const char *cmdq_mdp_parse_error_module(const struct cmdqRecStruct *task)
{
	const char *module = NULL;

	if ((task->engineFlag & CMDQ_ENG_WPE_GROUP_BITS)
		== task->engineFlag)
		module = "WPE_ONLY";

	/* for secure path, use HW flag is sufficient */
	do {
		if (module != NULL)
			break;

		if (!task->secData.is_secure) {
			/* normal path,
			 * need parse current running instruciton
			 * for more detail
			 */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(task->engineFlag)) {
			module = "MDP";
			break;
		}

		module = "CMDQ";
	} while (0);

	return module;
}

u64 cmdq_mdp_get_engine_group_bits(u32 engine_group)
{
	return gCmdqEngineGroupBits[engine_group];
}


static void cmdq_mdp_enable_common_clock(bool enable, u64 engine_flag)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		cmdq_mdp_enable_clock_TOP_CFG_VPP0(enable);
		cmdq_mdp_enable_clock_TOP_CFG_26M_VPP0(enable);
		//mdp_vppsys0_power_on();
		cmdq_mdp_enable_clock_TOP_CFG_VPP1(enable);
		cmdq_mdp_enable_clock_TOP_CFG_26M_VPP1(enable);
		//mdp_vppsys1_power_on();
		mdp_larb_power_on();
		cmdq_mdp_enable_clock_MDP_MUTEX0(enable);
		cmdq_mdp_enable_clock_DISP_MUTEX(enable);
	} else {
		/* disable, reverse the sequence */
		cmdq_mdp_enable_clock_DISP_MUTEX(enable);
		cmdq_mdp_enable_clock_MDP_MUTEX0(enable);
		mdp_larb_power_off();
		//mdp_vppsys1_power_off();
		cmdq_mdp_enable_clock_TOP_CFG_VPP1(enable);
		cmdq_mdp_enable_clock_TOP_CFG_26M_VPP1(enable);
		//mdp_vppsys0_power_off();
		cmdq_mdp_enable_clock_TOP_CFG_26M_VPP0(enable);
		cmdq_mdp_enable_clock_TOP_CFG_VPP0(enable);
	}
#endif	/* CMDQ_PWR_AWARE */
}


static void cmdq_mdp_check_hw_status(struct cmdqRecStruct *handle)
{
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
#define CMDQ_ENGINE_TRANS(eng_flags, eng_flags_sec, ENGINE) \
	do {	\
		if ((1LL << CMDQ_ENG_##ENGINE) & (eng_flags)) \
			(eng_flags_sec) |= (1LL << CMDQ_SEC_##ENGINE); \
	} while (0)

u64 cmdq_mdp_get_secure_engine(u64 engine_flags)
{
	u64 sec_eng_flag = 0;

	/* MDP engines */
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_RDMA0);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_RDMA2);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_RDMA3);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_WROT0);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_WROT2);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_WROT3);

	/* film grain */
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_FG0);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_FG2);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_FG3);

	/* ISP */
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, WPEI);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, WPEO);

	/* SVP HDR */
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_HDR0);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_HDR2);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_HDR3);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_AAL0);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_AAL2);
	CMDQ_ENGINE_TRANS(engine_flags, sec_eng_flag, MDP_AAL3);

	return sec_eng_flag;
}
#endif

void cmdq_mdp_resolve_token(u64 engine_flag,
	const struct cmdqRecStruct *task)
{
}

static void mdp_readback_aal_by_engine(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t pa, u32 param)
{
	phys_addr_t base;

	switch (engine) {
	case CMDQ_ENG_MDP_AAL0:
		base = mdp_module_pa.aal0;
		break;
	case CMDQ_ENG_MDP_AAL2:
		base = mdp_module_pa.aal2;
		break;
	case CMDQ_ENG_MDP_AAL3:
		base = mdp_module_pa.aal3;
		break;
	default:
		CMDQ_ERR("%s not support\n", __func__);
		return;
	}

	cmdq_mdp_get_func()->mdpReadbackAal(handle, engine, base, pa, param);
}

static void mdp_readback_hdr_by_engine(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t pa, u32 param)
{
	phys_addr_t base;

	switch (engine) {
	case CMDQ_ENG_MDP_HDR0:
		base = mdp_module_pa.hdr0;
		break;
	case CMDQ_ENG_MDP_HDR2:
		base = mdp_module_pa.hdr2;
		break;
	case CMDQ_ENG_MDP_HDR3:
		base = mdp_module_pa.hdr3;
		break;
	default:
		CMDQ_ERR("%s not support\n", __func__);
		return;
	}

	cmdq_mdp_get_func()->mdpReadbackHdr(handle, engine, base, pa, param);
}

void cmdq_mdp_compose_readback(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t addr, u32 param)
{
	switch (engine) {
	case CMDQ_ENG_MDP_AAL0:
	case CMDQ_ENG_MDP_AAL2:
	case CMDQ_ENG_MDP_AAL3:
		mdp_readback_aal_by_engine(handle, engine, addr, param);
		break;
	case CMDQ_ENG_MDP_HDR0:
	case CMDQ_ENG_MDP_HDR2:
	case CMDQ_ENG_MDP_HDR3:
		mdp_readback_hdr_by_engine(handle, engine, addr, param);
		break;
	default:
		CMDQ_ERR("%s engine not support:%hu\n", __func__, engine);
		break;
	}
}

u32 mdp_get_larb_id(u16 engine)
{
	u32 ret;

	switch (engine) {
	case ENGBASE_MDP_RDMA0:
	case ENGBASE_MDP_WROT0:
		ret = 4;
		break;
	case ENGBASE_MDP_RDMA2:
	case ENGBASE_MDP_WROT2:
	case ENGBASE_MDP_FG2:
		ret = 5;
		break;
	case ENGBASE_MDP_RDMA3:
	case ENGBASE_MDP_WROT3:
	case ENGBASE_MDP_FG3:
		ret = 6;
		break;
	case ENGBASE_VPP_WPE_A:
		ret = 7;
		break;
	default:
		ret = 0;
	break;
}

return ret;
}

static u32 *mdp_engine_base_get(void)
{
	return (u32 *)mdp_base;
}

static u32 mdp_engine_base_count(void)
{
	return (u32)ENGBASE_COUNT;
}

static bool mdp_check_camin_support(void)
{
	return false;
}

void mdp_handle_prepare(struct cmdqRecStruct *handle)
{
	/* TODO: */
	CMDQ_MSG("prepare mdp handle\n");
}

void mdp_handle_unprepare(struct cmdqRecStruct *handle)
{
	if (mdp_debug_get_info()->enable_crc) {
		if (handle->engineFlag & 1LL << CMDQ_ENG_MDP_WROT0) {
			CMDQ_ERR("WROT0 CRC:0x%08x\n",
				CMDQ_REG_GET32(MDP_WROT0_BASE + 0xEC));
			}
		if (handle->engineFlag & 1LL << CMDQ_ENG_MDP_WROT2) {
			CMDQ_ERR("WROT2 CRC:0x%08x\n",
				CMDQ_REG_GET32(MDP_WROT2_BASE + 0xEC));
			}
		if (handle->engineFlag & 1LL << CMDQ_ENG_MDP_WROT3) {
			CMDQ_ERR("WROT3 CRC:0x%08x\n",
				CMDQ_REG_GET32(MDP_WROT3_BASE + 0xEC));
			}
	}

	if (mdp_debug_get_info()->fg_debug) {
		if (handle->engineFlag & (1LL << CMDQ_ENG_MDP_FG2))
			cmdq_mdp_dump_fg(MDP_FG2_BASE, "FG2");

		if (handle->engineFlag & (1LL << CMDQ_ENG_MDP_FG3))
			cmdq_mdp_dump_fg(MDP_FG3_BASE, "FG3");
	}

	if (mdp_debug_get_info()->rdma_config_debug) {
		long rdma_base = 0;
		char *module_name = NULL;

		if (handle->engineFlag & 1LL << CMDQ_ENG_MDP_WROT0) {
			rdma_base = MDP_WROT0_BASE;
			module_name = "mdp_rdma0";
		}
		if (handle->engineFlag & 1LL << CMDQ_ENG_MDP_WROT2) {
			rdma_base = MDP_WROT2_BASE;
			module_name = "mdp_rdma2";
		}
		if (handle->engineFlag & 1LL << CMDQ_ENG_MDP_WROT3) {
			rdma_base = MDP_WROT3_BASE;
			module_name = "mdp_rdma3";
		}

		CMDQ_ERR("%s MDP_RDMA_SRC_BASE_0:0x%08x\n",
			module_name,
			CMDQ_REG_GET32(rdma_base + 0xF00));
		CMDQ_ERR("%s MDP_RDMA_SRC_BASE_1:0x%08x\n",
			module_name,
			CMDQ_REG_GET32(rdma_base + 0xF08));
		CMDQ_ERR("%s MDP_RDMA_SRC_BASE_2:0x%08x\n",
			module_name,
			CMDQ_REG_GET32(rdma_base + 0xF10));
		CMDQ_ERR("%s MDP_RDMA_UFO_DEC_LENGTH_BASE_Y:0x%08x\n",
			module_name,
			CMDQ_REG_GET32(rdma_base + 0xF20));
		CMDQ_ERR("%s MDP_RDMA_UFO_DEC_LENGTH_BASE_C:0x%08x\n",
			module_name,
			CMDQ_REG_GET32(rdma_base + 0xF28));
	}
}

void cmdq_mdp_platform_function_setting(void)
{
	struct cmdqMDPFuncStruct *pFunc = cmdq_mdp_get_func();

	pFunc->dumpMMSYSConfig = cmdq_mdp_dump_mmsys_config;

	pFunc->initModuleBaseVA = cmdq_mdp_init_module_base_VA;
	pFunc->deinitModuleBaseVA = cmdq_mdp_deinit_module_base_VA;

	pFunc->mdpClockIsOn = cmdq_mdp_clock_is_on;
	pFunc->enableMdpClock = cmdq_mdp_enable_clock;
	pFunc->initModuleCLK = cmdq_mdp_init_module_clk;
	pFunc->mdpDumpRsz = cmdq_mdp_dump_rsz;
	pFunc->mdpDumpMerge = cmdq_mdp_dump_merge;
	pFunc->mdpDumpTdshp = cmdq_mdp_dump_tdshp;
	pFunc->mdpClockOn = cmdqMdpClockOn;
	pFunc->mdpDumpInfo = cmdqMdpDumpInfo;
	pFunc->mdpResetEng = cmdqMdpResetEng;
	pFunc->mdpClockOff = cmdqMdpClockOff;

	pFunc->mdpInitialSet = cmdqMdpInitialSetting;

	pFunc->rdmaGetRegOffsetSrcAddr = cmdq_mdp_rdma_get_reg_offset_src_addr;
	pFunc->wrotGetRegOffsetDstAddr = cmdq_mdp_wrot_get_reg_offset_dst_addr;
	pFunc->wdmaGetRegOffsetDstAddr = cmdq_mdp_wdma_get_reg_offset_dst_addr;
	pFunc->parseErrModByEngFlag = cmdq_mdp_parse_error_module;
	pFunc->getEngineGroupBits = cmdq_mdp_get_engine_group_bits;
	pFunc->mdpEnableCommonClock = cmdq_mdp_enable_common_clock;
	pFunc->CheckHwStatus = cmdq_mdp_check_hw_status;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	pFunc->mdpGetSecEngine = cmdq_mdp_get_secure_engine;
#endif
	pFunc->resolve_token = cmdq_mdp_resolve_token;
	pFunc->mdpComposeReadback = cmdq_mdp_compose_readback;

	/* pmqos */
	pFunc->qosTransPort = cmdq_mdp_qos_translate_port;
	pFunc->qosInit = mdp_qos_init;
	pFunc->qosGetPath = mdp_qos_get_path;
	pFunc->qosClearAll = mdp_qos_clear_all;

	pFunc->getGroupMax = mdp_get_group_max;
	pFunc->getGroupIsp = mdp_get_group_isp_plat;
	pFunc->getGroupMdp = mdp_get_group_mdp;
	pFunc->getGroupWpe = mdp_get_group_wpe_plat;
	pFunc->getEngineBase = mdp_engine_base_get;
	pFunc->getEngineBaseCount = mdp_engine_base_count;
	pFunc->getEngineGroupName = mdp_get_engine_group_name;
	pFunc->getLarbID = mdp_get_larb_id;
	pFunc->mdpIsCaminSupport = mdp_check_camin_support;
	pFunc->handlePrepare = mdp_handle_prepare;
	pFunc->handleUnprepare = mdp_handle_unprepare;
}

