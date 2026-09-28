// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_basic_charger.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>

#include <tcpm.h>
#include "mtk_charger.h"
#include "battery_metrics.h"

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

void select_cv(struct mtk_charger *info)
{
	u32 constant_voltage;

	constant_voltage = mtk_get_battery_cv(info);
	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0)
			constant_voltage = info->sw_jeita.cv;

	if (info->custom_charging_cv != -1) {
		if (info->custom_charging_cv < constant_voltage) {
			constant_voltage = info->custom_charging_cv;
			chr_err("%s: top_off_mode CV:%duV", __func__,
				constant_voltage);
		}
	}

	if (info->custom_charging_mode != 0)
		if (constant_voltage > DEFAULT_DEMO_CHARGING_CV)
			constant_voltage = DEFAULT_DEMO_CHARGING_CV;

	info->setting.cv = constant_voltage;
}

static bool is_typec_adapter(struct mtk_charger *info)
{
	int rp;

	/* For adapter power detection to determine current via Rp */
	if (!info->tcpc) {
		info->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!info->tcpc) {
			pr_err("%s: Not found tcpc device\n", __func__);
			return false;
		}
	}

	if (info->power_detection.en &&
		(tcpm_inquire_typec_remote_rp_curr(info->tcpc) == 3000 ||
		tcpm_inquire_typec_remote_rp_curr(info->tcpc) == 1500))
		return true;

	rp = adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL);
	if (info->pd_type == MTK_PD_CONNECT_TYPEC_ONLY_SNK &&
			rp != 500 &&
			info->chr_type != POWER_SUPPLY_TYPE_USB &&
			info->chr_type != POWER_SUPPLY_TYPE_USB_CDP)
		return true;

	return false;
}

static bool support_fast_charging(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	int i = 0, state = 0;
	bool ret = false;

	for (i = 0; i < MAX_ALG_NO; i++) {
		alg = info->alg[i];
		if (alg == NULL)
			continue;

		if (info->enable_fast_charging_indicator &&
		    ((alg->alg_id & info->fast_charging_indicator) == 0))
			continue;

		chg_alg_set_current_limit(alg, &info->setting);
		state = chg_alg_is_algo_ready(alg);
		chr_debug("%s %s ret:%s\n", __func__, dev_name(&alg->dev),
			chg_alg_state_to_str(state));

		if (state == ALG_READY || state == ALG_RUNNING) {
			ret = true;
			break;
		}
	}
	return ret;
}

static void check_invalid_charger(struct mtk_charger *info)
{
	bool vdpm = false;
	bool idpm = false;
	int vbus_uv;
	int ret;
	int bak_cur = 0, bak_mivr = 0, bak_iusb = 0, cur_mivr = 0;

	if (info->invalid_charger_det_done)
		return;
	if (get_charger_type(info) == POWER_SUPPLY_TYPE_UNKNOWN)
		return;

	charger_dev_get_charging_current(info->chg1_dev, &bak_cur);
	charger_dev_get_mivr(info->chg1_dev, &bak_mivr);
	charger_dev_get_input_current(info->chg1_dev, &bak_iusb);
	pr_info("%s: bak_cur %d, bak_mivr %d, bak_iusb %d\n",
		__func__, bak_cur, bak_mivr, bak_iusb);

	charger_dev_set_mivr(info->chg1_dev, info->invchg_check_mivr_uv);
	charger_dev_set_input_current(info->chg1_dev, info->invchg_check_iusb_ua);
	charger_dev_set_charging_current(info->chg1_dev,
		info->invchg_check_ichg_ua);
	msleep(200);
	ret = charger_dev_get_indpm_state(info->chg1_dev, &vdpm, &idpm);
	charger_dev_get_mivr(info->chg1_dev, &cur_mivr);
	vbus_uv = get_vbus(info) * 1000;

	charger_dev_set_charging_current(info->chg1_dev, bak_cur);
	charger_dev_set_mivr(info->chg1_dev, bak_mivr);
	charger_dev_set_input_current(info->chg1_dev, bak_iusb);

	pr_info("%s: aicl %d, vbus %d, mivr %d, vdpm %d, idpm %d\n", __func__,
		info->invchg_check_iusb_ua, vbus_uv, cur_mivr, vdpm, idpm);

	if (ret != -ENOTSUPP && vdpm
	    && cur_mivr == info->invchg_check_mivr_uv) {
		pr_err("%s: invalid charger for vdpm set\n", __func__);
		info->invalid_charger_det_weak = true;
	}
	if (vbus_uv < info->invchg_check_mivr_uv && vbus_uv > 0
	    && get_charger_type(info) != POWER_SUPPLY_TYPE_UNKNOWN) {
		pr_err("%s: invalid charger for weak supply\n", __func__);
		info->invalid_charger_det_weak = true;
	}
	if (info->invalid_charger_det_weak) {
		switch_set_state(&info->invalid_charger, 1);
		bat_metrics_invalid_charger();
	}

	info->invalid_charger_det_done = true;
}

