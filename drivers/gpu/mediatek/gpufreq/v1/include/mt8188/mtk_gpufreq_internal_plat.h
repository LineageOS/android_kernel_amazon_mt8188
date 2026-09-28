/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef ___MT_GPUFREQ_INTERNAL_PLAT_H___
#define ___MT_GPUFREQ_INTERNAL_PLAT_H___

/**************************************************
 *  0:     all on when mtk probe init (freq/ Vgpu/ Vsram_gpu)
 *         disable DDK power on/off callback
 **************************************************/
#define MT_GPUFREQ_POWER_CTL_ENABLE	1

/**************************************************
 * (DVFS_ENABLE, CUST_CONFIG)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPP
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPP (do DVFS only onces)
 * (0, 0) -> DVFS disable
 **************************************************/
#define MT_GPUFREQ_DVFS_ENABLE          1
#define MT_GPUFREQ_CUST_CONFIG          0
#define MT_GPUFREQ_CUST_INIT_OPP        (g_opp_table_segment_1[8].gpufreq_khz)

/**************************************************
 * DVFS Setting
 **************************************************/
#define NUM_OF_OPP_IDX (sizeof(g_opp_table_segment_1) / \
			sizeof(g_opp_table_segment_1[0]))

/* On opp table, low vgpu will use the same vsram.
 * And hgih vgpu will have the same diff with vsram.
 *
 * if (vgpu <= FIXED_VSRAM_VOLT_THSRESHOLD) {
 *     vsram = FIXED_VSRAM_VOLT;
 * } else {
 *     vsram = vgpu + FIXED_VSRAM_VOLT_DIFF;
 * }
 */
#define FIXED_VSRAM_VOLT                (75000)
#define FIXED_VSRAM_VOLT_THSRESHOLD     (75000)
#define FIXED_VSRAM_VOLT_DIFF           (0)

/**************************************************
 * PMIC Setting
 **************************************************/
/* PMIC hardware range:
 * vgpu      0.3 ~ 1.19375 V
 * vsram_gpu 0.5 ~ 1.29375 V
 */
#define VGPU_MAX_VOLT                   (119375)        /* mV x 100 */
#define VGPU_MIN_VOLT                   (30000)         /* mV x 100 */
#define VSRAM_GPU_MAX_VOLT              (129375)        /* mV x 100 */
#define VSRAM_GPU_MIN_VOLT              (50000)         /* mV x 100 */
/*
 * (-100)mv <= (VSRAM - VGPU) <= (300)mV
 */
#define BUCK_DIFF_MAX                   (30000)         /* mV x 100 */
#define BUCK_DIFF_MIN                   (-10000)        /* mV x 100 */

/**************************************************
 * Clock Setting
 **************************************************/
#define GPU_MAX_FREQ                    (950000)        /* KHz */
#define GPU_MIN_FREQ                    (375000)        /* KHz */

/**************************************************
 * Reference Power Setting
 **************************************************/
#define GPU_ACT_REF_POWER               (1285)                /* mW  */
#define GPU_ACT_REF_FREQ                (900000)              /* KHz */
#define GPU_ACT_REF_VOLT                (90000)               /* mV x 100 */
#define PTPOD_DISABLE_VOLT              (75000)

/**************************************************
 * Battery Over Current Protect
 **************************************************/
#define MT_GPUFREQ_BATT_OC_PROTECT              0
#define MT_GPUFREQ_BATT_OC_LIMIT_FREQ           (485000)        /* KHz */

/**************************************************
 * Battery Percentage Protect
 **************************************************/
#define MT_GPUFREQ_BATT_PERCENT_PROTECT         0
#define MT_GPUFREQ_BATT_PERCENT_LIMIT_FREQ      (485000)        /* KHz */

/**************************************************
 * Low Battery Volume Protect
 **************************************************/
