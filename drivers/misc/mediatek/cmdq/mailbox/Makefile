# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2019 MediaTek Inc.
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include \
             -I$(srctree)/drivers/misc/mediatek/include \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/mdp/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/misc/mediatek/mmp/ \
             -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone/ \
             -I$(srctree)/drivers/misc/mediatek/tee_gp/include/

ifeq ($(or $(strip $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT)), $(strip $(CONFIG_MTK_CAM_SECURITY_SUPPORT))),y)

ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),m)
include $(srctree)/drivers/tee/teei/Makefile.include
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),m)
include $(srctree)/drivers/tee/gud/Makefile.include
endif

ccflags-y += -DCMDQ_SECURE_SUPPORT

obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-sec-drv.o

ifneq ($(findstring $(CONFIG_MTK_ENABLE_GENIEZONE),y m),)
ifneq ($(findstring $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT),y m),)
    ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
    ccflags-y += -DCMDQ_SECURE_MTEE_SUPPORT
    cmdq-sec-drv-y := cmdq_sec_mtee.o cmdq-sec-mailbox.o cmdq-sec-helper.o
endif
endif

ifneq ($(findstring $(CONFIG_MTK_TEE_GP_SUPPORT),y m),)
	ccflags-y += -DCMDQ_GP_SUPPORT
	cmdq-sec-drv-y := cmdq-sec-gp.o cmdq-sec-mailbox.o cmdq-sec-helper.o
endif

endif # $(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) or $(CONFIG_MTK_CAM_SECURITY_SUPPORT)

obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += mtk-cmdq-drv-ext.o
mtk-cmdq-drv-ext-y := mtk-cmdq-mailbox-ext.o mtk-cmdq-helper-ext.o cmdq-util.o

obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-test.o

ifneq ($(wildcard $(srctree)/$(src)/cmdq-platform-mt8188.c),)
obj-$(CONFIG_MTK_CMDQ_MBOX_EXT) += cmdq-platform-mt8188.o
endif
