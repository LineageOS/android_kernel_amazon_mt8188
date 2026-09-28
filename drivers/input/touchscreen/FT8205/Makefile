# Makefile for the focaltech touchscreen drivers.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-$(CONFIG_TOUCHSCREEN_FTS) += focaltech_fts.o
focaltech_fts-y := \
				focaltech_core.o \
				focaltech_ex_fun.o \
				focaltech_ex_mode.o \
				focaltech_gesture.o \
				focaltech_esdcheck.o \
				focaltech_point_report_check.o \
				focaltech_test/focaltech_test.o \
				focaltech_test/focaltech_test_ini.o \
				focaltech_test/supported_ic/focaltech_test_ft8205.o \
				focaltech_flash.o \
				focaltech_flash/focaltech_upgrade_ft8205.o \
				focaltech_i2c.o