#define MT_GPUFREQ_LOW_BATT_VOLT_PROTECT        0
#define MT_GPUFREQ_LOW_BATT_VOLT_LIMIT_FREQ     (485000)        /* KHz */

/**************************************************
 * DFD Dump
 **************************************************/
#define MT_GPUFREQ_DFD_ENABLE 0
#define MT_GPUFREQ_DFD_DEBUG 0

/**************************************************
 * Register Manipulations
 **************************************************/
#define READ_REGISTER_UINT32(reg)	\
	(*(unsigned int * const)(reg))
#define WRITE_REGISTER_UINT32(reg, val)	\
	((*(unsigned int * const)(reg)) = (val))
#define INREG32(x)	\
	READ_REGISTER_UINT32((unsigned int *)((void *)(x)))
#define OUTREG32(x, y)	\
	WRITE_REGISTER_UINT32((unsigned int *)((void *)(x)), (unsigned int)(y))
#define SETREG32(x, y)	\
	OUTREG32(x, INREG32(x)|(y))
#define CLRREG32(x, y)	\
	OUTREG32(x, INREG32(x)&~(y))
#define MASKREG32(x, y, z)	\
	OUTREG32(x, (INREG32(x)&~(y))|(z))
#define DRV_Reg32(addr)				INREG32(addr)
#define DRV_WriteReg32(addr, data)	OUTREG32(addr, data)
#define DRV_SetReg32(addr, data)	SETREG32(addr, data)
#define DRV_ClrReg32(addr, data)	CLRREG32(addr, data)

/**************************************************
 * Proc Node Definition
 **************************************************/
#ifdef CONFIG_PROC_FS
#define PROC_FOPS_RW(name)	\
	static int mt_ ## name ## _proc_open(	\
			struct inode *inode,	\
			struct file *file)	\
	{	\
		return single_open(	\
				file,	\
				mt_ ## name ## _proc_show,	\
				PDE_DATA(inode));	\
	}	\
	static const struct proc_ops mt_ ## name ## _proc_fops =	\
	{	\
		.proc_open = mt_ ## name ## _proc_open,	\
		.proc_read = seq_read,	\
		.proc_lseek = seq_lseek,	\
		.proc_release = single_release,	\
		.proc_write = mt_ ## name ## _proc_write,	\
	}
#define PROC_FOPS_RO(name)	\
	static int mt_ ## name ## _proc_open(	\
			struct inode *inode,	\
			struct file *file)	\
	{	\
		return single_open(	\
				file,	\
				mt_ ## name ## _proc_show,	\
				PDE_DATA(inode));	\
	}	\
	static const struct proc_ops mt_ ## name ## _proc_fops =	\
	{	\
		.proc_open = mt_ ## name ## _proc_open,	\
		.proc_read = seq_read,	\
		.proc_lseek = seq_lseek,	\
		.proc_release = single_release,	\
	}
