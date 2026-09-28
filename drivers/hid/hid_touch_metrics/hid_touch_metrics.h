#ifndef __HID_TOUCH_METRICS_H
#define __HID_TOUCH_METRICS_H

#include <linux/module.h>
#include <linux/init.h>
#include "touch_module_id.h"
#if IS_ENABLED(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#include <linux/metricslog.h>
#endif

#define TOUCH_METRICS_INFO(fmt, arg...)                                                  \
	({ pr_info("touch_metrics: (%d): " fmt, __LINE__, ##arg); })

#define TOUCH_METRICS_ERR(fmt, arg...)                                                   \
	({ pr_err("touch_metrics: (%d): " fmt, __LINE__, ##arg); })

#define ENV_SIZE 32
#define METRICS_STR_LEN 1024
#define BATTERY_LEVEL_LEN_MAX 5
#define TIME_24_HOURS (86400*HZ)
#define STYLUS_BATTERY_JITTER_THRESHOLD 8
#define LOW_BATTERY_LEVEL 40
#define DEFAULT_FW_VERSION 0

#define TOUCH_HID_METRICS_FMT                                                  \
	"%s:%s:100:%s,%s,%s,%s,%s,%s,%s,TouchModuleName=%s;SY,TouchFwVersion=%d;IN,TouchModuleId=%d;IN,TouchBootupStatus=%d;IN,TouchFwUpgradeResult=%d;IN:us-east-1"
#define STYLUS_BATTERY_ALGO_METRICS_FMT                                         \
	"%s:%s:100:%s,%s,%s,%s,%s,%s,%s,Low_Bat_Notification_Count=%d;IN,StylusBat_HW_Jitter_Count=%d;IN,StylusBat_SW_Jitter_Count=%d;IN,StylusBat_Level_Mode=%d;IN,StylusFwVersion=%d;IN,StylusID=%d;IN:us-east-1"

enum TP_BOOT_UP_STATUS {
	BOOT_UP_INIT_STATUS = 0,
	BOOT_UP_SUCCESS = 1,
	HID_DESCRIPTOR_ERROR = 2,
	HID_HW_RESET_FAIL = 3,
	ADD_HID_DEVICE_FAIL = 4,
	FW_RESET_STATUS_FAIL = 5,
	UNKNOWN_BOOT_UP_STATUS = 0xFF
};

enum TP_FW_UPGRADE_RESULT {
	FW_UPGRADE_STATE_INIT = 0,
	FW_NO_NEED_UPGRADE = 1,
	FW_UPGRADE_PASS = 2,
	FW_UPGRADE_FAIL = 3
};

typedef struct {
	char module_name[ENV_SIZE];
	int module_id;
} touch_module_info_t;

typedef struct {
	int fw_version;
	int bootup_status;
	int fw_upgrade_result;
} touch_metrics_info_t;

typedef struct {
	int bat_hw_jitter_cnt;
	int bat_sw_Jitter_cnt;
	int low_bat_cnt;
	int bat_level_mode;
	int fw_version;
	int stylus_id;
} stylus_metrics_info_t;

void tp_metrics_print_func(touch_metrics_info_t touch_metrics_data);
void stylus_metrics_print_func(stylus_metrics_info_t stylus_metrics_data);

#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif
extern touch_module_info_t touch_module_data;

#endif //__HID_TOUCH_METRICS_H
