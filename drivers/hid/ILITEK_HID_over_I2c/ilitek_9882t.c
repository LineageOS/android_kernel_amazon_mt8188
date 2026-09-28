/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2022 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ilitek_9882t_fw.h"
#include "ilitek_9882t.h"

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
touch_metrics_info_t touch_metrics_data;
#endif

/* Debug level */
bool debug_en = DEBUG_OUTPUT;

static struct workqueue_struct *esd_wq;
static struct workqueue_struct *bat_wq;
static struct delayed_work esd_work;
static struct delayed_work bat_work;

#if IS_ENABLED(CONFIG_IDME)
extern unsigned int idme_get_lcmid_info(void);
#endif
unsigned char lcm_vendor_id = LCM_VENDOR_ID_UNKNOWN;

static void get_lcm_id(void)
{
	unsigned int lcm_id = 0xFF;
#if IS_ENABLED(CONFIG_IDME)
	lcm_id = idme_get_lcmid_info();
	if (lcm_id == 0xFF)
		ILI_ERR("get lcm_id failed.\n");
#else
	ILI_ERR("%s, idme is not ready.\n", __func__);
#endif
	lcm_vendor_id = (unsigned char)lcm_id;
	ILI_INFO("[ilitek9882t_hid] %s, vendor id = 0x%x\n", __func__,
		 lcm_vendor_id);
}

int ili_hid_mp_test_handler(char *apk, bool lcm_on)
{
	int ret = 0;

	if (atomic_read(&ilits->fw_stat)) {
		ILI_ERR("fw upgrade processing, ignore\n");
		return -EMP_FW_PROC;
	}

	atomic_set(&ilits->mp_stat, ENABLE);

	if (ilits->actual_tp_mode != P5_X_FW_TEST_MODE) {
		ret = ili_hid_switch_tp_mode(P5_X_FW_TEST_MODE);
		if (ret < 0) {
			ILI_ERR("Switch MP mode failed\n");
			ret = -EMP_MODE;
			goto out;
		}
	}

	ret = ili_hid_mp_test_main(apk, lcm_on);

out:
	/*
	 * If there's running mp test with lcm off, we suspose that
	 * users will soon call resume from suspend. TP mode will be changed
	 * from MP to AP mode until resume finished.
	 */
	if (!lcm_on) {
		atomic_set(&ilits->mp_stat, DISABLE);
		return ret;
	}

	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	if (ilits->fw_upgrade_mode == UPGRADE_IRAM) {
		if (ili_hid_fw_upgrade_handler(NULL) < 0)
			ILI_ERR("FW upgrade failed during mp test\n");
	} else {
		if (ilits->cascade_info_block.nNum != 0) {
			if (ilits->reset == TP_HW_RST_ONLY) {
				ili_hid_wdt_reset_status(true, true);
				if (ili_hid_reset_ctrl(ilits->reset) < 0)
					ILI_ERR("TP Reset failed during mp test\n");
			} else {
				if (ili_hid_cascade_reset_ctrl(ilits->reset, true) < 0)
					ILI_ERR("TP Cascade Reset failed during mp test\n");
			}
		} else {
			if (ili_hid_reset_ctrl(ilits->reset) < 0)
				ILI_ERR("TP Reset failed during mp test\n");
		}
	}

	atomic_set(&ilits->mp_stat, DISABLE);
	return ret;
}

int ili_hid_switch_tp_mode(u8 mode)
{
	int ret = 0;
	bool ges_dbg = false;

	atomic_set(&ilits->tp_sw_mode, START);

	ilits->actual_tp_mode = mode;

	/* able to see cdc data in gesture mode */
	if (ilits->tp_data_format == DATA_FORMAT_DEBUG &&
	    ilits->actual_tp_mode == P5_X_FW_GESTURE_MODE)
		ges_dbg = true;

	switch (ilits->actual_tp_mode) {
	case P5_X_FW_AP_MODE:
		ILI_INFO("Switch to AP mode\n");
		ilits->wait_int_timeout = AP_INT_TIMEOUT;
		if (ilits->fw_upgrade_mode == UPGRADE_IRAM) {
			if (ili_hid_fw_upgrade_handler(NULL) < 0)
				ILI_ERR("FW upgrade failed\n");
		} else {
			if (ilits->cascade_info_block.nNum != 0) {
				if (ilits->reset == TP_HW_RST_ONLY) {
					ili_hid_wdt_reset_status(true, true);
					if (ili_hid_reset_ctrl(ilits->reset) <
					    0)
						ILI_ERR("TP Reset failed during ili_hid_switch_tp_mode\n");
				} else {
					if (ili_hid_cascade_reset_ctrl(
						    ilits->reset, true) < 0)
						ILI_ERR("TP Cascade Reset failed during ili_hid_switch_tp_mode\n");
				}
			} else {
				ret = ili_hid_reset_ctrl(ilits->reset);
			}
		}
		if (ret < 0)
			ILI_ERR("TP Reset failed\n");

		break;
	case P5_X_FW_GESTURE_MODE:
		ILI_INFO("Switch to Gesture mode\n");
		ilits->wait_int_timeout = AP_INT_TIMEOUT;
		ret = ilits->gesture_move_code(ilits->gesture_mode);
		if (ret < 0)
			ILI_ERR("Move gesture code failed\n");
		if (ges_dbg) {
			ILI_INFO("Enable gesture debug func\n");
			ili_hid_set_tp_data_len(DATA_FORMAT_GESTURE_DEBUG,
						false, NULL);
		}
		break;
	case P5_X_FW_TEST_MODE:
		ILI_INFO("Switch to Test mode\n");
		ilits->wait_int_timeout = MP_INT_TIMEOUT;
		ret = ilits->mp_move_code();
		break;
	default:
		ILI_ERR("Unknown TP mode: %x\n", mode);
		ret = -1;
		break;
	}

	if (ret < 0)
		ILI_ERR("Switch TP mode (%d) failed \n", mode);

	ILI_DBG("Actual TP mode = %d\n", ilits->actual_tp_mode);
	atomic_set(&ilits->tp_sw_mode, END);
	return ret;
}

