// SPDX-License-Identifier: GPL-2.0
/*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include "focal_hid_core.h"
#define FTS_ESDCHECK_EN 1

#define ESDCHECK_WAIT_TIME 2000    /* ms */
#define LCD_ESD_SUPPORT 1
#define ESDCHECK_INTRCNT_MAX 2
#define ESD_INTR_INTERVALS 200    /* unit:ms */

#define LCD_ESD_ERR_VALUE 0x55
#define ESD_SWITCH_REG 0x9B

struct fts_esdcheck_st {
	u8      mode                : 1;    /* 1- need check esd 0- no esd check */
	u8      suspend             : 1;
	u8      tp_esd_hang_cnt;         /* Flow Work Cnt(reg0x91) keep a same value for x times. >=5 times is ESD, need reset */
	u8      tp_esd_last_value;       /* Save Flow Work Cnt(reg0x91) value */
	u8      tp_esd_cur_value;
	u8      lcd_esd_value;
	u32     hardware_reset_cnt;
	struct mutex esd_mutex;
};

static struct fts_esdcheck_st fts_esdcheck_data;
static int tp_esd_error = 0;
#if defined(CONFIG_TP_TO_LCD_ESD_CHECK)
extern int disp_esd_check_register(int (*esd_check_lcd_error)(void));
#endif

#if LCD_ESD_SUPPORT
int esd_check_lcd_error(void)
{
	int lcm_esd_error = 0;
	int lcm_need_recovery = 0;

	mutex_lock(&fts_esdcheck_data.esd_mutex);
	if (fts_esdcheck_data.lcd_esd_value == LCD_ESD_ERR_VALUE) {
		FTS_ERROR("LCD ESD, need execute LCD reset");
		lcm_esd_error = 1;
		fts_esdcheck_data.lcd_esd_value = 0;
	}
	lcm_need_recovery = lcm_esd_error || tp_esd_error;
	tp_esd_error = 0;
	lcm_esd_error = 0;
	mutex_unlock(&fts_esdcheck_data.esd_mutex);

	return lcm_need_recovery;
}
EXPORT_SYMBOL(esd_check_lcd_error);
#endif

void fts_esd_hang_cnt(struct fts_hid_data *ts_data)
{
	if (ts_data->esd_support && !ts_data->suspended)
		fts_esdcheck_data.tp_esd_hang_cnt = 0;
}

int fts_update_esd_value(u8 tp_esd_value, u8 lcd_esd_value)
{
	mutex_lock(&fts_esdcheck_data.esd_mutex);
	fts_esdcheck_data.tp_esd_cur_value = tp_esd_value;
	fts_esdcheck_data.lcd_esd_value = lcd_esd_value;
	mutex_unlock(&fts_esdcheck_data.esd_mutex);

	return 0;
}

/*****************************************************************************
*  Name: fts_recovery_state
*  The TP firmware will report the recovery event when it starts to work
*  after LCD reset.We need to release touch points, handle gestures
*  and recovery the esd check function when the firmware works again.
*****************************************************************************/
int fts_recovery_state(struct fts_hid_data *ts_data)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	mt_release_contacts(ts_data->hdev);
	focal_wait_tp_to_valid();
	if (ts_data->gesture_mode && ts_data->suspended && !ts_data->lcd_disp_on) {
		FTS_INFO("gesture recovery...");
		focal_write_register(0xD0, ENABLE_GESTURE_MODE);
	}

	if (ts_data->esd_support && !ts_data->suspended) {
		FTS_INFO("esd recovery...");
		focal_esdcheck_switch(fts_hid_data, ENABLE_ESD_CHECK);
	}
	FTS_FUNC_EXIT();

	return 0;
}

static int esdcheck_algorithm(struct fts_hid_data *ts_data)
{
	unsigned long intr_timeout = msecs_to_jiffies(ESD_INTR_INTERVALS);

	/* 1. esdcheck is interrupt, then return */
	intr_timeout += ts_data->intr_jiffies;
	if (time_before(jiffies, intr_timeout))
		return 0;

	/* 2. check power state, if suspend, no need check esd */
	if (fts_esdcheck_data.suspend) {
		FTS_DEBUG("In suspend, don't check esd");
		/* because in suspend state, adb can be used, when upgrade FW, will
		 * active ESD check(active = 1); But in suspend, then will don't
		 * queue_delayed_work, when resume, don't check ESD again
		 */
		return 0;
	}

	/* 3. get Flow work cnt: 0x91 If no change for 3 times, then ESD and reset */
	mutex_lock(&fts_esdcheck_data.esd_mutex);
	if (fts_esdcheck_data.tp_esd_cur_value == fts_esdcheck_data.tp_esd_last_value) {
		FTS_DEBUG("reg0x91,val:%x,last:%x", fts_esdcheck_data.tp_esd_cur_value,
			fts_esdcheck_data.tp_esd_last_value);
		fts_esdcheck_data.tp_esd_hang_cnt++;
	} else {
		fts_esdcheck_data.tp_esd_hang_cnt = 0;
	}

	fts_esdcheck_data.tp_esd_last_value = fts_esdcheck_data.tp_esd_cur_value;
	/* Flow Work Cnt keep a value for 3 times, need execute TP reset */
	if (fts_esdcheck_data.tp_esd_hang_cnt >= 3) {
		FTS_ERROR("Hardware Reset=%d", fts_esdcheck_data.hardware_reset_cnt);
		fts_esdcheck_data.tp_esd_hang_cnt = 0;
		fts_esdcheck_data.hardware_reset_cnt++;
		tp_esd_error = 1;
	}
	mutex_unlock(&fts_esdcheck_data.esd_mutex);

	return 0;
}

