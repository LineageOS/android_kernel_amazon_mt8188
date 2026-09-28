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

#include "touch_module_id.h"

touch_module_info_t touch_module_data = {
	.module_name = "UNKNOWN_MODULE_NAME",
	.module_id = UNKNOWN_MODULE_ID,
};

static unsigned int get_module_id(void)
{
	unsigned int module_id = UNKNOWN_MODULE_ID;
#if IS_ENABLED(CONFIG_IDME)
	module_id = idme_get_lcmid_info();
	if (module_id == UNKNOWN_MODULE_ID)
		TOUCH_METRICS_ERR("get module_id failed.\n");
#else
	TOUCH_METRICS_INFO("%s, idme is not ready.\n", __func__);
#endif
	TOUCH_METRICS_INFO("module id = %d\n", module_id);

	return module_id;
}

void init_touch_module_info(void)
{
	touch_module_data.module_id = get_module_id();

	switch (touch_module_data.module_id) {
	case NT36523N_TG_INX:
		snprintf(touch_module_data.module_name, ENV_SIZE, "%s",
			 "Novatek_NT36523N_TG_INX");
		break;
	case NT36523N_KD_HSD:
		snprintf(touch_module_data.module_name, ENV_SIZE, "%s",
			 "Novatek_NT36523N_KD_HSD");
		break;
	case ILI_9882T_KD_BOE:
		snprintf(touch_module_data.module_name, ENV_SIZE, "%s",
			 "ILITEK_9882T_KD_BOE");
		break;
	case ILI_9882T_TG_HSD:
		snprintf(touch_module_data.module_name, ENV_SIZE, "%s",
			 "ILITEK_9882T_TG_HSD");
		break;
	case FT8205_TG_INX:
		snprintf(touch_module_data.module_name, ENV_SIZE, "%s",
			 "FOCAL_FT8205_TG_INX");
		break;
	case FT8205_KD_BOE:
		snprintf(touch_module_data.module_name, ENV_SIZE, "%s",
			 "FOCAL_FT8205_KD_BOE");
		break;
	default:
		snprintf(touch_module_data.module_name, ENV_SIZE, "%s",
			 "ERROR_MODULE_ID");
		break;
	}
}
