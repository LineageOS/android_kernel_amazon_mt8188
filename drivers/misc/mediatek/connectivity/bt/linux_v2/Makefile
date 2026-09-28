# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_MTK_BTMTK_SDIO_UNIFY) += btmtk_sdio_unify.o

btmtk_sdio_unify-y := \
	sdio/btmtksdio.o \
	btmtk_woble.o \
	btmtk_buffer_mode.o \
	btmtk_chip_reset.o \
	btmtk_main.o \
	btmtk_fw_log.o

ccflags-y += -I$(srctree)/$(src)/include
ccflags-y += -I$(srctree)/$(src)/include/sdio

ccflags-y += -DANDROID_OS
ccflags-y += -DCHIP_IF_SDIO
ccflags-y += -DSDIO_DEBUG=0

ccflags-y += -DCFG_CHIP_RESET_KO_SUPPORT

ccflags-y += -DCFG_SUPPORT_BT_DL_WIFI_PATCH=1
ccflags-y += -DCFG_SUPPORT_BT_DL_ZB_PATCH=0

ccflags-y += -DCFG_SUPPORT_BLUEZ=0
ccflags-y += -DCFG_SUPPORT_DVT=0
ccflags-y += -DCFG_SUPPORT_HW_DVT=0
ccflags-y += -DCFG_SUPPORT_MULTI_DEV_NODE=0
ccflags-y += -DCFG_SUPPORT_BMR_RX_CLK=0

ifeq ($(CONFIG_MTK_BTMTK_SDIO_UNIFY_DEBUG),y)
ccflags-y += -DBUILD_QA_DBG=1
ccflags-y += -DBTMTK_DEBUG_SOP
else
ccflags-y += -DBUILD_QA_DBG=0
endif