#define DETECTION_MIVR_UV 4400000
static int adapter_power_detection_by_ocp(struct mtk_charger *info)
{
	struct charger_data *pdata = &info->chg_data[CHG1_SETTING];
	int bak_cv_uv = 0, bak_iusb_ua = 0, bak_ichg_ua = 0, bak_mivr = 0;
	int cv_uv = info->data.battery_cv;
	int iusb_ua = info->power_detection.aicl_trigger_iusb;
	int ichg_ua = info->power_detection.aicl_trigger_ichg;

	/* Backup IUSB/ICHG/CV setting */
	charger_dev_get_constant_voltage(info->chg1_dev, &bak_cv_uv);
	charger_dev_get_input_current(info->chg1_dev, &bak_iusb_ua);
	charger_dev_get_charging_current(info->chg1_dev, &bak_ichg_ua);
	charger_dev_get_mivr(info->chg1_dev, &bak_mivr);
	pr_info("%s: backup IUSB[%d] ICHG[%d] CV[%d] MIVR[%d]\n",
			__func__, _uA_to_mA(bak_iusb_ua),
			_uA_to_mA(bak_ichg_ua), bak_cv_uv, bak_mivr);

	/* set higher setting to draw more power */
	pr_info("%s: set IUSB[%d] ICHG[%d] CV[%d] MIVR[%d] for detection\n",
			__func__, _uA_to_mA(iusb_ua), _uA_to_mA(ichg_ua),
			cv_uv, DETECTION_MIVR_UV);

	charger_dev_set_mivr(info->chg1_dev, DETECTION_MIVR_UV);
	charger_dev_set_constant_voltage(info->chg1_dev, cv_uv);
	charger_dev_set_input_current(info->chg1_dev, iusb_ua);
	charger_dev_set_charging_current(info->chg1_dev, ichg_ua);
	charger_dev_dump_registers(info->chg1_dev);

	/* Run AICL */
	msleep(50);
	info->chg1_dev->is_adapter_detect = true;
	charger_dev_run_aicl(info->chg1_dev,
			&pdata->input_current_limit_by_aicl);
	info->chg1_dev->is_adapter_detect = false;
	pr_info("%s: aicl result: %d mA\n", __func__,
			_uA_to_mA(pdata->input_current_limit_by_aicl));

	/* Restore IUSB/ICHG/CV setting */
	pr_info("%s: restore IUSB[%d] ICHG[%d] CV[%d] MIVR[%d]\n",
			__func__, _uA_to_mA(bak_iusb_ua),
			_uA_to_mA(bak_ichg_ua), bak_cv_uv, bak_mivr);
	charger_dev_set_mivr(info->chg1_dev, bak_mivr);
	charger_dev_set_constant_voltage(info->chg1_dev, bak_cv_uv);
	charger_dev_set_input_current(info->chg1_dev, bak_iusb_ua);
	charger_dev_set_charging_current(info->chg1_dev, bak_ichg_ua);
	charger_dev_dump_registers(info->chg1_dev);

	return pdata->input_current_limit_by_aicl;
}

