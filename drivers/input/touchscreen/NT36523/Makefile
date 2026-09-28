#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-$(CONFIG_TOUCHSCREEN_NT36XXX) += novatek_nvt.o
novatek_nvt-y := \
				nt36xxx.o \
				nt36xxx_fw_update.o \
				nt36xxx_ext_proc.o \
				nt36xxx_mp_ctrlram.o
