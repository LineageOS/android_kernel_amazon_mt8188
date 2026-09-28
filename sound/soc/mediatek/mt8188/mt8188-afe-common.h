/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt8188-afe-common.h  --  Mediatek 8188 audio driver definitions
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#ifndef _MT_8188_AFE_COMMON_H_
#define _MT_8188_AFE_COMMON_H_

#include <linux/list.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include "../common/mtk-base-afe.h"

enum {
	MT8188_DAI_START,
	MT8188_AFE_MEMIF_START = MT8188_DAI_START,
	MT8188_AFE_MEMIF_DL2 = MT8188_AFE_MEMIF_START,
	MT8188_AFE_MEMIF_DL3,
	MT8188_AFE_MEMIF_DL6,
	MT8188_AFE_MEMIF_DL7,
	MT8188_AFE_MEMIF_DL8,
	MT8188_AFE_MEMIF_DL10,
	MT8188_AFE_MEMIF_DL11,
	MT8188_AFE_MEMIF_UL_START,
	MT8188_AFE_MEMIF_UL1 = MT8188_AFE_MEMIF_UL_START,
	MT8188_AFE_MEMIF_UL2,
	MT8188_AFE_MEMIF_UL3,
	MT8188_AFE_MEMIF_UL4,
	MT8188_AFE_MEMIF_UL5,
	MT8188_AFE_MEMIF_UL6,
	MT8188_AFE_MEMIF_UL8,
	MT8188_AFE_MEMIF_UL9,
	MT8188_AFE_MEMIF_UL10,
	MT8188_AFE_MEMIF_END,
	MT8188_AFE_MEMIF_NUM = (MT8188_AFE_MEMIF_END - MT8188_AFE_MEMIF_START),
	MT8188_AFE_IO_START = MT8188_AFE_MEMIF_END,
	MT8188_AFE_IO_DL_SRC = MT8188_AFE_IO_START,
	MT8188_AFE_IO_DMIC_IN,
	MT8188_AFE_IO_DPTX,
	MT8188_AFE_IO_ETDM_START,
	MT8188_AFE_IO_ETDM1_IN = MT8188_AFE_IO_ETDM_START,
	MT8188_AFE_IO_ETDM2_IN,
	MT8188_AFE_IO_ETDM1_OUT,
	MT8188_AFE_IO_ETDM2_OUT,
	MT8188_AFE_IO_ETDM3_OUT,
	MT8188_AFE_IO_ETDM_END,
	MT8188_AFE_IO_ETDM_NUM =
		(MT8188_AFE_IO_ETDM_END - MT8188_AFE_IO_ETDM_START),
	MT8188_AFE_IO_GASRC_START = MT8188_AFE_IO_ETDM_END,
	MT8188_AFE_IO_GASRC0 = MT8188_AFE_IO_GASRC_START,
	MT8188_AFE_IO_GASRC1,
	MT8188_AFE_IO_GASRC2,
	MT8188_AFE_IO_GASRC3,
	MT8188_AFE_IO_GASRC4,
	MT8188_AFE_IO_GASRC5,
	MT8188_AFE_IO_GASRC6,
	MT8188_AFE_IO_GASRC7,
	MT8188_AFE_IO_GASRC8,
	MT8188_AFE_IO_GASRC9,
	MT8188_AFE_IO_GASRC10,
	MT8188_AFE_IO_GASRC11,
	MT8188_AFE_IO_GASRC_END,
	MT8188_AFE_IO_GASRC_NUM =
		(MT8188_AFE_IO_GASRC_END - MT8188_AFE_IO_GASRC_START),
	MT8188_AFE_IO_HW_GAIN_START = MT8188_AFE_IO_GASRC_END,
	MT8188_AFE_IO_HW_GAIN1 = MT8188_AFE_IO_HW_GAIN_START,
	MT8188_AFE_IO_HW_GAIN2,
	MT8188_AFE_IO_HW_GAIN_END,
	MT8188_AFE_IO_HW_GAIN_NUM =
		(MT8188_AFE_IO_HW_GAIN_END - MT8188_AFE_IO_HW_GAIN_START),
	MT8188_AFE_IO_MULTI_IN_START = MT8188_AFE_IO_HW_GAIN_END,
	MT8188_AFE_IO_MULTI_IN1 = MT8188_AFE_IO_MULTI_IN_START,
	MT8188_AFE_IO_MULTI_IN2,
	MT8188_AFE_IO_MULTI_IN_END,
	MT8188_AFE_IO_MULTI_IN_NUM =
		(MT8188_AFE_IO_MULTI_IN_END - MT8188_AFE_IO_MULTI_IN_START),
	MT8188_AFE_IO_PCM = MT8188_AFE_IO_MULTI_IN_END,
	MT8188_AFE_IO_SPDIF_IN,
	MT8188_AFE_IO_SPDIF_OUT,
	MT8188_AFE_IO_UL_SRC1,
	MT8188_AFE_IO_END,
	MT8188_AFE_IO_NUM = (MT8188_AFE_IO_END - MT8188_AFE_IO_START),
	MT8188_AFE_HOSTLESS_FE_START = MT8188_AFE_IO_END,
	MT8188_AFE_HOSTLESS_FE_ADDA_LPBK = MT8188_AFE_HOSTLESS_FE_START,
	MT8188_AFE_HOSTLESS_FE_DMIC_RECORD,
	MT8188_AFE_HOSTLESS_FE_ETDM1_LPBK,
	MT8188_AFE_HOSTLESS_FE_END,
	MT8188_AFE_HOSTLESS_BE_START = MT8188_AFE_HOSTLESS_FE_END,
	MT8188_AFE_HOSTLESS_BE_DL_VIRTUAL_SOURCE =
		MT8188_AFE_HOSTLESS_BE_START,
	MT8188_AFE_HOSTLESS_BE_END,
	MT8188_DAI_END = MT8188_AFE_HOSTLESS_BE_END,
	MT8188_DAI_NUM = (MT8188_DAI_END - MT8188_DAI_START),
};

