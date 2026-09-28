#
# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

SSPM_VER := v2
QOS_SHARE_SUPPORT=y

ccflags-y += -I$(srctree)/include/memory/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/$(MTK_PLATFORM)/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
ccflags-$(QOS_SHARE_SUPPORT) += -DQOS_SHARE_SUPPORT

obj-$(CONFIG_MTK_QOS_FRAMEWORK) += mtk_qos.o
mtk_qos-objs := mtk_qos_main.o mtk_qos_bound.o mtk_qos_sram.o mtk_qos_sysfs.o mtk_qos_ipi_v2.o mtk_qos_share.o

