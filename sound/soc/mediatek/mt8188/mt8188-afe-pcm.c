// SPDX-License-Identifier: GPL-2.0
/*
 * Mediatek ALSA SoC AFE platform driver for 8188
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/soc/mediatek/infracfg.h>
#include <sound/pcm_params.h>

#include "mt8188-afe-common.h"
#include "mt8188-afe-clk.h"
#include "mt8188-afe-debug.h"
#include "mt8188-reg.h"
#include "../common/mtk-afe-platform-driver.h"
#include "../common/mtk-afe-fe-dai.h"
#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
#include "../common/mtk-sram-manager.h"
#endif

#define MT8188_MEMIF_BUFFER_BYTES_ALIGN  (0x40)
#define MT8188_MEMIF_DL7_MAX_PERIOD_SIZE (0x3fff)

#define MEMIF_AXI_MINLEN 9 //register default value

struct mtk_dai_memif_priv {
	unsigned int asys_timing_sel;
	unsigned int fs_timing;
};

static const struct snd_pcm_hardware mt8188_afe_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_MMAP_VALID,
	.formats = SNDRV_PCM_FMTBIT_S16_LE |
		   SNDRV_PCM_FMTBIT_S24_LE |
		   SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min = 64,
	.period_bytes_max = 256 * 1024,
	.periods_min = 2,
	.periods_max = 256,
	.buffer_bytes_max = 256 * 2 * 1024,
};

struct mt8188_afe_rate {
	unsigned int rate;
	unsigned int reg_value;
};

static const struct mt8188_afe_rate mt8188_afe_rates[] = {
	{ .rate = 8000, .reg_value = 0, },
	{ .rate = 12000, .reg_value = 1, },
	{ .rate = 16000, .reg_value = 2, },
	{ .rate = 24000, .reg_value = 3, },
	{ .rate = 32000, .reg_value = 4, },
	{ .rate = 48000, .reg_value = 5, },
	{ .rate = 96000, .reg_value = 6, },
	{ .rate = 192000, .reg_value = 7, },
	{ .rate = 384000, .reg_value = 8, },
	{ .rate = 7350, .reg_value = 16, },
	{ .rate = 11025, .reg_value = 17, },
	{ .rate = 14700, .reg_value = 18, },
	{ .rate = 22050, .reg_value = 19, },
	{ .rate = 29400, .reg_value = 20, },
	{ .rate = 44100, .reg_value = 21, },
	{ .rate = 88200, .reg_value = 22, },
	{ .rate = 176400, .reg_value = 23, },
	{ .rate = 352800, .reg_value = 24, },
};

int mt8188_afe_fs_timing(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mt8188_afe_rates); i++)
		if (mt8188_afe_rates[i].rate == rate)
			return mt8188_afe_rates[i].reg_value;

	return -EINVAL;
}

static int mt8188_memif_fs(struct snd_pcm_substream *substream,
			   unsigned int rate)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_component *component = NULL;
	struct mtk_base_afe *afe = NULL;
	struct mt8188_afe_private *afe_priv = NULL;
	struct mtk_base_afe_memif *memif = NULL;
	struct mtk_dai_memif_priv *memif_priv = NULL;
	int fs = mt8188_afe_fs_timing(rate);
	int id = asoc_rtd_to_cpu(rtd, 0)->id;

	if (id < 0)
		return -EINVAL;

	component = snd_soc_rtdcom_lookup(rtd, AFE_PCM_NAME);
	if (!component)
		return -EINVAL;

	afe = snd_soc_component_get_drvdata(component);
	memif = &afe->memif[id];

	switch (memif->data->id) {
	case MT8188_AFE_MEMIF_DL10:
		fs = MT8188_ETDM_OUT3_1X_EN;
		break;
	case MT8188_AFE_MEMIF_UL8:
		fs = MT8188_ETDM_IN1_NX_EN;
		break;
	case MT8188_AFE_MEMIF_UL3:
		fs = MT8188_ETDM_IN2_NX_EN;
		break;
	default:
		afe_priv = afe->platform_priv;
		memif_priv = afe_priv->dai_priv[id];
		if (memif_priv->fs_timing)
			fs = memif_priv->fs_timing;
		break;
	}

	return fs;
}

static int mt8188_irq_fs(struct snd_pcm_substream *substream,
	unsigned int rate)
{
	int fs = mt8188_memif_fs(substream, rate);

	switch (fs) {
	case MT8188_ETDM_IN1_NX_EN:
		fs = MT8188_ETDM_IN1_1X_EN;
		break;
	case MT8188_ETDM_IN2_NX_EN:
		fs = MT8188_ETDM_IN2_1X_EN;
		break;
	default:
		break;
	}

	return fs;
}

enum {
	MT8188_AFE_CM0,
	MT8188_AFE_CM1,
	MT8188_AFE_CM2,
	MT8188_AFE_CM_NUM,
};

struct mt8188_afe_channel_merge {
	int id;
	int reg;
	unsigned int sel_shift;
	unsigned int sel_maskbit;
	unsigned int sel_default;
	unsigned int ch_num_shift;
	unsigned int ch_num_maskbit;
	unsigned int en_shift;
	unsigned int en_maskbit;
	unsigned int update_cnt_shift;
	unsigned int update_cnt_maskbit;
	unsigned int update_cnt_default;
};

static const struct mt8188_afe_channel_merge
	mt8188_afe_cm[MT8188_AFE_CM_NUM] = {
	[MT8188_AFE_CM0] = {
		.id = MT8188_AFE_CM0,
		.reg = AFE_CM0_CON,
		.sel_shift = 30,
		.sel_maskbit = 0x1,
		.sel_default = 1,
		.ch_num_shift = 2,
		.ch_num_maskbit = 0x3f,
		.en_shift = 0,
		.en_maskbit = 0x1,
		.update_cnt_shift = 16,
		.update_cnt_maskbit = 0x1fff,
		.update_cnt_default = 0x3,
	},
	[MT8188_AFE_CM1] = {
		.id = MT8188_AFE_CM1,
		.reg = AFE_CM1_CON,
		.sel_shift = 30,
		.sel_maskbit = 0x1,
		.sel_default = 1,
		.ch_num_shift = 2,
		.ch_num_maskbit = 0x1f,
		.en_shift = 0,
		.en_maskbit = 0x1,
		.update_cnt_shift = 16,
		.update_cnt_maskbit = 0x1fff,
		.update_cnt_default = 0x3,
	},
	[MT8188_AFE_CM2] = {
		.id = MT8188_AFE_CM2,
		.reg = AFE_CM2_CON,
		.sel_shift = 30,
		.sel_maskbit = 0x1,
		.sel_default = 1,
		.ch_num_shift = 2,
		.ch_num_maskbit = 0x1f,
		.en_shift = 0,
		.en_maskbit = 0x1,
		.update_cnt_shift = 16,
		.update_cnt_maskbit = 0x1fff,
		.update_cnt_default = 0x3,
	},
};

static int mt8188_afe_memif_is_ul(int id)
{
	if (id >= MT8188_AFE_MEMIF_UL_START && id < MT8188_AFE_MEMIF_END)
		return 1;
	else
		return 0;
}

static const struct mt8188_afe_channel_merge *mt8188_afe_found_cm(
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = -EINVAL;

	if (mt8188_afe_memif_is_ul(dai->id) == 0)
		return NULL;

	switch (dai->id) {
	case MT8188_AFE_MEMIF_UL9:
		id = MT8188_AFE_CM0;
		break;
	case MT8188_AFE_MEMIF_UL2:
		id = MT8188_AFE_CM1;
		break;
	case MT8188_AFE_MEMIF_UL10:
		id = MT8188_AFE_CM2;
		break;
	default:
		break;
	}

	if (id < 0) {
		dev_info(afe->dev, "%s, memif %d cannot find CM!\n", __func__, dai->id);
		return NULL;
	}

	return &mt8188_afe_cm[id];
}

static int mt8188_afe_config_cm(struct mtk_base_afe *afe,
	const struct mt8188_afe_channel_merge *cm, unsigned int channels)
{
	if (!cm)
		return -EINVAL;

	regmap_update_bits(afe->regmap,
		cm->reg,
		cm->sel_maskbit << cm->sel_shift,
		cm->sel_default << cm->sel_shift);

	regmap_update_bits(afe->regmap,
		cm->reg,
		cm->ch_num_maskbit << cm->ch_num_shift,
		(channels - 1) << cm->ch_num_shift);

	regmap_update_bits(afe->regmap,
		cm->reg,
		cm->update_cnt_maskbit << cm->update_cnt_shift,
		cm->update_cnt_default << cm->update_cnt_shift);

	return 0;
}

static int mt8188_afe_enable_cm(struct mtk_base_afe *afe,
	const struct mt8188_afe_channel_merge *cm, bool enable)
{
	if (!cm)
		return -EINVAL;

	regmap_update_bits(afe->regmap,
		cm->reg,
		cm->en_maskbit << cm->en_shift,
		enable << cm->en_shift);

	return 0;
}

static int mt8188_afe_fe_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	int ret = 0;

	ret = mtk_afe_fe_startup(substream, dai);

	snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
		MT8188_MEMIF_BUFFER_BYTES_ALIGN);

	if (id == MT8188_AFE_MEMIF_DL7) {
		ret = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_PERIOD_SIZE, 1,
			MT8188_MEMIF_DL7_MAX_PERIOD_SIZE);
		if (ret < 0) {
			dev_dbg(afe->dev, "hw_constraint_minmax failed\n");
			return ret;
		}
	}

	return ret;
}

static void mt8188_afe_fe_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	mtk_afe_fe_shutdown(substream, dai);
}

static int mt8188_afe_fe_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	const struct mtk_base_memif_data *data = memif->data;
	const struct mt8188_afe_channel_merge *cm = mt8188_afe_found_cm(dai);
	int ret;
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);
	snd_pcm_format_t format = params_format(params);

	mt8188_afe_config_cm(afe, cm, channels);

	if (data->ch_num_reg >= 0) {
		regmap_update_bits(afe->regmap, data->ch_num_reg,
			data->ch_num_maskbit << data->ch_num_shift,
			channels << data->ch_num_shift);
	}

#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
	/*
	 * hw_params may be called several time,
	 * free sram of this substream first
	 */
	mtk_audio_sram_free(afe->sram, substream);

	substream->runtime->dma_bytes = params_buffer_bytes(params);

	/*
	 * mtk_audio_sram_allocate allocates all free memory for one memif if
	 * it is required. When more than one memif uses SRAM at the same time,
	 * it's better to limit the requested size for one memif in case some
	 * application occupies all SRAM and it results in the low power feature
	 * using DRAM.
	 */
	if (memif->use_dram_only == 0 &&
	    mtk_audio_sram_allocate(afe->sram,
				    &substream->runtime->dma_addr,
				    &substream->runtime->dma_area,
				    substream->runtime->dma_bytes,
				    substream,
				    params_format(params), false) == 0) {
		memif->using_sram = 1;
	} else
#endif
	{
		memif->using_sram = 0;
		/* MT8188 AFE need to remap DRAM address. Cannot reuse mtk_afe_fe_hw_params */
		ret = snd_pcm_lib_malloc_pages(substream,
					       params_buffer_bytes(params));
		if (ret < 0)
			return ret;

		dev_info(afe->dev, "%s(), before remap: dma_addr %pad\n",
			 __func__, &substream->runtime->dma_addr);

		substream->runtime->dma_addr += EMI_OFFSET;
		if (afe->request_dram_resource)
			afe->request_dram_resource(afe->dev);
	}

	dev_info(afe->dev, "%s(), %s, ch %d, rate %d, fmt %d, dma_addr %pad, dma_area %p, dma_bytes 0x%zx\n",
		 __func__, memif->data->name, channels, rate, format,
		 &substream->runtime->dma_addr, substream->runtime->dma_area,
		 substream->runtime->dma_bytes);

	memset_io(substream->runtime->dma_area, 0,
		  substream->runtime->dma_bytes);

	/* set addr */
	ret = mtk_memif_set_addr(afe, id,
				 substream->runtime->dma_area,
				 substream->runtime->dma_addr,
				 substream->runtime->dma_bytes);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set addr, ret %d\n",
			 __func__, id, ret);
		return ret;
	}

	/* set channel */
	ret = mtk_memif_set_channel(afe, id, channels);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set channel %d, ret %d\n",
			 __func__, id, channels, ret);
		return ret;
	}

	/* set rate */
	ret = mtk_memif_set_rate_substream(substream, id, rate);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set rate %d, ret %d\n",
			 __func__, id, rate, ret);
		return ret;
	}

	/* set format */
	ret = mtk_memif_set_format(afe, id, format);
	if (ret) {
		dev_info(afe->dev, "%s(), error, id %d, set format %d, ret %d\n",
			 __func__, id, format, ret);
		return ret;
	}

	return 0;
}

static int mt8188_afe_fe_hw_free(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return mtk_afe_fe_hw_free(substream, dai);
}

static int mt8188_afe_fe_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return mtk_afe_fe_prepare(substream, dai);
}

static int mt8188_afe_fe_common_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime * const runtime = substream->runtime;
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int counter = runtime->period_size;
	int fs;
	int ret;

	dev_dbg(afe->dev, "%s, %s cmd=%d\n", __func__, memif->data->name, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = mtk_memif_set_enable(afe, id);
		if (ret) {
			dev_dbg(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
			return ret;
		}

		/* set irq counter */
		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   irq_data->irq_cnt_maskbit << irq_data->irq_cnt_shift,
				   counter << irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = afe->irq_fs(substream, runtime->rate);

		if (fs < 0)
			return -EINVAL;

		if (irq_data->irq_fs_reg >= 0)
			regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
					   irq_data->irq_fs_maskbit << irq_data->irq_fs_shift,
					   fs << irq_data->irq_fs_shift);

		/* add delay for uplink engine */
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			uint32_t sample_delay;
			sample_delay = ((MEMIF_AXI_MINLEN + 1) * 64 +
					(runtime->channels * runtime->sample_bits -1 ))/
					(runtime->channels * runtime->sample_bits) + 1;

			udelay(sample_delay * 1000000 / runtime->rate);
		}

		/* enable interrupt */
		regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
				   1 << irq_data->irq_en_shift,
				   1 << irq_data->irq_en_shift);

		return 0;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = mtk_memif_set_disable(afe, id);
		if (ret) {
			dev_dbg(afe->dev, "%s(), error, id %d, memif enable, ret %d\n",
				__func__, id, ret);
		}

		/* disable interrupt */
		regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
			1 << irq_data->irq_en_shift,
			0 << irq_data->irq_en_shift);
		/* and clear pending IRQ */
		regmap_write(afe->regmap, irq_data->irq_clr_reg,
			     1 << irq_data->irq_clr_shift);
		return ret;
	default:
		return -EINVAL;
	}
}

static int mt8188_afe_fe_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	struct mtk_base_afe *afe = snd_soc_dai_get_drvdata(dai);
	const struct mt8188_afe_channel_merge *cm = mt8188_afe_found_cm(dai);
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt8188_afe_enable_cm(afe, cm, true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8188_afe_enable_cm(afe, cm, false);
		break;
	default:
		break;
	}

	ret = mt8188_afe_fe_common_trigger(substream, cmd, dai);

	return ret;
}

static int mt8188_afe_fe_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	return 0;
}