static int adapter_power_detection(struct mtk_charger *info)
{
	struct charger_data *pdata = &info->chg_data[CHG1_SETTING];
	struct power_detection_data *det = &info->power_detection;
	int chr_type = info->chr_type;
	int aicl_ua = 0, rp_curr_ma = 0;
	static const char * const category_text[] = {
		"5W", "7.5W", "9W", "12W", "15W"
	};

	if (!det->en)
		return 0;

	if (det->iusb_ua)
		goto skip;

	/* Step 1: Determine Type-C adapter by Rp */
	if (!info->tcpc) {
		info->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!info->tcpc) {
			pr_err("%s: Not found tcpc device\n", __func__);
			return -ENODEV;
		}
	}

	rp_curr_ma = tcpm_inquire_typec_remote_rp_curr(info->tcpc);
	if (rp_curr_ma == 3000) {
		pdata->input_current_limit = 3000000;
		det->iusb_ua = det->adapter_15w_iusb_lim;;
		det->type = ADAPTER_15W;
		goto done;
	} else if (rp_curr_ma == 1500) {
		pdata->input_current_limit = 1500000;
		det->iusb_ua = det->adapter_7p5w_iusb_lim;;
		det->type = ADAPTER_7P5W;
		goto done;
	}

	if (chr_type != POWER_SUPPLY_TYPE_USB_DCP)
		return 0;

	/* Step 2: Run AICL for OCP detection on A2C adapter */
	aicl_ua = adapter_power_detection_by_ocp(info);
	if (aicl_ua < 0) {
		pdata->input_current_limit = det->adapter_12w_iusb_lim;
		det->iusb_ua = det->adapter_12w_iusb_lim;
		det->type = ADAPTER_12W;
		pr_info("%s: CV stage or 15W adapter, keep 12W as default\n",
			__func__);
		goto done;
	}

	/* Step 3: Determine adapter power categroy for 5W/9W/12W */
	if (aicl_ua >= det->adapter_12w_aicl_min) {
		pdata->input_current_limit = det->adapter_12w_iusb_lim;
		det->iusb_ua = det->adapter_12w_iusb_lim;
		det->type = ADAPTER_12W;
	} else if (aicl_ua >= det->adapter_9w_aicl_min) {
		pdata->input_current_limit = det->adapter_9w_iusb_lim;
		det->iusb_ua = det->adapter_9w_iusb_lim;
		det->type = ADAPTER_9W;
	} else {
		pdata->input_current_limit = det->adapter_5w_iusb_lim;
		det->iusb_ua = det->adapter_5w_iusb_lim;
		det->type = ADAPTER_5W;
	}

done:
	bat_metrics_adapter_power(det->type, _uA_to_mA(aicl_ua));
	pr_info("%s: detect %s adapter\n", __func__, category_text[det->type]);
	return 0;

skip:
	pdata->input_current_limit = det->iusb_ua;
	pr_info("%s: alread finish: %d mA\n",
		__func__, _uA_to_mA(pdata->input_current_limit));

	return 0;
}

static bool select_charging_current_limit(struct mtk_charger *info,
	struct chg_limit_setting *setting)
{
	struct charger_data *pdata, *pdata2, *pdata_dvchg, *pdata_dvchg2;
	bool is_basic = false;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret;

	select_cv(info);

	pdata = &info->chg_data[CHG1_SETTING];
	pdata2 = &info->chg_data[CHG2_SETTING];
	pdata_dvchg = &info->chg_data[DVCHG1_SETTING];
	pdata_dvchg2 = &info->chg_data[DVCHG2_SETTING];

