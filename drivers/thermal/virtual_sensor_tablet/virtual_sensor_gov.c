/*
 * Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <linux/thermal.h>
#include <linux/module.h>
#include "thermal_core.h"
#include "virtual_sensor_thermal.h"

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#include <linux/metricslog.h>
#endif

#define PREFIX "thermalthrottle:def"

/**
 * virtual_sensor_throttle
 * @tz - thermal_zone_device
 * @trip - the trip point
 *
 */
static int virtual_sensor_throttle(struct thermal_zone_device *tz, int trip)
{
	int trip_temp, temp;
	struct thermal_instance *tz_instance, *cd_instance;
	struct thermal_cooling_device *cdev;
	unsigned long target = 0;
	char data[3][25];
	char *envp[] = { data[0], data[1], data[2], NULL };
	unsigned long max_state;
	unsigned long cur_state;
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char key_buf[128];
	char dimensions_buf[128];
	const struct amazon_logger_ops *amazon_logger = amazon_logger_ops_get();
#endif
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!tz || !(tz->devdata)) {
		pr_err("%s tz:%p or tz->devdata is NULL!\n", __func__, tz);
		return -EINVAL;
	}
	tzone = tz->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	if (trip)
		return 0;

	mutex_lock(&tz->lock);
	list_for_each_entry(tz_instance, &tz->thermal_instances, tz_node) {
		if (tz_instance->trip != trip)
			continue;

		cdev = tz_instance->cdev;
		cdev->ops->get_max_state(cdev, &max_state);

		mutex_lock(&cdev->lock);
		list_for_each_entry(cd_instance, &cdev->thermal_instances,
				    cdev_node) {
			if (cd_instance->trip >= tz->trips)
				continue;

			if (trip == THERMAL_TRIPS_NONE)
				trip_temp = tz->forced_passive;
			else
				tz->ops->get_trip_temp(tz, cd_instance->trip,
						       &trip_temp);

			if (tz->temperature >= trip_temp)
				target = cd_instance->trip + 1;

			cd_instance->target = THERMAL_NO_TARGET;
		}

		target = (target > max_state) ? max_state : target;

		cdev->ops->get_cur_state(cdev, &cur_state);
		if (cur_state < target) {
			tz_instance->target = target;
			cdev->updated = false;
		} else if (cur_state > target) {
			if (trip == THERMAL_TRIPS_NONE)
				trip_temp = tz->forced_passive;
			else
				tz->ops->get_trip_temp(tz, cur_state - 1,
						       &trip_temp);

			if (tz->temperature <=
			    (trip_temp - pdata->trips[cur_state - 1].hyst)) {
				tz_instance->target = target;
				cdev->updated = false;
			} else {
				target = cur_state;
			}
		}
		mutex_unlock(&cdev->lock);

		if (cur_state != target) {
			thermal_cdev_update(cdev);

			if (target)
				tz->ops->get_trip_temp(tz, target - 1, &temp);
			else
				tz->ops->get_trip_temp(tz, target, &temp);

			if (target) {
				pr_err
				    ("VS cooler %s throttling, cur_temp=%d, trip_temp=%d, target=%lu\n",
				     cdev->type, tz->temperature, temp, target);
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
				snprintf(key_buf, 128, "vs_cooler_%s_throttling", cdev->type);
				snprintf(dimensions_buf, 128, "\"trip_temp\"#\"%d\"$\"target\"#\"%lu\"",
						temp, target);
				if (amazon_logger && amazon_logger->log_counter_to_vitals_v2) {
					amazon_logger->log_counter_to_vitals_v2(ANDROID_LOG_INFO,
						VITALS_THERMAL_GROUP_ID, VITALS_THERMAL_THROTTLE_SCHEMA_ID,
						"thermal", "thermal", "thermalthrottle",
						key_buf, tz->temperature, "temp",
						NULL, VITALS_NORMAL, dimensions_buf, NULL);
				}

#endif
			} else {
				pr_err
				    ("VS cooler %s unthrottling, cur_temp=%d, trip_temp=%d, target=%lu\n",
				     cdev->type, tz->temperature, temp, target);
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
				snprintf(key_buf, 128, "vs_cooler_%s_unthrottling", cdev->type);
				snprintf(dimensions_buf, 128, "\"trip_temp\"#\"%d\"$\"target\"#\"%lu\"",
						temp, target);
				amazon_logger->log_counter_to_vitals_v2(ANDROID_LOG_INFO,
					VITALS_THERMAL_GROUP_ID, VITALS_THERMAL_THROTTLE_SCHEMA_ID,
					"thermal", "thermal", "thermalthrottle",
					key_buf, tz->temperature, "temp",
					NULL, VITALS_NORMAL, dimensions_buf, NULL);
#endif
			}

			snprintf(data[0], sizeof(data[0]), "TRIP=%ld",
					target - 1);
			snprintf(data[1], sizeof(data[1]), "THERMAL_STATE=%ld",
				 target);
			/*
			 * PhoneWindowManagerMetricsHelper.java may need to filter out
			 * all but one cooling device type.
			 */
			snprintf(data[2], sizeof(data[2]), "CDEV_TYPE=%s",
				 cdev->type);
			kobject_uevent_env(&tz->device.kobj, KOBJ_CHANGE, envp);
		}
	}
	mutex_unlock(&tz->lock);
	return 0;
}

static struct thermal_governor thermal_gov_virtual_sensor = {
	.name = "virtual_sensor",
	.throttle = virtual_sensor_throttle,
};

static int __init thermal_gov_virtual_sensor_init(void)
{
	return thermal_register_governor(&thermal_gov_virtual_sensor);
}

static void __exit thermal_gov_virtual_sensor_exit(void)
{
	thermal_unregister_governor(&thermal_gov_virtual_sensor);
}

fs_initcall(thermal_gov_virtual_sensor_init);
module_exit(thermal_gov_virtual_sensor_exit);

MODULE_AUTHOR("Akwasi Boateng");
MODULE_DESCRIPTION("A simple level throttling thermal governor");
MODULE_LICENSE("GPL");