static const struct snd_soc_dai_ops mt8188_afe_fe_dai_ops = {
	.startup	= mt8188_afe_fe_startup,
	.shutdown	= mt8188_afe_fe_shutdown,
	.hw_params	= mt8188_afe_fe_hw_params,
	.hw_free	= mt8188_afe_fe_hw_free,
	.prepare	= mt8188_afe_fe_prepare,
	.trigger	= mt8188_afe_fe_trigger,
	.set_fmt	= mt8188_afe_fe_set_fmt,
};

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200|\
		       SNDRV_PCM_RATE_96000|\
		       SNDRV_PCM_RATE_176400|\
		       SNDRV_PCM_RATE_192000|\
		       SNDRV_PCM_RATE_352800|\
		       SNDRV_PCM_RATE_384000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver mt8188_memif_dai_driver[] = {
	/* FE DAIs: memory intefaces to CPU */
	{
		.name = "DL2",
		.id = MT8188_AFE_MEMIF_DL2,
		.playback = {
			.stream_name = "DL2",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "DL3",
		.id = MT8188_AFE_MEMIF_DL3,
		.playback = {
			.stream_name = "DL3",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "DL6",
		.id = MT8188_AFE_MEMIF_DL6,
		.playback = {
			.stream_name = "DL6",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "DL7",
		.id = MT8188_AFE_MEMIF_DL7,
		.playback = {
			.stream_name = "DL7",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "DL8",
		.id = MT8188_AFE_MEMIF_DL8,
		.playback = {
			.stream_name = "DL8",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "DL10",
		.id = MT8188_AFE_MEMIF_DL10,
		.playback = {
			.stream_name = "DL10",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "DL11",
		.id = MT8188_AFE_MEMIF_DL11,
		.playback = {
			.stream_name = "DL11",
			.channels_min = 1,
			.channels_max = 32,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL1",
		.id = MT8188_AFE_MEMIF_UL1,
		.capture = {
			.stream_name = "UL1",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL2",
		.id = MT8188_AFE_MEMIF_UL2,
		.capture = {
			.stream_name = "UL2",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL3",
		.id = MT8188_AFE_MEMIF_UL3,
		.capture = {
			.stream_name = "UL3",
			.channels_min = 1,
			.channels_max = 16,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL4",
		.id = MT8188_AFE_MEMIF_UL4,
		.capture = {
			.stream_name = "UL4",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL5",
		.id = MT8188_AFE_MEMIF_UL5,
		.capture = {
			.stream_name = "UL5",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL6",
		.id = MT8188_AFE_MEMIF_UL6,
		.capture = {
			.stream_name = "UL6",
			.channels_min = 1,
			.channels_max = 8,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL8",
		.id = MT8188_AFE_MEMIF_UL8,
		.capture = {
			.stream_name = "UL8",
			.channels_min = 1,
			.channels_max = 24,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL9",
		.id = MT8188_AFE_MEMIF_UL9,
		.capture = {
			.stream_name = "UL9",
			.channels_min = 1,
			.channels_max = 32,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
	{
		.name = "UL10",
		.id = MT8188_AFE_MEMIF_UL10,
		.capture = {
			.stream_name = "UL10",
			.channels_min = 1,
			.channels_max = 4,
			.rates = MTK_PCM_RATES,
			.formats = MTK_PCM_FORMATS,
		},
		.ops = &mt8188_afe_fe_dai_ops,
	},
};

static const struct snd_kcontrol_new o002_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN2, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I004 Switch", AFE_CONN2, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN2, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN2, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I022 Switch", AFE_CONN2, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN2_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I072 Switch", AFE_CONN2_2, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN2_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I168 Switch", AFE_CONN2_5, 8, 1, 0),
};

static const struct snd_kcontrol_new o003_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I005 Switch", AFE_CONN3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I006 Switch", AFE_CONN3, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN3, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I023 Switch", AFE_CONN3, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN3_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I073 Switch", AFE_CONN3_2, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN3_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I169 Switch", AFE_CONN3_5, 9, 1, 0),
};

static const struct snd_kcontrol_new o004_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN4, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I006 Switch", AFE_CONN4, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I008 Switch", AFE_CONN4, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I014 Switch", AFE_CONN4, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I024 Switch", AFE_CONN4, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I074 Switch", AFE_CONN4_2, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN4_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN4_3, 2, 1, 0),
};

static const struct snd_kcontrol_new o005_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN5, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I007 Switch", AFE_CONN5, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I010 Switch", AFE_CONN5, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I015 Switch", AFE_CONN5, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I025 Switch", AFE_CONN5, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I075 Switch", AFE_CONN5_2, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN5_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN5_3, 3, 1, 0),
};

static const struct snd_kcontrol_new o006_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN6, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I008 Switch", AFE_CONN6, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I016 Switch", AFE_CONN6, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I026 Switch", AFE_CONN6, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I076 Switch", AFE_CONN6_2, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN6_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN6_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN6_3, 4, 1, 0),
};

static const struct snd_kcontrol_new o007_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN7, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I009 Switch", AFE_CONN7, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I017 Switch", AFE_CONN7, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I027 Switch", AFE_CONN7, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I077 Switch", AFE_CONN7_2, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN7_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN7_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN7_3, 5, 1, 0),
};

static const struct snd_kcontrol_new o008_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I010 Switch", AFE_CONN8, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I018 Switch", AFE_CONN8, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I028 Switch", AFE_CONN8, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I078 Switch", AFE_CONN8_2, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN8_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN8_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN8_3, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN8_3, 6, 1, 0),
};

static const struct snd_kcontrol_new o009_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I011 Switch", AFE_CONN9, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I019 Switch", AFE_CONN9, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I029 Switch", AFE_CONN9, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I079 Switch", AFE_CONN9_2, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN9_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN9_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN9_3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN9_3, 7, 1, 0),
};

static const struct snd_kcontrol_new o010_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I022 Switch", AFE_CONN10, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I030 Switch", AFE_CONN10, 30, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I046 Switch", AFE_CONN10_1, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I072 Switch", AFE_CONN10_2, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I080 Switch", AFE_CONN10_2, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN10_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN10_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN10_3, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN10_3, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I104 Switch", AFE_CONN10_3, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I188 Switch", AFE_CONN10_5, 28, 1, 0),
};

static const struct snd_kcontrol_new o011_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I023 Switch", AFE_CONN11, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I031 Switch", AFE_CONN11, 31, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I047 Switch", AFE_CONN11_1, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I073 Switch", AFE_CONN11_2, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I081 Switch", AFE_CONN11_2, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN11_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN11_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN11_3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN11_3, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I105 Switch", AFE_CONN11_3, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I189 Switch", AFE_CONN11_5, 29, 1, 0),
};

static const struct snd_kcontrol_new o012_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I024 Switch", AFE_CONN12, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I032 Switch", AFE_CONN12_1, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I048 Switch", AFE_CONN12_1, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I074 Switch", AFE_CONN12_2, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I082 Switch", AFE_CONN12_2, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN12_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN12_3, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN12_3, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I104 Switch", AFE_CONN12_3, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I106 Switch", AFE_CONN12_3, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I190 Switch", AFE_CONN12_5, 30, 1, 0),
};

static const struct snd_kcontrol_new o013_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I025 Switch", AFE_CONN13, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I033 Switch", AFE_CONN13_1, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I049 Switch", AFE_CONN13_1, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I075 Switch", AFE_CONN13_2, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I083 Switch", AFE_CONN13_2, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN13_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN13_3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN13_3, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I105 Switch", AFE_CONN13_3, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I107 Switch", AFE_CONN13_3, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I191 Switch", AFE_CONN13_5, 31, 1, 0),
};

static const struct snd_kcontrol_new o014_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I026 Switch", AFE_CONN14, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I034 Switch", AFE_CONN14_1, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I050 Switch", AFE_CONN14_1, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I076 Switch", AFE_CONN14_2, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I084 Switch", AFE_CONN14_2, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN14_3, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN14_3, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I104 Switch", AFE_CONN14_3, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I106 Switch", AFE_CONN14_3, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I108 Switch", AFE_CONN14_3, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I192 Switch", AFE_CONN14_6, 0, 1, 0),
};

static const struct snd_kcontrol_new o015_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I027 Switch", AFE_CONN15, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I035 Switch", AFE_CONN15_1, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I051 Switch", AFE_CONN15_1, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I077 Switch", AFE_CONN15_2, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I085 Switch", AFE_CONN15_2, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN15_3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN15_3, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I105 Switch", AFE_CONN15_3, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I107 Switch", AFE_CONN15_3, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I109 Switch", AFE_CONN15_3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I193 Switch", AFE_CONN15_6, 1, 1, 0),
};

static const struct snd_kcontrol_new o016_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I028 Switch", AFE_CONN16, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I036 Switch", AFE_CONN16_1, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I052 Switch", AFE_CONN16_1, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I078 Switch", AFE_CONN16_2, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I086 Switch", AFE_CONN16_2, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN16_3, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I104 Switch", AFE_CONN16_3, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I106 Switch", AFE_CONN16_3, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I108 Switch", AFE_CONN16_3, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I110 Switch", AFE_CONN16_3, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I194 Switch", AFE_CONN16_6, 2, 1, 0),
};

static const struct snd_kcontrol_new o017_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I029 Switch", AFE_CONN17, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I037 Switch", AFE_CONN17_1, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I053 Switch", AFE_CONN17_1, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I079 Switch", AFE_CONN17_2, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I087 Switch", AFE_CONN17_2, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN17_3, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I105 Switch", AFE_CONN17_3, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I107 Switch", AFE_CONN17_3, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I109 Switch", AFE_CONN17_3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I111 Switch", AFE_CONN17_3, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I195 Switch", AFE_CONN17_6, 3, 1, 0),
};

static const struct snd_kcontrol_new o018_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I080 Switch", AFE_CONN18_2, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I104 Switch", AFE_CONN18_3, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I106 Switch", AFE_CONN18_3, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I108 Switch", AFE_CONN18_3, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I110 Switch", AFE_CONN18_3, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I112 Switch", AFE_CONN18_3, 16, 1, 0),
};

static const struct snd_kcontrol_new o019_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I081 Switch", AFE_CONN19_2, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I105 Switch", AFE_CONN19_3, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I107 Switch", AFE_CONN19_3, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I109 Switch", AFE_CONN19_3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I111 Switch", AFE_CONN19_3, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I113 Switch", AFE_CONN19_3, 17, 1, 0),
};

static const struct snd_kcontrol_new o020_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I082 Switch", AFE_CONN20_2, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I106 Switch", AFE_CONN20_3, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I108 Switch", AFE_CONN20_3, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I110 Switch", AFE_CONN20_3, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I112 Switch", AFE_CONN20_3, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I114 Switch", AFE_CONN20_3, 18, 1, 0),
};

static const struct snd_kcontrol_new o021_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I083 Switch", AFE_CONN21_2, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I107 Switch", AFE_CONN21_3, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I109 Switch", AFE_CONN21_3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I111 Switch", AFE_CONN21_3, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I113 Switch", AFE_CONN21_3, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I115 Switch", AFE_CONN21_3, 19, 1, 0),
};

static const struct snd_kcontrol_new o022_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I084 Switch", AFE_CONN22_2, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I108 Switch", AFE_CONN22_3, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I110 Switch", AFE_CONN22_3, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I112 Switch", AFE_CONN22_3, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I114 Switch", AFE_CONN22_3, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I116 Switch", AFE_CONN22_3, 20, 1, 0),
};

static const struct snd_kcontrol_new o023_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I085 Switch", AFE_CONN23_2, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I109 Switch", AFE_CONN23_3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I111 Switch", AFE_CONN23_3, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I113 Switch", AFE_CONN23_3, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I115 Switch", AFE_CONN23_3, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I117 Switch", AFE_CONN23_3, 21, 1, 0),
};

static const struct snd_kcontrol_new o024_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I086 Switch", AFE_CONN24_2, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I110 Switch", AFE_CONN24_3, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I112 Switch", AFE_CONN24_3, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I114 Switch", AFE_CONN24_3, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I116 Switch", AFE_CONN24_3, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I118 Switch", AFE_CONN24_3, 22, 1, 0),
};

static const struct snd_kcontrol_new o025_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I087 Switch", AFE_CONN25_2, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I111 Switch", AFE_CONN25_3, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I113 Switch", AFE_CONN25_3, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I115 Switch", AFE_CONN25_3, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I117 Switch", AFE_CONN25_3, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I119 Switch", AFE_CONN25_3, 23, 1, 0),
};

static const struct snd_kcontrol_new o026_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I046 Switch", AFE_CONN26_1, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I112 Switch", AFE_CONN26_3, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I114 Switch", AFE_CONN26_3, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I116 Switch", AFE_CONN26_3, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I118 Switch", AFE_CONN26_3, 22, 1, 0),
};

static const struct snd_kcontrol_new o027_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I047 Switch", AFE_CONN27_1, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I113 Switch", AFE_CONN27_3, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I115 Switch", AFE_CONN27_3, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I117 Switch", AFE_CONN27_3, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I119 Switch", AFE_CONN27_3, 23, 1, 0),
};

static const struct snd_kcontrol_new o028_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I048 Switch", AFE_CONN28_1, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I114 Switch", AFE_CONN28_3, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I116 Switch", AFE_CONN28_3, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I118 Switch", AFE_CONN28_3, 22, 1, 0),
};

static const struct snd_kcontrol_new o029_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I049 Switch", AFE_CONN29_1, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I115 Switch", AFE_CONN29_3, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I117 Switch", AFE_CONN29_3, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I119 Switch", AFE_CONN29_3, 23, 1, 0),
};

static const struct snd_kcontrol_new o030_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I050 Switch", AFE_CONN30_1, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I116 Switch", AFE_CONN30_3, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I118 Switch", AFE_CONN30_3, 22, 1, 0),
};

static const struct snd_kcontrol_new o031_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I051 Switch", AFE_CONN31_1, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I117 Switch", AFE_CONN31_3, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I119 Switch", AFE_CONN31_3, 23, 1, 0),
};

static const struct snd_kcontrol_new o032_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I052 Switch", AFE_CONN32_1, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I118 Switch", AFE_CONN32_3, 22, 1, 0),
};

static const struct snd_kcontrol_new o033_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I053 Switch", AFE_CONN33_1, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I119 Switch", AFE_CONN33_3, 23, 1, 0),
};

static const struct snd_kcontrol_new o034_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN34, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I002 Switch", AFE_CONN34, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN34, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN34_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN34_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN34_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN34_3, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN34_3, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I104 Switch", AFE_CONN34_3, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I106 Switch", AFE_CONN34_3, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I108 Switch", AFE_CONN34_3, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I110 Switch", AFE_CONN34_3, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I112 Switch", AFE_CONN34_3, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I114 Switch", AFE_CONN34_3, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I116 Switch", AFE_CONN34_3, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I118 Switch", AFE_CONN34_3, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I136 Switch", AFE_CONN34_4, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I138 Switch", AFE_CONN34_4, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I168 Switch", AFE_CONN34_5, 8, 1, 0),
};

static const struct snd_kcontrol_new o035_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN35, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I003 Switch", AFE_CONN35, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN35, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN35_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN35_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN35_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN35_3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN35_3, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I105 Switch", AFE_CONN35_3, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I107 Switch", AFE_CONN35_3, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I109 Switch", AFE_CONN35_3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I111 Switch", AFE_CONN35_3, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I113 Switch", AFE_CONN35_3, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I115 Switch", AFE_CONN35_3, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I117 Switch", AFE_CONN35_3, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I119 Switch", AFE_CONN35_3, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I137 Switch", AFE_CONN35_4, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I139 Switch", AFE_CONN35_4, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I168 Switch", AFE_CONN35_5, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I169 Switch", AFE_CONN35_5, 9, 1, 0),
};

static const struct snd_kcontrol_new o036_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I000 Switch", AFE_CONN36, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN36, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I070 Switch", AFE_CONN36_2, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN36_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN36_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN36_3, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN36_3, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I104 Switch", AFE_CONN36_3, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I106 Switch", AFE_CONN36_3, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I108 Switch", AFE_CONN36_3, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I110 Switch", AFE_CONN36_3, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I112 Switch", AFE_CONN36_3, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I114 Switch", AFE_CONN36_3, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I116 Switch", AFE_CONN36_3, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I118 Switch", AFE_CONN36_3, 22, 1, 0),
};

static const struct snd_kcontrol_new o037_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I001 Switch", AFE_CONN37, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN37, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I071 Switch", AFE_CONN37_2, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN37_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN37_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN37_3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN37_3, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I105 Switch", AFE_CONN37_3, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I107 Switch", AFE_CONN37_3, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I109 Switch", AFE_CONN37_3, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I111 Switch", AFE_CONN37_3, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I113 Switch", AFE_CONN37_3, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I115 Switch", AFE_CONN37_3, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I117 Switch", AFE_CONN37_3, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I119 Switch", AFE_CONN37_3, 23, 1, 0),
};

static const struct snd_kcontrol_new o038_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I004 Switch", AFE_CONN38, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I022 Switch", AFE_CONN38, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN38_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I168 Switch", AFE_CONN38_5, 8, 1, 0),
};

static const struct snd_kcontrol_new o039_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I005 Switch", AFE_CONN39, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I023 Switch", AFE_CONN39, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN39_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I169 Switch", AFE_CONN39_5, 9, 1, 0),
};

static const struct snd_kcontrol_new o040_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I002 Switch", AFE_CONN40, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I004 Switch", AFE_CONN40, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I012 Switch", AFE_CONN40, 12, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I022 Switch", AFE_CONN40, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN40_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I168 Switch", AFE_CONN40_5, 8, 1, 0),
};

static const struct snd_kcontrol_new o041_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I003 Switch", AFE_CONN41, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I005 Switch", AFE_CONN41, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I006 Switch", AFE_CONN41, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I013 Switch", AFE_CONN41, 13, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I023 Switch", AFE_CONN41, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN41_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I169 Switch", AFE_CONN41_5, 9, 1, 0),
};

static const struct snd_kcontrol_new o042_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I006 Switch", AFE_CONN42, 6, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I008 Switch", AFE_CONN42, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I014 Switch", AFE_CONN42, 14, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I024 Switch", AFE_CONN42, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN42_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN42_3, 2, 1, 0),
};

static const struct snd_kcontrol_new o043_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I007 Switch", AFE_CONN43, 7, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I010 Switch", AFE_CONN43, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I015 Switch", AFE_CONN43, 15, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I025 Switch", AFE_CONN43, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN43_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN43_3, 3, 1, 0),
};

static const struct snd_kcontrol_new o044_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I008 Switch", AFE_CONN44, 8, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I016 Switch", AFE_CONN44, 16, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I026 Switch", AFE_CONN44, 26, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN44_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN44_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN44_3, 4, 1, 0),
};

static const struct snd_kcontrol_new o045_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I009 Switch", AFE_CONN45, 9, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I017 Switch", AFE_CONN45, 17, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I027 Switch", AFE_CONN45, 27, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN45_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN45_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN45_3, 5, 1, 0),
};

static const struct snd_kcontrol_new o046_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I010 Switch", AFE_CONN46, 10, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I018 Switch", AFE_CONN46, 18, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I028 Switch", AFE_CONN46, 28, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN46_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN46_3, 2, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I100 Switch", AFE_CONN46_3, 4, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I102 Switch", AFE_CONN46_3, 6, 1, 0),
};

static const struct snd_kcontrol_new o047_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I011 Switch", AFE_CONN47, 11, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I019 Switch", AFE_CONN47, 19, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I029 Switch", AFE_CONN47, 29, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN47_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN47_3, 3, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I101 Switch", AFE_CONN47_3, 5, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I103 Switch", AFE_CONN47_3, 7, 1, 0),
};

