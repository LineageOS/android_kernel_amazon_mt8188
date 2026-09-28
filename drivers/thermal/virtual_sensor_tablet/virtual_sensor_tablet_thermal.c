/*
 * Copyright 2022 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/list_sort.h>
#include "virtual_sensor_thermal.h"
#include <charger_class.h>
#include "thermal_core.h"
#include <linux/version.h>
#include <charger_class.h>

#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
#include <linux/amzn_sign_of_life.h>
#endif

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#include <linux/metricslog.h>
#define VIRTUAL_SENSOR_METRICS_STR_LEN 128
/*
 * Define the metrics log printing interval.
 * If virtual sensor throttles, the interval
 * is 0x3F*3 seconds(3 means polling interval of virtual_sensor).
 * If doesn't throttle, the interval is 0x3FF*3 seconds.
 */
#define VIRTUAL_SENSOR_THROTTLE_TIME_MASK 0x3F
#define VIRTUAL_SENSOR_UNTHROTTLE_TIME_MASK 0x3FF
static unsigned long virtual_sensor_temp = 25000;
#endif

#include <thermal_core.h>

#define DRIVER_NAME "virtual_sensor-thermal"
#define THERMAL_NAME "virtual_sensor"
#define COMPATIBLE_NAME "amazon,virtual_sensor-thermal"
#define BUF_SIZE 128
#define DMF 1000
#define MASK (0x0FFF)

static unsigned int virtual_sensor_nums;
static struct vs_thermal_platform_data *virtual_sensor_thermal_data;

#define PREFIX "thermalsensor:def"

static int match(struct thermal_zone_device *tz,
		 struct thermal_cooling_device *cdev)
{
	int i;
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;
	const struct thermal_zone_params *tzp;

	if (!tz || !(tz->devdata) || !(tz->tzp)) {
		pr_err("%s tz:%p maybe devdata or tzp is NULL!\n", __func__, tz);
		return -EINVAL;
	}
	tzp = tz->tzp;
	tzone = tz->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}
	if (strncmp(tz->type, "virtual_sensor", strlen("virtual_sensor")))
		return -EINVAL;

	for (i = 0; i < tzp->num_tbps; i++) {
		if (tzp->tbp[i].cdev) {
			if (!strcmp(tzp->tbp[i].cdev->type, cdev->type))
				return -EEXIST;
		}
	}

	for (i = 0; i < pdata->num_cdevs; i++) {
		pr_debug("pdata->cdevs[%d] %s cdev->type %s\n", i,
			 pdata->cdevs[i], cdev->type);
		if (strlen(cdev->type) == strlen(pdata->cdevs[i]))
			if (!strncmp(pdata->cdevs[i], cdev->type, strlen(cdev->type)))
				return 0;
	}

	return -EINVAL;
}