	info->setting.charging_current_limit1 = -1;
	info->setting.input_current_limit1 = -1;
	info->setting.charging_current_limit2 = -1;
	info->setting.input_current_limit2 = -1;

	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		is_basic = true;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		is_basic = true;
		goto done;
	}

	if ((info->bootmode == 1) ||
	    (info->bootmode == 5)) {
		pdata->input_current_limit = 200000; /* 200mA */
		is_basic = true;
		goto done;
	}

	if (info->atm_enabled == true
		&& (info->chr_type == POWER_SUPPLY_TYPE_USB ||
		info->chr_type == POWER_SUPPLY_TYPE_USB_CDP)
		) {
		pdata->input_current_limit = 100000; /* 100mA */
		is_basic = true;
		goto done;
	}

	if (info->chr_type == POWER_SUPPLY_TYPE_USB &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
		pdata->input_current_limit =
				info->data.usb_charger_current;
		/* it can be larger */
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
		pdata->input_current_limit =
			info->data.charging_host_charger_current;
		pdata->charging_current_limit =
			info->data.charging_host_charger_current;
		is_basic = true;

	} else if (info->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
		pdata->input_current_limit =
			info->data.ac_charger_input_current;
		pdata->charging_current_limit =
			info->data.ac_charger_current;
		if (info->config == DUAL_CHARGERS_IN_SERIES) {
			pdata2->input_current_limit =
				pdata->input_current_limit;
			pdata2->charging_current_limit = 2000000;
		}
	} else if (info->chr_type == POWER_SUPPLY_TYPE_MAINS &&
	    info->usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
		/* NONSTANDARD_CHARGER */
		pdata->input_current_limit =
			info->data.non_std_ac_charger_current;
		pdata->charging_current_limit =
			info->data.non_std_ac_charger_current;
		is_basic = true;
	} else {
		/*chr_type && usb_type cannot match above, set 500mA*/
		pdata->input_current_limit =
				info->data.usb_charger_current;
		pdata->charging_current_limit =
				info->data.usb_charger_current;
		is_basic = true;
	}

	if (support_fast_charging(info))
		is_basic = false;
	else {
		is_basic = true;
		/* AICL */
		if (info->power_detection.iusb_ua < 3000000)
			charger_dev_run_aicl(info->chg1_dev, &pdata->input_current_limit_by_aicl);

		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
		if (is_typec_adapter(info)) {
			if (tcpm_inquire_typec_remote_rp_curr(info->tcpc) == 3000) {
				pdata->input_current_limit = 3000000;
				pdata->charging_current_limit = 3000000;
			} else if (tcpm_inquire_typec_remote_rp_curr(info->tcpc) == 1500) {
				pdata->input_current_limit = 1500000;
				pdata->charging_current_limit = 2000000;
			} else {
				chr_err("type-C: inquire rp error\n");
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 500000;
			}

			chr_err("type-C:%d current:%d\n",
				info->pd_type,
				tcpm_inquire_typec_remote_rp_curr(info->tcpc));
		}
	}

	check_invalid_charger(info);
	adapter_power_detection(info);

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
			&& info->chr_type == POWER_SUPPLY_TYPE_USB) {
			chr_debug("USBIF & STAND_HOST skip current check\n");
		} else {
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				if (info->data.temp_t0_to_t1_charger_current != -1) {
					if (info->data.temp_t0_to_t1_charger_current <
						pdata->charging_current_limit) {
						pdata->charging_current_limit =
							info->data.temp_t0_to_t1_charger_current;
						info->setting.charging_current_limit1 =
							info->data.temp_t0_to_t1_charger_current;
					}
				} else {
					pr_debug("not set current limit for TEMP_T0_TO_T1\n");
				}
			}

			if (info->sw_jeita.sm == TEMP_T3_TO_T4) {
				if (info->data.temp_t3_to_t4_charger_current != -1) {
					if (info->data.temp_t3_to_t4_charger_current <
						pdata->charging_current_limit) {
						pdata->charging_current_limit =
							info->data.temp_t3_to_t4_charger_current;
						info->setting.charging_current_limit1 =
							info->data.temp_t3_to_t4_charger_current;
					}
				} else {
					pr_debug("not set current limit for TEMP_T3_TO_T4\n");
				}
			}
		}
	}

	sc_select_charging_current(info, pdata);

	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
			pdata->charging_current_limit) {
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
			info->setting.charging_current_limit1 =
					pdata->thermal_charging_current_limit;
		}
	}

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
			pdata->input_current_limit) {
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
			info->setting.input_current_limit1 =
					pdata->input_current_limit;
		}
	}

	if (pdata2->thermal_charging_current_limit != -1) {
		if (pdata2->thermal_charging_current_limit <
			pdata2->charging_current_limit) {
			pdata2->charging_current_limit =
					pdata2->thermal_charging_current_limit;
			info->setting.charging_current_limit2 =
					pdata2->charging_current_limit;
		}
	}

	if (pdata2->thermal_input_current_limit != -1) {
		if (pdata2->thermal_input_current_limit <
			pdata2->input_current_limit) {
			pdata2->input_current_limit =
					pdata2->thermal_input_current_limit;
			info->setting.input_current_limit2 =
					pdata2->input_current_limit;
		}
	}

	if (is_basic == true && pdata->input_current_limit_by_aicl != -1) {
		if (info->vbus_recovery_from_uvlo) {
			info->vbus_recovery_from_uvlo = false;
			chr_err("%s: vbus recovery from UVLO, ignore aicl\n", __func__);
		} else {
			if (pdata->input_current_limit_by_aicl <
				pdata->input_current_limit)
				pdata->input_current_limit =
				pdata->input_current_limit_by_aicl;
		}
	}
	info->setting.input_current_limit_dvchg1 =
		pdata_dvchg->thermal_input_current_limit;