int ili_hid_gesture_recovery(void)
{
	int ret = 0;

	atomic_set(&ilits->esd_stat, START);

	ILI_INFO("Doing gesture recovery\n");
	ret = ilits->ges_recover();

	atomic_set(&ilits->esd_stat, END);
	return ret;
}

void ili_hid_spi_recovery(void)
{
	atomic_set(&ilits->esd_stat, START);

	ILI_INFO("Doing spi recovery\n");
	if (ili_hid_fw_upgrade_handler(NULL) < 0)
		ILI_ERR("FW upgrade failed\n");

	atomic_set(&ilits->esd_stat, END);
}
#if 0
int ili_hid_wq_esd_spi_check(void)
{
	int ret = 0;
	u8 tx = SPI_WRITE, rx = 0;

	ret = ilits->spi_write_then_read(ilits->spi, &tx, 1, &rx, 1);
	ILI_DBG("spi esd check = 0x%x\n", ret);
	if (ret == DO_SPI_RECOVER) {
		ILI_ERR("ret = 0x%x\n", ret);
		return -1;
	}
	return 0;
}
#endif
int ili_hid_wq_esd_i2c_check(void)
{
	ILI_DBG("");
	return 0;
}

static void ilitek_tddi_wq_esd_check(struct work_struct *work)
{
	if (mutex_is_locked(&ilits->touch_mutex)) {
		ILI_INFO("touch is locked, ignore\n");
		return;
	}
	mutex_lock(&ilits->touch_mutex);
	if (ilits->esd_recover() < 0) {
		ILI_ERR("SPI ACK failed, doing spi recovery\n");
		ili_hid_spi_recovery();
	}
	mutex_unlock(&ilits->touch_mutex);
	complete_all(&ilits->esd_done);
	ili_hid_wq_ctrl(WQ_ESD, ENABLE);
}

static int read_power_status(u8 *buf)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
	struct file *f = NULL;
	mm_segment_t old_fs;
	ssize_t byte = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	f = filp_open(POWER_STATUS_PATH, O_RDONLY, 0);
	if (ERR_ALLOC_MEM(f)) {
		ILI_ERR("Failed to open %s\n", POWER_STATUS_PATH);
		return -1;
	}

	f->f_op->llseek(f, 0, SEEK_SET);
	byte = f->f_op->read(f, buf, 20, &f->f_pos);

	ILI_DBG("Read %d bytes\n", (int)byte);

	set_fs(old_fs);
	filp_close(f, NULL);
#else
	return -1;
#endif
	return 0;
}

static void ilitek_tddi_wq_bat_check(struct work_struct *work)
{
	u8 str[20] = { 0 };
	static int charge_mode;

	if (read_power_status(str) < 0)
		ILI_ERR("Read power status failed\n");

	ILI_DBG("Batter Status: %s\n", str);

	if (strstr(str, "Charging") != NULL || strstr(str, "Full") != NULL ||
	    strstr(str, "Fully charged") != NULL) {
		if (charge_mode != 1) {
			ILI_DBG("Charging mode\n");
			if (ili_hid_ic_func_ctrl("plug", DISABLE) < 0) // plug in
				ILI_ERR("Write plug in failed\n");
			charge_mode = 1;
		}
	} else {
		if (charge_mode != 2) {
			ILI_DBG("Not charging mode\n");
			if (ili_hid_ic_func_ctrl("plug", ENABLE) < 0) // plug out
				ILI_ERR("Write plug out failed\n");
			charge_mode = 2;
		}
	}
	ili_hid_wq_ctrl(WQ_BAT, ENABLE);
}

void ili_hid_wq_ctrl(int type, int ctrl)
{
	switch (type) {
	case WQ_ESD:
		if (ilits->esd_func_ctrl || ilits->wq_ctrl) {
			if (!esd_wq) {
				ILI_ERR("wq esd is null\n");
				break;
			}
			ilits->wq_esd_ctrl = ctrl;
			if (ctrl == ENABLE) {
				ILI_DBG("execute esd check\n");
				if (!queue_delayed_work(esd_wq, &esd_work,
					    msecs_to_jiffies(WQ_ESD_DELAY)))
					ILI_DBG("esd check was already on queue\n");
			} else {
				cancel_delayed_work_sync(&esd_work);
				flush_workqueue(esd_wq);
				ILI_DBG("cancel esd wq\n");
			}
		}
		break;
	case WQ_BAT:
		if (ENABLE_WQ_BAT || ilits->wq_ctrl) {
			if (!bat_wq) {
				ILI_ERR("WQ BAT is null\n");
				break;
			}
			ilits->wq_bat_ctrl = ctrl;
			if (ctrl == ENABLE) {
				ILI_DBG("execute bat check\n");
				if (!queue_delayed_work(bat_wq, &bat_work,
					    msecs_to_jiffies(WQ_BAT_DELAY)))
					ILI_DBG("bat check was already on queue\n");
			} else {
				cancel_delayed_work_sync(&bat_work);
				flush_workqueue(bat_wq);
				ILI_DBG("cancel bat wq\n");
			}
		}
		break;
	default:
		ILI_ERR("Unknown WQ type, %d\n", type);
		break;
	}
}