static int virtual_sensor_thermal_get_temp(struct thermal_zone_device *thermal,
					   int *t)
{
	struct thermal_dev *tdev;
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;
	long temp = 0;
	long tempv = 0;
	int alpha, offset, weight;
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char buf[VIRTUAL_SENSOR_METRICS_STR_LEN];
	const struct amazon_logger_ops *amazon_logger = amazon_logger_ops_get();
#endif

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	atomic_inc(&tzone->query_count);
#endif
	list_for_each_entry(tdev, &pdata->ts_list, node) {
		temp = tdev->dev_ops->get_temp(tdev);
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
		/*
		 * Log in metrics around every 1 hour normally
		 * and 3 mins wheny throttling
		 */
		if (!(atomic_read(&tzone->query_count) & tzone->mask)) {
			snprintf(buf, VIRTUAL_SENSOR_METRICS_STR_LEN, "%s_%s_temp", thermal->type, tdev->name);
			if (amazon_logger && amazon_logger->log_counter_to_vitals_v2) {
				amazon_logger->log_counter_to_vitals_v2(ANDROID_LOG_INFO,
					VITALS_THERMAL_GROUP_ID, VITALS_THERMAL_SENSOR_SCHEMA_ID,
					"thermal", "thermal", "thermalsensor",
					buf, temp, "temp", NULL, VITALS_NORMAL,
					NULL, NULL);
			}
		}
#endif

		alpha = tdev->tdp->alpha;
		offset = tdev->tdp->offset;
		weight = tdev->tdp->weight;
		if (!tdev->off_temp) {
			tdev->off_temp = temp - offset;
		} else {
			tdev->off_temp = alpha * (temp - offset) +
			    (DMF - alpha) * tdev->off_temp;
			tdev->off_temp /= DMF;
		}

		tempv += (weight * tdev->off_temp) / DMF;
	}

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	/*
	 * Log in metrics around every 1 hour normally
	 * and 3 mins wheny throttling
	 */
	if (!(atomic_read(&tzone->query_count) & tzone->mask)) {
		snprintf(buf, VIRTUAL_SENSOR_METRICS_STR_LEN, "%s_temp", thermal->type);
		if (amazon_logger && amazon_logger->log_counter_to_vitals_v2) {
			amazon_logger->log_counter_to_vitals_v2(ANDROID_LOG_INFO,
				VITALS_THERMAL_GROUP_ID, VITALS_THERMAL_SENSOR_SCHEMA_ID,
				"thermal", "thermal", "thermalsensor",
				buf, tempv, "temp", NULL, VITALS_NORMAL,
				NULL, NULL);
		}
	}

	if (tempv > pdata->trips[0].temp)
		tzone->mask = VIRTUAL_SENSOR_THROTTLE_TIME_MASK;
	else
		tzone->mask = VIRTUAL_SENSOR_UNTHROTTLE_TIME_MASK;
#endif

	*t = (unsigned long)tempv;
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	virtual_sensor_temp = (unsigned long)tempv;
#endif

	return 0;
}

static int virtual_sensor_thermal_change_mode(struct thermal_zone_device *thermal,
					   enum thermal_device_mode mode)
{
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	pdata->mode = mode;
	if (mode == THERMAL_DEVICE_DISABLED) {
		tzone->tz->polling_delay = 0;
		return 0;
	}

	return 0;
}

static int virtual_sensor_thermal_get_trip_type(struct thermal_zone_device
						*thermal, int trip,
						enum thermal_trip_type *type)
{
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	*type = pdata->trips[trip].type;
	return 0;
}

static int virtual_sensor_thermal_get_trip_temp(struct thermal_zone_device
						*thermal, int trip, int *temp)
{
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	*temp = pdata->trips[trip].temp;
	return 0;
}

static int virtual_sensor_thermal_set_trip_temp(struct thermal_zone_device
						*thermal, int trip, int temp)
{
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	pdata->trips[trip].temp = temp;
	return 0;
}

static int virtual_sensor_thermal_get_crit_temp(struct thermal_zone_device
						*thermal, int *temp)
{
	int i;
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
		if (pdata->trips[i].type == THERMAL_TRIP_CRITICAL) {
			*temp = pdata->trips[i].temp;
			return 0;
		}
	}
	return -EINVAL;
}

static int virtual_sensor_thermal_get_trip_hyst(struct thermal_zone_device
						*thermal, int trip, int *hyst)
{
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	*hyst = pdata->trips[trip].hyst;
	return 0;
}

static int virtual_sensor_thermal_set_trip_hyst(struct thermal_zone_device
						*thermal, int trip, int hyst)
{
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	pdata->trips[trip].hyst = hyst;
	return 0;
}

#ifdef CONFIG_THERMAL_SHUTDOWN_LAST_KMESG
static void last_kmsg_thermal_shutdown(const char* msg)
{
	int rc = 0;
	char *argv[] = {
		"/system_ext/bin/crashreport",
		"thermal_shutdown",
		NULL
	};

	pr_err("%s:%s start to save last kmsg\n", __func__, msg);
	/* UMH_WAIT_PROC UMH_WAIT_EXEC */
	rc = call_usermodehelper(argv[0], argv, NULL, UMH_WAIT_EXEC);
	pr_err("%s: save last kmsg finish\n", __func__);

	if (rc < 0)
		pr_err("call crashreport failed, rc = %d\n", rc);
	else
		msleep(6000);	/* 6000ms */
}
#else
static inline void last_kmsg_thermal_shutdown(const char* msg) {}
#endif