done:

	if (pdata->force_input_current_limit != -1) {
		pdata->input_current_limit = pdata->force_input_current_limit;
		info->setting.input_current_limit1 = pdata->force_input_current_limit;
		info->setting.input_current_limit2 = pdata->force_input_current_limit;
	}

	if (pdata->force_charging_current != -1) {
		pdata->charging_current_limit = pdata->force_charging_current;
		info->setting.charging_current_limit1 = pdata->force_charging_current;
		info->setting.charging_current_limit2 = pdata->force_charging_current;
	}

	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -EOPNOTSUPP && pdata->charging_current_limit < ichg1_min) {
		pdata->charging_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_charging_current is too low %d %d\n",
			pdata->charging_current_limit, ichg1_min);
		is_basic = true;
	}

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -EOPNOTSUPP && pdata->input_current_limit < aicr1_min) {
		pdata->input_current_limit = 0;
		/* For TC_018, pleasae don't modify the format */
		chr_err("min_input_current is too low %d %d\n",
			pdata->input_current_limit, aicr1_min);
		is_basic = true;
	}
	/* For TC_018, pleasae don't modify the format */
	chr_err("m:%d chg1:%d,%d,%d,%d chg2:%d,%d,%d,%d dvchg1:%d sc:%d %d %d type:%d:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d bm:%d b:%d\n",
		info->config,
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		_uA_to_mA(pdata2->thermal_input_current_limit),
		_uA_to_mA(pdata2->thermal_charging_current_limit),
		_uA_to_mA(pdata2->input_current_limit),
		_uA_to_mA(pdata2->charging_current_limit),
		_uA_to_mA(pdata_dvchg->thermal_input_current_limit),
		info->sc.pre_ibat,
		info->sc.sc_ibat,
		info->sc.solution,
		info->chr_type, info->pd_type,
		info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled,
		info->bootmode, is_basic);

	return is_basic;
}