static const struct snd_kcontrol_new o182_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I020 Switch", AFE_CONN182, 20, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I022 Switch", AFE_CONN182, 22, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I024 Switch", AFE_CONN182, 24, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I096 Switch", AFE_CONN182_3, 0, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I098 Switch", AFE_CONN182_3, 2, 1, 0),
};

static const struct snd_kcontrol_new o183_mix[] = {
	SOC_DAPM_SINGLE_AUTODISABLE("I021 Switch", AFE_CONN183, 21, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I023 Switch", AFE_CONN183, 23, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I025 Switch", AFE_CONN183, 25, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I097 Switch", AFE_CONN183_3, 1, 1, 0),
	SOC_DAPM_SINGLE_AUTODISABLE("I099 Switch", AFE_CONN183_3, 3, 1, 0),
};

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
static unsigned int mt8188_irq_num_to_mask_bit(unsigned int irq_num)
{
	unsigned int bit_shift;

	if (irq_num <= MT8188_AFE_IRQ_10)
		bit_shift = irq_num - MT8188_AFE_IRQ_1 + 16;
	else
		bit_shift = irq_num - MT8188_AFE_IRQ_13;

	return bit_shift;
}

static const struct mt8188_afe_channel_merge *mt8188_adsp_found_cm(int memif_id)
{
	int id = -EINVAL;

	switch (memif_id) {
	case MT8188_AFE_MEMIF_UL9:
		id = MT8188_AFE_CM0;
		break;
	case MT8188_AFE_MEMIF_UL2:
		id = MT8188_AFE_CM1;
		break;
	case MT8188_AFE_MEMIF_UL10:
		id = MT8188_AFE_CM2;
		break;
	default:
		break;
	}

	if (id < 0)
		return NULL;

	return &mt8188_afe_cm[id];
}

static int mt8188_adsp_set_afe_memif(struct mtk_base_afe *afe,
				     int memif_id,
				     unsigned int channels,
				     unsigned int rate,
				     snd_pcm_format_t format)
{
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];
	const struct mt8188_afe_channel_merge *cm =
		mt8188_adsp_found_cm(memif_id);
	unsigned int maskbit;
	int ret, fs;

	/* enable agent */
	regmap_update_bits(afe->regmap, memif->data->agent_disable_reg,
			   1 << memif->data->agent_disable_shift,
			   0 << memif->data->agent_disable_shift);

	if (cm == NULL)
		return -EINVAL;
	mt8188_afe_config_cm(afe, cm, channels);
	if (memif->data->ch_num_reg >= 0) {
		maskbit = memif->data->ch_num_maskbit;
		regmap_update_bits(afe->regmap, memif->data->ch_num_reg,
				   maskbit << memif->data->ch_num_shift,
				   channels << memif->data->ch_num_shift);
	}

	dev_info(afe->dev,
		 "%s(), %s, ch %d, rate %d, fmt %d, phys_buf_addr 0x%x\n",
		 __func__,
		 memif->data->name, channels, rate, format,
		 memif->phys_buf_addr);

	/* start */
	regmap_write(afe->regmap, memif->data->reg_ofs_base,
		     memif->phys_buf_addr);
	/* end */
	regmap_write(afe->regmap, memif->data->reg_ofs_end,
		     memif->phys_buf_addr + memif->buffer_size - 1);
	/* set MSB to 33-bit */
	regmap_update_bits(afe->regmap, memif->data->msb_reg,
			   1 << memif->data->msb_shift,
			   0 << memif->data->msb_shift);
	regmap_update_bits(afe->regmap, memif->data->msb2_reg,
			   1 << memif->data->msb2_shift,
			   0 << memif->data->msb2_shift);

	/* set channel */
	ret = mtk_memif_set_channel(afe, memif_id, channels);
	if (ret) {
		dev_info(afe->dev,
			 "%s(), error, id %d, set channel %d, ret %d\n",
			 __func__, memif_id, channels, ret);
		return ret;
	}

	/* set rate */
	if (memif->data->fs_shift < 0)
		return 0;
	fs = mt8188_afe_fs_timing(rate);
	if (fs < 0)
		return -EINVAL;
	regmap_update_bits(afe->regmap, memif->data->fs_reg,
			   memif->data->fs_maskbit << memif->data->fs_shift,
			   fs << memif->data->fs_shift);

	/* set format */
	ret = mtk_memif_set_format(afe, memif_id, format);
	if (ret) {
		dev_info(afe->dev,
			 "%s(), error, id %d, set format %d, ret %d\n",
			 __func__, memif_id, format, ret);
		return ret;
	}

	return 0;
}