static void ilitek_tddi_wq_init(void)
{
	esd_wq = alloc_workqueue("esd_check", WQ_MEM_RECLAIM, 0);
	bat_wq = alloc_workqueue("bat_check", WQ_MEM_RECLAIM, 0);

	WARN_ON(!esd_wq);
	WARN_ON(!bat_wq);

	INIT_DELAYED_WORK(&esd_work, ilitek_tddi_wq_esd_check);
	INIT_DELAYED_WORK(&bat_work, ilitek_tddi_wq_bat_check);

#if RESUME_BY_DDI
	resume_by_ddi_wq = create_singlethread_workqueue("resume_by_ddi_wq");
	WARN_ON(!resume_by_ddi_wq);
	INIT_WORK(&resume_by_ddi_work, ilitek_resume_by_ddi_work);
#endif
}
#if 1
int ili_hid_sleep_handler(int mode)
{
	int ret = 0;
	int wake_status = 0;
	bool sense_stop = true;
	struct i2c_client *client = ilits->i2c;

	mutex_lock(&ilits->touch_mutex);
	atomic_set(&ilits->tp_sleep, START);

	if (atomic_read(&ilits->fw_stat) || atomic_read(&ilits->mp_stat)) {
		ILI_INFO("fw upgrade or mp still running, ignore sleep requst\n");
		atomic_set(&ilits->tp_sleep, END);
		mutex_unlock(&ilits->touch_mutex);
		return 0;
	}

	ili_hid_wq_ctrl(WQ_ESD, DISABLE);
	ili_hid_wq_ctrl(WQ_BAT, DISABLE);
	// ili_hid_irq_disable();

	ILI_INFO("Sleep Mode = %d\n", mode);

	if (ilits->ss_ctrl)
		sense_stop = true;
	else if ((ilits->chip->core_ver >= CORE_VER_1430))
		sense_stop = false;
	else
		sense_stop = true;

	switch (mode) {
	case TP_SUSPEND:
		ILI_INFO("TP suspend start\n");
		ilits->tp_suspend = true;
		ilits->power_status = false;
		if (sense_stop) {
			if (ili_hid_ic_func_ctrl("sense", DISABLE) < 0)
				ILI_ERR("Write sense stop cmd failed\n");
			if (ili_hid_ic_check_busy(50, 20, ON) < 0)
				ILI_ERR("Check busy timeout during suspend\n");
		}

		if ((atomic_read(&ilits->gesture)) || (ilits->prox_face_mode == true)) {
			ili_hid_switch_tp_mode(P5_X_FW_GESTURE_MODE);
			wake_status = enable_irq_wake(client->irq);
			if (!wake_status)
				ilits->irq_wake_enabled = true;
			else
				ILI_ERR("Failed to enable irq wake: %d\n", wake_status);
		} else {
			if (ili_hid_ic_func_ctrl("sleep", SLEEP_IN) < 0)
				ILI_ERR("Write sleep in cmd failed\n");
			ili_hid_irq_disable();
		}
		ILI_INFO("TP suspend end\n");
		break;
	case TP_DEEP_SLEEP:
		ILI_INFO("TP deep suspend start\n");
		ilits->tp_suspend = true;
		ilits->power_status = false;
		if (sense_stop) {
			if (ili_hid_ic_func_ctrl("sense", DISABLE) < 0)
				ILI_ERR("Write sense stop cmd failed\n");

			if (ili_hid_ic_check_busy(50, 20, ON) < 0)
				ILI_ERR("Check busy timeout during deep suspend\n");
		}

		if ((atomic_read(&ilits->gesture)) || (ilits->prox_face_mode == true)) {
			ili_hid_switch_tp_mode(P5_X_FW_GESTURE_MODE);
			wake_status = enable_irq_wake(client->irq);
			if (!wake_status)
				ilits->irq_wake_enabled = true;
			else
				ILI_ERR("Failed to enable irq wake: %d\n", wake_status);
		} else {
			if (ili_hid_ic_func_ctrl("sleep", DEEP_SLEEP_IN) < 0)
				ILI_ERR("Write deep sleep in cmd failed\n");
			ili_hid_irq_disable();
		}
		ILI_INFO("TP deep suspend end\n");
		break;
	case TP_RESUME:
		if (ilitek_hdev)
			mt_release_contacts(ilitek_hdev);
#if !RESUME_BY_DDI
		ILI_INFO("TP resume start\n");
		ilits->tp_suspend = false;
		if (ilits->irq_wake_enabled) {
			wake_status = disable_irq_wake(client->irq);
			if (!wake_status)
				ilits->irq_wake_enabled = false;
			else
				ILI_ERR("Failed to disable irq wake: %d\n", wake_status);
		}

		ili_hid_irq_enable();

		/* Set tp as demo mode and reload code if it's iram. */
		ilits->actual_tp_mode = P5_X_FW_AP_MODE;
		if ((ilits->proxmity_face == false) &&
		    (ilits->power_status == false) &&
		    (ilits->prox_face_mode == true)) {
			ili_hid_ic_func_ctrl("sleep", 0x01);
			ili_hid_ic_func_ctrl("sense", 0x01);
			if (ili_hid_set_tp_data_len(DATA_FORMAT_DEMO, false, NULL) < 0)
				ILI_ERR("Failed to set tp data length\n");
		} else if (ilits->fw_upgrade_mode == UPGRADE_IRAM) {
			if (ili_hid_fw_upgrade_handler(NULL) < 0)
				ILI_ERR("FW upgrade failed during resume\n");
		} else {
			if (ilits->cascade_info_block.nNum != 0) {
				if (ilits->reset == TP_HW_RST_ONLY) {
					ili_hid_wdt_reset_status(true, true);
					if (ili_hid_reset_ctrl(ilits->reset) < 0)
						ILI_ERR("TP Reset failed during resume\n");
				} else {
					if (ili_hid_cascade_reset_ctrl(
						    ilits->reset, true) < 0)
						ILI_ERR("TP Cascade Reset failed during resume\n");
				}
			} else {
				if (ili_hid_reset_ctrl(ilits->reset) < 0)
					ILI_ERR("TP Reset failed during resume\n");
			}
			ili_hid_ic_func_ctrl_reset();
		}
		ilits->power_status = true;
#if (PROXIMITY_BY_FW_MODE == PROXIMITY_SUSPEND_RESUME)
		ilits->proxmity_face = false;
#endif
		ILI_INFO("TP resume end\n");
#endif
		ili_hid_wq_ctrl(WQ_ESD, ENABLE);
		ili_hid_wq_ctrl(WQ_BAT, ENABLE);
		// ili_hid_irq_enable();
		break;
	default:
		ILI_ERR("Unknown sleep mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}

	// ili_hid_touch_release_all_point();
	atomic_set(&ilits->tp_sleep, END);
	mutex_unlock(&ilits->touch_mutex);
	return ret;
}
#endif
int ili_hid_fw_upgrade_handler(void *data)
{
	int ret = 0;

	atomic_set(&ilits->fw_stat, START);

	ilitek_hid_tddi_flash_protect(DISABLE, OFF); //flash un-protect for i2c project

	ilits->fw_update_stat = FW_STAT_INIT;

	panel_fw_upgrade_mode_set_by_ilitek(true);
	__pm_stay_awake(ilits->ili_upg_wakelock);

	ret = ili_hid_fw_upgrade(ilits->fw_open);
	if (ret != 0) {
		ILI_ERR("FW upgrade fail\n");
		ilits->fw_update_stat = FW_UPDATE_FAIL;
	} else {
		ILI_INFO("FW upgrade pass\n");
#if CHARGER_NOTIFIER_CALLBACK
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
		/* add_for_charger_start */
		if ((ilits->usb_plug_status) &&
		    (ilits->actual_tp_mode != P5_X_FW_TEST_MODE)) {
			ret = ili_hid_ic_func_ctrl("plug", !ilits->usb_plug_status); // plug in
			if (ret < 0) {
				ILI_ERR("Write plug in failed\n");
			}
		}
		/*  add_for_charger_end  */
#endif
#endif
		ilits->fw_update_stat = FW_UPDATE_PASS;
#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
		touch_metrics_data.bootup_status = BOOT_UP_SUCCESS;
#endif
	}

	ilitek_hid_tddi_flash_protect(ENABLE, OFF); //flash protect for i2c project

	__pm_relax(ilits->ili_upg_wakelock);
	panel_fw_upgrade_mode_set_by_ilitek(false);

	atomic_set(&ilits->fw_stat, END);

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	touch_metrics_data.fw_version = (ilits->chip->fw_ver >> 8) & 0xFF;
	tp_metrics_print_func(touch_metrics_data);
#endif

	if (!ilits->boot)
		ilits->boot = true;
	return ret;
}

int ili_hid_set_tp_data_len(int format, bool send, u8 *data)
{
	u8 cmd[10] = { 0 }, ctrl = 0, debug_ctrl = 0, data_type = 0;
	u16 self_key = 2;
	int ret = 0, tp_mode = ilits->actual_tp_mode, len = 0,
	    geture_info_length = 0, demo_mode_packet_len = 0;

	if (data == NULL) {
		data_type = P5_X_FW_SIGNAL_DATA_MODE;
		ILI_ERR("Data Type is Null, Set Single data type\n");
	} else {
		data_type = data[0];
		ILI_INFO("Set data type = 0x%X\n", data[0]);
	}

	if (ilits->rib.nCustomerType == ilits->customertype_off &&
	    ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		geture_info_length = P5_X_GESTURE_INFO_LENGTH;
		if (ilits->PenType == POSITION_PEN_TYPE_ON)
			demo_mode_packet_len = P5_X_5B_LOW_RESOLUTION_LENGTH;
		else
			demo_mode_packet_len = P5_X_DEMO_MODE_PACKET_LEN;
	} else if (ilits->rib.nCustomerType != ilits->customertype_off &&
		   ilits->rib.nReportResolutionMode ==
			   POSITION_LOW_RESOLUTION) {
		geture_info_length = P5_X_GESTURE_INFO_LENGTH;
		demo_mode_packet_len =
			P5_X_5B_LOW_RESOLUTION_LENGTH + P5_X_CUSTOMER_LENGTH;
	} else if (ilits->rib.nCustomerType != ilits->customertype_off &&
		   ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		geture_info_length = P5_X_GESTURE_INFO_LENGTH_HIGH_RESOLUTION;
		demo_mode_packet_len =
			P5_X_DEMO_MODE_PACKET_LEN_HIGH_RESOLUTION +
			P5_X_CUSTOMER_LENGTH;
	} else if (ilits->rib.nCustomerType == ilits->customertype_off &&
		   ilits->rib.nReportResolutionMode ==
			   POSITION_HIGH_RESOLUTION) {
		geture_info_length = P5_X_GESTURE_INFO_LENGTH_HIGH_RESOLUTION;
		demo_mode_packet_len = P5_X_DEMO_MODE_PACKET_LEN_HIGH_RESOLUTION;
	}

	switch (format) {
	case DATA_FORMAT_DEMO:
		if (ilits->PenType == POSITION_PEN_TYPE_ON) {
			ilits->touch_num = MAX_TOUCH_NUM + MAX_PEN_NUM;
			ilits->pen_info.report_type = P5_X_HAND_PEN_TYPE;
			ilits->pen_info.pen_data_mode = HAND_PEN_DEMO_MODE;
			len = demo_mode_packet_len + P5_X_PEN_DATA_LEN;
		} else {
			len = demo_mode_packet_len;
		}
		ctrl = DATA_FORMAT_DEMO_CMD;
		break;
	case DATA_FORMAT_GESTURE_DEMO:
		len = demo_mode_packet_len;
		ctrl = DATA_FORMAT_DEMO_CMD;
		break;
	case DATA_FORMAT_DEBUG:
		if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
			len = P5_X_DEBUG_LOW_RESOLUTION_FINGER_DATA_LENGTH;
		} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
			len = P5_X_DEBUG_HIGH_RESOLUTION_FINGER_DATA_LENGTH;
		}

		if (ilits->PenType == POSITION_PEN_TYPE_ON) {
			ilits->touch_num = MAX_TOUCH_NUM + MAX_PEN_NUM;
			ilits->pen_info.report_type = P5_X_HAND_PEN_TYPE;

			ilits->cdc_data_len =
				(ilits->xch_num * ilits->ych_num * 2) +
				((ilits->stx + ilits->srx) * 2) +
				(2 * self_key);

			if (data_type == P5_X_FW_RAW_DATA_MODE) {
				ilits->pen_info.pen_data_mode =
					HAND_PEN_RAW_DATA_MODE;
				len += ilits->cdc_data_len + P5_X_PEN_DATA_LEN +
				       (((ilits->pen_info_block.nPxRaw *
					  ilits->xch_num * 2) +
					 (ilits->pen_info_block.nPyRaw *
					  ilits->ych_num * 2)) * 2 + 4) +
				       P5_X_OTHER_DATA_LEN;
			} else {
				ilits->pen_info.pen_data_mode =
					HAND_PEN_SIGNAL_MODE;
				len += ilits->cdc_data_len + P5_X_PEN_DATA_LEN +
				       (((ilits->pen_info_block.nPxVa *
					  ilits->xch_num * 2) +
					 (ilits->pen_info_block.nPyVa *
					  ilits->ych_num * 2)) * 2 + 4) +
				       P5_X_OTHER_DATA_LEN;
			}
		} else {
			len += (2 * ilits->xch_num * ilits->ych_num) +
			       (ilits->stx * 2) + (ilits->srx * 2) + (2 * self_key) + 16;
		}

		if (ilits->rib.nCustomerType != ilits->customertype_off)
			len += P5_X_CUSTOMER_LENGTH;
		len += P5_X_INFO_CHECKSUM_LENGTH;

		ctrl = DATA_FORMAT_DEBUG_CMD;
		break;
	case DATA_FORMAT_GESTURE_DEBUG:
		len = (2 * ilits->xch_num * ilits->ych_num) + (ilits->stx * 2) +
		      (ilits->srx * 2);
		len += 2 * self_key + (8 * 2) + 1 + 35;
		ctrl = DATA_FORMAT_DEBUG_CMD;
		break;
	case DATA_FORMAT_DEMO_DEBUG_INFO:
		/*only suport SPI interface now, so defult use size 1024 buffer*/
		len = demo_mode_packet_len + P5_X_DEMO_DEBUG_INFO_ID0_LENGTH +
		      P5_X_INFO_HEADER_LENGTH;
		ctrl = DATA_FORMAT_DEMO_DEBUG_INFO_CMD;
		break;
	case DATA_FORMAT_GESTURE_INFO:
		len = geture_info_length;
		ctrl = DATA_FORMAT_GESTURE_INFO_CMD;
		break;
	case DATA_FORMAT_GESTURE_NORMAL:
		len = P5_X_GESTURE_NORMAL_LENGTH;
		ctrl = DATA_FORMAT_GESTURE_NORMAL_CMD;
		break;
	case DATA_FORMAT_GESTURE_SPECIAL_DEMO:
		if (ilits->gesture_demo_ctrl == ENABLE) {
			if (ilits->gesture_mode == DATA_FORMAT_GESTURE_INFO)
				len = geture_info_length +
				      P5_X_INFO_HEADER_LENGTH +
				      P5_X_INFO_CHECKSUM_LENGTH;
			else
				len = demo_mode_packet_len +
				      P5_X_INFO_HEADER_LENGTH +
				      P5_X_INFO_CHECKSUM_LENGTH;
		} else {
			if (ilits->gesture_mode == DATA_FORMAT_GESTURE_INFO)
				len = geture_info_length;
			else
				len = P5_X_GESTURE_NORMAL_LENGTH;
		}
		ILI_INFO("Gesture demo mode control = %d\n",
			 ilits->gesture_demo_ctrl);
		ili_hid_ic_func_ctrl("gesture_demo_en",
				     ilits->gesture_demo_ctrl);
		ILI_INFO("knock_en setting\n");
		ili_hid_ic_func_ctrl("knock_en", 0x8);
		break;
	case DATA_FORMAT_DEBUG_LITE_ROI:
		debug_ctrl = DATA_FORMAT_DEBUG_LITE_ROI_CMD;
		ctrl = DATA_FORMAT_DEBUG_LITE_CMD;
		data_type = data[0];
		break;
	case DATA_FORMAT_DEBUG_LITE_WINDOW:
		debug_ctrl = DATA_FORMAT_DEBUG_LITE_WINDOW_CMD;
		ctrl = DATA_FORMAT_DEBUG_LITE_CMD;
		data_type = data[0];
		break;
	case DATA_FORMAT_DEBUG_LITE_AREA:
		if (data == NULL) {
			ILI_ERR("DATA_FORMAT_DEBUG_LITE_AREA error cmd\n");
			return -1;
		}
		debug_ctrl = DATA_FORMAT_DEBUG_LITE_AREA_CMD;
		ctrl = DATA_FORMAT_DEBUG_LITE_CMD;

		if (ilits->chip->core_ver >= CORE_VER_1700) {
			data_type = data[0];
			cmd[4] = data[1];
			cmd[5] = data[2];
			cmd[6] = data[3];
			cmd[7] = data[4];
		} else {
			cmd[3] = data[0];
			cmd[4] = data[1];
			cmd[5] = data[2];
			cmd[6] = data[3];
		}

		break;
	default:
		ILI_ERR("Unknow TP data format\n");
		return -1;
	}

	if (ctrl == DATA_FORMAT_DEBUG_LITE_CMD) {
		len = P5_X_DEBUG_LITE_LENGTH;

		if (ilits->chip->core_ver >= CORE_VER_1700) {
			cmd[0] = P5_X_NEW_CONTROL_FORMAT;
			cmd[1] = ctrl;
			cmd[2] = data_type;
			cmd[3] = debug_ctrl;
			ret = ilits->wrapper(cmd, 11, NULL, 0, ON, OFF);
		} else {
			cmd[0] = P5_X_MODE_CONTROL;
			cmd[1] = ctrl;
			cmd[2] = debug_ctrl;
			ret = ilits->wrapper(cmd, 10, NULL, 0, ON, OFF);
		}

		if (ret < 0) {
			ILI_ERR("switch to format %d failed\n", format);
			ili_hid_switch_tp_mode(P5_X_FW_AP_MODE);
		}

	} else if ((atomic_read(&ilits->tp_reset) == END) &&
		   (tp_mode == P5_X_FW_AP_MODE ||
		    format == DATA_FORMAT_GESTURE_DEMO ||
		    format == DATA_FORMAT_GESTURE_DEBUG ||
		    format == DATA_FORMAT_DEMO)) {
		if (ilits->chip->core_ver >= CORE_VER_1700) {
			cmd[0] = P5_X_NEW_CONTROL_FORMAT;
			cmd[1] = ctrl;
			cmd[2] = data_type;
			ret = ilits->wrapper(cmd, 3, NULL, 0, ON, OFF);
		} else {
			cmd[0] = P5_X_MODE_CONTROL;
			cmd[1] = ctrl;
			ret = ilits->wrapper(cmd, 2, NULL, 0, ON, OFF);
		}

		if (ret < 0) {
			ILI_ERR("switch to format %d failed\n", format);
			ili_hid_switch_tp_mode(P5_X_FW_AP_MODE);
		}
	} else if (tp_mode == P5_X_FW_GESTURE_MODE) {
		/*set gesture symbol*/
		ili_hid_set_gesture_symbol();
		if (send) {
			if (ilits->proxmity_face == true)
				ret = ili_hid_ic_func_ctrl("lpwg", 0x20);
			else
				ret = ili_hid_ic_func_ctrl("lpwg", ctrl);
			if (ret < 0)
				ILI_ERR("write gesture mode failed\n");
		}
	}

	ilits->tp_data_format = format;
	ilits->tp_data_len = len;
	ILI_INFO("TP mode = %d, format = %d, len = %d\n", tp_mode,
		 ilits->tp_data_format, ilits->tp_data_len);

	if (ilits->PenType == POSITION_PEN_TYPE_ON) {
		ILI_INFO("Pen Type = 0x%X, Max Touch Num = %d, Pen Data Mode = %d\n",
			ilits->pen_info.report_type, ilits->touch_num,
			ilits->pen_info.pen_data_mode);
	}
	return ret;
}