static int do_algorithm(struct mtk_charger *info)
{
	struct chg_alg_device *alg;
	struct charger_data *pdata;
	struct chg_alg_notify notify;
	bool is_basic = true;
	bool chg_done = false;
	int i;
	int ret;
	int val = 0;

	pdata = &info->chg_data[CHG1_SETTING];
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	is_basic = select_charging_current_limit(info, &info->setting);

	if (info->is_chg_done != chg_done) {
		if (chg_done) {
			charger_dev_do_event(info->chg1_dev, EVENT_FULL, 0);
			charger_dev_enable_ir_comp(info->chg1_dev, false);
			chr_err("%s battery full\n", __func__);
		} else {
			charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
			chr_err("%s battery recharge\n", __func__);
		}
	}

	chr_err("%s is_basic:%d\n", __func__, is_basic);
	if (is_basic != true) {
		is_basic = true;
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;

			if (info->enable_fast_charging_indicator &&
			    ((alg->alg_id & info->fast_charging_indicator) == 0))
				continue;

			if (!info->enable_hv_charging ||
			    pdata->charging_current_limit == 0 ||
			    pdata->input_current_limit == 0) {
				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000)
					chg_alg_stop_algo(alg);
				chr_err("%s: alg:%s alg_vbus:%d\n", __func__,
					dev_name(&alg->dev), val);
				continue;
			}

			if (chg_done != info->is_chg_done) {
				if (chg_done) {
					notify.evt = EVT_FULL;
					notify.value = 0;
				} else {
					notify.evt = EVT_RECHARGE;
					notify.value = 0;
				}
				chg_alg_notifier_call(alg, &notify);
				chr_err("%s notify:%d\n", __func__, notify.evt);
			}

			chg_alg_set_current_limit(alg, &info->setting);
			ret = chg_alg_is_algo_ready(alg);

			chr_err("%s %s ret:%s\n", __func__,
				dev_name(&alg->dev),
				chg_alg_state_to_str(ret));

			if (ret == ALG_INIT_FAIL || ret == ALG_TA_NOT_SUPPORT) {
				/* try next algorithm */
				continue;
			} else if (ret == ALG_TA_CHECKING || ret == ALG_DONE ||
						ret == ALG_NOT_READY) {
				/* wait checking , use basic first */
				is_basic = true;
				break;
			} else if (ret == ALG_READY || ret == ALG_RUNNING) {
				is_basic = false;
				//chg_alg_set_setting(alg, &info->setting);
				chg_alg_start_algo(alg);
				break;
			} else {
				chr_err("algorithm ret is error");
				is_basic = true;
			}
		}
	} else {
		if (info->enable_hv_charging != true ||
		    pdata->charging_current_limit == 0 ||
		    pdata->input_current_limit == 0) {
			for (i = 0; i < MAX_ALG_NO; i++) {
				alg = info->alg[i];
				if (alg == NULL)
					continue;

				chg_alg_get_prop(alg, ALG_MAX_VBUS, &val);
				if (val > 5000 && chg_alg_is_algo_running(alg))
					chg_alg_stop_algo(alg);

				chr_err("%s: Stop hv charging. en_hv:%d alg:%s alg_vbus:%d\n",
					__func__, info->enable_hv_charging,
					dev_name(&alg->dev), val);
			}
		}
	}
	info->is_chg_done = chg_done;

	if (is_basic == true) {
		charger_dev_set_input_current(info->chg1_dev,
			pdata->input_current_limit);
		charger_dev_set_charging_current(info->chg1_dev,
			pdata->charging_current_limit);
		charger_dev_set_constant_voltage(info->chg1_dev,
			info->setting.cv);
	}

	if (pdata->input_current_limit == 0 ||
	    pdata->charging_current_limit == 0)
		charger_dev_enable(info->chg1_dev, false);
	else {
		alg = get_chg_alg_by_name("pe5");
		ret = chg_alg_is_algo_ready(alg);
		if (!(ret == ALG_READY || ret == ALG_RUNNING))
			charger_dev_enable(info->chg1_dev, true);
	}

	if (info->chg1_dev != NULL)
		charger_dev_dump_registers(info->chg1_dev);

	if (info->chg2_dev != NULL)
		charger_dev_dump_registers(info->chg2_dev);

	return 0;
}

