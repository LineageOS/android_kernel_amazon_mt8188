# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt8188-afe-objs := \
	mt8188-afe-clk.o \
	mt8188-afe-debug.o \
	mt8188-afe-pcm.o \
	mt8188-audsys-clk.o \
	mt8188-dai-adda.o \
	mt8188-dai-dir.o \
	mt8188-dai-dit.o \
	mt8188-dai-dmic.o \
	mt8188-dai-etdm.o \
	mt8188-dai-gasrc.o \
	mt8188-dai-hostless.o \
	mt8188-dai-hw-gain.o \
	mt8188-dai-multi-in.o \
	mt8188-dai-pcm.o

obj-$(CONFIG_SND_SOC_MT8188) += snd-soc-mt8188-afe.o

# adsp platform driver
ifneq ($(CONFIG_SND_SOC_MT8188_ADSP),)
snd-soc-mt8188-adsp-pcm-objs := \
	mt8188-adsp-pcm.o mt8188-adsp-utils.o
endif

obj-$(CONFIG_SND_SOC_MT8188_ADSP) += snd-soc-mt8188-adsp-pcm.o

ifneq ($(CONFIG_SND_SOC_MT8188_ADSP),)
DSP_VERSION := $(subst ",,$(CONFIG_MTK_HIFIXDSP_VERSION))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifidsp_ipi
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifidsp_audio_ipi/common/framework
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifidsp_audio_ipi/common/utility
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hifidsp_audio_ipi/$(DSP_VERSION)
endif

# machine driver
obj-$(CONFIG_SND_SOC_MT8188_EVB) += mt8188-evb.o
obj-$(CONFIG_SND_SOC_MT8188_P1V1) += mt8188-evb.o
obj-$(CONFIG_SND_SOC_MT8188_SUNSTONE) += mt8188-sunstone.o
