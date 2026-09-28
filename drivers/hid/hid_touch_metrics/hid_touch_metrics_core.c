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

#include "hid_touch_metrics.h"

void tp_metrics_print_func(touch_metrics_info_t metrics_data)
{
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char touch_metrics_buf[METRICS_STR_LEN] = { 0 };
	const struct amazon_logger_ops *amazon_logger = amazon_logger_ops_get();

	if (amazon_logger && amazon_logger->minerva_metrics_log)
		amazon_logger->minerva_metrics_log(
			touch_metrics_buf, METRICS_STR_LEN,
			TOUCH_HID_METRICS_FMT, MINERVA_INPUT_TOUCH_GROUP_ID,
			MINERVA_INPUT_TOUCH_SCHEMA_ID, PREDEFINED_ESSENTIAL_KEY,
			PREDEFINED_DEVICE_ID_KEY, PREDEFINED_CUSTOMER_ID_KEY,
			PREDEFINED_OS_KEY, PREDEFINED_DEVICE_LANGUAGE_KEY,
			PREDEFINED_TZ_KEY, PREDEFINED_MODEL_KEY,
			touch_module_data.module_name, metrics_data.fw_version,
			touch_module_data.module_id, metrics_data.bootup_status,
			metrics_data.fw_upgrade_result);
#endif
}
EXPORT_SYMBOL_GPL(tp_metrics_print_func);

void stylus_metrics_print_func(stylus_metrics_info_t stylus_metrics_data)
{
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char stylus_metrics_buf[METRICS_STR_LEN] = { 0 };
	const struct amazon_logger_ops *amazon_logger = amazon_logger_ops_get();

	if (amazon_logger && amazon_logger->minerva_metrics_log)
		amazon_logger->minerva_metrics_log(
			stylus_metrics_buf, METRICS_STR_LEN,
			STYLUS_BATTERY_ALGO_METRICS_FMT, MINERVA_STYLUS_BATTERY_ALGO_GROUP_ID,
			MINERVA_STYLUS_BATTERY_ALGO_SCHEMA_ID, PREDEFINED_ESSENTIAL_KEY,
			PREDEFINED_DEVICE_ID_KEY, PREDEFINED_CUSTOMER_ID_KEY,
			PREDEFINED_OS_KEY, PREDEFINED_DEVICE_LANGUAGE_KEY,
			PREDEFINED_TZ_KEY, PREDEFINED_MODEL_KEY,
			stylus_metrics_data.low_bat_cnt, stylus_metrics_data.bat_hw_jitter_cnt,
			stylus_metrics_data.bat_sw_Jitter_cnt, stylus_metrics_data.bat_level_mode,
			stylus_metrics_data.fw_version, stylus_metrics_data.stylus_id);
#endif
}
EXPORT_SYMBOL_GPL(stylus_metrics_print_func);

static int __init hid_touch_metrics_init(void)
{
    TOUCH_METRICS_INFO("%s enter.\n", __FUNCTION__);
    init_touch_module_info();
    TOUCH_METRICS_INFO("%s out.\n", __FUNCTION__);

    return 0;
}

static void __exit hid_touch_metrics_exit(void)
{
    TOUCH_METRICS_INFO("%s enter.\n", __FUNCTION__);
    TOUCH_METRICS_INFO("%s out.\n", __FUNCTION__);
}

module_init(hid_touch_metrics_init);
module_exit(hid_touch_metrics_exit);
MODULE_LICENSE("GPL");
