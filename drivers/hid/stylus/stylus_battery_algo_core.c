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
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
static stylus_metrics_info_t stylus_metrics_data;
struct delayed_work metrics_work;
static int last_mode;
#endif

/* stylus device number */
static dev_t stylus_dev_num;
static struct device *stylus_dev;
static struct class *stylus_class;

/* stylus data */
static stylus_data_t stylus_data;

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
static void metrics_stylus_battery_algo_work(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);

	stylus_metrics_print_func(stylus_metrics_data);
	stylus_metrics_data.bat_hw_jitter_cnt = 0;
	stylus_metrics_data.bat_sw_Jitter_cnt = 0;
	stylus_metrics_data.low_bat_cnt = 0;
	stylus_metrics_data.bat_level_mode = 0;

	schedule_delayed_work(dw, TIME_24_HOURS);
}
#endif

static int initStylusData(void)
{
	if (!(stylus_data.circular_buffer) || !(stylus_data.uevent_info)) {
		STYLUS_ERR("init buffer failed\n");
		return -EINVAL;
	}
	memset(stylus_data.circular_buffer, 0, sizeof(stylus_circular_buffer_t));
	memset(stylus_data.uevent_info, 0, sizeof(stylus_uevent_info_t));
	stylus_data.circular_buffer->front = -1;
	stylus_data.circular_buffer->rear = -1;
	stylus_data.battery_capacity = 0;
	stylus_data.battery_capacity_filter = 0;
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	last_mode = 0;
#endif

	return 0;
}

/* Function to insert a value into the circular buffer */
static void buffer_insert_tail(stylus_packet_info_t value)
{
	stylus_battery_cap_info_t stylus_battery_cap_info = { 0 };

	if (stylus_data.circular_buffer) {
		if (stylus_data.circular_buffer->rear >= 0
			&& value.time <= stylus_data.circular_buffer->values[stylus_data.circular_buffer->rear].time)
			STYLUS_WARN("The current acquisition timestamp is earlier than the previous one\n");

		stylus_battery_cap_info.capacity = value.capacity;
		stylus_battery_cap_info.time = value.time;
		stylus_data.circular_buffer->rear = (stylus_data.circular_buffer->rear + 1) % BUFFER_SIZE;
		stylus_data.circular_buffer->values[stylus_data.circular_buffer->rear] = stylus_battery_cap_info;
	}
}

/* Function to calculate the mode value (most frequently used number) from the circular buffer */
static int calculateMode(void)
{
	int i = 0, j = 0, temp = 0;
	int battery_reports_count[MAX_BATTERY_LEVEL + 1] = { 0 }; /* there are only MAX_BATTERY_LEVEL possible level because stylus quantization */
	int validvote = 0;
	s64 latestTime = 0;
	s64 nodeTime = 0;
	int index = 0;
	int maxfeq = 0;
#ifdef BATTERY_ALGO_V2
	int minCap = MAX_THEORETICAL_BATTERY;
	int maxCap = MIN_THEORETICAL_BATTERY;
#endif

	if (!(stylus_data.circular_buffer)) {
		STYLUS_ERR("stylus_data.circular_buffer is NULL\n");
		return -EINVAL;
	}

	/* Loop from the rear of FIFO, find all nodes are within last MAX_TIME_DIFF seconds */
	latestTime = stylus_data.circular_buffer->values[stylus_data.circular_buffer->rear].time;
	for (i = 0; i < BUFFER_SIZE; i++) {
		index = stylus_data.circular_buffer->rear - i;
		if (index < 0)
			index += BUFFER_SIZE;

		nodeTime = stylus_data.circular_buffer->values[index].time;
		if ((nodeTime == 0) || (latestTime - nodeTime > MAX_TIME_DIFF) || (latestTime - nodeTime < 0))
			break;
		temp = stylus_data.circular_buffer->values[index].capacity;
#ifdef BATTERY_ALGO_V2
		if (temp < minCap)
			minCap = temp;
		if (temp > maxCap)
			maxCap = temp;
#endif
		battery_reports_count[temp]++;
		validvote++;
	}

	/* skip if there are no enough valid nodes */
	if (validvote < MIN_SAMPLE_DATA_COUNT) {
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
		last_mode = 0;
#endif
		return NEED_MORE_SAMPLE;
	}
#ifdef BATTERY_ALGO_V2
	/*
	 * return -2 which is an invalid battery level if there max
	 * and min is larger than normal +/-4, meaning there is osicllation
	 */
	if (maxCap - minCap > BATTERY_JITTER_THRESHOLD)
		return ABNORMAL_JITTER_GAP;
#endif

	/* find the Mode, (most frequency number) */
	index = 0;
	maxfeq = battery_reports_count[0];
	for (j = 1; j <= MAX_BATTERY_LEVEL; j++) {
		if (battery_reports_count[j] > maxfeq) {
			maxfeq = battery_reports_count[j];
			index = j;
		}
	}

	STYLUS_DEBUG("%s, bat %2d, mode %2d\n", __FUNCTION__, index, battery_reports_count[index]);
    return index;
}