static int virtual_sensor_thermal_notify(struct thermal_zone_device *thermal,
					 int trip, enum thermal_trip_type type)
{
	char data[20];
	char *envp[] = { data, NULL };
	int trip_temp = 0;

	if (!thermal) {
		pr_err("%s thermal:%p is NULL!\n", __func__, thermal);
		return -EINVAL;
	}

	snprintf(data, sizeof(data), "%s", "SHUTDOWN_WARNING");
	kobject_uevent_env(&thermal->device.kobj, KOBJ_CHANGE, envp);

	if (type == THERMAL_TRIP_CRITICAL) {
		if (thermal->ops->get_trip_temp)
			thermal->ops->get_trip_temp(thermal, trip, &trip_temp);

		pr_err("[%s][%s]type:[%s] Thermal shutdown, "
			"current temp=%d, trip=%d, trip_temp=%d\n",
			__func__, dev_name(&thermal->device), thermal->type,
			thermal->temperature, trip, trip_temp);

#if IS_ENABLED(CONFIG_AMAZON_SIGN_OF_LIFE)
		life_cycle_set_thermal_shutdown_reason
			(THERMAL_SHUTDOWN_REASON_PCB);
#endif
		last_kmsg_thermal_shutdown(dev_name(&thermal->device));
		set_shutdown_enable_dcap();
	}

	return 0;
}

static struct thermal_zone_device_ops virtual_sensor_tz_dev_ops = {
	.get_temp = virtual_sensor_thermal_get_temp,
	.change_mode = virtual_sensor_thermal_change_mode,
	.get_trip_type = virtual_sensor_thermal_get_trip_type,
	.get_trip_temp = virtual_sensor_thermal_get_trip_temp,
	.set_trip_temp = virtual_sensor_thermal_set_trip_temp,
	.get_crit_temp = virtual_sensor_thermal_get_crit_temp,
	.get_trip_hyst = virtual_sensor_thermal_get_trip_hyst,
	.set_trip_hyst = virtual_sensor_thermal_set_trip_hyst,
	.notify = virtual_sensor_thermal_notify,
};

static ssize_t params_show(struct device *dev,
			   struct device_attribute *devattr, char *buf)
{
	int o = 0;
	int a = 0;
	int w = 0;
	char pbufo[BUF_SIZE];
	char pbufa[BUF_SIZE];
	char pbufw[BUF_SIZE];
	int alpha, offset, weight;
	struct thermal_dev *tdev;
	struct thermal_zone_device *thermal =
	    container_of(dev, struct thermal_zone_device, device);
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	o += scnprintf(pbufo + o, BUF_SIZE - o, "offsets ");
	a += scnprintf(pbufa + a, BUF_SIZE - a, "alphas ");
	w += scnprintf(pbufw + w, BUF_SIZE - w, "weights ");

	list_for_each_entry(tdev, &pdata->ts_list, node) {
		alpha = tdev->tdp->alpha;
		offset = tdev->tdp->offset;
		weight = tdev->tdp->weight;

		o += scnprintf(pbufo + o, BUF_SIZE - o, "%d ", offset);
		a += scnprintf(pbufa + a, BUF_SIZE - a, "%d ", alpha);
		w += scnprintf(pbufw + w, BUF_SIZE - w, "%d ", weight);
	}
	return scnprintf(buf, 3 * BUF_SIZE + 6, "%s\n%s\n%s\n", pbufo, pbufa, pbufw);
}

static ssize_t trips_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct thermal_zone_device *thermal =
	    container_of(dev, struct thermal_zone_device, device);

	if (!thermal) {
		pr_err("%s thermal:%p is NULL!\n", __func__, thermal);
		return -EINVAL;
	}

	return scnprintf(buf, BUF_SIZE, "%d\n", thermal->trips);
}