enum {
	MT8188_TOP_CG_A1SYS_TIMING,
	MT8188_TOP_CG_A2SYS_TIMING,
	MT8188_TOP_CG_A3SYS_TIMING,
	MT8188_TOP_CG_A4SYS_TIMING,
	MT8188_TOP_CG_26M_TIMING,
	MT8188_TOP_CG_GASRC0,
	MT8188_TOP_CG_GASRC1,
	MT8188_TOP_CG_GASRC2,
	MT8188_TOP_CG_GASRC3,
	MT8188_TOP_CG_GASRC4,
	MT8188_TOP_CG_GASRC5,
	MT8188_TOP_CG_GASRC6,
	MT8188_TOP_CG_GASRC7,
	MT8188_TOP_CG_GASRC8,
	MT8188_TOP_CG_GASRC9,
	MT8188_TOP_CG_GASRC10,
	MT8188_TOP_CG_GASRC11,
	MT8188_TOP_CG_NUM,
};

enum {
	MT8188_AFE_IRQ_1,
	MT8188_AFE_IRQ_2,
	MT8188_AFE_IRQ_3,
	MT8188_AFE_IRQ_8,
	MT8188_AFE_IRQ_9,
	MT8188_AFE_IRQ_10,
	MT8188_AFE_IRQ_13,
	MT8188_AFE_IRQ_14,
	MT8188_AFE_IRQ_15,
	MT8188_AFE_IRQ_16,
	MT8188_AFE_IRQ_17,
	MT8188_AFE_IRQ_18,
	MT8188_AFE_IRQ_19,
	MT8188_AFE_IRQ_20,
	MT8188_AFE_IRQ_21,
	MT8188_AFE_IRQ_22,
	MT8188_AFE_IRQ_23,
	MT8188_AFE_IRQ_24,
	MT8188_AFE_IRQ_25,
	MT8188_AFE_IRQ_26,
	MT8188_AFE_IRQ_27,
	MT8188_AFE_IRQ_28,
	MT8188_AFE_IRQ_NUM,
};

enum {
	MT8188_ETDM_OUT1_1X_EN = 9,
	MT8188_ETDM_OUT2_1X_EN = 10,
	MT8188_ETDM_OUT3_1X_EN = 11,
	MT8188_ETDM_IN1_1X_EN = 12,
	MT8188_ETDM_IN2_1X_EN = 13,
	MT8188_ETDM_IN1_NX_EN = 25,
	MT8188_ETDM_IN2_NX_EN = 26,
};

enum {
	MT8188_MTKAIF_MISO_0,
	MT8188_MTKAIF_MISO_1,
	MT8188_MTKAIF_MISO_NUM,
};