static void esdcheck_func(struct work_struct *work)
{
	struct fts_hid_data *ts_data = container_of(work,
				struct fts_hid_data, esdcheck_work.work);

	if (ts_data->esd_support && fts_esdcheck_data.mode) {
		esdcheck_algorithm(ts_data);
		queue_delayed_work(ts_data->ts_workqueue, &ts_data->esdcheck_work,
				msecs_to_jiffies(ESDCHECK_WAIT_TIME));
	}
}

/*****************************************************************************
*  Name: fts_esdcheck_switch
*  Brief: FTS esd check function switch.
*  Input:   enable:  1 - Enable esd check
*                    0 - Disable esd check
*  Output:
*  Return:
*****************************************************************************/
void focal_esdcheck_switch(struct fts_hid_data *ts_data, bool enable)
{
	if (ts_data->esd_support) {
		FTS_FUNC_ENTER();
		if (fts_esdcheck_data.mode == ENABLE_ESD_CHECK) {
			if (enable) {
				FTS_DEBUG("ESD check start");
				fts_esdcheck_data.tp_esd_hang_cnt = 0;
				fts_esdcheck_data.tp_esd_last_value = 0;
				focal_write_register(ESD_SWITCH_REG, 1);
				queue_delayed_work(ts_data->ts_workqueue, &ts_data->esdcheck_work,
						msecs_to_jiffies(ESDCHECK_WAIT_TIME));
			} else {
				FTS_DEBUG("ESD check stop");
				cancel_delayed_work_sync(&ts_data->esdcheck_work);
				focal_write_register(ESD_SWITCH_REG, 0);
			}
		}
		FTS_FUNC_EXIT();
	}
}

void focal_esdcheck_suspend(struct fts_hid_data *ts_data)
{
	FTS_FUNC_ENTER();
	focal_esdcheck_switch(ts_data, DISABLE_ESD_CHECK);
	fts_esdcheck_data.suspend = 1;
	FTS_FUNC_EXIT();
}

void focal_esdcheck_resume(struct fts_hid_data *ts_data)
{
	FTS_FUNC_ENTER();
	focal_esdcheck_switch(ts_data, ENABLE_ESD_CHECK);
	fts_esdcheck_data.suspend = 0;
	FTS_FUNC_EXIT();
}

static ssize_t fts_esdcheck_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct input_dev *input_dev = fts_hid_data->ts_input_dev;

	mutex_lock(&input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable esdcheck");
		fts_hid_data->esd_support = ENABLE_ESD_CHECK;
		focal_esdcheck_switch(fts_hid_data, ENABLE_ESD_CHECK);
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable esdcheck");
		focal_esdcheck_switch(fts_hid_data, DISABLE_ESD_CHECK);
		fts_hid_data->esd_support = DISABLE_ESD_CHECK;
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_esdcheck_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	int count;
	struct input_dev *input_dev = fts_hid_data->ts_input_dev;

	mutex_lock(&input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "Esd check: %s\n",
			 fts_hid_data->esd_support ? "On" : "Off");
	mutex_unlock(&input_dev->mutex);

	return count;
}

static DEVICE_ATTR(fts_esd_mode, S_IRUGO | S_IWUSR, fts_esdcheck_show, fts_esdcheck_store);

static struct attribute *fts_esd_mode_attrs[] = {
	&dev_attr_fts_esd_mode.attr,
	NULL,
};

static struct attribute_group fts_esd_group = {
	.attrs = fts_esd_mode_attrs,
};

static int fts_create_esd_sysfs(struct device *dev)
{
	int ret = 0;

	ret = sysfs_create_group(&dev->kobj, &fts_esd_group);
	if (ret != 0) {
		FTS_ERROR("%s(sysfs) create fail", __func__);
		sysfs_remove_group(&dev->kobj, &fts_esd_group);
		return ret;
	}

	return 0;
}

void focal_esdcheck_init(struct fts_hid_data *ts_data)
{
	FTS_FUNC_ENTER();
	INIT_DELAYED_WORK(&ts_data->esdcheck_work, esdcheck_func);
	memset((u8 *)&fts_esdcheck_data, 0, sizeof(struct fts_esdcheck_st));
	mutex_init(&fts_esdcheck_data.esd_mutex);
	fts_esdcheck_data.mode = ENABLE_ESD_CHECK;
	fts_create_esd_sysfs(&ts_data->hdev->dev);
	ts_data->esd_support = FTS_ESDCHECK_EN;
	focal_esdcheck_switch(ts_data, ENABLE_ESD_CHECK);
#if defined(CONFIG_TP_TO_LCD_ESD_CHECK)
	disp_esd_check_register(esd_check_lcd_error);
#endif
	FTS_FUNC_EXIT();
}

int focal_esdcheck_exit(struct fts_hid_data *ts_data)
{
	fts_esdcheck_data.mode = DISABLE_ESD_CHECK;
	ts_data->esd_support = DISABLE_ESD_CHECK;
	if (ts_data->ts_workqueue)
		cancel_delayed_work_sync(&ts_data->esdcheck_work);
	sysfs_remove_group(&ts_data->dev->kobj, &fts_esd_group);

	return 0;
}