static int mt8188_adsp_set_afe_memif_enable(struct mtk_base_afe *afe,
					    int memif_id, unsigned int rate,
					    unsigned int period_size, int cmd)
{
	struct mtk_base_afe_memif *memif = &afe->memif[memif_id];
	struct mtk_base_afe_irq *irqs = &afe->irqs[memif->irq_usage];
	const struct mtk_base_irq_data *irq_data = irqs->irq_data;
	unsigned int mask_bit = mt8188_irq_num_to_mask_bit(irq_data->id);
	const struct mt8188_afe_channel_merge *cm =
		mt8188_adsp_found_cm(memif_id);
	unsigned int counter = period_size;
	unsigned int maskbit;
	int fs, ret;

	if (cm == NULL)
		return -EINVAL;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		mt8188_afe_enable_cm(afe, cm, true);

		ret = mtk_memif_set_enable(afe, memif_id);
		if (ret) {
			dev_info(afe->dev,
				 "%s(), error, id %d, memif enable, ret %d\n",
				 __func__, memif_id, ret);
			return ret;
		}

		/* set irq mask (only send to dsp) */
		regmap_update_bits(afe->regmap, AFE_IRQ_MASK,
				   1 << mask_bit, 0 << mask_bit);
		regmap_update_bits(afe->regmap, ASYS_IRQ_MASK,
				   1 << mask_bit, 0 << mask_bit);
		regmap_update_bits(afe->regmap, ADSP_IRQ_MASK,
				   1 << mask_bit, 1 << mask_bit);

		/* set irq counter */
		maskbit = irq_data->irq_cnt_maskbit;
		regmap_update_bits(afe->regmap, irq_data->irq_cnt_reg,
				   maskbit << irq_data->irq_cnt_shift,
				   counter << irq_data->irq_cnt_shift);

		/* set irq fs */
		fs = mt8188_afe_fs_timing(rate);
		if (fs < 0)
			return -EINVAL;

		if (irq_data->irq_fs_reg >= 0) {
			maskbit = irq_data->irq_fs_maskbit;
			regmap_update_bits(afe->regmap, irq_data->irq_fs_reg,
					   maskbit << irq_data->irq_fs_shift,
					   fs << irq_data->irq_fs_shift);
		}

		/* enable interrupt */
		regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
				   1 << irq_data->irq_en_shift,
				   1 << irq_data->irq_en_shift);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		mt8188_afe_enable_cm(afe, cm, false);

		ret = mtk_memif_set_disable(afe, memif_id);
		if (ret) {
			dev_info(afe->dev,
				 "%s(), error, id %d, memif enable, ret %d\n",
				 __func__, memif_id, ret);
		}

		/* disable interrupt */
		regmap_update_bits(afe->regmap, irq_data->irq_en_reg,
				   1 << irq_data->irq_en_shift,
				   0 << irq_data->irq_en_shift);
		/* and clear pending IRQ */
		regmap_write(afe->regmap, irq_data->irq_clr_reg,
			     1 << irq_data->irq_clr_shift);
		/* recover irq mask to afe, and clear mask to adsp. */
		regmap_update_bits(afe->regmap, AFE_IRQ_MASK,
				   1 << mask_bit, 1 << mask_bit);
		regmap_update_bits(afe->regmap, ADSP_IRQ_MASK,
				   1 << mask_bit, 0 << mask_bit);
		break;
	default:
		dev_info(afe->dev, "%s(), error, cmd %d not defined!\n",
			 __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void mt8188_adsp_set_afe_init(struct mtk_base_afe *afe)
{
	dev_dbg(afe->dev, "%s, dev=%p\n", __func__, afe->dev);
}

static void mt8188_adsp_set_afe_uninit(struct mtk_base_afe *afe)
{
	dev_dbg(afe->dev, "%s\n", __func__);
}
#endif

static const char * const dl8_dl11_data_sel_mux_text[] = {
	"dl8", "dl11",
};

static SOC_ENUM_SINGLE_DECL(dl8_dl11_data_sel_mux_enum,
	AFE_DAC_CON2, 0, dl8_dl11_data_sel_mux_text);

static const struct snd_kcontrol_new dl8_dl11_data_sel_mux =
	SOC_DAPM_ENUM("DL8_DL11 Sink",
		dl8_dl11_data_sel_mux_enum);

static const struct snd_soc_dapm_widget mt8188_memif_widgets[] = {
	/* DL6 */
	SND_SOC_DAPM_MIXER("I000", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I001", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DL3 */
	SND_SOC_DAPM_MIXER("I020", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I021", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DL11 */
	SND_SOC_DAPM_MIXER("I022", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I023", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I024", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I025", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I026", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I027", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I028", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I029", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I030", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I031", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I032", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I033", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I034", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I035", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I036", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I037", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DL11/DL8 */
	SND_SOC_DAPM_MIXER("I046", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I047", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I048", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I049", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I050", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I051", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I052", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I053", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I054", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I055", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I056", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I057", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I058", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I059", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I060", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I061", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* DL2 */
	SND_SOC_DAPM_MIXER("I070", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I071", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("DL8_DL11 Mux",
		SND_SOC_NOPM, 0, 0, &dl8_dl11_data_sel_mux),

	/* UL9 */
	SND_SOC_DAPM_MIXER("O002", SND_SOC_NOPM, 0, 0,
		o002_mix, ARRAY_SIZE(o002_mix)),
	SND_SOC_DAPM_MIXER("O003", SND_SOC_NOPM, 0, 0,
		o003_mix, ARRAY_SIZE(o003_mix)),
	SND_SOC_DAPM_MIXER("O004", SND_SOC_NOPM, 0, 0,
		o004_mix, ARRAY_SIZE(o004_mix)),
	SND_SOC_DAPM_MIXER("O005", SND_SOC_NOPM, 0, 0,
		o005_mix, ARRAY_SIZE(o005_mix)),
	SND_SOC_DAPM_MIXER("O006", SND_SOC_NOPM, 0, 0,
		o006_mix, ARRAY_SIZE(o006_mix)),
	SND_SOC_DAPM_MIXER("O007", SND_SOC_NOPM, 0, 0,
		o007_mix, ARRAY_SIZE(o007_mix)),
	SND_SOC_DAPM_MIXER("O008", SND_SOC_NOPM, 0, 0,
		o008_mix, ARRAY_SIZE(o008_mix)),
	SND_SOC_DAPM_MIXER("O009", SND_SOC_NOPM, 0, 0,
		o009_mix, ARRAY_SIZE(o009_mix)),
	SND_SOC_DAPM_MIXER("O010", SND_SOC_NOPM, 0, 0,
		o010_mix, ARRAY_SIZE(o010_mix)),
	SND_SOC_DAPM_MIXER("O011", SND_SOC_NOPM, 0, 0,
		o011_mix, ARRAY_SIZE(o011_mix)),
	SND_SOC_DAPM_MIXER("O012", SND_SOC_NOPM, 0, 0,
		o012_mix, ARRAY_SIZE(o012_mix)),
	SND_SOC_DAPM_MIXER("O013", SND_SOC_NOPM, 0, 0,
		o013_mix, ARRAY_SIZE(o013_mix)),
	SND_SOC_DAPM_MIXER("O014", SND_SOC_NOPM, 0, 0,
		o014_mix, ARRAY_SIZE(o014_mix)),
	SND_SOC_DAPM_MIXER("O015", SND_SOC_NOPM, 0, 0,
		o015_mix, ARRAY_SIZE(o015_mix)),
	SND_SOC_DAPM_MIXER("O016", SND_SOC_NOPM, 0, 0,
		o016_mix, ARRAY_SIZE(o016_mix)),
	SND_SOC_DAPM_MIXER("O017", SND_SOC_NOPM, 0, 0,
		o017_mix, ARRAY_SIZE(o017_mix)),
	SND_SOC_DAPM_MIXER("O018", SND_SOC_NOPM, 0, 0,
		o018_mix, ARRAY_SIZE(o018_mix)),
	SND_SOC_DAPM_MIXER("O019", SND_SOC_NOPM, 0, 0,
		o019_mix, ARRAY_SIZE(o019_mix)),
	SND_SOC_DAPM_MIXER("O020", SND_SOC_NOPM, 0, 0,
		o020_mix, ARRAY_SIZE(o020_mix)),
	SND_SOC_DAPM_MIXER("O021", SND_SOC_NOPM, 0, 0,
		o021_mix, ARRAY_SIZE(o021_mix)),
	SND_SOC_DAPM_MIXER("O022", SND_SOC_NOPM, 0, 0,
		o022_mix, ARRAY_SIZE(o022_mix)),
	SND_SOC_DAPM_MIXER("O023", SND_SOC_NOPM, 0, 0,
		o023_mix, ARRAY_SIZE(o023_mix)),
	SND_SOC_DAPM_MIXER("O024", SND_SOC_NOPM, 0, 0,
		o024_mix, ARRAY_SIZE(o024_mix)),
	SND_SOC_DAPM_MIXER("O025", SND_SOC_NOPM, 0, 0,
		o025_mix, ARRAY_SIZE(o025_mix)),
	SND_SOC_DAPM_MIXER("O026", SND_SOC_NOPM, 0, 0,
		o026_mix, ARRAY_SIZE(o026_mix)),
	SND_SOC_DAPM_MIXER("O027", SND_SOC_NOPM, 0, 0,
		o027_mix, ARRAY_SIZE(o027_mix)),
	SND_SOC_DAPM_MIXER("O028", SND_SOC_NOPM, 0, 0,
		o028_mix, ARRAY_SIZE(o028_mix)),
	SND_SOC_DAPM_MIXER("O029", SND_SOC_NOPM, 0, 0,
		o029_mix, ARRAY_SIZE(o029_mix)),
	SND_SOC_DAPM_MIXER("O030", SND_SOC_NOPM, 0, 0,
		o030_mix, ARRAY_SIZE(o030_mix)),
	SND_SOC_DAPM_MIXER("O031", SND_SOC_NOPM, 0, 0,
		o031_mix, ARRAY_SIZE(o031_mix)),
	SND_SOC_DAPM_MIXER("O032", SND_SOC_NOPM, 0, 0,
		o032_mix, ARRAY_SIZE(o032_mix)),
	SND_SOC_DAPM_MIXER("O033", SND_SOC_NOPM, 0, 0,
		o033_mix, ARRAY_SIZE(o033_mix)),

	/* UL4 */
	SND_SOC_DAPM_MIXER("O034", SND_SOC_NOPM, 0, 0,
		o034_mix, ARRAY_SIZE(o034_mix)),
	SND_SOC_DAPM_MIXER("O035", SND_SOC_NOPM, 0, 0,
		o035_mix, ARRAY_SIZE(o035_mix)),

	/* UL5 */
	SND_SOC_DAPM_MIXER("O036", SND_SOC_NOPM, 0, 0,
		o036_mix, ARRAY_SIZE(o036_mix)),
	SND_SOC_DAPM_MIXER("O037", SND_SOC_NOPM, 0, 0,
		o037_mix, ARRAY_SIZE(o037_mix)),

	/* UL10 */
	SND_SOC_DAPM_MIXER("O038", SND_SOC_NOPM, 0, 0,
		o038_mix, ARRAY_SIZE(o038_mix)),
	SND_SOC_DAPM_MIXER("O039", SND_SOC_NOPM, 0, 0,
		o039_mix, ARRAY_SIZE(o039_mix)),
	SND_SOC_DAPM_MIXER("O182", SND_SOC_NOPM, 0, 0,
		o182_mix, ARRAY_SIZE(o182_mix)),
	SND_SOC_DAPM_MIXER("O183", SND_SOC_NOPM, 0, 0,
		o183_mix, ARRAY_SIZE(o183_mix)),

	/* UL2 */
	SND_SOC_DAPM_MIXER("O040", SND_SOC_NOPM, 0, 0,
		o040_mix, ARRAY_SIZE(o040_mix)),
	SND_SOC_DAPM_MIXER("O041", SND_SOC_NOPM, 0, 0,
		o041_mix, ARRAY_SIZE(o041_mix)),
	SND_SOC_DAPM_MIXER("O042", SND_SOC_NOPM, 0, 0,
		o042_mix, ARRAY_SIZE(o042_mix)),
	SND_SOC_DAPM_MIXER("O043", SND_SOC_NOPM, 0, 0,
		o043_mix, ARRAY_SIZE(o043_mix)),
	SND_SOC_DAPM_MIXER("O044", SND_SOC_NOPM, 0, 0,
		o044_mix, ARRAY_SIZE(o044_mix)),
	SND_SOC_DAPM_MIXER("O045", SND_SOC_NOPM, 0, 0,
		o045_mix, ARRAY_SIZE(o045_mix)),
	SND_SOC_DAPM_MIXER("O046", SND_SOC_NOPM, 0, 0,
		o046_mix, ARRAY_SIZE(o046_mix)),
	SND_SOC_DAPM_MIXER("O047", SND_SOC_NOPM, 0, 0,
		o047_mix, ARRAY_SIZE(o047_mix)),
};

static const struct snd_soc_dapm_route mt8188_memif_routes[] = {
	{"I000", NULL, "DL6"},
	{"I001", NULL, "DL6"},

	{"I020", NULL, "DL3"},
	{"I021", NULL, "DL3"},

	{"I022", NULL, "DL11"},
	{"I023", NULL, "DL11"},
	{"I024", NULL, "DL11"},
	{"I025", NULL, "DL11"},
	{"I026", NULL, "DL11"},
	{"I027", NULL, "DL11"},
	{"I028", NULL, "DL11"},
	{"I029", NULL, "DL11"},
	{"I030", NULL, "DL11"},
	{"I031", NULL, "DL11"},
	{"I032", NULL, "DL11"},
	{"I033", NULL, "DL11"},
	{"I034", NULL, "DL11"},
	{"I035", NULL, "DL11"},
	{"I036", NULL, "DL11"},
	{"I037", NULL, "DL11"},

	{"DL8_DL11 Mux", "dl8", "DL8"},
	{"DL8_DL11 Mux", "dl11", "DL11"},

	{"I046", NULL, "DL8_DL11 Mux"},
	{"I047", NULL, "DL8_DL11 Mux"},
	{"I048", NULL, "DL8_DL11 Mux"},
	{"I049", NULL, "DL8_DL11 Mux"},
	{"I050", NULL, "DL8_DL11 Mux"},
	{"I051", NULL, "DL8_DL11 Mux"},
	{"I052", NULL, "DL8_DL11 Mux"},
	{"I053", NULL, "DL8_DL11 Mux"},
	{"I054", NULL, "DL8_DL11 Mux"},
	{"I055", NULL, "DL8_DL11 Mux"},
	{"I056", NULL, "DL8_DL11 Mux"},
	{"I057", NULL, "DL8_DL11 Mux"},
	{"I058", NULL, "DL8_DL11 Mux"},
	{"I059", NULL, "DL8_DL11 Mux"},
	{"I060", NULL, "DL8_DL11 Mux"},
	{"I061", NULL, "DL8_DL11 Mux"},

	{"I070", NULL, "DL2"},
	{"I071", NULL, "DL2"},

	{"UL9", NULL, "O002"},
	{"UL9", NULL, "O003"},
	{"UL9", NULL, "O004"},
	{"UL9", NULL, "O005"},
	{"UL9", NULL, "O006"},
	{"UL9", NULL, "O007"},
	{"UL9", NULL, "O008"},
	{"UL9", NULL, "O009"},
	{"UL9", NULL, "O010"},
	{"UL9", NULL, "O011"},
	{"UL9", NULL, "O012"},
	{"UL9", NULL, "O013"},
	{"UL9", NULL, "O014"},
	{"UL9", NULL, "O015"},
	{"UL9", NULL, "O016"},
	{"UL9", NULL, "O017"},
	{"UL9", NULL, "O018"},
	{"UL9", NULL, "O019"},
	{"UL9", NULL, "O020"},
	{"UL9", NULL, "O021"},
	{"UL9", NULL, "O022"},
	{"UL9", NULL, "O023"},
	{"UL9", NULL, "O024"},
	{"UL9", NULL, "O025"},
	{"UL9", NULL, "O026"},
	{"UL9", NULL, "O027"},
	{"UL9", NULL, "O028"},
	{"UL9", NULL, "O029"},
	{"UL9", NULL, "O030"},
	{"UL9", NULL, "O031"},
	{"UL9", NULL, "O032"},
	{"UL9", NULL, "O033"},

	{"UL4", NULL, "O034"},
	{"UL4", NULL, "O035"},

	{"UL5", NULL, "O036"},
	{"UL5", NULL, "O037"},

	{"UL10", NULL, "O038"},
	{"UL10", NULL, "O039"},
	{"UL10", NULL, "O182"},
	{"UL10", NULL, "O183"},

	{"UL2", NULL, "O040"},
	{"UL2", NULL, "O041"},
	{"UL2", NULL, "O042"},
	{"UL2", NULL, "O043"},
	{"UL2", NULL, "O044"},
	{"UL2", NULL, "O045"},
	{"UL2", NULL, "O046"},
	{"UL2", NULL, "O047"},

	{"O004", "I000 Switch", "I000"},
	{"O005", "I001 Switch", "I001"},

	{"O006", "I000 Switch", "I000"},
	{"O007", "I001 Switch", "I001"},

	{"O002", "I004 Switch", "I004"},
	{"O003", "I005 Switch", "I005"},
	{"O004", "I006 Switch", "I006"},
	{"O005", "I007 Switch", "I007"},
	{"O006", "I008 Switch", "I008"},
	{"O007", "I009 Switch", "I009"},
	{"O008", "I010 Switch", "I010"},
	{"O009", "I011 Switch", "I011"},

	{"O003", "I006 Switch", "I006"},
	{"O004", "I008 Switch", "I008"},
	{"O005", "I010 Switch", "I010"},

	{"O002", "I096 Switch", "I096"},
	{"O003", "I097 Switch", "I097"},
	{"O004", "I098 Switch", "I098"},
	{"O005", "I099 Switch", "I099"},
	{"O006", "I100 Switch", "I100"},
	{"O007", "I101 Switch", "I101"},
	{"O008", "I102 Switch", "I102"},
	{"O009", "I103 Switch", "I103"},
	{"O010", "I104 Switch", "I104"},
	{"O011", "I105 Switch", "I105"},
	{"O012", "I106 Switch", "I106"},
	{"O013", "I107 Switch", "I107"},
	{"O014", "I108 Switch", "I108"},
	{"O015", "I109 Switch", "I109"},
	{"O016", "I110 Switch", "I110"},
	{"O017", "I111 Switch", "I111"},
	{"O018", "I112 Switch", "I112"},
	{"O019", "I113 Switch", "I113"},
	{"O020", "I114 Switch", "I114"},
	{"O021", "I115 Switch", "I115"},
	{"O022", "I116 Switch", "I116"},
	{"O023", "I117 Switch", "I117"},
	{"O024", "I118 Switch", "I118"},
	{"O025", "I119 Switch", "I119"},

	{"O004", "I096 Switch", "I096"},
	{"O005", "I097 Switch", "I097"},
	{"O006", "I098 Switch", "I098"},
	{"O007", "I099 Switch", "I099"},
	{"O008", "I100 Switch", "I100"},
	{"O009", "I101 Switch", "I101"},
	{"O010", "I102 Switch", "I102"},
	{"O011", "I103 Switch", "I103"},
	{"O012", "I104 Switch", "I104"},
	{"O013", "I105 Switch", "I105"},
	{"O014", "I106 Switch", "I106"},
	{"O015", "I107 Switch", "I107"},
	{"O016", "I108 Switch", "I108"},
	{"O017", "I109 Switch", "I109"},
	{"O018", "I110 Switch", "I110"},
	{"O019", "I111 Switch", "I111"},
	{"O020", "I112 Switch", "I112"},
	{"O021", "I113 Switch", "I113"},
	{"O022", "I114 Switch", "I114"},
	{"O023", "I115 Switch", "I115"},
	{"O024", "I116 Switch", "I116"},
	{"O025", "I117 Switch", "I117"},
	{"O026", "I118 Switch", "I118"},
	{"O027", "I119 Switch", "I119"},

	{"O006", "I096 Switch", "I096"},
	{"O007", "I097 Switch", "I097"},
	{"O008", "I098 Switch", "I098"},
	{"O009", "I099 Switch", "I099"},
	{"O010", "I100 Switch", "I100"},
	{"O011", "I101 Switch", "I101"},
	{"O012", "I102 Switch", "I102"},
	{"O013", "I103 Switch", "I103"},
	{"O014", "I104 Switch", "I104"},
	{"O015", "I105 Switch", "I105"},
	{"O016", "I106 Switch", "I106"},
	{"O017", "I107 Switch", "I107"},
	{"O018", "I108 Switch", "I108"},
	{"O019", "I109 Switch", "I109"},
	{"O020", "I110 Switch", "I110"},
	{"O021", "I111 Switch", "I111"},
	{"O022", "I112 Switch", "I112"},
	{"O023", "I113 Switch", "I113"},
	{"O024", "I114 Switch", "I114"},
	{"O025", "I115 Switch", "I115"},
	{"O026", "I116 Switch", "I116"},
	{"O027", "I117 Switch", "I117"},
	{"O028", "I118 Switch", "I118"},
	{"O029", "I119 Switch", "I119"},

	{"O008", "I096 Switch", "I096"},
	{"O009", "I097 Switch", "I097"},
	{"O010", "I098 Switch", "I098"},
	{"O011", "I099 Switch", "I099"},
	{"O012", "I100 Switch", "I100"},
	{"O013", "I101 Switch", "I101"},
	{"O014", "I102 Switch", "I102"},
	{"O015", "I103 Switch", "I103"},
	{"O016", "I104 Switch", "I104"},
	{"O017", "I105 Switch", "I105"},
	{"O018", "I106 Switch", "I106"},
	{"O019", "I107 Switch", "I107"},
	{"O020", "I108 Switch", "I108"},
	{"O021", "I109 Switch", "I109"},
	{"O022", "I110 Switch", "I110"},
	{"O023", "I111 Switch", "I111"},
	{"O024", "I112 Switch", "I112"},
	{"O025", "I113 Switch", "I113"},
	{"O026", "I114 Switch", "I114"},
	{"O027", "I115 Switch", "I115"},
	{"O028", "I116 Switch", "I116"},
	{"O029", "I117 Switch", "I117"},
	{"O030", "I118 Switch", "I118"},
	{"O031", "I119 Switch", "I119"},

	{"O010", "I096 Switch", "I096"},
	{"O011", "I097 Switch", "I097"},
	{"O012", "I098 Switch", "I098"},
	{"O013", "I099 Switch", "I099"},
	{"O014", "I100 Switch", "I100"},
	{"O015", "I101 Switch", "I101"},
	{"O016", "I102 Switch", "I102"},
	{"O017", "I103 Switch", "I103"},
	{"O018", "I104 Switch", "I104"},
	{"O019", "I105 Switch", "I105"},
	{"O020", "I106 Switch", "I106"},
	{"O021", "I107 Switch", "I107"},
	{"O022", "I108 Switch", "I108"},
	{"O023", "I109 Switch", "I109"},
	{"O024", "I110 Switch", "I110"},
	{"O025", "I111 Switch", "I111"},
	{"O026", "I112 Switch", "I112"},
	{"O027", "I113 Switch", "I113"},
	{"O028", "I114 Switch", "I114"},
	{"O029", "I115 Switch", "I115"},
	{"O030", "I116 Switch", "I116"},
	{"O031", "I117 Switch", "I117"},
	{"O032", "I118 Switch", "I118"},
	{"O033", "I119 Switch", "I119"},

	{"O010", "I022 Switch", "I022"},
	{"O011", "I023 Switch", "I023"},
	{"O012", "I024 Switch", "I024"},
	{"O013", "I025 Switch", "I025"},
	{"O014", "I026 Switch", "I026"},
	{"O015", "I027 Switch", "I027"},
	{"O016", "I028 Switch", "I028"},
	{"O017", "I029 Switch", "I029"},

	{"O010", "I046 Switch", "I046"},
	{"O011", "I047 Switch", "I047"},
	{"O012", "I048 Switch", "I048"},
	{"O013", "I049 Switch", "I049"},
	{"O014", "I050 Switch", "I050"},
	{"O015", "I051 Switch", "I051"},
	{"O016", "I052 Switch", "I052"},
	{"O017", "I053 Switch", "I053"},

	{"O002", "I022 Switch", "I022"},
	{"O003", "I023 Switch", "I023"},
	{"O004", "I024 Switch", "I024"},
	{"O005", "I025 Switch", "I025"},
	{"O006", "I026 Switch", "I026"},
	{"O007", "I027 Switch", "I027"},
	{"O008", "I028 Switch", "I028"},
	{"O009", "I029 Switch", "I029"},
	{"O010", "I030 Switch", "I030"},
	{"O011", "I031 Switch", "I031"},
	{"O012", "I032 Switch", "I032"},
	{"O013", "I033 Switch", "I033"},
	{"O014", "I034 Switch", "I034"},
	{"O015", "I035 Switch", "I035"},
	{"O016", "I036 Switch", "I036"},
	{"O017", "I037 Switch", "I037"},
	{"O026", "I046 Switch", "I046"},
	{"O027", "I047 Switch", "I047"},
	{"O028", "I048 Switch", "I048"},
	{"O029", "I049 Switch", "I049"},
	{"O030", "I050 Switch", "I050"},
	{"O031", "I051 Switch", "I051"},
	{"O032", "I052 Switch", "I052"},
	{"O033", "I053 Switch", "I053"},

	{"O002", "I000 Switch", "I000"},
	{"O003", "I001 Switch", "I001"},
	{"O002", "I020 Switch", "I020"},
	{"O003", "I021 Switch", "I021"},
	{"O002", "I070 Switch", "I070"},
	{"O003", "I071 Switch", "I071"},

	{"O034", "I000 Switch", "I000"},
	{"O035", "I001 Switch", "I001"},
	{"O034", "I002 Switch", "I002"},
	{"O035", "I003 Switch", "I003"},
	{"O034", "I020 Switch", "I020"},
	{"O035", "I021 Switch", "I021"},
	{"O034", "I070 Switch", "I070"},
	{"O035", "I071 Switch", "I071"},

	{"O034", "I096 Switch", "I096"},
	{"O035", "I097 Switch", "I097"},
	{"O034", "I098 Switch", "I098"},
	{"O035", "I099 Switch", "I099"},
	{"O034", "I100 Switch", "I100"},
	{"O035", "I101 Switch", "I101"},
	{"O034", "I102 Switch", "I102"},
	{"O035", "I103 Switch", "I103"},
	{"O034", "I104 Switch", "I104"},
	{"O035", "I105 Switch", "I105"},
	{"O034", "I106 Switch", "I106"},
	{"O035", "I107 Switch", "I107"},
	{"O034", "I108 Switch", "I108"},
	{"O035", "I109 Switch", "I109"},
	{"O034", "I110 Switch", "I110"},
	{"O035", "I111 Switch", "I111"},
	{"O034", "I112 Switch", "I112"},
	{"O035", "I113 Switch", "I113"},
	{"O034", "I114 Switch", "I114"},
	{"O035", "I115 Switch", "I115"},
	{"O034", "I116 Switch", "I116"},
	{"O035", "I117 Switch", "I117"},
	{"O034", "I118 Switch", "I118"},
	{"O035", "I119 Switch", "I119"},

	{"O034", "I136 Switch", "I136"},
	{"O035", "I137 Switch", "I137"},
	{"O034", "I138 Switch", "I138"},
	{"O035", "I139 Switch", "I139"},

	{"O036", "I000 Switch", "I000"},
	{"O037", "I001 Switch", "I001"},
	{"O036", "I000 Switch", "I000V"},
	{"O037", "I001 Switch", "I001V"},
	{"O036", "I020 Switch", "I020"},
	{"O037", "I021 Switch", "I021"},
	{"O036", "I070 Switch", "I070"},
	{"O037", "I071 Switch", "I071"},

	{"O036", "I096 Switch", "I096"},
	{"O037", "I097 Switch", "I097"},
	{"O036", "I098 Switch", "I098"},
	{"O037", "I099 Switch", "I099"},
	{"O036", "I100 Switch", "I100"},
	{"O037", "I101 Switch", "I101"},
	{"O036", "I102 Switch", "I102"},
	{"O037", "I103 Switch", "I103"},
	{"O036", "I104 Switch", "I104"},
	{"O037", "I105 Switch", "I105"},
	{"O036", "I106 Switch", "I106"},
	{"O037", "I107 Switch", "I107"},
	{"O036", "I108 Switch", "I108"},
	{"O037", "I109 Switch", "I109"},
	{"O036", "I110 Switch", "I110"},
	{"O037", "I111 Switch", "I111"},
	{"O036", "I112 Switch", "I112"},
	{"O037", "I113 Switch", "I113"},
	{"O036", "I114 Switch", "I114"},
	{"O037", "I115 Switch", "I115"},
	{"O036", "I116 Switch", "I116"},
	{"O037", "I117 Switch", "I117"},
	{"O036", "I118 Switch", "I118"},
	{"O037", "I119 Switch", "I119"},

	{"O038", "I004 Switch", "I004"},
	{"O039", "I005 Switch", "I005"},

	{"O038", "I022 Switch", "I022"},
	{"O039", "I023 Switch", "I023"},
	{"O182", "I024 Switch", "I024"},
	{"O183", "I025 Switch", "I025"},

	{"O038", "I096 Switch", "I096"},
	{"O039", "I097 Switch", "I097"},
	{"O182", "I098 Switch", "I098"},
	{"O183", "I099 Switch", "I099"},

	{"O038", "I168 Switch", "I168"},
	{"O039", "I169 Switch", "I169"},

	{"O182", "I020 Switch", "I020"},
	{"O183", "I021 Switch", "I021"},

	{"O182", "I022 Switch", "I022"},
	{"O183", "I023 Switch", "I023"},

	{"O182", "I096 Switch", "I096"},
	{"O183", "I097 Switch", "I097"},

	{"O040", "I022 Switch", "I022"},
	{"O041", "I023 Switch", "I023"},
	{"O042", "I024 Switch", "I024"},
	{"O043", "I025 Switch", "I025"},
	{"O044", "I026 Switch", "I026"},
	{"O045", "I027 Switch", "I027"},
	{"O046", "I028 Switch", "I028"},
	{"O047", "I029 Switch", "I029"},

	{"O040", "I004 Switch", "I004"},
	{"O041", "I005 Switch", "I005"},
	{"O042", "I006 Switch", "I006"},
	{"O043", "I007 Switch", "I007"},
	{"O044", "I008 Switch", "I008"},
	{"O045", "I009 Switch", "I009"},
	{"O046", "I010 Switch", "I010"},
	{"O047", "I011 Switch", "I011"},

	{"O041", "I006 Switch", "I006"},
	{"O042", "I008 Switch", "I008"},
	{"O043", "I010 Switch", "I010"},

	{"O040", "I002 Switch", "I002"},
	{"O041", "I003 Switch", "I003"},

	{"O002", "I012 Switch", "I012"},
	{"O003", "I013 Switch", "I013"},
	{"O004", "I014 Switch", "I014"},
	{"O005", "I015 Switch", "I015"},
	{"O006", "I016 Switch", "I016"},
	{"O007", "I017 Switch", "I017"},
	{"O008", "I018 Switch", "I018"},
	{"O009", "I019 Switch", "I019"},
	{"O010", "I188 Switch", "I188"},
	{"O011", "I189 Switch", "I189"},
	{"O012", "I190 Switch", "I190"},
	{"O013", "I191 Switch", "I191"},
	{"O014", "I192 Switch", "I192"},
	{"O015", "I193 Switch", "I193"},
	{"O016", "I194 Switch", "I194"},
	{"O017", "I195 Switch", "I195"},

	{"O040", "I012 Switch", "I012"},
	{"O041", "I013 Switch", "I013"},
	{"O042", "I014 Switch", "I014"},
	{"O043", "I015 Switch", "I015"},
	{"O044", "I016 Switch", "I016"},
	{"O045", "I017 Switch", "I017"},
	{"O046", "I018 Switch", "I018"},
	{"O047", "I019 Switch", "I019"},

	{"O040", "I096 Switch", "I096"},
	{"O041", "I097 Switch", "I097"},
	{"O042", "I098 Switch", "I098"},
	{"O043", "I099 Switch", "I099"},
	{"O044", "I100 Switch", "I100"},
	{"O045", "I101 Switch", "I101"},
	{"O046", "I102 Switch", "I102"},
	{"O047", "I103 Switch", "I103"},

	{"O042", "I096 Switch", "I096"},
	{"O043", "I097 Switch", "I097"},
	{"O044", "I098 Switch", "I098"},
	{"O045", "I099 Switch", "I099"},
	{"O046", "I100 Switch", "I100"},
	{"O047", "I101 Switch", "I101"},

	{"O044", "I096 Switch", "I096"},
	{"O045", "I097 Switch", "I097"},
	{"O046", "I098 Switch", "I098"},
	{"O047", "I099 Switch", "I099"},

	{"O046", "I096 Switch", "I096"},
	{"O047", "I097 Switch", "I097"},

	{"O002", "I072 Switch", "I072"},
	{"O003", "I073 Switch", "I073"},
	{"O004", "I074 Switch", "I074"},
	{"O005", "I075 Switch", "I075"},
	{"O006", "I076 Switch", "I076"},
	{"O007", "I077 Switch", "I077"},
	{"O008", "I078 Switch", "I078"},
	{"O009", "I079 Switch", "I079"},
	{"O010", "I080 Switch", "I080"},
	{"O011", "I081 Switch", "I081"},
	{"O012", "I082 Switch", "I082"},
	{"O013", "I083 Switch", "I083"},
	{"O014", "I084 Switch", "I084"},
	{"O015", "I085 Switch", "I085"},
	{"O016", "I086 Switch", "I086"},
	{"O017", "I087 Switch", "I087"},

	{"O010", "I072 Switch", "I072"},
	{"O011", "I073 Switch", "I073"},
	{"O012", "I074 Switch", "I074"},
	{"O013", "I075 Switch", "I075"},
	{"O014", "I076 Switch", "I076"},
	{"O015", "I077 Switch", "I077"},
	{"O016", "I078 Switch", "I078"},
	{"O017", "I079 Switch", "I079"},
	{"O018", "I080 Switch", "I080"},
	{"O019", "I081 Switch", "I081"},
	{"O020", "I082 Switch", "I082"},
	{"O021", "I083 Switch", "I083"},
	{"O022", "I084 Switch", "I084"},
	{"O023", "I085 Switch", "I085"},
	{"O024", "I086 Switch", "I086"},
	{"O025", "I087 Switch", "I087"},

	{"O002", "I168 Switch", "I168"},
	{"O003", "I169 Switch", "I169"},

	{"O034", "I168 Switch", "I168"},
	{"O035", "I168 Switch", "I168"},
	{"O035", "I169 Switch", "I169"},

	{"O040", "I168 Switch", "I168"},
	{"O041", "I169 Switch", "I169"},
};

static const char * const mt8188_afe_1x_en_sel_text[] = {
	"a1sys_a2sys", "a3sys", "a4sys",
};

static const unsigned int mt8188_afe_1x_en_sel_values[] = {
	0, 1, 2,
};

static int mt8188_memif_1x_en_sel_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_memif_priv *memif_priv;
	unsigned int dai_id = kcontrol->id.device;
	long val = ucontrol->value.integer.value[0];
	int ret = 0;

	memif_priv = afe_priv->dai_priv[dai_id];

	if (val == memif_priv->asys_timing_sel)
		return 0;

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);

	memif_priv->asys_timing_sel = val;

	return ret;
}

static int mt8188_asys_irq_1x_en_sel_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	unsigned int id = kcontrol->id.device;
	long val = ucontrol->value.integer.value[0];
	int ret = 0;

	if (val == afe_priv->irq_priv[id].asys_timing_sel)
		return 0;

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);

	afe_priv->irq_priv[id].asys_timing_sel = val;

	return ret;
}

static SOC_VALUE_ENUM_SINGLE_DECL(dl2_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 18, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl3_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 20, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl6_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 22, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl7_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 24, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl8_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 26, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl10_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 28, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl11_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 30, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul1_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 0, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul2_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 2, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul3_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 4, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul4_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 6, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul5_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 8, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul6_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 10, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul8_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 12, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul9_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 14, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul10_1x_en_sel_enum,
			A3_A4_TIMING_SEL1, 16, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);

static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq1_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 0, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq2_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 2, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq3_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 4, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq4_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 6, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq5_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 8, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq6_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 10, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq7_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 12, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq8_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 14, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq9_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 16, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq10_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 18, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq11_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 20, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq12_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 22, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq13_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 24, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq14_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 26, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq15_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 28, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(asys_irq16_1x_en_sel_enum,
			A3_A4_TIMING_SEL6, 30, 0x3,
			mt8188_afe_1x_en_sel_text,
			mt8188_afe_1x_en_sel_values);

static const char * const mt8188_afe_fs_timing_sel_text[] = {
	"asys",
	"etdmout1_1x_en",
	"etdmout2_1x_en",
	"etdmout3_1x_en",
	"etdmin1_1x_en",
	"etdmin2_1x_en",
	"etdmin1_nx_en",
	"etdmin2_nx_en",
};

static const unsigned int mt8188_afe_fs_timing_sel_values[] = {
	0,
	MT8188_ETDM_OUT1_1X_EN,
	MT8188_ETDM_OUT2_1X_EN,
	MT8188_ETDM_OUT3_1X_EN,
	MT8188_ETDM_IN1_1X_EN,
	MT8188_ETDM_IN2_1X_EN,
	MT8188_ETDM_IN1_NX_EN,
	MT8188_ETDM_IN2_NX_EN,
};

static int mt8188_memif_fs_timing_sel_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_memif_priv *memif_priv;
	unsigned int dai_id = kcontrol->id.device;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;

	memif_priv = afe_priv->dai_priv[dai_id];

	ucontrol->value.enumerated.item[0] =
		snd_soc_enum_val_to_item(e, memif_priv->fs_timing);

	return 0;
}