int ili_hid_set_pen_data_len(u8 header, u8 ctrl, u8 type)
{
	int len = 0;
	u8 ret = 0, self_key = 2;
	u8 cmd[3] = { 0 };

	cmd[0] = header;
	cmd[1] = ctrl;
	cmd[2] = type;
	ret = ilits->wrapper(cmd, 3, NULL, 0, ON, OFF);
	ILI_INFO("write = 0x%X, 0x%X, 0x%X\n", cmd[0], cmd[1], cmd[2]);

	if (ilits->rib.nReportResolutionMode == POSITION_LOW_RESOLUTION) {
		len = P5_X_DEBUG_LOW_RESOLUTION_FINGER_DATA_LENGTH;
	} else if (ilits->rib.nReportResolutionMode == POSITION_HIGH_RESOLUTION) {
		len = P5_X_DEBUG_HIGH_RESOLUTION_FINGER_DATA_LENGTH;
	}

	switch (ilits->pen_info.report_type) {
	case P5_X_ONLY_PEN_TYPE:
		ilits->cdc_data_len = (ilits->stx + ilits->srx) * 2;
		len += ilits->cdc_data_len + P5_X_PEN_DATA_LEN;

		if (type == P5_X_FW_RAW_DATA_MODE) {
			len += ((ilits->pen_info_block.nPxRaw * ilits->xch_num * 2) +
				(ilits->pen_info_block.nPyRaw * ilits->ych_num * 2)) * 2 + 4;
			ilits->pen_info.pen_data_mode = PEN_ONLY_RAW_DATA_MODE;
		} else {
			len += ((ilits->pen_info_block.nPxVa * ilits->xch_num * 2) +
				(ilits->pen_info_block.nPyVa * ilits->ych_num * 2)) * 2 + 4;
			ilits->pen_info.pen_data_mode = PEN_ONLY_SIGNAL_MODE;
		}
		break;
	case P5_X_ONLY_HAND_TYPE:
		if (type == P5_X_FW_RAW_DATA_MODE)
			ilits->pen_info.pen_data_mode = HAND_ONLY_RAW_DATA_MODE;
		else
			ilits->pen_info.pen_data_mode = HAND_ONLY_SIGNAL_MODE;

		ilits->cdc_data_len = (ilits->xch_num * ilits->ych_num * 2) +
				      ((ilits->stx + ilits->srx) * 2) + (2 * self_key);
		len += ilits->cdc_data_len;

		if (ilits->rib.nCustomerType != ilits->customertype_off)
			len += P5_X_CUSTOMER_LENGTH;
		if (ilits->PenType == POSITION_PEN_TYPE_ON)
			len += P5_X_PEN_DATA_LEN;
		break;

	default:
		ILI_ERR("PEN Type = 0x%X, CMD Error\n",
			ilits->pen_info.report_type);
		break;
	}
	len += P5_X_OTHER_DATA_LEN + P5_X_INFO_CHECKSUM_LENGTH; //17
	ilits->tp_data_len = len;
	ILI_INFO("Packet Length = %d, Pen Type = 0x%X, Pen Data Mode = %d\n",
		 ilits->tp_data_len, ilits->pen_info.report_type,
		 ilits->pen_info.pen_data_mode);

	return ret;
}