static int stylus_battery_capacity_jitter_filter(
				     int stylus_battery_capacity,
				     bool *stylus_battery_capacity_changed)
{
	if (stylus_battery_capacity_changed == NULL) {
		STYLUS_ERR("stylus_battery_capacity_changed is NULL\n");
		return -EINVAL;
	}

	/* algorithm for eliminating power jitter */
	if (stylus_battery_capacity > stylus_data.battery_capacity &&
	    stylus_battery_capacity < stylus_data.battery_capacity_filter) {
		stylus_battery_capacity = stylus_data.battery_capacity;
	} else if (stylus_battery_capacity != stylus_data.battery_capacity) {
		if (printk_ratelimit())
			STYLUS_INFO("jitter_filter: pen=%2x time=%2lld bat=%2d prev_bat=%2d.\n",
					stylus_data.packet_info.pen_id,
					stylus_data.circular_buffer->values[stylus_data.circular_buffer->rear].time,
					stylus_battery_capacity,
					stylus_data.battery_capacity);
		stylus_data.battery_capacity = stylus_battery_capacity;
		stylus_data.battery_capacity_filter =
			STYLUS_BATTERY_CAPACITY_JITTER_BUF(
			stylus_data.battery_capacity);
		*stylus_battery_capacity_changed = true;
	}

	return stylus_battery_capacity;
}

static int stylus_battery_capacity_algo(int capacity, bool *stylus_battery_capacity_changed)
{
	int stylus_battery_algo = -1, new_stylus_battery_capacity = -1;
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	static int last_reported_cap;
#endif

	if (!(stylus_data.circular_buffer) || !stylus_battery_capacity_changed) {
		STYLUS_ERR("stylus_data.circular_buffer or  is stylus_battery_capacity_changed is NULL\n");
		return -EINVAL;
	}

	/* support a debug interface with a fixed battery level*/
	if (battery_level_debug > 0)
		stylus_data.packet_info.capacity = battery_level_debug;

	/*  Insert the node into the buffer */
	buffer_insert_tail(stylus_data.packet_info);

	stylus_battery_algo = calculateMode();
	if (stylus_battery_algo > 0) {
		new_stylus_battery_capacity = stylus_battery_capacity_jitter_filter(
				stylus_battery_algo, stylus_battery_capacity_changed);
		if (*stylus_battery_capacity_changed) {
			if (printk_ratelimit())
				STYLUS_INFO("battery changed: pen %2x time %2lld bat %2d mode %2d out %2d\n",
					stylus_data.packet_info.pen_id, stylus_data.packet_info.time,
					stylus_data.packet_info.capacity, stylus_battery_algo,
					new_stylus_battery_capacity);
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
			/* determine if there is sw jitter in the stylus battery value */
			if (last_reported_cap &&
			    abs(new_stylus_battery_capacity - last_reported_cap) >
			    STYLUS_BATTERY_JITTER_THRESHOLD)
				stylus_metrics_data.bat_sw_Jitter_cnt = 1;
			last_reported_cap = new_stylus_battery_capacity;
#endif
		}
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
		if (last_mode && last_mode != stylus_battery_algo) {
			if (new_stylus_battery_capacity <= LOW_BATTERY_LEVEL)
				stylus_metrics_data.low_bat_cnt = 1;
			stylus_metrics_print_func(stylus_metrics_data);
			stylus_metrics_data.bat_hw_jitter_cnt = 0;
			stylus_metrics_data.bat_sw_Jitter_cnt = 0;
			stylus_metrics_data.low_bat_cnt = 0;
		} else { // it says it is the first time the battery been updated
			stylus_metrics_data.bat_level_mode = stylus_battery_algo;
		}
		last_mode = stylus_battery_algo;
#endif
	}

	STYLUS_DEBUG("%s pen %2x time %2lld bat %2d mode %2d out %2d\n",
			__FUNCTION__,
			stylus_data.packet_info.pen_id, stylus_data.packet_info.time,
			stylus_data.packet_info.capacity, stylus_battery_algo,
			new_stylus_battery_capacity);

	return new_stylus_battery_capacity;
}

static int stylus_report_uevent(struct device *dev,
				       struct kobj_uevent_env *env)
{
	int ret = 0;

