# Makefile for the focaltech touchscreen drivers.

ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
obj-$(CONFIG_HID_TOUCHSCREEN_FTS)	 := focal_hid.o
focal_hid-objs := focal_hid_multitouch.o focal_hid_core.o focal_hid_esd.o \
                  focal_hid_upgrade.o focal_hid_i2c.o \
                  focaltech_test/focaltech_test.o focaltech_test/focaltech_test_ini.o \
                  focaltech_test/supported_ic/focaltech_test_ft8205.o