static int mt8188_memif_fs_timing_sel_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_memif_priv *memif_priv;
	unsigned int dai_id = kcontrol->id.device;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	unsigned int prev_item = 0;

	if (item[0] >= e->items)
		return -EINVAL;

	memif_priv = afe_priv->dai_priv[dai_id];

	prev_item = snd_soc_enum_val_to_item(e, memif_priv->fs_timing);

	if (item[0] == prev_item)
		return 0;

	memif_priv->fs_timing = snd_soc_enum_item_to_val(e, item[0]);

	return 0;
}

static SOC_VALUE_ENUM_SINGLE_DECL(dl2_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl3_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl6_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl8_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(dl11_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul2_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul4_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul5_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul9_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);
static SOC_VALUE_ENUM_SINGLE_DECL(ul10_fs_timing_sel_enum,
				  SND_SOC_NOPM, 0, 0,
				  mt8188_afe_fs_timing_sel_text,
				  mt8188_afe_fs_timing_sel_values);

static const struct snd_kcontrol_new mt8188_memif_controls[] = {
	MT8188_SOC_ENUM_EXT("dl2_1x_en_sel",
		dl2_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_DL2),
	MT8188_SOC_ENUM_EXT("dl3_1x_en_sel",
		dl3_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_DL3),
	MT8188_SOC_ENUM_EXT("dl6_1x_en_sel",
		dl6_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_DL6),
	MT8188_SOC_ENUM_EXT("dl7_1x_en_sel",
		dl7_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_DL7),
	MT8188_SOC_ENUM_EXT("dl8_1x_en_sel",
		dl8_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_DL8),
	MT8188_SOC_ENUM_EXT("dl10_1x_en_sel",
		dl10_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_DL10),
	MT8188_SOC_ENUM_EXT("dl11_1x_en_sel",
		dl11_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_DL11),
	MT8188_SOC_ENUM_EXT("ul1_1x_en_sel",
		ul1_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL1),
	MT8188_SOC_ENUM_EXT("ul2_1x_en_sel",
		ul2_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL2),
	MT8188_SOC_ENUM_EXT("ul3_1x_en_sel",
		ul3_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL3),
	MT8188_SOC_ENUM_EXT("ul4_1x_en_sel",
		ul4_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL4),
	MT8188_SOC_ENUM_EXT("ul5_1x_en_sel",
		ul5_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL5),
	MT8188_SOC_ENUM_EXT("ul6_1x_en_sel",
		ul6_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL6),
	MT8188_SOC_ENUM_EXT("ul8_1x_en_sel",
		ul8_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL8),
	MT8188_SOC_ENUM_EXT("ul9_1x_en_sel",
		ul9_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL9),
	MT8188_SOC_ENUM_EXT("ul10_1x_en_sel",
		ul10_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_memif_1x_en_sel_put,
		MT8188_AFE_MEMIF_UL10),
	MT8188_SOC_ENUM_EXT("asys_irq1_1x_en_sel",
		asys_irq1_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_13),
	MT8188_SOC_ENUM_EXT("asys_irq2_1x_en_sel",
		asys_irq2_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_14),
	MT8188_SOC_ENUM_EXT("asys_irq3_1x_en_sel",
		asys_irq3_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_15),
	MT8188_SOC_ENUM_EXT("asys_irq4_1x_en_sel",
		asys_irq4_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_16),
	MT8188_SOC_ENUM_EXT("asys_irq5_1x_en_sel",
		asys_irq5_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_17),
	MT8188_SOC_ENUM_EXT("asys_irq6_1x_en_sel",
		asys_irq6_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_18),
	MT8188_SOC_ENUM_EXT("asys_irq7_1x_en_sel",
		asys_irq7_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_19),
	MT8188_SOC_ENUM_EXT("asys_irq8_1x_en_sel",
		asys_irq8_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_20),
	MT8188_SOC_ENUM_EXT("asys_irq9_1x_en_sel",
		asys_irq9_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_21),
	MT8188_SOC_ENUM_EXT("asys_irq10_1x_en_sel",
		asys_irq10_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_22),
	MT8188_SOC_ENUM_EXT("asys_irq11_1x_en_sel",
		asys_irq11_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_23),
	MT8188_SOC_ENUM_EXT("asys_irq12_1x_en_sel",
		asys_irq12_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_24),
	MT8188_SOC_ENUM_EXT("asys_irq13_1x_en_sel",
		asys_irq13_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_25),
	MT8188_SOC_ENUM_EXT("asys_irq14_1x_en_sel",
		asys_irq14_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_26),
	MT8188_SOC_ENUM_EXT("asys_irq15_1x_en_sel",
		asys_irq15_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_27),
	MT8188_SOC_ENUM_EXT("asys_irq16_1x_en_sel",
		asys_irq16_1x_en_sel_enum,
		snd_soc_get_enum_double,
		mt8188_asys_irq_1x_en_sel_put,
		MT8188_AFE_IRQ_28),
	MT8188_SOC_ENUM_EXT("dl2_fs_timing_sel",
		dl2_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_DL2),
	MT8188_SOC_ENUM_EXT("dl3_fs_timing_sel",
		dl3_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_DL3),
	MT8188_SOC_ENUM_EXT("dl6_fs_timing_sel",
		dl6_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_DL6),
	MT8188_SOC_ENUM_EXT("dl8_fs_timing_sel",
		dl8_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_DL8),
	MT8188_SOC_ENUM_EXT("dl11_fs_timing_sel",
		dl11_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_DL11),
	MT8188_SOC_ENUM_EXT("ul2_fs_timing_sel",
		ul2_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_UL2),
	MT8188_SOC_ENUM_EXT("ul4_fs_timing_sel",
		ul4_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_UL4),
	MT8188_SOC_ENUM_EXT("ul5_fs_timing_sel",
		ul5_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_UL5),
	MT8188_SOC_ENUM_EXT("ul9_fs_timing_sel",
		ul9_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_UL9),
	MT8188_SOC_ENUM_EXT("ul10_fs_timing_sel",
		ul10_fs_timing_sel_enum,
		mt8188_memif_fs_timing_sel_get,
		mt8188_memif_fs_timing_sel_put,
		MT8188_AFE_MEMIF_UL10),
};

static const struct snd_soc_component_driver mt8188_afe_pcm_dai_component = {
	.name = "mt8188-afe-pcm-dai",
};

static const struct mtk_base_memif_data memif_data[MT8188_AFE_MEMIF_NUM] = {
	[MT8188_AFE_MEMIF_DL2] = {
		.name = "DL2",
		.id = MT8188_AFE_MEMIF_DL2,
		.reg_ofs_base = AFE_DL2_BASE,
		.reg_ofs_cur = AFE_DL2_CUR,
		.reg_ofs_end = AFE_DL2_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON0,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = 0,
		.int_odd_flag_reg = -1,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 18,
		.hd_reg = AFE_DL2_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 18,
		.ch_num_reg = AFE_DL2_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x1f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 18,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 18,
	},
	[MT8188_AFE_MEMIF_DL3] = {
		.name = "DL3",
		.id = MT8188_AFE_MEMIF_DL3,
		.reg_ofs_base = AFE_DL3_BASE,
		.reg_ofs_cur = AFE_DL3_CUR,
		.reg_ofs_end = AFE_DL3_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON0,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = 0,
		.int_odd_flag_reg = -1,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 19,
		.hd_reg = AFE_DL3_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 19,
		.ch_num_reg = AFE_DL3_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x1f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 19,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 19,
	},
	[MT8188_AFE_MEMIF_DL6] = {
		.name = "DL6",
		.id = MT8188_AFE_MEMIF_DL6,
		.reg_ofs_base = AFE_DL6_BASE,
		.reg_ofs_cur = AFE_DL6_CUR,
		.reg_ofs_end = AFE_DL6_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON1,
		.fs_shift = 0,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = 0,
		.int_odd_flag_reg = -1,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 22,
		.hd_reg = AFE_DL6_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 22,
		.ch_num_reg = AFE_DL6_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x1f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 22,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 22,
	},
	[MT8188_AFE_MEMIF_DL7] = {
		.name = "DL7",
		.id = MT8188_AFE_MEMIF_DL7,
		.reg_ofs_base = AFE_DL7_BASE,
		.reg_ofs_cur = AFE_DL7_CUR,
		.reg_ofs_end = AFE_DL7_END,
		.fs_reg = -1,
		.fs_shift = 0,
		.fs_maskbit = 0,
		.mono_reg = -1,
		.mono_shift = 0,
		.int_odd_flag_reg = -1,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 23,
		.hd_reg = AFE_DL7_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 23,
		.ch_num_reg = AFE_DL7_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x1f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 23,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 23,
	},
	[MT8188_AFE_MEMIF_DL8] = {
		.name = "DL8",
		.id = MT8188_AFE_MEMIF_DL8,
		.reg_ofs_base = AFE_DL8_BASE,
		.reg_ofs_cur = AFE_DL8_CUR,
		.reg_ofs_end = AFE_DL8_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON1,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = 0,
		.int_odd_flag_reg = -1,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 24,
		.hd_reg = AFE_DL8_CON0,
		.hd_shift = 6,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 24,
		.ch_num_reg = AFE_DL8_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x3f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 24,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 24,
	},
	[MT8188_AFE_MEMIF_DL10] = {
		.name = "DL10",
		.id = MT8188_AFE_MEMIF_DL10,
		.reg_ofs_base = AFE_DL10_BASE,
		.reg_ofs_cur = AFE_DL10_CUR,
		.reg_ofs_end = AFE_DL10_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON1,
		.fs_shift = 20,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = 0,
		.int_odd_flag_reg = -1,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 26,
		.hd_reg = AFE_DL10_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 26,
		.ch_num_reg = AFE_DL10_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x1f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 26,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 26,
	},
	[MT8188_AFE_MEMIF_DL11] = {
		.name = "DL11",
		.id = MT8188_AFE_MEMIF_DL11,
		.reg_ofs_base = AFE_DL11_BASE,
		.reg_ofs_cur = AFE_DL11_CUR,
		.reg_ofs_end = AFE_DL11_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON1,
		.fs_shift = 25,
		.fs_maskbit = 0x1f,
		.mono_reg = -1,
		.mono_shift = 0,
		.int_odd_flag_reg = -1,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 27,
		.hd_reg = AFE_DL11_CON0,
		.hd_shift = 7,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 27,
		.ch_num_reg = AFE_DL11_CON0,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0x7f,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 27,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 27,
	},
	[MT8188_AFE_MEMIF_UL1] = {
		.name = "UL1",
		.id = MT8188_AFE_MEMIF_UL1,
		.reg_ofs_base = AFE_UL1_BASE,
		.reg_ofs_cur = AFE_UL1_CUR,
		.reg_ofs_end = AFE_UL1_END,
		.fs_reg = -1,
		.fs_shift = 0,
		.fs_maskbit = 0,
		.mono_reg = AFE_UL1_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL1_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 1,
		.hd_reg = AFE_UL1_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 0,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 0,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 0,
	},
	[MT8188_AFE_MEMIF_UL2] = {
		.name = "UL2",
		.id = MT8188_AFE_MEMIF_UL2,
		.reg_ofs_base = AFE_UL2_BASE,
		.reg_ofs_cur = AFE_UL2_CUR,
		.reg_ofs_end = AFE_UL2_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 5,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL2_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL2_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 2,
		.hd_reg = AFE_UL2_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 1,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 1,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 1,
	},
	[MT8188_AFE_MEMIF_UL3] = {
		.name = "UL3",
		.id = MT8188_AFE_MEMIF_UL3,
		.reg_ofs_base = AFE_UL3_BASE,
		.reg_ofs_cur = AFE_UL3_CUR,
		.reg_ofs_end = AFE_UL3_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL3_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL3_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 3,
		.hd_reg = AFE_UL3_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 2,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 2,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 2,
	},
	[MT8188_AFE_MEMIF_UL4] = {
		.name = "UL4",
		.id = MT8188_AFE_MEMIF_UL4,
		.reg_ofs_base = AFE_UL4_BASE,
		.reg_ofs_cur = AFE_UL4_CUR,
		.reg_ofs_end = AFE_UL4_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL4_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL4_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 4,
		.hd_reg = AFE_UL4_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 3,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 3,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 3,
	},
	[MT8188_AFE_MEMIF_UL5] = {
		.name = "UL5",
		.id = MT8188_AFE_MEMIF_UL5,
		.reg_ofs_base = AFE_UL5_BASE,
		.reg_ofs_cur = AFE_UL5_CUR,
		.reg_ofs_end = AFE_UL5_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON2,
		.fs_shift = 20,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL5_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL5_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 5,
		.hd_reg = AFE_UL5_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 4,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 4,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 4,
	},
	[MT8188_AFE_MEMIF_UL6] = {
		.name = "UL6",
		.id = MT8188_AFE_MEMIF_UL6,
		.reg_ofs_base = AFE_UL6_BASE,
		.reg_ofs_cur = AFE_UL6_CUR,
		.reg_ofs_end = AFE_UL6_END,
		.fs_reg = -1,
		.fs_shift = 0,
		.fs_maskbit = 0,
		.mono_reg = AFE_UL6_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL6_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 6,
		.hd_reg = AFE_UL6_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 5,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 5,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 5,
	},
	[MT8188_AFE_MEMIF_UL8] = {
		.name = "UL8",
		.id = MT8188_AFE_MEMIF_UL8,
		.reg_ofs_base = AFE_UL8_BASE,
		.reg_ofs_cur = AFE_UL8_CUR,
		.reg_ofs_end = AFE_UL8_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON3,
		.fs_shift = 5,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL8_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL8_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 8,
		.hd_reg = AFE_UL8_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 7,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 7,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 7,
	},
	[MT8188_AFE_MEMIF_UL9] = {
		.name = "UL9",
		.id = MT8188_AFE_MEMIF_UL9,
		.reg_ofs_base = AFE_UL9_BASE,
		.reg_ofs_cur = AFE_UL9_CUR,
		.reg_ofs_end = AFE_UL9_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON3,
		.fs_shift = 10,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL9_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL9_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 9,
		.hd_reg = AFE_UL9_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 8,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 8,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 8,
	},
	[MT8188_AFE_MEMIF_UL10] = {
		.name = "UL10",
		.id = MT8188_AFE_MEMIF_UL10,
		.reg_ofs_base = AFE_UL10_BASE,
		.reg_ofs_cur = AFE_UL10_CUR,
		.reg_ofs_end = AFE_UL10_END,
		.fs_reg = AFE_MEMIF_AGENT_FS_CON3,
		.fs_shift = 15,
		.fs_maskbit = 0x1f,
		.mono_reg = AFE_UL10_CON0,
		.mono_shift = 1,
		.int_odd_flag_reg = AFE_UL10_CON0,
		.int_odd_flag_shift = 0,
		.enable_reg = AFE_DAC_CON0,
		.enable_shift = 10,
		.hd_reg = AFE_UL10_CON0,
		.hd_shift = 5,
		.agent_disable_reg = AUDIO_TOP_CON5,
		.agent_disable_shift = 9,
		.ch_num_reg = -1,
		.ch_num_shift = 0,
		.ch_num_maskbit = 0,
		.msb_reg = AFE_NORMAL_BASE_ADR_MSB,
		.msb_shift = 9,
		.msb2_reg = AFE_NORMAL_END_ADR_MSB,
		.msb2_shift = 9,
	},
};

static const struct mtk_base_irq_data irq_data[MT8188_AFE_IRQ_NUM] = {
	[MT8188_AFE_IRQ_1] = {
		.id = MT8188_AFE_IRQ_1,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0,
		.irq_en_reg = AFE_IRQ1_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 0,
		.irq_status_shift = 16,
	},
	[MT8188_AFE_IRQ_2] = {
		.id = MT8188_AFE_IRQ_2,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0,
		.irq_en_reg = AFE_IRQ2_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 1,
		.irq_status_shift = 17,
	},
	[MT8188_AFE_IRQ_3] = {
		.id = MT8188_AFE_IRQ_3,
		.irq_cnt_reg = AFE_IRQ3_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0,
		.irq_en_reg = AFE_IRQ3_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 2,
		.irq_status_shift = 18,
	},
	[MT8188_AFE_IRQ_8] = {
		.id = MT8188_AFE_IRQ_8,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0,
		.irq_en_reg = AFE_IRQ8_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 7,
		.irq_status_shift = 23,
	},
	[MT8188_AFE_IRQ_9] = {
		.id = MT8188_AFE_IRQ_9,
		.irq_cnt_reg = AFE_IRQ9_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0,
		.irq_en_reg = AFE_IRQ9_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 8,
		.irq_status_shift = 24,
	},
	[MT8188_AFE_IRQ_10] = {
		.id = MT8188_AFE_IRQ_10,
		.irq_cnt_reg = -1,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0,
		.irq_fs_reg = -1,
		.irq_fs_shift = 0,
		.irq_fs_maskbit = 0,
		.irq_en_reg = AFE_IRQ10_CON,
		.irq_en_shift = 31,
		.irq_clr_reg = AFE_IRQ_MCU_CLR,
		.irq_clr_shift = 9,
		.irq_status_shift = 25,
	},
	[MT8188_AFE_IRQ_13] = {
		.id = MT8188_AFE_IRQ_13,
		.irq_cnt_reg = ASYS_IRQ1_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ1_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ1_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 0,
		.irq_status_shift = 0,
	},
	[MT8188_AFE_IRQ_14] = {
		.id = MT8188_AFE_IRQ_14,
		.irq_cnt_reg = ASYS_IRQ2_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ2_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ2_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 1,
		.irq_status_shift = 1,
	},
	[MT8188_AFE_IRQ_15] = {
		.id = MT8188_AFE_IRQ_15,
		.irq_cnt_reg = ASYS_IRQ3_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ3_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ3_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 2,
		.irq_status_shift = 2,
	},
	[MT8188_AFE_IRQ_16] = {
		.id = MT8188_AFE_IRQ_16,
		.irq_cnt_reg = ASYS_IRQ4_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ4_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ4_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 3,
		.irq_status_shift = 3,
	},
	[MT8188_AFE_IRQ_17] = {
		.id = MT8188_AFE_IRQ_17,
		.irq_cnt_reg = ASYS_IRQ5_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ5_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ5_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 4,
		.irq_status_shift = 4,
	},
	[MT8188_AFE_IRQ_18] = {
		.id = MT8188_AFE_IRQ_18,
		.irq_cnt_reg = ASYS_IRQ6_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ6_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ6_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 5,
		.irq_status_shift = 5,
	},
	[MT8188_AFE_IRQ_19] = {
		.id = MT8188_AFE_IRQ_19,
		.irq_cnt_reg = ASYS_IRQ7_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ7_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ7_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 6,
		.irq_status_shift = 6,
	},
	[MT8188_AFE_IRQ_20] = {
		.id = MT8188_AFE_IRQ_20,
		.irq_cnt_reg = ASYS_IRQ8_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ8_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ8_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 7,
		.irq_status_shift = 7,
	},
	[MT8188_AFE_IRQ_21] = {
		.id = MT8188_AFE_IRQ_21,
		.irq_cnt_reg = ASYS_IRQ9_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ9_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ9_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 8,
		.irq_status_shift = 8,
	},
	[MT8188_AFE_IRQ_22] = {
		.id = MT8188_AFE_IRQ_22,
		.irq_cnt_reg = ASYS_IRQ10_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ10_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ10_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 9,
		.irq_status_shift = 9,
	},
	[MT8188_AFE_IRQ_23] = {
		.id = MT8188_AFE_IRQ_23,
		.irq_cnt_reg = ASYS_IRQ11_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ11_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ11_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 10,
		.irq_status_shift = 10,
	},
	[MT8188_AFE_IRQ_24] = {
		.id = MT8188_AFE_IRQ_24,
		.irq_cnt_reg = ASYS_IRQ12_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ12_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ12_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 11,
		.irq_status_shift = 11,
	},
	[MT8188_AFE_IRQ_25] = {
		.id = MT8188_AFE_IRQ_25,
		.irq_cnt_reg = ASYS_IRQ13_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ13_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ13_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 12,
		.irq_status_shift = 12,
	},
	[MT8188_AFE_IRQ_26] = {
		.id = MT8188_AFE_IRQ_26,
		.irq_cnt_reg = ASYS_IRQ14_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ14_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ14_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 13,
		.irq_status_shift = 13,
	},
	[MT8188_AFE_IRQ_27] = {
		.id = MT8188_AFE_IRQ_27,
		.irq_cnt_reg = ASYS_IRQ15_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ15_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ15_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 14,
		.irq_status_shift = 14,
	},
	[MT8188_AFE_IRQ_28] = {
		.id = MT8188_AFE_IRQ_28,
		.irq_cnt_reg = ASYS_IRQ16_CON,
		.irq_cnt_shift = 0,
		.irq_cnt_maskbit = 0xffffff,
		.irq_fs_reg = ASYS_IRQ16_CON,
		.irq_fs_shift = 24,
		.irq_fs_maskbit = 0x1ffff,
		.irq_en_reg = ASYS_IRQ16_CON,
		.irq_en_shift = 31,
		.irq_clr_reg =  ASYS_IRQ_CLR,
		.irq_clr_shift = 15,
		.irq_status_shift = 15,
	},
};

static const int mt8188_afe_memif_const_irqs[MT8188_AFE_MEMIF_NUM] = {
	[MT8188_AFE_MEMIF_DL2] = MT8188_AFE_IRQ_13,
	[MT8188_AFE_MEMIF_DL3] = MT8188_AFE_IRQ_14,
	[MT8188_AFE_MEMIF_DL6] = MT8188_AFE_IRQ_15,
	[MT8188_AFE_MEMIF_DL7] = MT8188_AFE_IRQ_1,
	[MT8188_AFE_MEMIF_DL8] = MT8188_AFE_IRQ_16,
	[MT8188_AFE_MEMIF_DL10] = MT8188_AFE_IRQ_17,
	[MT8188_AFE_MEMIF_DL11] = MT8188_AFE_IRQ_18,
	[MT8188_AFE_MEMIF_UL1] = MT8188_AFE_IRQ_3,
	[MT8188_AFE_MEMIF_UL2] = MT8188_AFE_IRQ_19,
	[MT8188_AFE_MEMIF_UL3] = MT8188_AFE_IRQ_20,
	[MT8188_AFE_MEMIF_UL4] = MT8188_AFE_IRQ_21,
	[MT8188_AFE_MEMIF_UL5] = MT8188_AFE_IRQ_22,
	[MT8188_AFE_MEMIF_UL6] = MT8188_AFE_IRQ_9,
	[MT8188_AFE_MEMIF_UL8] = MT8188_AFE_IRQ_23,
	[MT8188_AFE_MEMIF_UL9] = MT8188_AFE_IRQ_24,
	[MT8188_AFE_MEMIF_UL10] = MT8188_AFE_IRQ_25,
};

static bool mt8188_is_volatile_reg(struct device *dev, unsigned int reg)
{
	/* these auto-gen reg has read-only bit, so put it as volatile */
	/* volatile reg cannot be cached, so cannot be set when power off */
	switch (reg) {
	case AUDIO_TOP_CON0:
	case AUDIO_TOP_CON1:
	case AUDIO_TOP_CON3:
	case AUDIO_TOP_CON4:
	case AUDIO_TOP_CON5:
	case AUDIO_TOP_CON6:
	case ASYS_IRQ_CLR:
	case ASYS_IRQ_STATUS:
	case ASYS_IRQ_MON1:
	case ASYS_IRQ_MON2:
	case AFE_IRQ_MCU_CLR:
	case AFE_IRQ_STATUS:
	case AFE_IRQ3_CON_MON:
	case AFE_IRQ_MCU_MON2:
	case ADSP_IRQ_STATUS:
	case AUDIO_TOP_STA0:
	case AUDIO_TOP_STA1:
	case AFE_GAIN1_CUR:
	case AFE_GAIN2_CUR:
	case AFE_GAIN1_CON0:
	case AFE_GAIN1_CON1:
	case AFE_GAIN1_CON2:
	case AFE_GAIN1_CON3:
	case AFE_GAIN2_CON0:
	case AFE_GAIN2_CON1:
	case AFE_GAIN2_CON2:
	case AFE_GAIN2_CON3:
	case AFE_IEC_BURST_INFO:
	case AFE_IEC_CHL_STAT0:
	case AFE_IEC_CHL_STAT1:
	case AFE_IEC_CHR_STAT0:
	case AFE_IEC_CHR_STAT1:
	case AFE_SPDIFIN_CHSTS1:
	case AFE_SPDIFIN_CHSTS2:
	case AFE_SPDIFIN_CHSTS3:
	case AFE_SPDIFIN_CHSTS4:
	case AFE_SPDIFIN_CHSTS5:
	case AFE_SPDIFIN_CHSTS6:
	case AFE_SPDIFIN_DEBUG1:
	case AFE_SPDIFIN_DEBUG2:
	case AFE_SPDIFIN_DEBUG3:
	case AFE_SPDIFIN_DEBUG4:
	case AFE_SPDIFIN_EC:
	case AFE_SPDIFIN_CKLOCK_CFG:
	case AFE_SPDIFIN_BR_DBG1:
	case AFE_SPDIFIN_CKFBDIV:
	case AFE_SPDIFIN_INT_EXT:
	case AFE_SPDIFIN_INT_EXT2:
	case SPDIFIN_FREQ_STATUS:
	case SPDIFIN_USERCODE1:
	case SPDIFIN_USERCODE2:
	case SPDIFIN_USERCODE3:
	case SPDIFIN_USERCODE4:
	case SPDIFIN_USERCODE5:
	case SPDIFIN_USERCODE6:
	case SPDIFIN_USERCODE7:
	case SPDIFIN_USERCODE8:
	case SPDIFIN_USERCODE9:
	case SPDIFIN_USERCODE10:
	case SPDIFIN_USERCODE11:
	case SPDIFIN_USERCODE12:
	case AFE_CM0_MON:
	case AFE_CM1_MON:
	case AFE_CM2_MON:
	case AFE_MPHONE_MULTI_DET_MON0:
	case AFE_MPHONE_MULTI_DET_MON1:
	case AFE_MPHONE_MULTI_DET_MON2:
	case AFE_MPHONE_MULTI2_DET_MON0:
	case AFE_MPHONE_MULTI2_DET_MON1:
	case AFE_MPHONE_MULTI2_DET_MON2:
	case AFE_ADDA_MTKAIF_MON0:
	case AFE_ADDA_MTKAIF_MON1:
	case AFE_AUD_PAD_TOP:
	case AFE_ADDA6_MTKAIF_MON0:
	case AFE_ADDA6_MTKAIF_MON1:
	case AFE_ADDA6_SRC_DEBUG_MON0:
	case AFE_ADDA6_UL_SRC_MON0:
	case AFE_ADDA6_UL_SRC_MON1:
	case AFE_ASRC11_NEW_CON8:
	case AFE_ASRC11_NEW_CON9:
	case AFE_ASRC12_NEW_CON8:
	case AFE_ASRC12_NEW_CON9:
	case AFE_LRCK_CNT:
	case AFE_DAC_MON0:
	case AFE_DL2_CUR:
	case AFE_DL3_CUR:
	case AFE_DL6_CUR:
	case AFE_DL7_CUR:
	case AFE_DL8_CUR:
	case AFE_DL10_CUR:
	case AFE_DL11_CUR:
	case AFE_UL1_CUR:
	case AFE_UL2_CUR:
	case AFE_UL3_CUR:
	case AFE_UL4_CUR:
	case AFE_UL5_CUR:
	case AFE_UL6_CUR:
	case AFE_UL8_CUR:
	case AFE_UL9_CUR:
	case AFE_UL10_CUR:
	case AFE_DL8_CHK_SUM1:
	case AFE_DL8_CHK_SUM2:
	case AFE_DL8_CHK_SUM3:
	case AFE_DL8_CHK_SUM4:
	case AFE_DL8_CHK_SUM5:
	case AFE_DL8_CHK_SUM6:
	case AFE_DL10_CHK_SUM1:
	case AFE_DL10_CHK_SUM2:
	case AFE_DL10_CHK_SUM3:
	case AFE_DL10_CHK_SUM4:
	case AFE_DL10_CHK_SUM5:
	case AFE_DL10_CHK_SUM6:
	case AFE_DL11_CHK_SUM1:
	case AFE_DL11_CHK_SUM2:
	case AFE_DL11_CHK_SUM3:
	case AFE_DL11_CHK_SUM4:
	case AFE_DL11_CHK_SUM5:
	case AFE_DL11_CHK_SUM6:
	case AFE_UL1_CHK_SUM1:
	case AFE_UL1_CHK_SUM2:
	case AFE_UL2_CHK_SUM1:
	case AFE_UL2_CHK_SUM2:
	case AFE_UL3_CHK_SUM1:
	case AFE_UL3_CHK_SUM2:
	case AFE_UL4_CHK_SUM1:
	case AFE_UL4_CHK_SUM2:
	case AFE_UL5_CHK_SUM1:
	case AFE_UL5_CHK_SUM2:
	case AFE_UL6_CHK_SUM1:
	case AFE_UL6_CHK_SUM2:
	case AFE_UL8_CHK_SUM1:
	case AFE_UL8_CHK_SUM2:
	case AFE_DL2_CHK_SUM1:
	case AFE_DL2_CHK_SUM2:
	case AFE_DL3_CHK_SUM1:
	case AFE_DL3_CHK_SUM2:
	case AFE_DL6_CHK_SUM1:
	case AFE_DL6_CHK_SUM2:
	case AFE_DL7_CHK_SUM1:
	case AFE_DL7_CHK_SUM2:
	case AFE_UL9_CHK_SUM1:
	case AFE_UL9_CHK_SUM2:
	case AFE_BUS_MON1:
	case UL1_MOD2AGT_CNT_LAT:
	case UL2_MOD2AGT_CNT_LAT:
	case UL3_MOD2AGT_CNT_LAT:
	case UL4_MOD2AGT_CNT_LAT:
	case UL5_MOD2AGT_CNT_LAT:
	case UL6_MOD2AGT_CNT_LAT:
	case UL8_MOD2AGT_CNT_LAT:
	case UL9_MOD2AGT_CNT_LAT:
	case UL10_MOD2AGT_CNT_LAT:
	case AFE_MEMIF_BUF_FULL_MON:
	case AFE_MEMIF_BUF_MON1:
	case AFE_MEMIF_BUF_MON3:
	case AFE_MEMIF_BUF_MON4:
	case AFE_MEMIF_BUF_MON5:
	case AFE_MEMIF_BUF_MON6:
	case AFE_MEMIF_BUF_MON7:
	case AFE_MEMIF_BUF_MON8:
	case AFE_MEMIF_BUF_MON9:
	case AFE_MEMIF_BUF_MON10:
	case DL2_AGENT2MODULE_CNT:
	case DL3_AGENT2MODULE_CNT:
	case DL6_AGENT2MODULE_CNT:
	case DL7_AGENT2MODULE_CNT:
	case DL8_AGENT2MODULE_CNT:
	case DL10_AGENT2MODULE_CNT:
	case DL11_AGENT2MODULE_CNT:
	case UL1_MODULE2AGENT_CNT:
	case UL2_MODULE2AGENT_CNT:
	case UL3_MODULE2AGENT_CNT:
	case UL4_MODULE2AGENT_CNT:
	case UL5_MODULE2AGENT_CNT:
	case UL6_MODULE2AGENT_CNT:
	case UL8_MODULE2AGENT_CNT:
	case UL9_MODULE2AGENT_CNT:
	case UL10_MODULE2AGENT_CNT:
	case AFE_DMIC0_SRC_DEBUG_MON0:
	case AFE_DMIC0_UL_SRC_MON0:
	case AFE_DMIC0_UL_SRC_MON1:
	case AFE_DMIC1_SRC_DEBUG_MON0:
	case AFE_DMIC1_UL_SRC_MON0:
	case AFE_DMIC1_UL_SRC_MON1:
	case AFE_DMIC2_SRC_DEBUG_MON0:
	case AFE_DMIC2_UL_SRC_MON0:
	case AFE_DMIC2_UL_SRC_MON1:
	case AFE_DMIC3_SRC_DEBUG_MON0:
	case AFE_DMIC3_UL_SRC_MON0:
	case AFE_DMIC3_UL_SRC_MON1:
	case DMIC_GAIN1_CUR:
	case DMIC_GAIN2_CUR:
	case DMIC_GAIN3_CUR:
	case DMIC_GAIN4_CUR:
	case DMIC_GAIN1_CON0:
	case DMIC_GAIN1_CON1:
	case DMIC_GAIN1_CON2:
	case DMIC_GAIN1_CON3:
	case DMIC_GAIN2_CON0:
	case DMIC_GAIN2_CON1:
	case DMIC_GAIN2_CON2:
	case DMIC_GAIN2_CON3:
	case DMIC_GAIN3_CON0:
	case DMIC_GAIN3_CON1:
	case DMIC_GAIN3_CON2:
	case DMIC_GAIN3_CON3:
	case DMIC_GAIN4_CON0:
	case DMIC_GAIN4_CON1:
	case DMIC_GAIN4_CON2:
	case DMIC_GAIN4_CON3:
	case DMIC_BYPASS_HW_GAIN:
	case ETDM_IN1_MONITOR:
	case ETDM_IN2_MONITOR:
	case ETDM_OUT1_MONITOR:
	case ETDM_OUT2_MONITOR:
	case ETDM_OUT3_MONITOR:
	case AFE_ADDA_SRC_DEBUG_MON0:
	case AFE_ADDA_SRC_DEBUG_MON1:
	case AFE_ADDA_DL_SDM_FIFO_MON:
	case AFE_ADDA_DL_SRC_LCH_MON:
	case AFE_ADDA_DL_SRC_RCH_MON:
	case AFE_ADDA_DL_SDM_OUT_MON:
	case AFE_GASRC0_NEW_CON8:
	case AFE_GASRC0_NEW_CON9:
	case AFE_GASRC0_NEW_CON12:
	case AFE_GASRC1_NEW_CON8:
	case AFE_GASRC1_NEW_CON9:
	case AFE_GASRC1_NEW_CON12:
	case AFE_GASRC2_NEW_CON8:
	case AFE_GASRC2_NEW_CON9:
	case AFE_GASRC2_NEW_CON12:
	case AFE_GASRC3_NEW_CON8:
	case AFE_GASRC3_NEW_CON9:
	case AFE_GASRC3_NEW_CON12:
	case AFE_GASRC4_NEW_CON8:
	case AFE_GASRC4_NEW_CON9:
	case AFE_GASRC4_NEW_CON12:
	case AFE_GASRC5_NEW_CON8:
	case AFE_GASRC5_NEW_CON9:
	case AFE_GASRC5_NEW_CON12:
	case AFE_GASRC6_NEW_CON8:
	case AFE_GASRC6_NEW_CON9:
	case AFE_GASRC6_NEW_CON12:
	case AFE_GASRC7_NEW_CON8:
	case AFE_GASRC7_NEW_CON9:
	case AFE_GASRC7_NEW_CON12:
	case AFE_GASRC8_NEW_CON8:
	case AFE_GASRC8_NEW_CON9:
	case AFE_GASRC8_NEW_CON12:
	case AFE_GASRC9_NEW_CON8:
	case AFE_GASRC9_NEW_CON9:
	case AFE_GASRC9_NEW_CON12:
	case AFE_GASRC10_NEW_CON8:
	case AFE_GASRC10_NEW_CON9:
	case AFE_GASRC10_NEW_CON12:
	case AFE_GASRC11_NEW_CON8:
	case AFE_GASRC11_NEW_CON9:
	case AFE_GASRC11_NEW_CON12:
		return true;
	default:
		return false;
	};
}

static const struct regmap_config mt8188_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.volatile_reg = mt8188_is_volatile_reg,
	.max_register = AFE_MAX_REGISTER,
	.num_reg_defaults_raw = ((AFE_MAX_REGISTER / 4) + 1),
	.cache_type = REGCACHE_FLAT,
};

