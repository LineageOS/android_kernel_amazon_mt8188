/*
 * Copyright 2023 Amazon Technologies, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License Version 2.
 * You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include "stylus_battery_algo.h"

int battery_level_debug;

/* for debug */
static ssize_t battery_level_debug_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int32_t ret = 0;

	ret = snprintf(buf, PAGE_SIZE, " battery_level_debug = %d\n", battery_level_debug);
	if (ret < 0)
		STYLUS_ERR("snprintf copy string failed %d", ret);

	return ret;
}

static ssize_t  battery_level_debug_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	uint8_t value = 0;

	if (kstrtou8(buf, 10, &value) || value > 100) {
		STYLUS_ERR("Invalid value:%d\n", value);
		return -EINVAL;
	}
	battery_level_debug = value;

	return count;
}

/* manually fill in the packet information of the stylus, and prohibit the use of the stylus at this time */
static ssize_t algo_packet_debug_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int size = 0;
	stylus_packet_info_t packet_info = { 0 };

	size = sscanf(buf, "%lld %d %x %x", &packet_info.time, &packet_info.capacity,
			&packet_info.vendor_id, &packet_info.pen_id);
	STYLUS_DEBUG("TEST DATA: time %lld bat %d vid %x pid %x\n",
			packet_info.time, packet_info.capacity,
			packet_info.vendor_id, packet_info.pen_id);
	if (size == 4) {
		stylus_info_packet(packet_info);
		stylus_info_process();
	} else {
		STYLUS_ERR("parse stylus manually fill packet data fail %d\n", size);
	}

	return count;
}

static DEVICE_ATTR_RW(battery_level_debug);
static DEVICE_ATTR_WO(algo_packet_debug);
static struct attribute *stylus_debug_attrs[] = {
	&dev_attr_battery_level_debug.attr,
	&dev_attr_algo_packet_debug.attr,
	NULL
};

static const struct attribute_group stylus_debug_attribute_group = {
	.attrs = stylus_debug_attrs
};

int stylus_debug_init(struct device *stylus_dev)
{
	int ret = 0;

	if (!stylus_dev) {
		STYLUS_ERR("%s failed, stylus_dev is null.\n", __FUNCTION__);
		return -EINVAL;
	}

	if (sysfs_create_group(&stylus_dev->kobj, &stylus_debug_attribute_group)) {
		STYLUS_ERR("%s failed, Failed to create stylus debug node.\n", __FUNCTION__);
		ret = -EINVAL;
	}
	battery_level_debug = 0;
	return ret;
}

int stylus_debug_deinit(struct device *stylus_dev)
{
	if (!stylus_dev) {
		STYLUS_ERR("%s failed, stylus_dev is null.\n", __FUNCTION__);
		return -EINVAL;
	}

	sysfs_remove_group(&stylus_dev->kobj, &stylus_debug_attribute_group);

	return 0;
}