int ili_hid_cascade_reset_ctrl(int reset_mode, bool enter_ice)
{
	int ret = 0;

	ili_hid_wdt_reset_status(enter_ice, false);

#if (TDDI_INTERFACE == BUS_I2C)
	ilits->i2c_addr_change = CLIENT;

	if (ili_hid_reset_ctrl(reset_mode) < 0) {
		ILI_ERR("TP Reset failed during init\n");
		ret = -EFW_REST;
	}

	ilits->i2c_addr_change = MASTER;
	if (reset_mode != TP_IC_CODE_RST)
		atomic_set(&ilits->ice_stat, ENABLE);

	if (ili_hid_reset_ctrl(reset_mode) < 0) {
		ILI_ERR("TP Reset failed during init\n");
		ret = -EFW_REST;
	}

#else
	if (ili_hid_reset_ctrl(reset_mode) < 0) {
		ILI_ERR("TP Reset failed during init\n");
		ret = -EFW_REST;
	}
#endif

	if (reset_mode == TP_IC_CODE_RST) {
		ili_hid_ice_mode_ctrl_by_mode(DISABLE, OFF, BOTH);
		// ili_hid_cascade_sync_ctrl(true);
	}

	return ret;
}

int ili_hid_reset_ctrl(int mode)
{
	int ret = 0;

	atomic_set(&ilits->tp_reset, START);

#if MULTI_REPORT_RATE
	if (mode == TP_IC_WHOLE_RST_WITH_FLASH || mode == TP_HW_RST_ONLY) {
		if (ili_hid_ic_report_rate_register_set() < 0)
			ILI_ERR("write report rate password failed\n");
	}
#endif

	switch (mode) {
	case TP_IC_CODE_RST:
		ILI_INFO("TP IC Code RST \n");
		ret = ili_hid_ic_code_reset(OFF);
		ilits->pll_clk_wakeup = false;
		if (ret < 0)
			ILI_ERR("IC Code reset failed\n");
		break;
	case TP_IC_WHOLE_RST_WITH_FLASH:
		ILI_INFO("TP IC whole RST\n");
		ret = ili_hid_ic_whole_reset(OFF, ON);
		if (ret < 0)
			ILI_ERR("IC whole reset failed\n");
		ilits->pll_clk_wakeup = true;
		break;
	case TP_IC_WHOLE_RST_WITHOUT_FLASH:
		ILI_INFO("TP IC whole RST without flash\n");
		ret = ili_hid_ic_whole_reset(OFF, OFF);
		if (ret < 0)
			ILI_ERR("IC whole reset without flash failed\n");
		ilits->pll_clk_wakeup = false;
		break;
	case TP_HW_RST_ONLY:
		ILI_INFO("TP HW RST\n");
		ili_hid_tp_reset();
		ilits->pll_clk_wakeup = true;
		break;
	default:
		ILI_ERR("Unknown reset mode, %d\n", mode);
		ret = -EINVAL;
		break;
	}
	/*
	 * Since OTP must be folloing with reset, except for code rest,
	 * the stat of ice mode should be set as 0.
	 */
	if (mode != TP_IC_CODE_RST)
		atomic_set(&ilits->ice_stat, DISABLE);

	if (ili_hid_set_tp_data_len(DATA_FORMAT_DEMO, false, NULL) < 0)
		ILI_ERR("Failed to set tp data length\n");

	ilits->client_pc_latch = 0;
	ilits->master_pc_latch = 0;

	atomic_set(&ilits->tp_reset, END);
	return ret;
}