#define AFE_IRQ_CLR_BITS (0x387)
#define ASYS_IRQ_CLR_BITS (0xffff)

static irqreturn_t mt8188_afe_irq_handler(int irq_id, void *dev_id)
{
	struct mtk_base_afe *afe = dev_id;
	unsigned int val = 0;
	unsigned int asys_irq_clr_bits = 0;
	unsigned int afe_irq_clr_bits = 0;
	unsigned int irq_status_bits = 0;
	unsigned int irq_clr_bits = 0;
	unsigned int mcu_irq_mask = 0;
	int i = 0;
	int ret = 0;

	ret = regmap_read(afe->regmap, AFE_IRQ_STATUS, &val);
	if (ret) {
		dev_info(afe->dev, "%s irq status err\n", __func__);
		afe_irq_clr_bits = AFE_IRQ_CLR_BITS;
		asys_irq_clr_bits = ASYS_IRQ_CLR_BITS;
		goto err_irq;
	}

	ret = regmap_read(afe->regmap, AFE_IRQ_MASK, &mcu_irq_mask);
	if (ret) {
		dev_info(afe->dev, "%s read irq mask err\n", __func__);
		afe_irq_clr_bits = AFE_IRQ_CLR_BITS;
		asys_irq_clr_bits = ASYS_IRQ_CLR_BITS;
		goto err_irq;
	}

	/* only clr cpu irq */
	val &= mcu_irq_mask;

	for (i = 0; i < MT8188_AFE_MEMIF_NUM; i++) {
		struct mtk_base_afe_memif *memif = &afe->memif[i];
		struct mtk_base_irq_data const *irq_data;

		if (memif->irq_usage < 0)
			continue;

		irq_data = afe->irqs[memif->irq_usage].irq_data;

		irq_status_bits = BIT(irq_data->irq_status_shift);
		irq_clr_bits = BIT(irq_data->irq_clr_shift);

		if (!(val & irq_status_bits))
			continue;

		if (irq_data->irq_clr_reg == ASYS_IRQ_CLR)
			asys_irq_clr_bits |= irq_clr_bits;
		else
			afe_irq_clr_bits |= irq_clr_bits;

		snd_pcm_period_elapsed(memif->substream);
	}

err_irq:
	/* clear irq */
	if (asys_irq_clr_bits)
		regmap_write(afe->regmap, ASYS_IRQ_CLR, asys_irq_clr_bits);
	if (afe_irq_clr_bits)
		regmap_write(afe->regmap, AFE_IRQ_MCU_CLR, afe_irq_clr_bits);

	return IRQ_HANDLED;
}