static ssize_t trips_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int trips = 0;
	struct thermal_zone_device *thermal =
	    container_of(dev, struct thermal_zone_device, device);
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	if (sscanf(buf, "%d\n", &trips) != 1)
		return -EINVAL;
	if (trips < 0)
		return -EINVAL;

	pdata->num_trips = trips;
	thermal->trips = pdata->num_trips;
	return count;
}

static ssize_t polling_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct thermal_zone_device *thermal =
	    container_of(dev, struct thermal_zone_device, device);

	if (!thermal) {
		pr_err("%s thermal:%p is NULL!\n", __func__, thermal);
		return -EINVAL;
	}

	return scnprintf(buf, BUF_SIZE, "%d\n", thermal->polling_delay);
}

static ssize_t polling_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int polling_delay = 0;
	struct thermal_zone_device *thermal =
	    container_of(dev, struct thermal_zone_device, device);
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata;

	if (!thermal || !(thermal->devdata)) {
		pr_err("%s thermal:%p or thermal->devdata is NULL!\n", __func__, thermal);
		return -EINVAL;
	}
	tzone = thermal->devdata;
	pdata = tzone->pdata;
	if (!pdata) {
		pr_err("%s pdata is NULL!\n", __func__);
		return -EINVAL;
	}

	if (sscanf(buf, "%d\n", &polling_delay) != 1)
		return -EINVAL;
	if (polling_delay < 0)
		return -EINVAL;

	pdata->polling_delay = polling_delay;
	thermal->polling_delay = pdata->polling_delay;
	thermal_zone_device_update(thermal, THERMAL_EVENT_UNSPECIFIED);
	return count;
}

static DEVICE_ATTR(trips, 0644, trips_show, trips_store);
static DEVICE_ATTR(polling, 0644, polling_show, polling_store);
static DEVICE_ATTR(params, 0444, params_show, NULL);

static int virtual_sensor_create_sysfs(struct virtual_sensor_thermal_zone
				       *tzone)
{
	int ret = 0;

	ret = device_create_file(&tzone->tz->device, &dev_attr_params);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);
	ret = device_create_file(&tzone->tz->device, &dev_attr_polling);
	if (ret)
		pr_err("%s Failed to create polling attr\n", __func__);
	ret = device_create_file(&tzone->tz->device, &dev_attr_trips);
	if (ret)
		pr_err("%s Failed to create trips attr\n", __func__);
	return ret;
}

static void virtual_sensor_thermal_parse_node_int_array(const struct device_node
							*np,
							const char *node_name,
							unsigned long
							*tripsdata)
{
	u32 array[THERMAL_MAX_TRIPS] = { 0 };
	int i = 0;

	if (of_property_read_u32_array(np, node_name, array, ARRAY_SIZE(array))
	    == 0) {
		for (i = 0; i < ARRAY_SIZE(array); i++) {
			tripsdata[i] = (unsigned long)array[i];
			pr_debug("Get %s %ld\n", node_name, tripsdata[i]);
		}
	} else
		pr_notice("Get %s failed\n", node_name);
}

static void virtual_sensor_thermal_tripsdata_convert(char *type,
						     unsigned long *tripsdata,
						     struct trip_t *trips)
{
	int i = 0;

	if (strncmp(type, "type", 4) == 0) {
		for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
			switch (tripsdata[i]) {
			case 0:
				trips[i].type = THERMAL_TRIP_ACTIVE;
				break;
			case 1:
				trips[i].type = THERMAL_TRIP_PASSIVE;
				break;
			case 2:
				trips[i].type = THERMAL_TRIP_HOT;
				break;
			case 3:
				trips[i].type = THERMAL_TRIP_CRITICAL;
				break;
			default:
				pr_notice("unknown type!\n");
			}
			pr_debug("tripsdata[%d]=%ld\n", i, tripsdata[i]);
		}
	} else if (strncmp(type, "temp", 4) == 0) {
		for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
			trips[i].temp = tripsdata[i];
			pr_debug("tripsdata[%d]=%ld\n", i, tripsdata[i]);
		}
	} else {
		for (i = 0; i < THERMAL_MAX_TRIPS; i++) {
			trips[i].hyst = tripsdata[i];
			pr_debug("tripsdata[%d]=%ld\n", i, tripsdata[i]);
		}
	}
}