static int ilitek_get_tp_module(void)
{
	/*
	 * TODO: users should implement this function
	 * if there are various tp modules been used in projects.
	 */
	get_lcm_id();
	return lcm_vendor_id;
}

static void ili_update_tp_module_info(void)
{
	int module;

	module = ilitek_get_tp_module();

	switch (module) {
	case MODEL_BOE:
		ilits->md_name = "BOE";
		ilits->md_fw_filp_path = BOE_FW_FILP_PATH;
		ilits->md_fw_rq_path = BOE_FW_REQUEST_PATH;
		ilits->md_ini_path = BOE_INI_NAME_PATH;
		ilits->md_ini_rq_path = BOE_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_BOE;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_BOE);
		break;
	case MODEL_HSD:
		ilits->md_name = "HSD";
		ilits->md_fw_filp_path = HSD_FW_FILP_PATH;
		ilits->md_fw_rq_path = HSD_FW_REQUEST_PATH;
		ilits->md_ini_path = HSD_INI_NAME_PATH;
		ilits->md_ini_rq_path = HSD_INI_REQUEST_PATH;
		ilits->md_fw_ili = CTPM_FW_HSD;
		ilits->md_fw_ili_size = sizeof(CTPM_FW_HSD);
		break;
	default:
		ILI_ERR("Couldn't find any tp modules");
		break;
	}

	ILI_INFO("Found %s module: ini path = %s, fw path = (%s, %s, %d)\n",
		 ilits->md_name, ilits->md_ini_path, ilits->md_fw_filp_path,
		 ilits->md_fw_rq_path, ilits->md_fw_ili_size);

	ilits->tp_module = module;
}

