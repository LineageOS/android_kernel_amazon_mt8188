# SPDX-License-Identifier: GPL-2.0
#
# virtual sensor thermal specific Makefile
#
# Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y  += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y  += -I$(srctree)/drivers/thermal/
ccflags-y += -I$(srctree)/drivers/power/supply

# platform thermal drivers
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_tablet_thermal.o

#thermal sensors
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_thermal_sensors.o

# platform interface
virtual_sensor_thermal_sensors-y += virtual_sensor_thermal_sensor.o virtual_sensor_interface.o

# governors
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_gov.o

# cooling devices
obj-$(CONFIG_VIRTUAL_SENSOR_THERMAL) += virtual_sensor_cooler_bcct.o \
					virtual_sensor_cooler_backlight.o \
					virtual_sensor_cooler_budget.o \
					virtual_sensor_cooler_devfreq.o
