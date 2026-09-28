# SPDX-License-Identifier: GPL-2.0-only
#
# Driver locate under i2c-hid/
#
# To include this Makefile, add
# ...
# obj-$(CONFIG_I2C_HID_OF_NVT_TS) += i2c_hid_of_nvt_ts/
# ...
# in i2c-hid/Makefile
#
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-y += -I$(srctree)/drivers/hid/stylus/
ccflags-y += -I$(srctree)/drivers/hid/hid_touch_metrics/
obj-$(CONFIG_I2C_HID_OF_NVT_TS)				+= i2c-hid-nt36523n.o
i2c-hid-nt36523n-y := i2c-hid-of-nvt-ts.o \
		nvt_i2c_hid.o \
		nvt_i2c_hid_ext_proc.o \
		nvt_i2c_hid_fw_update.o \
		nvt_i2c_hid_selftest.o