static void virtual_sensor_thermal_init_trips(const struct device_node *np,
					      struct trip_t *trips)
{
	unsigned long tripsdata[THERMAL_MAX_TRIPS] = { 0 };

	virtual_sensor_thermal_parse_node_int_array(np, "temp", tripsdata);
	virtual_sensor_thermal_tripsdata_convert("temp", tripsdata, trips);
	virtual_sensor_thermal_parse_node_int_array(np, "type", tripsdata);
	virtual_sensor_thermal_tripsdata_convert("type", tripsdata, trips);
	virtual_sensor_thermal_parse_node_int_array(np, "hyst", tripsdata);
	virtual_sensor_thermal_tripsdata_convert("hyst", tripsdata, trips);
}

static int virtual_sensor_thermal_init_tbp(struct thermal_zone_params *tzp,
					   struct platform_device *pdev)
{
	int i;
	struct thermal_bind_params *tbp;

	tbp = devm_kzalloc(&pdev->dev,
			 sizeof(struct thermal_bind_params) * tzp->num_tbps,
			 GFP_KERNEL);
	if (!tbp)
		return -ENOMEM;

	for (i = 0; i < tzp->num_tbps; i++) {
		tbp[i].cdev = NULL;
		tbp[i].weight = 0;
		tbp[i].trip_mask = MASK;
		tbp[i].match = match;
	}
	tzp->tbp = tbp;

	return 0;
}

static int virtual_sensor_thermal_init_cust_data_from_dt(struct platform_device
							 *dev)
{
	int ret = 0;
	struct device_node *np = dev->dev.of_node;
	struct vs_thermal_platform_data *p_virtual_sensor_thermal_data;
	int i = 0;

	dev->id = 0;
	thermal_parse_node_int(np, "dev_id", &dev->id);
	if (dev->id < virtual_sensor_nums)
		p_virtual_sensor_thermal_data =
		    &virtual_sensor_thermal_data[dev->id];
	else {
		ret = -1;
		pr_err("dev->id invalid!\n");
		return ret;
	}

	thermal_parse_node_int(np, "num_trips",
			       &p_virtual_sensor_thermal_data->num_trips);
	thermal_parse_node_int(np, "mode",
			       (int *)&p_virtual_sensor_thermal_data->mode);
	thermal_parse_node_int(np, "polling_delay",
			       &p_virtual_sensor_thermal_data->polling_delay);
	virtual_sensor_thermal_parse_node_string_index(np, "governor_name", 0,
						       p_virtual_sensor_thermal_data->tzp.governor_name);
	thermal_parse_node_int(np, "num_tbps",
			       &p_virtual_sensor_thermal_data->tzp.num_tbps);
	virtual_sensor_thermal_init_trips(np,
					  p_virtual_sensor_thermal_data->trips);

	thermal_parse_node_int(np, "num_cdevs",
			       &p_virtual_sensor_thermal_data->num_cdevs);
	if (p_virtual_sensor_thermal_data->num_cdevs > THERMAL_MAX_TRIPS)
		p_virtual_sensor_thermal_data->num_cdevs = THERMAL_MAX_TRIPS;

	while (i < p_virtual_sensor_thermal_data->num_cdevs) {
		virtual_sensor_thermal_parse_node_string_index(np, "cdev_names",
							       i,
							       p_virtual_sensor_thermal_data->cdevs[i]);
		i++;
	}

	ret = virtual_sensor_thermal_init_tbp(&p_virtual_sensor_thermal_data->tzp,dev);

	return ret;
}