static int mt8188_afe_runtime_suspend(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct arm_smccc_res res;

	dev_dbg(dev, "%s\n", __func__);

	if (!afe->regmap || afe_priv->pm_runtime_bypass_reg_ctl)
		goto skip_regmap;

	mt8188_afe_disable_main_clock(afe);

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);

skip_regmap:
	mt8188_afe_disable_reg_rw_clk(afe);

	/*
	 * MT8188 AFE hardware has a control reg to disable AFE.
	 * This reg can only be controlled in ARM supervisor mode.
	 */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
		      MT8188_AUDIO_SMC_OP_DISABLE_NORMAL_AFE,
		      0, 0, 0, 0, 0, 0, &res);

	return 0;
}

static int mt8188_afe_runtime_resume(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct arm_smccc_res res;

	dev_dbg(dev, "%s\n", __func__);

	/*
	 * MT8188 AFE hardware has a control reg to enable AFE.
	 * This reg can only be controlled in ARM supervisor mode.
	 */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
		      MT8188_AUDIO_SMC_OP_ENABLE_NORMAL_AFE,
		      0, 0, 0, 0, 0, 0, &res);

	mt8188_afe_enable_reg_rw_clk(afe);

	if (!afe->regmap || afe_priv->pm_runtime_bypass_reg_ctl)
		goto skip_regmap;

	regcache_cache_only(afe->regmap, false);
	regcache_sync(afe->regmap);

	mt8188_afe_enable_main_clock(afe);

skip_regmap:
	return 0;
}

static int mt8188_afe_suspend(struct device *dev)
{
#ifdef CONFIG_SND_SOC_MT8188_ADSP
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dev, "%s\n", __func__);

	mt8188_afe_get_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A1SYS_HP_SEL],
				  &afe_priv->backup_a1sys_hp_parents);
	mt8188_afe_get_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_AUD_INTBUS_SEL],
				  &afe_priv->backup_aud_intbus_parents);
	mt8188_afe_get_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A2SYS_SEL],
				  &afe_priv->backup_a2sys_parents);
	mt8188_afe_get_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A3SYS_SEL],
				  &afe_priv->backup_a3sys_parents);
	mt8188_afe_get_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A4SYS_SEL],
				  &afe_priv->backup_a4sys_parents);
	dev_dbg(afe->dev,
		"%s(), after get clk parent inbus=%p, a1sys=%p, a2sys=%p, a3sys=%p, a4sys=%p\n",
		__func__, afe_priv->backup_aud_intbus_parents,
		afe_priv->backup_a1sys_hp_parents,
		afe_priv->backup_a2sys_parents,
		afe_priv->backup_a3sys_parents,
		afe_priv->backup_a4sys_parents);

	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A1SYS_HP_SEL],
				  afe_priv->clk[MT8188_CLK_XTAL_26M]);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_AUD_INTBUS_SEL],
				  afe_priv->clk[MT8188_CLK_XTAL_26M]);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A2SYS_SEL],
				  afe_priv->clk[MT8188_CLK_XTAL_26M]);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A3SYS_SEL],
				  afe_priv->clk[MT8188_CLK_XTAL_26M]);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A4SYS_SEL],
				  afe_priv->clk[MT8188_CLK_XTAL_26M]);
#endif
	return 0;
}

static int mt8188_afe_resume(struct device *dev)
{
#ifdef CONFIG_SND_SOC_MT8188_ADSP
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt8188_afe_private *afe_priv = afe->platform_priv;

	dev_dbg(dev, "%s\n", __func__);

	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_AUD_INTBUS_SEL],
				  afe_priv->backup_aud_intbus_parents);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A1SYS_HP_SEL],
				  afe_priv->backup_a1sys_hp_parents);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A2SYS_SEL],
				  afe_priv->backup_a2sys_parents);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A3SYS_SEL],
				  afe_priv->backup_a3sys_parents);
	mt8188_afe_set_clk_parent(afe,
				  afe_priv->clk[MT8188_CLK_TOP_A4SYS_SEL],
				  afe_priv->backup_a4sys_parents);
#endif
	return 0;
}

static int mt8188_afe_component_probe(struct snd_soc_component *component)
{
	struct mtk_base_afe *afe = snd_soc_component_get_drvdata(component);
	int ret = 0;

	snd_soc_component_init_regmap(component, afe->regmap);

	ret = mtk_afe_add_sub_dai_control(component);
	if (ret)
		return ret;

	ret = mt8188_afe_add_debug_controls(component);
	if (ret)
		return ret;

	ret = mt8188_afe_debugfs_init(component);
	if (ret)
		return ret;

	ret = mt8188_afe_add_clk_controls(component);
	if (ret)
		return ret;

	return ret;
}

static void mt8188_afe_component_remove(struct snd_soc_component *component)
{
	mt8188_afe_debugfs_exit(component);
}

static const struct snd_soc_component_driver mt8188_afe_component = {
	.name = AFE_PCM_NAME,
	.pointer       = mtk_afe_pcm_pointer,
	.pcm_construct = mtk_afe_pcm_new,
	.pcm_destruct  = mtk_afe_pcm_free,
	.probe         = mt8188_afe_component_probe,
	.remove        = mt8188_afe_component_remove,
	.copy_user     = mtk_afe_pcm_copy_user,
#if !IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	.suspend       = mtk_afe_suspend,
	.resume        = mtk_afe_resume,
#endif
};

static int init_memif_priv_data(struct mtk_base_afe *afe)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	struct mtk_dai_memif_priv *memif_priv;
	int i;

	for (i = MT8188_AFE_MEMIF_START; i < MT8188_AFE_MEMIF_END; i++) {
		memif_priv = devm_kzalloc(afe->dev,
					  sizeof(struct mtk_dai_memif_priv),
					  GFP_KERNEL);
		if (!memif_priv)
			return -ENOMEM;

		afe_priv->dai_priv[i] = memif_priv;
	}

	return 0;
}

