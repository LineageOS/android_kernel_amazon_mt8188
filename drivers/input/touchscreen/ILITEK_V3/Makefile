# Makefile for the ilitek touchscreen drivers.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -Wall

ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V3/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ILITEK_V3/firmware/

obj-$(CONFIG_TOUCHSCREEN_ILI9882) += ilitek.o
ilitek-y := \
		ilitek_v3.o \
		ilitek_v3_i2c.o \
		ilitek_v3_core.o \
		ilitek_v3_ic.o \
		ilitek_v3_touch.o \
		ilitek_v3_mp.o \
		ilitek_v3_flash.o \
		ilitek_v3_node.o