int ili_hid_tddi_init(void)
{
#if (BOOT_FW_UPDATE | HOST_DOWN_LOAD)
	struct task_struct *fw_boot_th;
#endif
	ilits->ili_upg_wakelock = wakeup_source_register(NULL, "ili_upg_wakelock");
	if (!ilits->ili_upg_wakelock) {
		ILI_ERR("wakeup source request failed\n");
		return -EINVAL;
	}

	ILI_INFO("driver version = %s\n", DRIVER_VERSION);

	mutex_init(&ilits->touch_mutex);
	mutex_init(&ilits->debug_mutex);
	mutex_init(&ilits->debug_read_mutex);
	init_waitqueue_head(&(ilits->inq));
	spin_lock_init(&ilits->irq_spin);
	init_completion(&ilits->esd_done);

	atomic_set(&ilits->ice_stat, DISABLE);
	atomic_set(&ilits->tp_reset, END);
	atomic_set(&ilits->fw_stat, END);
	atomic_set(&ilits->mp_stat, DISABLE);
	atomic_set(&ilits->tp_sleep, END);
	atomic_set(&ilits->cmd_int_check, DISABLE);
	atomic_set(&ilits->esd_stat, END);
	atomic_set(&ilits->tp_sw_mode, END);
	atomic_set(&ilits->ignore_report, END);

	ilits->pen_serial = INVALID_PEN_SERIAL;
	ilits->hid_report_irq = false;

#if IS_ENABLED(CONFIG_HID_TOUCH_METRICS)
	touch_metrics_data.fw_version = DEFAULT_FW_VERSION;
	touch_metrics_data.bootup_status = BOOT_UP_INIT_STATUS;
	touch_metrics_data.fw_upgrade_result = FW_UPGRADE_STATE_INIT;
#endif

	ili_hid_ic_init();
	ilitek_tddi_wq_init();

	/* Must do hw reset once in first time for work normally if tp reset is avaliable */
#if !TDDI_RST_BIND
	if (ili_hid_reset_ctrl(ilits->reset) < 0)
		ILI_ERR("TP Reset failed during init\n");
#endif

	ilits->demo_debug_info[0] = ili_hid_demo_debug_info_id0;
	ilits->tp_data_format = DATA_FORMAT_DEMO;
	ilits->boot = false;

	/*
	 * This status of ice enable will be reset until process of fw upgrade runs.
	 * it might cause unknown problems if we disable ice mode without any
	 * codes inside touch ic.
	 */
#if (TDDI_INTERFACE == BUS_I2C && ENABLE_CASCADE)
	ili_hid_cascade_sync_ctrl(false);
	ili_hid_ice_mode_ctrl_by_mode(ENABLE, OFF, BOTH);
#elif (TDDI_INTERFACE == BUS_SPI && ENABLE_CASCADE)
	ili_hid_set_bypass_mode();
#else
	if (ili_hid_ice_mode_ctrl(ENABLE, OFF) < 0)
		ILI_ERR("Failed to enable ice mode during ili_hid_tddi_init\n");
#endif

	if (ili_hid_ic_dummy_check() < 0) {
		ILI_ERR("Not found ilitek chip\n");
		return -ENODEV;
	}
#if ENABLE_CASCADE
	if (ili_hid_cascade_ic_get_info(false, false) < 0)
		ILI_ERR("Client Chip info is incorrect\n");
#else
	if (ili_hid_ic_get_info() < 0)
		ILI_ERR("Chip info is incorrect\n");
#endif

	ili_update_tp_module_info();

	ili_hid_node_init();

	ili_hid_fw_read_flash_info(OFF);

#if (BOOT_FW_UPDATE | HOST_DOWN_LOAD)
	fw_boot_th = kthread_run(ili_hid_fw_upgrade_handler, NULL, "ili_fw_boot");
	if (fw_boot_th == (struct task_struct *)ERR_PTR) {
		fw_boot_th = NULL;
		/*WARN_ON(!fw_boot_th);*/
		ILI_ERR("Failed to create fw upgrade thread\n");
	}
#endif

	ili_hid_ic_edge_palm_para_init();

	return 0;
}

void ili_hid_dev_remove(bool flag)
{
	ILI_INFO("remove ilitek dev\n");

	if (!ilits)
		return;

	if (esd_wq != NULL) {
		cancel_delayed_work_sync(&esd_work);
		flush_workqueue(esd_wq);
		destroy_workqueue(esd_wq);
	}
	if (bat_wq != NULL) {
		cancel_delayed_work_sync(&bat_work);
		flush_workqueue(bat_wq);
		destroy_workqueue(bat_wq);
	}

	if (ilits->ili_upg_wakelock)
		wakeup_source_unregister(ilits->ili_upg_wakelock);

	kfree(ilits->tr_buf);
	kfree(ilits->gcoord);

	ili_sysfs_remove_group(ilits);
}

int ili_hid_dev_init(struct ilitek_hwif_info *hwif, struct i2c_client *client)
{
	ILI_INFO("TP Interface: %s\n",
		 (hwif->bus_type == BUS_I2C) ? "I2C" : "SPI");
	return ili_hid_interface_dev_init(hwif, client);
}