static int mt8188_dai_memif_register(struct mtk_base_afe *afe)
{
	struct mtk_base_afe_dai *dai;

	dai = devm_kzalloc(afe->dev, sizeof(*dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;

	list_add(&dai->list, &afe->sub_dais);

	dai->dai_drivers = mt8188_memif_dai_driver;
	dai->num_dai_drivers = ARRAY_SIZE(mt8188_memif_dai_driver);

	dai->dapm_widgets = mt8188_memif_widgets;
	dai->num_dapm_widgets = ARRAY_SIZE(mt8188_memif_widgets);
	dai->dapm_routes = mt8188_memif_routes;
	dai->num_dapm_routes = ARRAY_SIZE(mt8188_memif_routes);
	dai->controls = mt8188_memif_controls;
	dai->num_controls = ARRAY_SIZE(mt8188_memif_controls);

	return init_memif_priv_data(afe);
}

typedef int (*dai_register_cb)(struct mtk_base_afe *);
static const dai_register_cb dai_register_cbs[] = {
	mt8188_dai_adda_register,
	mt8188_dai_dir_register,
	mt8188_dai_dit_register,
	mt8188_dai_dmic_register,
	mt8188_dai_etdm_register,
	mt8188_dai_gasrc_register,
	mt8188_dai_hostless_register,
	mt8188_dai_hw_gain_register,
	mt8188_dai_multi_in_register,
	mt8188_dai_pcm_register,
	mt8188_dai_memif_register,
};

static const struct reg_sequence mt8188_afe_reg_defaults[] = {
	{ AFE_IRQ_MASK, 0x387ffff },
	{ AFE_IRQ3_CON, BIT(30) },
	{ AFE_IRQ9_CON, BIT(30) },
	{ ETDM_IN1_CON4, 0x12000100 },
	{ ETDM_IN2_CON4, 0x12000100 },
};

static const struct reg_sequence mt8188_cg_patch[] = {
	{ AUDIO_TOP_CON0, 0xfffffffb },
	{ AUDIO_TOP_CON1, 0xfffffff8 },
};

#if !IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
static const unsigned int afe_backup_list[] = {
	AUDIO_TOP_CON5,
};
#endif

static int mt8188_afe_init_registers(struct mtk_base_afe *afe)
{
	return regmap_multi_reg_write(afe->regmap,
			mt8188_afe_reg_defaults,
			ARRAY_SIZE(mt8188_afe_reg_defaults));
}

static int mt8188_afe_parse_of(struct mtk_base_afe *afe,
			       struct device_node *np)
{
	struct mt8188_afe_private *afe_priv = afe->platform_priv;
	int force_cnt;
	u32 *memif_idx;
	int ret;
	int i;

	force_cnt = of_property_count_u32_elems(np, "mediatek,memif_force_sram");

	if (force_cnt > 0) {
		if (force_cnt > MT8188_AFE_MEMIF_NUM) {
			dev_info(afe->dev, "%d > memif num\n", force_cnt);
			force_cnt = MT8188_AFE_MEMIF_NUM;
		}

		memif_idx = devm_kcalloc(afe->dev, force_cnt,
					sizeof(*memif_idx), GFP_KERNEL);
		if (!memif_idx)
			return -ENOMEM;

		ret = of_property_read_u32_array(np, "mediatek,memif_force_sram",
						 memif_idx, force_cnt);
		if (!ret) {
			for (i = 0; i < force_cnt; i++) {
				if (memif_idx[i] >= MT8188_AFE_MEMIF_NUM)
					continue;

				afe->memif[memif_idx[i]].use_dram_only = 0;
				dev_info(afe->dev, "memif %u use sram\n", memif_idx[i]);
			}
		}
	}

#if IS_ENABLED(CONFIG_SND_SOC_MT6359)
	afe_priv->topckgen = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
							     "topckgen");
	if (IS_ERR(afe_priv->topckgen)) {
		dev_info(afe->dev, "%s() Cannot find topckgen controller: %ld\n",
			__func__, PTR_ERR(afe_priv->topckgen));
	}
#endif
	return 0;
}

static int mt8188_afe_priv_setup(struct platform_device *pdev,
		struct mt8188_afe_private *afe_priv)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	ret = devm_of_platform_populate(&pdev->dev);

	afe_priv->adsp_data.set_afe_memif =
		mt8188_adsp_set_afe_memif;
	afe_priv->adsp_data.set_afe_memif_enable =
		mt8188_adsp_set_afe_memif_enable;
	afe_priv->adsp_data.set_afe_init =
		mt8188_adsp_set_afe_init;
	afe_priv->adsp_data.set_afe_uninit =
		mt8188_adsp_set_afe_uninit;
#endif

	return ret;
}

#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
static int mt8188_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode)
{
	/* dummy function for mtk-sram-manager */
	return 0;
}

static const struct mtk_audio_sram_ops mt8188_sram_ops = {
	.set_sram_mode = mt8188_set_sram_mode,
};
#endif

#define MT8188_DELAY_US	10
#define MT8188_TIMEOUT_US	USEC_PER_SEC

static int bus_protect_enable(struct regmap *regmap)
{
	int ret;
	u32 val;
	u32 mask;

	val = 0;
	mask = MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP1;
	regmap_write(regmap, MT8188_TOP_AXI_PROT_EN_2_SET, mask);

	ret = regmap_read_poll_timeout(regmap, MT8188_TOP_AXI_PROT_EN_2_STA,
				       val, (val & mask) == mask,
				       MT8188_DELAY_US, MT8188_TIMEOUT_US);
	if (ret)
		return ret;

	val = 0;
	mask = MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP2;
	regmap_write(regmap, MT8188_TOP_AXI_PROT_EN_2_SET, mask);

	ret = regmap_read_poll_timeout(regmap, MT8188_TOP_AXI_PROT_EN_2_STA,
				       val, (val & mask) == mask,
				       MT8188_DELAY_US, MT8188_TIMEOUT_US);
	if (ret)
		return ret;

	return 0;
}

static int bus_protect_disable(struct regmap *regmap)
{
	int ret;
	u32 val;
	u32 mask;

	val = 0;
	mask = MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP2;
	regmap_write(regmap, MT8188_TOP_AXI_PROT_EN_2_CLR, mask);

	ret = regmap_read_poll_timeout(regmap, MT8188_TOP_AXI_PROT_EN_2_STA,
				       val, !(val & mask),
				       MT8188_DELAY_US, MT8188_TIMEOUT_US);
	if (ret)
		return ret;

	val = 0;
	mask = MT8188_TOP_AXI_PROT_EN_2_AUDIO_STEP1;
	regmap_write(regmap, MT8188_TOP_AXI_PROT_EN_2_CLR, mask);

	ret = regmap_read_poll_timeout(regmap, MT8188_TOP_AXI_PROT_EN_2_STA,
				       val, !(val & mask),
				       MT8188_DELAY_US, MT8188_TIMEOUT_US);
	if (ret)
		return ret;

	return 0;
}

static int mt8188_afe_pcm_dev_probe(struct platform_device *pdev)
{
	struct mtk_base_afe *afe;
	struct mt8188_afe_private *afe_priv;
	struct device *dev;
	int i, irq_id, ret;
	struct snd_soc_component *component;
	struct reset_control *rstc;
	struct resource *res;
	void __iomem *bus_remap_ctrl;
	struct regmap *infra_ao;
	uint32_t val;

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(33));
	if (ret)
		return ret;

	afe = devm_kzalloc(&pdev->dev, sizeof(*afe), GFP_KERNEL);
	if (!afe)
		return -ENOMEM;

	afe->platform_priv = devm_kzalloc(&pdev->dev, sizeof(*afe_priv),
					  GFP_KERNEL);
	if (!afe->platform_priv)
		return -ENOMEM;

	afe_priv = afe->platform_priv;
	afe->dev = &pdev->dev;
	dev = afe->dev;

	afe->base_addr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(afe->base_addr)) {
		dev_info(&pdev->dev, "AFE base_addr not found\n");
		return PTR_ERR(afe->base_addr);
	}

	infra_ao = syscon_regmap_lookup_by_phandle(dev->of_node,
						   "mediatek,infracfg");
	if (IS_ERR(infra_ao)) {
		dev_info(afe->dev, "%s() Cannot find infra_ao controller: %ld\n",
			__func__, PTR_ERR(infra_ao));
		return PTR_ERR(infra_ao);
	}

	/* reset controller to reset audio regs before regmap cache */
	rstc = devm_reset_control_get_exclusive(dev, "audiosys");
	if (IS_ERR(rstc)) {
		ret = PTR_ERR(rstc);
		dev_info(dev, "could not get audiosys reset:%d\n", ret);
		return ret;
	}

	ret = bus_protect_enable(infra_ao);
	if (ret) {
		dev_info(&pdev->dev, "bus_protect_enable err = %d", ret);
		return ret;
	}

	ret = reset_control_reset(rstc);
	if (ret) {
		dev_info(dev, "failed to trigger audio reset:%d\n", ret);
		bus_protect_disable(infra_ao);
		return ret;
	}

	ret = bus_protect_disable(infra_ao);
	if (ret) {
		dev_info(&pdev->dev, "bus_protect_disable err = %d", ret);
		return ret;
	}

	/* initial audio related clock */
	ret = mt8188_afe_init_clock(afe);
	if (ret) {
		dev_info(&pdev->dev, "init clock error\n");
		return ret;
	}

	spin_lock_init(&afe_priv->afe_ctrl_lock);

	/* AFE SRAM */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		afe_priv->sram_base = res->start;
		afe_priv->sram_size = resource_size(res);
	}
	dev_info(dev, "%s, AFE_SRAM init with res=%d\n", __func__, res);

	mutex_init(&afe->irq_alloc_lock);

	/* irq initialize */
	afe->irqs_size = MT8188_AFE_IRQ_NUM;
	afe->irqs = devm_kcalloc(dev, afe->irqs_size, sizeof(*afe->irqs),
				 GFP_KERNEL);
	if (!afe->irqs)
		return -ENOMEM;

	for (i = 0; i < afe->irqs_size; i++)
		afe->irqs[i].irq_data = &irq_data[i];

#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
	/* init sram */
	afe->sram = devm_kzalloc(dev, sizeof(struct mtk_audio_sram),
				 GFP_KERNEL);
	if (!afe->sram)
		return -ENOMEM;

	ret = mtk_audio_sram_init(dev, afe->sram, &mt8188_sram_ops);
	if (ret)
		return ret;
#endif

	/* init memif */
	afe->memif_size = MT8188_AFE_MEMIF_NUM;
	afe->memif = devm_kcalloc(dev, afe->memif_size, sizeof(*afe->memif),
				  GFP_KERNEL);
	if (!afe->memif)
		return -ENOMEM;

	for (i = 0; i < afe->memif_size; i++) {
		afe->memif[i].data = &memif_data[i];
		afe->memif[i].irq_usage = mt8188_afe_memif_const_irqs[i];
		afe->memif[i].const_irq = 1;
		afe->irqs[afe->memif[i].irq_usage].irq_occupyed = true;
#if IS_ENABLED(CONFIG_SND_SOC_MTK_SRAM)
		afe->memif[i].use_dram_only = 1;
#endif
	}

	/* request irq */
	irq_id = platform_get_irq(pdev, 0);
	if (irq_id < 0) {
		dev_info(dev, "%s no irq found\n", dev->of_node->name);
		return -ENXIO;
	}
	dev_info(afe->dev, "%s irq_id=%d\n", __func__, irq_id);
	ret = devm_request_irq(dev, irq_id, mt8188_afe_irq_handler,
			       IRQF_TRIGGER_NONE, "asys-isr", (void *)afe);
	if (ret) {
		dev_info(dev, "could not request_irq for asys-isr\n");
		return ret;
	}

	/* init sub_dais */
	INIT_LIST_HEAD(&afe->sub_dais);

	for (i = 0; i < ARRAY_SIZE(dai_register_cbs); i++) {
		ret = dai_register_cbs[i](afe);
		if (ret) {
			dev_info(afe->dev, "dai register i %d fail, ret %d\n",
				 i, ret);
			return ret;
		}
	}

	/* init dai_driver and component_driver */
	ret = mtk_afe_combine_sub_dai(afe);
	if (ret) {
		dev_info(afe->dev, "mtk_afe_combine_sub_dai fail, ret %d\n",
			 ret);
		return ret;
	}

	afe->mtk_afe_hardware = &mt8188_afe_hardware;
	afe->memif_fs = mt8188_memif_fs;
	afe->irq_fs = mt8188_irq_fs;

#if !IS_ENABLED(CONFIG_SND_SOC_MT8188_ADSP)
	afe->reg_back_up_list = afe_backup_list;
	afe->reg_back_up_list_num = ARRAY_SIZE(afe_backup_list);
	afe->reg_back_up = devm_kcalloc(dev, afe->reg_back_up_list_num,
					sizeof(unsigned int), GFP_KERNEL);
	if (!afe->reg_back_up)
		return -ENOMEM;
#endif

	afe->runtime_resume = mt8188_afe_runtime_resume;
	afe->runtime_suspend = mt8188_afe_runtime_suspend;

	afe->request_dram_resource = mt8188_afe_dram_request;
	afe->release_dram_resource = mt8188_afe_dram_release;

	platform_set_drvdata(pdev, afe);

	pm_runtime_enable(dev);
	if (!pm_runtime_enabled(dev)) {
		ret = mt8188_afe_runtime_resume(dev);
		if (ret)
			goto err_pm_disable;
	}

	/* enable clock for regcache get default value from hw */
	afe_priv->pm_runtime_bypass_reg_ctl = true;
	pm_runtime_get_sync(&pdev->dev);

	afe->regmap = devm_regmap_init_mmio(&pdev->dev, afe->base_addr,
					    &mt8188_afe_regmap_config);
	if (IS_ERR(afe->regmap)) {
		ret = PTR_ERR(afe->regmap);
		goto err_pm_put;
	}

	ret = regmap_register_patch(afe->regmap, mt8188_cg_patch,
				    ARRAY_SIZE(mt8188_cg_patch));
	if (ret < 0) {
		dev_info(dev, "Failed to apply cg patch\n");
		goto err_pm_put;
	}

	/* bus init for DRAM remap */
	bus_remap_ctrl = ioremap(BUS_REMAP_CTRL, 4);
	val = readl(bus_remap_ctrl) | EMI_OFFSET_EN | SUB_EMI_OFFSET_EN;
	writel(val, bus_remap_ctrl);
	iounmap(bus_remap_ctrl);

	ret = mt8188_afe_priv_setup(pdev, afe_priv);
	if (ret)
		goto err_populate;

	ret = mt8188_afe_parse_of(afe, pdev->dev.of_node);
	if (ret)
		goto err_populate;

	/* register component */
	ret = devm_snd_soc_register_component(dev, &mt8188_afe_component,
					      NULL, 0);
	if (ret) {
		dev_dbg(dev, "err_platform\n");
		goto err_platform;
	}

	ret = devm_snd_soc_register_component(afe->dev,
					      &mt8188_afe_pcm_dai_component,
					      afe->dai_drivers,
					      afe->num_dai_drivers);
	if (ret) {
		dev_info(dev, "err_dai_component\n");
		return ret;
	}

	component = devm_kzalloc(&pdev->dev, sizeof(*component), GFP_KERNEL);
	if (!component)
		return -ENOMEM;
	ret = snd_soc_component_initialize(component,
					   &mt8188_afe_pcm_dai_component,
					   &pdev->dev);
	if (ret < 0)
		return ret;
#ifdef CONFIG_DEBUG_FS
	component->debugfs_prefix = "pcm";
#endif
	ret = snd_soc_add_component(component, NULL, 0);
	if (ret) {
		dev_info(dev, "err_add_component\n");
		goto err_dai_component;
	}

	mt8188_afe_init_registers(afe);

	pm_runtime_put_sync(&pdev->dev);
	afe_priv->pm_runtime_bypass_reg_ctl = false;

	regcache_cache_only(afe->regmap, true);
	regcache_mark_dirty(afe->regmap);
	dev_info(dev, "%s, done!\n", __func__);

	return 0;

err_dai_component:
err_platform:
	mt8188_afe_disable_reg_rw_clk(afe);
err_populate:
err_pm_put:
	pm_runtime_put_sync(dev);
err_pm_disable:
	pm_runtime_disable(dev);

	dev_info(dev, "%s()-, fail\n", __func__);

	return ret;
}

static int mt8188_afe_pcm_dev_remove(struct platform_device *pdev)
{
	struct mtk_base_afe *afe = platform_get_drvdata(pdev);

	snd_soc_unregister_component(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		mt8188_afe_runtime_suspend(&pdev->dev);

	mt8188_afe_deinit_clock(afe);

	return 0;
}

static const struct of_device_id mt8188_afe_pcm_dt_match[] = {
	{ .compatible = "mediatek,mt8188-audio", },
	{},
};
MODULE_DEVICE_TABLE(of, mt8188_afe_pcm_dt_match);

static const struct dev_pm_ops mt8188_afe_pm_ops = {
	SET_RUNTIME_PM_OPS(mt8188_afe_runtime_suspend,
			   mt8188_afe_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(mt8188_afe_suspend, mt8188_afe_resume)
};

static struct platform_driver mt8188_afe_pcm_driver = {
	.driver = {
		   .name = "mt8188-audio",
		   .of_match_table = mt8188_afe_pcm_dt_match,
#ifdef CONFIG_PM
		   .pm = &mt8188_afe_pm_ops,
#endif
	},
	.probe = mt8188_afe_pcm_dev_probe,
	.remove = mt8188_afe_pcm_dev_remove,
};

module_platform_driver(mt8188_afe_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC AFE platform driver for 8188");
MODULE_AUTHOR("Chun-Chia.Chiu <chun-chia.chiu@mediatek.com>");
MODULE_LICENSE("GPL v2");