	if (!(stylus_data.uevent_info)) {
		STYLUS_ERR("stylus_data.uevent_info is NULL\n");
		return -EINVAL;
	}

	ret = add_uevent_var(env, "STYLUS_LEVEL=%s",
			     stylus_data.uevent_info->stylus_capacity);
	ret = add_uevent_var(env, "STYLUS_VENDOR=%s",
			     stylus_data.uevent_info->stylus_vendor);
	ret = add_uevent_var(env, "STYLUS_VENDOR_ID=%s",
			     stylus_data.uevent_info->stylus_vendor_id);
	ret = add_uevent_var(env, "STYLUS_STATUS=%s",
			     stylus_data.uevent_info->stylus_status);
	ret = add_uevent_var(env, "STYLUS_SERIAL=%s",
			     stylus_data.uevent_info->stylus_serial);
	ret = add_uevent_var(env, "STYLUS_FW_VERSION=%s",
			     stylus_data.uevent_info->stylus_fw_version);

	return ret;
}

int stylus_info_process(void)
{
	int ret = 0;
	int new_stylus_battery_capacity = 0;
	bool stylus_battery_capacity_changed = false;
	static uint32_t prev_pen_id = INVALID_PEN_ID;
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	static int last_stylus_capacity;
#endif

	if (!stylus_dev || !(stylus_data.circular_buffer) || !(stylus_data.uevent_info)) {
		STYLUS_ERR("stylus device or stylus data is NULL\n");
		return -EINVAL;
	}

	/* only handles 1p pens */
	if (stylus_data.packet_info.vendor_id != STYLUS_VENDOR_USAGE_1P) {
		if (printk_ratelimit())
			STYLUS_ERR("%s unknown stylus vendor_id %x.\n", __FUNCTION__, stylus_data.packet_info.vendor_id);
		memset(stylus_data.uevent_info, 0, sizeof(stylus_uevent_info_t));
		return -EINVAL;
	}

	/* stylus battery capacity filter outliers */
	if ((stylus_data.packet_info.capacity <  MIN_BATTERY_LEVEL)
		|| (stylus_data.packet_info.capacity > MAX_BATTERY_LEVEL)) {
		STYLUS_ERR("%s Invalid battery level %d\n", __FUNCTION__, stylus_data.packet_info.capacity);
		return -EINVAL;
	}

	/* new pen, init stylus data*/
	if (prev_pen_id != stylus_data.packet_info.pen_id) {
		STYLUS_INFO("%s new pen %x, prev_pen %x.\n", __FUNCTION__, stylus_data.packet_info.pen_id, prev_pen_id);
		prev_pen_id = stylus_data.packet_info.pen_id;
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
		stylus_metrics_data.stylus_id = prev_pen_id;
		stylus_metrics_data.fw_version = stylus_data.packet_info.fw_version;
		last_stylus_capacity = 0;
#endif
		ret = initStylusData();
		if (ret < 0) {
			STYLUS_ERR("%s, new pen, initStylusData failed.\n", __FUNCTION__);
			return -EINVAL;
		}
	}

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	/* determine if there is hw jitter in the stylus battery value */
	if (last_stylus_capacity &&
	    abs(stylus_data.packet_info.capacity - last_stylus_capacity) >
	    STYLUS_BATTERY_JITTER_THRESHOLD)
		stylus_metrics_data.bat_hw_jitter_cnt++;
	last_stylus_capacity = stylus_data.packet_info.capacity;
#endif

	new_stylus_battery_capacity =  stylus_battery_capacity_algo(stylus_data.packet_info.capacity,
			&stylus_battery_capacity_changed);
	if (new_stylus_battery_capacity <= 0)
		return 0;

	if (stylus_battery_capacity_changed) {
		/* set uevent env value */
		snprintf(stylus_data.uevent_info->stylus_vendor, ENV_SIZE, "%s", "amzn");
		snprintf(stylus_data.uevent_info->stylus_vendor_id, ENV_SIZE, "0x%x",
			stylus_data.packet_info.vendor_id);
		snprintf(stylus_data.uevent_info->stylus_capacity, ENV_SIZE, "%d",
			new_stylus_battery_capacity);
		snprintf(stylus_data.uevent_info->stylus_serial, ENV_SIZE, "%8X",
			stylus_data.packet_info.pen_id);
		snprintf(stylus_data.uevent_info->stylus_fw_version,
			ENV_SIZE, "0x%x", stylus_data.packet_info.fw_version);
		if ((new_stylus_battery_capacity <= 40) &&
			(new_stylus_battery_capacity > 0))
			snprintf(stylus_data.uevent_info->stylus_status, ENV_SIZE, "%s",
				"LOW ");
		else
			snprintf(stylus_data.uevent_info->stylus_status, ENV_SIZE, "%s",
				"NORMAL ");

		kobject_uevent(&stylus_dev->kobj, KOBJ_CHANGE);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(stylus_info_process);

void stylus_info_packet(stylus_packet_info_t packet_info)
{
	stylus_data.packet_info = packet_info;
}
EXPORT_SYMBOL_GPL(stylus_info_packet);

static int stylus_dev_set_drvdata(void)
{
	int ret = 0;

	stylus_data.circular_buffer = vmalloc(sizeof(stylus_circular_buffer_t));
	if (!stylus_data.circular_buffer) {
		STYLUS_ERR("Failed to allocate stylus buffer\n");
		return -ENOMEM;
	}

	stylus_data.uevent_info = vmalloc(sizeof(stylus_uevent_info_t));
	if (!stylus_data.uevent_info) {
		STYLUS_ERR("Failed to allocate stylus_data.uevent_info\n");
		ret = -ENOMEM;
		goto stylus_uevent_alloc_failed;
	}

	ret = initStylusData();
	if (ret) {
		STYLUS_ERR("initStylusData failed.\n");
		goto stylus_data_init_failed;
	}

	return 0;
stylus_data_init_failed:
	kfree(stylus_data.uevent_info);
	stylus_data.uevent_info = NULL;
stylus_uevent_alloc_failed:
	kfree(stylus_data.circular_buffer);
	stylus_data.circular_buffer = NULL;
	return ret;
}

static void stylus_dev_remove_drvdata(void)
{
	if (stylus_data.circular_buffer) {
		vfree(stylus_data.circular_buffer);
		stylus_data.circular_buffer = NULL;
	}
	if (stylus_data.uevent_info) {
		vfree(stylus_data.uevent_info);
		stylus_data.uevent_info = NULL;
	}
}

static int __init stylus_uevent_init(void)
{
	int ret = 0;

	STYLUS_INFO("%s enter.\n", __FUNCTION__);
	ret = alloc_chrdev_region(&stylus_dev_num, 0, 1,
				  "stylus_dev_num");
	if (stylus_dev_num < 0) {
		STYLUS_ERR("stylus dev num chrdev region fail");
		return -EINVAL;
	}

	stylus_class = class_create(THIS_MODULE, "stylus_uevent");
	if (IS_ERR(stylus_class)) {
		STYLUS_ERR("create demo class fail\n");
		ret = -EINVAL;
		goto stylus_class_creat_fail;
	}

	stylus_dev =
		device_create(stylus_class, NULL, stylus_dev_num,
			      NULL, "stylus_dev");
	if (stylus_dev == NULL) {
		STYLUS_ERR("device_create fail\n");
		ret = -EINVAL;
		goto stylus_dev_creat_fail;
	}
	stylus_class->dev_uevent = stylus_report_uevent;

	ret = stylus_dev_set_drvdata();
	if (ret) {
		STYLUS_ERR("stylus_dev_set_drvdata failed.\n");
		goto stylus_dev_set_drvdata_failed;
	}

	ret = stylus_debug_init(stylus_dev);
	if (ret) {
		STYLUS_ERR("stylus_debug_init failed.\n");
		goto stylus_debug_init_failed;
	}

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	INIT_DELAYED_WORK(&metrics_work, metrics_stylus_battery_algo_work);
	schedule_delayed_work(&metrics_work, TIME_24_HOURS);
#endif

	STYLUS_INFO("%s out.\n", __FUNCTION__);
	return 0;

stylus_debug_init_failed:
	stylus_dev_remove_drvdata();
stylus_dev_set_drvdata_failed:
	device_destroy(stylus_class, stylus_dev_num);
stylus_dev_creat_fail:
	class_destroy(stylus_class);
	stylus_class = NULL;
stylus_class_creat_fail:
	unregister_chrdev_region(stylus_dev_num, 1);
	return ret;
}

static void __exit stylus_uevent_exit(void)
{
	if (stylus_dev_num >= 0)
		unregister_chrdev_region(stylus_dev_num, 1);
	if (stylus_dev)
		device_destroy(stylus_class, stylus_dev_num);
	if (stylus_class)
		class_destroy(stylus_class);
	stylus_dev_remove_drvdata();
	stylus_debug_deinit(stylus_dev);
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	cancel_delayed_work(&metrics_work);
#endif
}

module_init(stylus_uevent_init);
module_exit(stylus_uevent_exit);
MODULE_LICENSE("GPL");