static int enable_charging(struct mtk_charger *info,
						bool en)
{
	int i;
	struct chg_alg_device *alg;


	chr_err("%s %d\n", __func__, en);

	if (en == false) {
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = info->alg[i];
			if (alg == NULL)
				continue;
			chg_alg_stop_algo(alg);
		}
		charger_dev_enable(info->chg1_dev, false);
		charger_dev_do_event(info->chg1_dev, EVENT_DISCHARGE, 0);
	} else {
		charger_dev_enable(info->chg1_dev, true);
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
	}

	return 0;
}

static int charger_dev_event(struct notifier_block *nb, unsigned long event,
				void *v)
{
	struct chg_alg_device *alg;
	struct chg_alg_notify notify;
	struct mtk_charger *info =
			container_of(nb, struct mtk_charger, chg1_nb);
	struct chgdev_notify *data = v;
	int i;

	chr_err("%s %d\n", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		notify.evt = EVT_FULL;
		notify.value = 0;
	for (i = 0; i < 10; i++) {
		alg = info->alg[i];
		chg_alg_notifier_call(alg, &notify);
	}

		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		pr_info("%s: safety timer timeout\n", __func__);
		bat_metrics_chg_fault(METRICS_FAULT_SAFETY_TIMEOUT);
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		pr_info("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		bat_metrics_chg_fault(METRICS_FAULT_VBUS_OVP);
		break;
	case CHARGER_DEV_NOTIFY_UPDATE:
		pr_info("%s: update charge status\n", __func__);
		_wake_up_charger(info);
		return NOTIFY_DONE;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

static int to_alg_notify_evt(unsigned long evt)
{
	switch (evt) {
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		return EVT_VBUSOVP;
	case CHARGER_DEV_NOTIFY_IBUSOCP:
		return EVT_IBUSOCP;
	case CHARGER_DEV_NOTIFY_IBUSUCP_FALL:
		return EVT_IBUSUCP_FALL;
	case CHARGER_DEV_NOTIFY_BAT_OVP:
		return EVT_VBATOVP;
	case CHARGER_DEV_NOTIFY_IBATOCP:
		return EVT_IBATOCP;
	case CHARGER_DEV_NOTIFY_VBATOVP_ALARM:
		return EVT_VBATOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VBUSOVP_ALARM:
		return EVT_VBUSOVP_ALARM;
	case CHARGER_DEV_NOTIFY_VOUTOVP:
		return EVT_VOUTOVP;
	case CHARGER_DEV_NOTIFY_VDROVP:
		return EVT_VDROVP;
	default:
		return -EINVAL;
	}
}

static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct mtk_charger *info =
		container_of(nb, struct mtk_charger, dvchg1_nb);
	int alg_evt = to_alg_notify_evt(event);

	chr_info("%s %ld", __func__, event);
	if (alg_evt < 0)
		return NOTIFY_DONE;
	mtk_chg_alg_notify_call(info, alg_evt, 0);
	return NOTIFY_OK;
}


int mtk_basic_charger_init(struct mtk_charger *info)
{

	info->algo.do_algorithm = do_algorithm;
	info->algo.enable_charging = enable_charging;
	info->algo.do_event = charger_dev_event;
	info->algo.do_dvchg1_event = dvchg1_dev_event;
	info->algo.do_dvchg2_event = dvchg2_dev_event;
	//info->change_current_setting = mtk_basic_charging_current;
	return 0;
}
