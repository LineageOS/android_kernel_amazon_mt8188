#ifndef __STYLUS_BATTERY_ALGO_H
#define __STYLUS_BATTERY_ALGO_H

#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
#include "hid_touch_metrics.h"
#endif

/* stylus uevent size */
#define ENV_SIZE 32

#define BATTERY_ALGO_V2 /* Stylus algorithm V2 is enabling */

#define MAX_BATTERY_LEVEL 100
#define MIN_BATTERY_LEVEL 1
#define STYLUS_VENDOR_USAGE_1P 0x132

#define SAMPLE_INTERVAL 100 /* sample battery levels every 100ms. */
#define MAX_TIME_DIFF (1800 * SAMPLE_INTERVAL) /* calculate samples within 180 seconds. */
#define BUFFER_SIZE 2000 /* maximum sample count */
#define MIN_SAMPLE_DATA_COUNT 150 /* calculate with >=150 samples. */

/* stylus battery capacity jitter parameter, BATTERY_CAPACITY_JITTER_BUF: 0~100 */
#define BATTERY_CAPACITY_JITTER_BUF 20
#define STYLUS_BATTERY_CAPACITY_JITTER_BUF(X)                                  \
	(X + BATTERY_CAPACITY_JITTER_BUF)
#ifdef BATTERY_ALGO_V2
#define BATTERY_JITTER_THRESHOLD 8
#define MIN_THEORETICAL_BATTERY 0
#define MAX_THEORETICAL_BATTERY 100
#endif

#define INVALID_PEN_ID 0

/* Return reason list */
#define NEED_MORE_SAMPLE -1
#define ABNORMAL_JITTER_GAP -2

#define STYLUS_INFO(fmt, arg...)                                                  \
	({ pr_info("Stylus: (%d): " fmt, __LINE__, ##arg); })

#define STYLUS_ERR(fmt, arg...)                                                   \
	({ pr_err("Stylus: (%d): " fmt, __LINE__, ##arg); })

 #define STYLUS_WARN(fmt, arg...)                                                   \
	({ pr_warn("Stylus: (%d): " fmt, __LINE__, ##arg); })

#define STYLUS_DEBUG(fmt, arg...)                                            \
	({ pr_debug("Stylus: (%d): " fmt, __LINE__, ##arg); })

typedef struct {
    uint32_t pen_id;
    u16 vendor_id;
    u16 fw_version;
    int capacity;
    s64 time;
} stylus_packet_info_t;

typedef struct {
    int capacity;
    s64 time;
} stylus_battery_cap_info_t;

/* Structure for the circular buffer */
typedef struct {
    stylus_battery_cap_info_t values[BUFFER_SIZE];
    int front;
    int rear;
} stylus_circular_buffer_t;

typedef struct {
    char stylus_vendor[ENV_SIZE];
    char stylus_vendor_id[ENV_SIZE];
    char stylus_capacity[ENV_SIZE];
    char stylus_status[ENV_SIZE];
    char stylus_fw_version[ENV_SIZE];
    char stylus_serial[ENV_SIZE];
} stylus_uevent_info_t;

typedef struct {
    stylus_packet_info_t packet_info;
    stylus_uevent_info_t *uevent_info;
    stylus_circular_buffer_t *circular_buffer;
    int battery_capacity;
    int battery_capacity_filter;
} stylus_data_t;

int stylus_info_process(void);
void stylus_info_packet(stylus_packet_info_t packet_info);
int stylus_debug_init(struct device *stylus_dev);
int stylus_debug_deinit(struct device *stylus_dev);

extern int battery_level_debug;
#endif //__STYLUS_BATTERY_ALGO_H