#define PROC_ENTRY(name) \
	{__stringify(name), &mt_ ## name ## _proc_fops}
#endif

/**************************************************
 * Operation Definition
 **************************************************/
#define VOLT_NORMALIZATION(volt)	\
	((volt % 625) ? (volt - (volt % 625) + 625) : volt)
#ifndef MIN
#define MIN(x, y)	(((x) < (y)) ? (x) : (y))
#endif

#define GPUOP(khz, vgpu, vsram, aging_margin)	\
	{							\
		.gpufreq_khz = khz,				\
		.gpufreq_vgpu = vgpu,				\
		.gpufreq_vsram = vsram,				\
		.gpufreq_aging_margin = aging_margin,		\
	}

/**************************************************
 * Enumerations
 **************************************************/
enum g_segment_id_enum {
	MT8188_SEGMENT = 1,
};

enum g_segment_table_enum {
	SEGMENT_TABLE_1 = 1,
	SEGMENT_TABLE_2,
	SEGMENT_TABLE_3,
};

enum g_clock_source_enum  {
	CLOCK_MAIN = 0,
	CLOCK_SUB,
};

enum g_limit_enable_enum  {
	LIMIT_DISABLE = 0,
	LIMIT_ENABLE,
};

enum {
	GPUFREQ_LIMIT_PRIO_NONE,	/* the lowest priority */
	GPUFREQ_LIMIT_PRIO_1,
	GPUFREQ_LIMIT_PRIO_2,
	GPUFREQ_LIMIT_PRIO_3,
	GPUFREQ_LIMIT_PRIO_4,
	GPUFREQ_LIMIT_PRIO_5,
	GPUFREQ_LIMIT_PRIO_6,
	GPUFREQ_LIMIT_PRIO_7,
	GPUFREQ_LIMIT_PRIO_8		/* the highest priority */
};

struct gpudvfs_limit {
	unsigned int kicker;
	char *name;
	unsigned int prio;
	unsigned int upper_idx;
	unsigned int upper_enable;
	unsigned int lower_idx;
	unsigned int lower_enable;
};

#define LIMIT_IDX_DEFAULT -1
#define GPU_CORE_NUM 3

struct gpudvfs_limit limit_table[] = {
	{KIR_STRESS,		"STRESS",	GPUFREQ_LIMIT_PRIO_8,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PROC,			"PROC",		GPUFREQ_LIMIT_PRIO_7,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PTPOD,			"PTPOD",	GPUFREQ_LIMIT_PRIO_6,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_THERMAL,		"THERMAL",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_OC,		"BATT_OC",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_LOW,		"BATT_LOW",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_BATT_PERCENT,	"BATT_PERCENT",	GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_PBM,			"PBM",		GPUFREQ_LIMIT_PRIO_5,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
	{KIR_POLICY,		"POLICY",	GPUFREQ_LIMIT_PRIO_4,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE,
		LIMIT_IDX_DEFAULT, LIMIT_ENABLE},
};

/**************************************************
 * Structures
 **************************************************/
struct opp_table_info {
	unsigned int gpufreq_khz;
	unsigned int gpufreq_vgpu;
	unsigned int gpufreq_vsram;
	unsigned int gpufreq_aging_margin;
};

struct g_clk_info {
	struct clk *clk_mux;
	struct clk *clk_pll_src;
	struct clk *clk_main_parent;
	struct clk *clk_sub_parent;
	struct clk *subsys_bg3d;
};

struct g_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vsram_gpu;
};

struct g_mtcmos_info {
	unsigned int num_domains;
	struct device *pm_domain_devs[GPU_CORE_NUM];
};

/**************************************************
 * External functions declaration
 **************************************************/
extern bool mtk_get_gpu_loading(unsigned int *pLoading);

/**************************************************
 * global value definition
 **************************************************/
struct opp_table_info *g_opp_table;

#if MT_GPUFREQ_CUST_CONFIG
/**************************************************
 * PTPOD definition
 **************************************************/
unsigned int g_ptpod_opp_idx_table_segment[] = {
	0, 4, 8, 11,
	14, 16, 18, 20,
	22, 24, 26, 28,
	30, 32, 34, 36
};
/**************************************************
 * GPU OPP table definition
 **************************************************/
struct opp_table_info g_opp_table_segment_1[] = {
	GPUOP(950000, 80000, 80000, 1875), /* 0 sign off */
	GPUOP(941000, 79375, 79375, 1875), /* 1 */
	GPUOP(932000, 78750, 78750, 1875), /* 2 */
	GPUOP(923000, 78125, 78125, 1875), /* 3 */
	GPUOP(915000, 77500, 77500, 1875), /* 4 */
	GPUOP(906000, 76875, 76875, 1875), /* 5 */
	GPUOP(897000, 76250, 76250, 1875), /* 6 */
	GPUOP(888000, 75625, 75625, 1875), /* 7 */
	GPUOP(880000, 75000, 75000, 1875), /* 8 sign off */
	GPUOP(865000, 74375, 75000, 1875), /* 9 */
	GPUOP(850000, 73750, 75000, 1875), /*10 */
	GPUOP(835000, 73125, 75000, 1875), /*11 */
	GPUOP(820000, 72500, 75000, 1875), /*12 */
	GPUOP(805000, 71875, 75000, 1875), /*13 */
	GPUOP(790000, 71250, 75000, 1875), /*14 */
	GPUOP(775000, 70625, 75000, 1875), /*15 */
	GPUOP(760000, 70000, 75000, 1250), /*16 */
	GPUOP(745000, 69375, 75000, 1250), /*17 */
	GPUOP(730000, 68750, 75000, 1250), /*18 */
	GPUOP(715000, 68125, 75000, 1250), /*19 */
	GPUOP(700000, 67500, 75000, 1250), /*20 */
	GPUOP(685000, 66875, 75000, 1250), /*21 */
	GPUOP(670000, 66250, 75000, 1250), /*22 */
	GPUOP(655000, 65625, 75000, 1250), /*23 */
	GPUOP(640000, 65000, 75000, 1250), /*24 sign off */
	GPUOP(619000, 64375, 75000, 1250), /*25 */
	GPUOP(598000, 63750, 75000, 1250), /*26 */
	GPUOP(577000, 63125, 75000, 1250), /*27 */
	GPUOP(556000, 62500, 75000, 625),  /*28 */
	GPUOP(535000, 61875, 75000, 625),  /*29 */
	GPUOP(515000, 61250, 75000, 625),  /*30 */
	GPUOP(494000, 60625, 75000, 625),  /*31 */
	GPUOP(473000, 60000, 75000, 625),  /*32 */
	GPUOP(452000, 59375, 75000, 625),  /*33 */
	GPUOP(431000, 58750, 75000, 625),  /*34 */
	GPUOP(410000, 58125, 75000, 625),  /*35 */
	GPUOP(390000, 57500, 75000, 625),  /*36 sign off */
};

struct opp_table_info g_opp_table_segment_2[] = {
	GPUOP(950000, 80000, 80000, 1875), /* 0 sign off */
	GPUOP(941000, 79375, 79375, 1875), /* 1 */
	GPUOP(932000, 78750, 78750, 1875), /* 2 */
	GPUOP(923000, 78125, 78125, 1875), /* 3 */
	GPUOP(915000, 77500, 77500, 1875), /* 4 */
	GPUOP(906000, 76875, 76875, 1875), /* 5 */
	GPUOP(897000, 76250, 76250, 1875), /* 6 */
	GPUOP(888000, 75625, 75625, 1875), /* 7 */
	GPUOP(880000, 75000, 75000, 1875), /* 8 sign off */
	GPUOP(865000, 74375, 75000, 1875), /* 9 */
	GPUOP(850000, 73750, 75000, 1875), /*10 */
	GPUOP(835000, 73125, 75000, 1875), /*11 */
	GPUOP(820000, 72500, 75000, 1875), /*12 */
	GPUOP(805000, 71875, 75000, 1875), /*13 */
	GPUOP(790000, 71250, 75000, 1875), /*14 */
	GPUOP(775000, 70625, 75000, 1875), /*15 */
	GPUOP(760000, 70000, 75000, 1250), /*16 */
	GPUOP(745000, 69375, 75000, 1250), /*17 */
	GPUOP(730000, 68750, 75000, 1250), /*18 */
	GPUOP(715000, 68125, 75000, 1250), /*19 */
	GPUOP(700000, 67500, 75000, 1250), /*20 */
	GPUOP(685000, 66875, 75000, 1250), /*21 */
	GPUOP(670000, 66250, 75000, 1250), /*22 */
	GPUOP(655000, 65625, 75000, 1250), /*23 */
	GPUOP(640000, 65000, 75000, 1250), /*24 sign off */
	GPUOP(619000, 65000, 75000, 1250), /*25 */
	GPUOP(598000, 64375, 75000, 1250), /*26 */
	GPUOP(577000, 63750, 75000, 1250), /*27 */
	GPUOP(556000, 63750, 75000, 625),  /*28 */
	GPUOP(535000, 63125, 75000, 625),  /*29 */
	GPUOP(515000, 62500, 75000, 625),  /*30 */
	GPUOP(494000, 62500, 75000, 625),  /*31 */
	GPUOP(473000, 61875, 75000, 625),  /*32 */
	GPUOP(452000, 61250, 75000, 625),  /*33 */
	GPUOP(431000, 61250, 75000, 625),  /*34 */
	GPUOP(410000, 60625, 75000, 625),  /*35 */
	GPUOP(390000, 60000, 75000, 625),  /*36 sign off */
};

struct opp_table_info g_opp_table_segment_3[] = {
	GPUOP(950000, 80000, 80000, 1875), /* 0 sign off */
	GPUOP(941000, 79375, 79375, 1875), /* 1 */
	GPUOP(932000, 78750, 78750, 1875), /* 2 */
	GPUOP(923000, 78125, 78125, 1875), /* 3 */
	GPUOP(915000, 77500, 77500, 1875), /* 4 */
	GPUOP(906000, 76875, 76875, 1875), /* 5 */
	GPUOP(897000, 76250, 76250, 1875), /* 6 */
	GPUOP(888000, 75625, 75625, 1875), /* 7 */
	GPUOP(880000, 75000, 75000, 1875), /* 8 sign off */
	GPUOP(865000, 74375, 75000, 1875), /* 9 */
	GPUOP(850000, 73750, 75000, 1875), /*10 */
	GPUOP(835000, 73125, 75000, 1875), /*11 */
	GPUOP(820000, 72500, 75000, 1875), /*12 */
	GPUOP(805000, 71875, 75000, 1875), /*13 */
	GPUOP(790000, 71250, 75000, 1875), /*14 */
	GPUOP(775000, 70625, 75000, 1875), /*15 */
	GPUOP(760000, 70000, 75000, 1250), /*16 */
	GPUOP(745000, 69375, 75000, 1250), /*17 */
	GPUOP(730000, 68750, 75000, 1250), /*18 */
	GPUOP(715000, 68125, 75000, 1250), /*19 */
	GPUOP(700000, 67500, 75000, 1250), /*20 */
	GPUOP(685000, 66875, 75000, 1250), /*21 */
	GPUOP(670000, 66250, 75000, 1250), /*22 */
	GPUOP(655000, 65625, 75000, 1250), /*23 */
	GPUOP(640000, 65000, 75000, 1250), /*24 sign off */
	GPUOP(619000, 65000, 75000, 1250), /*25 */
	GPUOP(598000, 65000, 75000, 1250), /*26 */
	GPUOP(577000, 64375, 75000, 1250), /*27 */
	GPUOP(556000, 64375, 75000, 625),  /*28 */
	GPUOP(535000, 64375, 75000, 625),  /*29 */
	GPUOP(515000, 63750, 75000, 625),  /*30 */
	GPUOP(494000, 63750, 75000, 625),  /*31 */
	GPUOP(473000, 63750, 75000, 625),  /*32 */
	GPUOP(452000, 63125, 75000, 625),  /*33 */
	GPUOP(431000, 63125, 75000, 625),  /*34 */
	GPUOP(410000, 63125, 75000, 625),  /*35 */
	GPUOP(390000, 62500, 75000, 625),  /*36 sign off */
};
#else

/**************************************************
 * PTPOD definition
 **************************************************/
unsigned int g_ptpod_opp_idx_table_segment[] = {
	0, 1, 2, 3,
	4, 5, 6, 7,
	8, 9, 10, 11,
	12, 13, 14, 15
};
/**************************************************
 * GPU OPP table definition number 16
 **************************************************/
struct opp_table_info g_opp_table_segment_1[] = {
	GPUOP(950000, 80000, 80000, 1875), /* 0 sign off */
	GPUOP(915000, 77500, 77500, 1875), /* 4 */
	GPUOP(880000, 75000, 75000, 1875), /* 8 sign off */
	GPUOP(835000, 73125, 75000, 1875), /*11 */
	GPUOP(790000, 71250, 75000, 1875), /*14 */
	GPUOP(760000, 70000, 75000, 1250), /*16 */
	GPUOP(730000, 68750, 75000, 1250), /*18 */
	GPUOP(700000, 67500, 75000, 1250), /*20 */
	GPUOP(670000, 66250, 75000, 1250), /*22 */
	GPUOP(640000, 65000, 75000, 1250), /*24 sign off */
	GPUOP(598000, 63750, 75000, 1250), /*26 */
	GPUOP(556000, 62500, 75000, 625),  /*28 */
	GPUOP(515000, 61250, 75000, 625),  /*30 */
	GPUOP(473000, 60000, 75000, 625),  /*32 */
	GPUOP(431000, 58750, 75000, 625),  /*34 */
	GPUOP(390000, 57500, 75000, 625),  /*36 sign off */
};

struct opp_table_info g_opp_table_segment_2[] = {
	GPUOP(950000, 80000, 80000, 1875), /* 0 sign off */
	GPUOP(915000, 77500, 77500, 1875), /* 4 */
	GPUOP(880000, 75000, 75000, 1875), /* 8 sign off */
	GPUOP(835000, 73125, 75000, 1875), /*11 */
	GPUOP(790000, 71250, 75000, 1875), /*14 */
	GPUOP(760000, 70000, 75000, 1250), /*16 */
	GPUOP(730000, 68750, 75000, 1250), /*18 */
	GPUOP(700000, 67500, 75000, 1250), /*20 */
	GPUOP(670000, 66250, 75000, 1250), /*22 */
	GPUOP(640000, 65000, 75000, 1250), /*24 sign off */
	GPUOP(598000, 64375, 75000, 1250), /*26 */
	GPUOP(556000, 63750, 75000, 625),  /*28 */
	GPUOP(515000, 62500, 75000, 625),  /*30 */
	GPUOP(473000, 61875, 75000, 625),  /*32 */
	GPUOP(431000, 61250, 75000, 625),  /*34 */
	GPUOP(390000, 60000, 75000, 625),  /*36 sign off */
};

struct opp_table_info g_opp_table_segment_3[] = {
	GPUOP(950000, 80000, 80000, 1875), /* 0 sign off */
	GPUOP(915000, 77500, 77500, 1875), /* 4 */
	GPUOP(880000, 75000, 75000, 1875), /* 8 sign off */
	GPUOP(835000, 73125, 75000, 1875), /*11 */
	GPUOP(790000, 71250, 75000, 1875), /*14 */
	GPUOP(760000, 70000, 75000, 1250), /*16 */
	GPUOP(730000, 68750, 75000, 1250), /*18 */
	GPUOP(700000, 67500, 75000, 1250), /*20 */
	GPUOP(670000, 66250, 75000, 1250), /*22 */
	GPUOP(640000, 65000, 75000, 1250), /*24 sign off */
	GPUOP(598000, 65000, 75000, 1250), /*26 */
	GPUOP(556000, 64375, 75000, 625),  /*28 */
	GPUOP(515000, 63750, 75000, 625),  /*30 */
	GPUOP(473000, 63750, 75000, 625),  /*32 */
	GPUOP(431000, 63125, 75000, 625),  /*34 */
	GPUOP(390000, 62500, 75000, 625),  /*36 sign off */
};
#endif
#endif /* ___MT_GPUFREQ_INTERNAL_PLAT_H___ */