/* MT8188 SMC CALL Operations */
enum mt8188_audio_smc_call_op {
	MT8188_AUDIO_SMC_OP_INIT = 0,
	MT8188_AUDIO_SMC_OP_DRAM_REQUEST,
	MT8188_AUDIO_SMC_OP_DRAM_RELEASE,
	MT8188_AUDIO_SMC_OP_FM_REQUEST,
	MT8188_AUDIO_SMC_OP_FM_RELEASE,
	MT8188_AUDIO_SMC_OP_ADSP_REQUEST,
	MT8188_AUDIO_SMC_OP_ADSP_RELEASE,
	MT8188_AUDIO_SMC_OP_ENABLE_NORMAL_AFE,
	MT8188_AUDIO_SMC_OP_DISABLE_NORMAL_AFE,
	MT8188_AUDIO_SMC_OP_NUM
};

struct mtk_dai_memif_irq_priv {
	unsigned int asys_timing_sel;
};

struct mtkaif_param {
	bool mtkaif_calibration_ok;
	int mtkaif_chosen_phase[MT8188_MTKAIF_MISO_NUM];
	int mtkaif_phase_cycle[MT8188_MTKAIF_MISO_NUM];
	int mtkaif_dmic_on;
	int mtkaif_adda6_only;
};

struct clk;

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
struct mt8188_adsp_data {
	/* information adsp supply */
	bool adsp_on;
	int (*hostless_active)(void);
	/* information afe supply */
	int (*set_afe_memif)(struct mtk_base_afe *afe, int memif_id,
				unsigned int channels, unsigned int rate,
				snd_pcm_format_t format);
	int (*set_afe_memif_enable)(struct mtk_base_afe *afe,
				int memif_id, unsigned int rate,
				unsigned int period_size, int cmd);
	void (*set_afe_init)(struct mtk_base_afe *afe);
	void (*set_afe_uninit)(struct mtk_base_afe *afe);
};
#endif

struct mt8188_afe_private {
	struct clk **clk;
	struct clk_lookup **lookup;
	struct regmap *topckgen;
	int pm_runtime_bypass_reg_ctl;
#ifdef CONFIG_DEBUG_FS
	struct dentry **debugfs_dentry;
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	struct mt8188_adsp_data adsp_data;

	/* backup clk parents for resume */
	struct clk *backup_aud_intbus_parents;
	struct clk *backup_a1sys_hp_parents;
	struct clk *backup_a2sys_parents;
	struct clk *backup_a3sys_parents;
	struct clk *backup_a4sys_parents;
#endif
	int afe_on_ref_cnt;
	int top_cg_ref_cnt[MT8188_TOP_CG_NUM];
	spinlock_t afe_ctrl_lock;
	int dram_resource_counter;
	struct mtk_dai_memif_irq_priv irq_priv[MT8188_AFE_IRQ_NUM];
	struct mtkaif_param mtkaif_params;
	uint32_t sram_base;
	uint32_t sram_size;

	/* dai */
	void *dai_priv[MT8188_DAI_NUM];
};

int mt8188_afe_fs_timing(unsigned int rate);
/* dai register */
int mt8188_dai_adda_register(struct mtk_base_afe *afe);
int mt8188_dai_dir_register(struct mtk_base_afe *afe);
int mt8188_dai_dit_register(struct mtk_base_afe *afe);
int mt8188_dai_dmic_register(struct mtk_base_afe *afe);
int mt8188_dai_etdm_register(struct mtk_base_afe *afe);
int mt8188_dai_gasrc_register(struct mtk_base_afe *afe);
int mt8188_dai_hostless_register(struct mtk_base_afe *afe);
int mt8188_dai_hw_gain_register(struct mtk_base_afe *afe);
int mt8188_dai_multi_in_register(struct mtk_base_afe *afe);
int mt8188_dai_pcm_register(struct mtk_base_afe *afe);

#define MT8188_SOC_ENUM_EXT(xname, xenum, xhandler_get, xhandler_put, id) \
{ \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = xhandler_get, .put = xhandler_put, \
	.device = id, \
	.private_value = (unsigned long)&xenum, \
}

#define MT8188_SOC_SINGLE_EXT(xname, xreg, xshift, xmax, xinvert,\
	xhandler_get, xhandler_put, id, sub_id) \
{ \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_volsw, \
	.get = xhandler_get, .put = xhandler_put, \
	.device = id, \
	.subdevice = sub_id, \
	.private_value = SOC_SINGLE_VALUE(xreg, xshift, xmax, xinvert, 0), \
}

#endif