#ifdef DEBUG
static void test_data(void)
{
	int i = 0;
	int j = 0;
	int h = 0;

	for (i = 0; i < virtual_sensor_nums; i++) {
		pr_debug("num_trips %d\n",
			 virtual_sensor_thermal_data[i].num_trips);
		pr_debug("mode %d\n", virtual_sensor_thermal_data[i].mode);
		pr_debug("polling_delay %d\n",
			 virtual_sensor_thermal_data[i].polling_delay);
		pr_debug("governor_name %s\n",
			 virtual_sensor_thermal_data[i].tzp.governor_name);
		pr_debug("num_tbps %d\n",
			 virtual_sensor_thermal_data[i].tzp.num_tbps);
		while (j < THERMAL_MAX_TRIPS) {
			pr_debug("trips[%d] %d\n", j,
				 virtual_sensor_thermal_data[i].trips[j].temp);
			pr_debug("type[%d] %d\n", j,
				 virtual_sensor_thermal_data[i].trips[j].type);
			pr_debug("hyst[%d] %d\n", j,
				 virtual_sensor_thermal_data[i].trips[j].hyst);
			j++;
		}
		j = 0;
		pr_debug("num_cdevs %d\n",
			 virtual_sensor_thermal_data[i].num_cdevs);

		while (h < virtual_sensor_thermal_data[i].num_cdevs) {
			pr_debug("cdevs[%d] %s\n", h,
				 virtual_sensor_thermal_data[i].cdevs[h]);
			h++;
		}
		h = 0;
	}
}
#endif

void print_virtual_thermal_zone_dts(
	const struct virtual_sensor_thermal_zone *tzone)
{
	int i;
        char* cdevs;

	pr_info("Print info: virtual thermal dts.");
	for (i = 0; i < virtual_sensor_nums; i++) {
		int j = 0;
		int h = 0;
		int offset1 = 0;
		int offset2 = 0;
		int offset3 = 0;
		char trips[THERMAL_NAME_LENGTH * THERMAL_MAX_TRIPS];
		char hyst[THERMAL_NAME_LENGTH * THERMAL_MAX_TRIPS];
		int cdevs_name_length = THERMAL_NAME_LENGTH * virtual_sensor_thermal_data[i].num_cdevs;

		cdevs = kzalloc(cdevs_name_length * sizeof(char), GFP_KERNEL);
		if (!cdevs)
			return ;

		pr_info("virtual_sensor%d", i + 1);
		pr_info("governor_name %s",
			virtual_sensor_thermal_data[i].tzp.governor_name);
		while (j < virtual_sensor_thermal_data[i].num_cdevs) {
			offset1 += scnprintf(cdevs + offset1,
				(THERMAL_NAME_LENGTH *
				virtual_sensor_thermal_data[i].num_cdevs)
				- offset1, "%s ",
				virtual_sensor_thermal_data[i].cdevs[j]);
			j++;
		}
		while (h < THERMAL_MAX_TRIPS) {
			offset2 += scnprintf(trips + offset2,
				(THERMAL_NAME_LENGTH * THERMAL_MAX_TRIPS)
				- offset2, "%d ",
				virtual_sensor_thermal_data[i].trips[h].temp);
			offset3 += scnprintf(hyst + offset3,
				(THERMAL_NAME_LENGTH * THERMAL_MAX_TRIPS)
				- offset3, "%d ",
				virtual_sensor_thermal_data[i].trips[h].hyst);
			h++;
		}
		pr_info("cdevs %s\n", cdevs);
		pr_info("trips %s\n", trips);
		pr_info("hyst %s\n", hyst);
		pr_info("polling_delay %d\n",
				virtual_sensor_thermal_data[i].polling_delay);
		kfree(cdevs);
	}
}

static int virtual_sensor_thermal_probe(struct platform_device *pdev)
{
	int ret;
	struct virtual_sensor_thermal_zone *tzone;
	struct vs_thermal_platform_data *pdata = NULL;
	char thermal_name[THERMAL_NAME_LENGTH];

#ifdef CONFIG_OF
	pr_notice("virtual_sensor thermal custom init by DTS!\n");
	ret = virtual_sensor_thermal_init_cust_data_from_dt(pdev);
	if (ret) {
		pr_err("%s: init data error\n", __func__);
		return ret;
	}
#endif
	if (pdev->id < virtual_sensor_nums)
		pdata = &virtual_sensor_thermal_data[pdev->id];
	else
		pr_err("pdev->id is invalid!\n");

	if (!pdata)
		return -EINVAL;
#ifdef DEBUG
	pr_notice("%s %d test data\n", __func__, __LINE__);
	test_data();
#endif
	tzone = devm_kzalloc(&pdev->dev, sizeof(*tzone), GFP_KERNEL);
	if (!tzone)
		return -ENOMEM;
	memset(tzone, 0, sizeof(*tzone));

	tzone->pdata = pdata;
	snprintf(thermal_name, sizeof(thermal_name), "%s%d", THERMAL_NAME,
		 pdev->id + 1);

	tzone->tz = thermal_zone_device_register(thermal_name,
						 THERMAL_MAX_TRIPS,
						 MASK,
						 tzone,
						 &virtual_sensor_tz_dev_ops,
						 &pdata->tzp,
						 0, pdata->polling_delay);
	if (IS_ERR(tzone->tz)) {
		pr_err("%s Failed to register thermal zone device\n", __func__);
		kfree(tzone);
		return -EINVAL;
	}
	tzone->tz->trips = pdata->num_trips;

#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	tzone->mask = VIRTUAL_SENSOR_UNTHROTTLE_TIME_MASK;
#endif
	ret = virtual_sensor_create_sysfs(tzone);
	platform_set_drvdata(pdev, tzone);
	pdata->mode = THERMAL_DEVICE_ENABLED;
	if (pdev->id == virtual_sensor_nums - 1)
		print_virtual_thermal_zone_dts(tzone);
	kobject_uevent(&tzone->tz->device.kobj, KOBJ_ADD);
	return ret;
}

static int virtual_sensor_thermal_remove(struct platform_device *pdev)
{
	struct virtual_sensor_thermal_zone *tzone = platform_get_drvdata(pdev);

	if (tzone) {
		if (tzone->tz)
			thermal_zone_device_unregister(tzone->tz);
		kfree(tzone);
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id virtual_sensor_thermal_of_match[] = {
	{.compatible = COMPATIBLE_NAME,},
	{},
};

MODULE_DEVICE_TABLE(of, virtual_sensor_thermal_of_match);
#endif

static struct platform_driver virtual_sensor_thermal_zone_driver = {
	.probe = virtual_sensor_thermal_probe,
	.remove = virtual_sensor_thermal_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = virtual_sensor_thermal_of_match,
#endif
		   },
};

static int __init virtual_sensor_thermal_init(void)
{
	int err = 0;
	struct vs_zone_dts_data *vs_thermal_data;

	vs_thermal_data = get_vs_thermal_data();

	if (!vs_thermal_data || !vs_thermal_data->vs_pdata) {
		pr_err("%s: init_vs_thermal_platform_data error\n", __func__);
		return -1;
	}
	virtual_sensor_nums = vs_thermal_data->vs_zone_dts_nums;
	virtual_sensor_thermal_data = vs_thermal_data->vs_pdata;

	err = platform_driver_register(&virtual_sensor_thermal_zone_driver);
	if (err) {
		pr_err("%s: Failed to register driver %s\n", __func__,
		       virtual_sensor_thermal_zone_driver.driver.name);
		return err;
	}

	return err;
}

static void __exit virtual_sensor_thermal_exit(void)
{
	platform_driver_unregister(&virtual_sensor_thermal_zone_driver);
}

late_initcall(virtual_sensor_thermal_init);
module_exit(virtual_sensor_thermal_exit);

MODULE_DESCRIPTION("VIRTUAL_SENSOR pcb virtual sensor thermal zone driver");
MODULE_AUTHOR("Akwasi Boateng <boatenga@amazon.com>");
MODULE_LICENSE("GPL");
