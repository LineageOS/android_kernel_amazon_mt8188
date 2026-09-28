// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   reset_hif.c
 *  \brief  reset hif
 *
 *  This file contains all implementations of reset module
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */


/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>	/* sdio_readl(), etc */
#include <linux/mmc/host.h>		/* mmc_add_host(), etc */
#include <linux/mmc/sdio_ids.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>

#include "reset.h"
#if CFG_RESETKO_CONN_DYNAMIC_POWER_CTRL && defined(_HIF_PCIE)
#include "pcie-mediatek-gen3.h"
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#ifndef CHIP_RESET_DTS_COMPATIBLE_NAME
#define CHIP_RESET_DTS_COMPATIBLE_NAME		"mediatek,mtk-wifi-reset"
#endif
#ifndef CHIP_RESET_GPIO_PROPERTY_NAME
#define CHIP_RESET_GPIO_PROPERTY_NAME		"reset-gpio-num"
#endif
#ifndef CHIP_RESET_INVERT_PROPERTY_NAME
#define CHIP_RESET_INVERT_PROPERTY_NAME		"invert-ms"
#endif
#ifndef CHIP_RESET_DEFAULT_VAL_PROPERTY_NAME
#define CHIP_RESET_DEFAULT_VAL_PROPERTY_NAME	"default-gpio-val"
#endif
#ifndef CHIP_POWER_DTS_COMPATIBLE_NAME
#define CHIP_POWER_DTS_COMPATIBLE_NAME		"mediatek,mtk-wifi-power"
#endif
#ifndef CHIP_POWER_GPIO_PROPERTY_NAME
#define CHIP_POWER_GPIO_PROPERTY_NAME		"power-gpio-num"
#endif
#ifndef CHIP_POWER_DEFAULT_VAL_PROPERTY_NAME
#define CHIP_POWER_DEFAULT_VAL_PROPERTY_NAME	"default-gpio-val"
#endif
#ifndef RESET_PIN_SET_LOW_TIME
#define RESET_PIN_SET_LOW_TIME			50
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */


/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */


/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */


/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */


/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */


/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
static void dtsGetResetGpioInfo(uint32_t dongle_id, struct device_node *node)
{
#if CFG_RESETKO_SUPPORT_MULTI_CARD
	int i;
#endif
	int i4Status;
	unsigned int gpio_num, default_level, action_level, invert_time;

	if (!node)
		node = of_find_compatible_node(NULL, NULL,
					       CHIP_RESET_DTS_COMPATIBLE_NAME);
	if (!node) {
		MR_Err("[%d] %s: Failed to find dts node: %s\n",
		       dongle_id, __func__, CHIP_RESET_DTS_COMPATIBLE_NAME);
		return;
	}
	if (of_property_read_u32(node, CHIP_RESET_GPIO_PROPERTY_NAME,
				&gpio_num) != 0) {
		MR_Err("[%d] %s: Failed to get gpio_num: %s\n",
		       dongle_id, __func__, CHIP_RESET_GPIO_PROPERTY_NAME);
		return;
	}
	if (of_property_read_u32(node, CHIP_RESET_INVERT_PROPERTY_NAME,
				&invert_time) != 0) {
		MR_Err("[%d] %s: Failed to get invert_time: %s\n",
		       dongle_id, __func__, CHIP_RESET_INVERT_PROPERTY_NAME);
		invert_time = RESET_PIN_SET_LOW_TIME;
	}
	if (of_property_read_u32(node, CHIP_RESET_DEFAULT_VAL_PROPERTY_NAME,
				&default_level) != 0) {
		MR_Err("[%d] %s: Failed to get default_level: %s\n",
		       dongle_id, __func__,
		       CHIP_RESET_DEFAULT_VAL_PROPERTY_NAME);
		default_level = 1;
	}
	default_level = (default_level == 0) ? 0 : 1;
	action_level = (default_level == 0) ? 1 : 0;

	MR_Info("[%d] %s: read wifi reset gpio %d pull %s %dms from dts\n",
		dongle_id, __func__,
		gpio_num, (action_level == 0) ? "down" : "up", invert_time);

	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	resetInfo[dongle_id].resetGpioInfo.gpio_num = gpio_num;
	resetInfo[dongle_id].resetGpioInfo.default_level = default_level;
	resetInfo[dongle_id].resetGpioInfo.action_level = action_level;
	resetInfo[dongle_id].resetGpioInfo.invert_time = invert_time;

#if CFG_RESETKO_SUPPORT_MULTI_CARD
	for (i = 0; i < MAX_DONGLE_NUM; i++) {
		if ((i != dongle_id) &&
		    (gpio_num == resetInfo[i].resetGpioInfo.gpio_num)) {
			MR_Info(
			  "[%d] %s: gpio %d already requested by dongle [%d]\n",
			  dongle_id, __func__, gpio_num, i);
			resetInfo[dongle_id].resetGpioInfo.flag_inited = true;
			return;
		}
	}
#endif

	i4Status = gpio_request(resetInfo[dongle_id].resetGpioInfo.gpio_num,
				"wifi-reset");
	if (i4Status < 0) {
		MR_Err("[%d] %s: gpio %d request failed, ret = %d\n",
		       dongle_id, __func__,
		       resetInfo[dongle_id].resetGpioInfo.gpio_num, i4Status);
		resetInfo[dongle_id].resetGpioInfo.flag_inited = false;
	} else {
		resetInfo[dongle_id].resetGpioInfo.flag_inited = true;
	}
}

static void dtsGetPowerGpioInfo(uint32_t dongle_id, struct device_node *node)
{
#if CFG_RESETKO_SUPPORT_MULTI_CARD
	int i;
#endif
	int i4Status;
	unsigned int gpio_num, default_level, action_level;

	if (!node)
		node = of_find_compatible_node(NULL, NULL,
					       CHIP_POWER_DTS_COMPATIBLE_NAME);
	if (!node) {
		MR_Err("[%d] %s: Failed to find dts node: %s\n",
		       dongle_id, __func__, CHIP_POWER_DTS_COMPATIBLE_NAME);
		return;
	}
	if (of_property_read_u32(node, CHIP_POWER_GPIO_PROPERTY_NAME,
				&gpio_num) != 0) {
		MR_Err("[%d] %s: Failed to get gpio_num: %s\n",
		       dongle_id, __func__, CHIP_POWER_GPIO_PROPERTY_NAME);
		return;
	}
	if (of_property_read_u32(node, CHIP_POWER_DEFAULT_VAL_PROPERTY_NAME,
				&default_level) != 0) {
		MR_Err("[%d] %s: Failed to get default_level: %s\n",
		       dongle_id, __func__,
		       CHIP_POWER_DEFAULT_VAL_PROPERTY_NAME);
		return;
	}
	default_level = (default_level == 0) ? 0 : 1;
	action_level = (default_level == 0) ? 1 : 0;

	MR_Info("[%d] %s: read wifi power gpio %d pull %s from dts\n",
		dongle_id, __func__,
		gpio_num, (action_level == 0) ? "down" : "up");

	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	resetInfo[dongle_id].powerGpioInfo.gpio_num = gpio_num;
	resetInfo[dongle_id].powerGpioInfo.switch_off_level = default_level;
	resetInfo[dongle_id].powerGpioInfo.switch_on_level = action_level;

#if CFG_RESETKO_SUPPORT_MULTI_CARD
	for (i = 0; i < MAX_DONGLE_NUM; i++) {
		if ((i != dongle_id) &&
		    (gpio_num == resetInfo[i].powerGpioInfo.gpio_num)) {
			MR_Info(
			  "[%d] %s: gpio %d already requested by dongle [%d]\n",
			  dongle_id, __func__, gpio_num, i);
			resetInfo[dongle_id].powerGpioInfo.flag_inited = true;
			return;
		}
	}
#endif

	i4Status = gpio_request(resetInfo[dongle_id].powerGpioInfo.gpio_num,
				"wifi-power");
	if (i4Status < 0) {
		MR_Err("[%d] %s: gpio %d request failed, ret = %d\n",
		       dongle_id, __func__,
		       resetInfo[dongle_id].powerGpioInfo.gpio_num, i4Status);
		resetInfo[dongle_id].powerGpioInfo.flag_inited = false;
	} else {
		resetInfo[dongle_id].powerGpioInfo.flag_inited = true;
	}
}

void resetHif_Init(uint32_t dongle_id, struct device_node *node)
{
	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	resetInfo[dongle_id].prSdioHost = NULL;
	resetInfo[dongle_id].isSdioAdded = false;
	resetInfo[dongle_id].pciBusId = INVALID_BUS_ID;
	resetInfo[dongle_id].isPcieAdded = false;

	resetInfo[dongle_id].isResetGpioReleased = true;
	memset(&resetInfo[dongle_id].resetGpioInfo, 0,
		sizeof(struct ResetGpioInfo));
	dtsGetResetGpioInfo(dongle_id, node);

	/*platform HW power is ON after platform finish boot*/
	resetInfo[dongle_id].isPowerSwitchOn = true;
	memset(&resetInfo[dongle_id].powerGpioInfo, 0,
		sizeof(struct PowerGpioInfo));
	dtsGetPowerGpioInfo(dongle_id, node);
}

void resetHif_Uninit(uint32_t dongle_id)
{
#if CFG_RESETKO_SUPPORT_MULTI_CARD
	int i;
#endif
	bool skip;

	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	skip = false;
	if (resetInfo[dongle_id].resetGpioInfo.flag_inited) {
		resetInfo[dongle_id].resetGpioInfo.flag_inited = false;
#if CFG_RESETKO_SUPPORT_MULTI_CARD
		for (i = 0; i < MAX_DONGLE_NUM; i++) {
			if ((resetInfo[i].resetGpioInfo.flag_inited == true) &&
			    (resetInfo[dongle_id].resetGpioInfo.gpio_num ==
			     resetInfo[i].resetGpioInfo.gpio_num)) {
				skip = true;
				break;
			}
		}
#endif
		if (!skip)
			gpio_free(resetInfo[dongle_id].resetGpioInfo.gpio_num);
	}

	skip = false;
	if (resetInfo[dongle_id].powerGpioInfo.flag_inited) {
		resetInfo[dongle_id].powerGpioInfo.flag_inited = false;
#if CFG_RESETKO_SUPPORT_MULTI_CARD
		for (i = 0; i < MAX_DONGLE_NUM; i++) {
			if ((resetInfo[i].powerGpioInfo.flag_inited == true) &&
			    (resetInfo[dongle_id].powerGpioInfo.gpio_num ==
			     resetInfo[i].powerGpioInfo.gpio_num)) {
				skip = true;
				break;
			}
		}
#endif
		if (!skip)
			gpio_free(resetInfo[dongle_id].powerGpioInfo.gpio_num);
	}
}

enum ReturnStatus resetHif_UpdateSdioHost(uint32_t dongle_id, void *data)
{
	struct mmc_host *host;
	struct sdio_func *func = (struct sdio_func *)data;

	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return RESET_RETURN_STATUS_FAIL;
	}

	if (!func || !func->card)
		return RESET_RETURN_STATUS_FAIL;

	resetInfo[dongle_id].isSdioAdded = true;
	host = func->card->host;
	if (resetInfo[dongle_id].prSdioHost != (void *)host) {
		MR_Warn("[0] %s: sdio host is updated as %p\n", __func__, host);
		resetInfo[dongle_id].prSdioHost = (void *)host;
		dump_stack();
	}
	MR_Info("[0] %s: update sdio host as %p\n", __func__,
		resetInfo[dongle_id].prSdioHost);

	return RESET_RETURN_STATUS_SUCCESS;
}

void resetHif_SdioRemoveHost(uint32_t dongle_id)
{
	struct mmc_host *prSdioHost;

	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return;
	}

	prSdioHost = (struct mmc_host *)resetInfo[dongle_id].prSdioHost;
	if (!prSdioHost) {
		MR_Err("[0] %s: sdio host is NULL\n", __func__);
		return;
	}
	if (!resetInfo[dongle_id].isSdioAdded) {
		MR_Err("[0] %s: sdio is already removed\n", __func__);
		return;
	}
	prSdioHost->rescan_entered = 0;
	MR_Warn("[0] %s: mmc_remove_host\n", __func__);
	mmc_remove_host(prSdioHost);
	resetInfo[dongle_id].isSdioAdded = false;
}

void resetHif_SdioAddHost(uint32_t dongle_id)
{
	struct mmc_host *prSdioHost;

	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return;
	}

	prSdioHost = (struct mmc_host *)resetInfo[dongle_id].prSdioHost;
	if (!prSdioHost) {
		MR_Err("[0] %s: sdio host is NULL\n", __func__);
		return;
	}
	if (resetInfo[dongle_id].isSdioAdded) {
		MR_Err("[0] %s: sdio is already added\n", __func__);
		return;
	}
	prSdioHost->rescan_entered = 0;
	MR_Warn("[0] %s: mmc_add_host\n", __func__);
	mmc_add_host(prSdioHost);
	resetInfo[dongle_id].isSdioAdded = true;
}

bool resetHif_isSdioAdded(uint32_t dongle_id)
{
	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return false;
	}
	return resetInfo[dongle_id].isSdioAdded;
}

enum ReturnStatus resetHif_UpdatePcieHost(uint32_t dongle_id, void *data)
{
	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return RESET_RETURN_STATUS_FAIL;
	}

	resetInfo[dongle_id].isPcieAdded = true;
	resetInfo[dongle_id].pciBusId = *((uint32_t *)data);
	MR_Info("[0] %s: update pcie host as 0x%x\n", __func__,
		resetInfo[dongle_id].pciBusId);

	return RESET_RETURN_STATUS_SUCCESS;
}

void resetHif_PcieRemoveHost(uint32_t dongle_id)
{
	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return;
	}

	if (resetInfo[dongle_id].pciBusId == INVALID_BUS_ID) {
		MR_Err("[0] %s: pci host is invalid\n", __func__);
		return;
	}
	if (!resetInfo[dongle_id].isPcieAdded) {
		MR_Err("[0] %s: pci host is already removed\n", __func__);
		return;
	}
#if CFG_RESETKO_CONN_DYNAMIC_POWER_CTRL
	MR_Warn("[0] %s: mtk_pcie_remove_port\n", __func__);
	mtk_pcie_remove_port(resetInfo[dongle_id].pciBusId >> 16);
#endif
	resetInfo[dongle_id].isPcieAdded = false;
}

void resetHif_PcieAddHost(uint32_t dongle_id)
{
	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return;
	}

	if (resetInfo[dongle_id].pciBusId == INVALID_BUS_ID) {
		MR_Err("[0] %s: pci host is invalid\n", __func__);
		return;
	}

	if (resetInfo[dongle_id].isPcieAdded) {
		MR_Err("[0] %s: pci host is already added\n", __func__);
		return;
	}
#if CFG_RESETKO_CONN_DYNAMIC_POWER_CTRL
	MR_Warn("[0] %s: mtk_pcie_probe_port\n", __func__);
	mtk_pcie_probe_port(resetInfo[dongle_id].pciBusId >> 16);
#endif
	resetInfo[dongle_id].isPcieAdded = true;
}

bool resetHif_isPcieAdded(uint32_t dongle_id)
{
	if (dongle_id >= MAX_DONGLE_NUM) {
		MR_Err("[0] %s: unknown dongle_id %d\n", __func__, dongle_id);
		return false;
	}
	return resetInfo[dongle_id].isPcieAdded;
}

static int resetHif_GpioOutput(unsigned int gpo, unsigned int val)
{
#if ((CFG_CHIP_RESET_USE_MSTAR_GPIO_API == 1) && (CFG_ENABLE_GKI_SUPPORT != 1))
	typedef void (*gpioMstarFunc)(uint32_t);
	gpioMstarFunc pFuncSetLow = NULL;
	gpioMstarFunc pFuncSetHigh = NULL;
	char *func_name_L = "MDrv_GPIO_Set_Low";
	char *func_name_H = "MDrv_GPIO_Set_High";
	int ret = -EIO;

	if (val) {
		pFuncSetHigh = (gpioMstarFunc)__symbol_get(func_name_H);
		if (pFuncSetHigh) {
			pFuncSetHigh(gpo);
			__symbol_put(func_name_H);
			ret = 0;
		}
	} else {
		pFuncSetLow = (gpioMstarFunc)__symbol_get(func_name_L);
		if (pFuncSetLow) {
			pFuncSetLow(gpo);
			__symbol_put(func_name_L);
			ret = 0;
		}
	}
	return ret;
#else
	return gpio_direction_output(gpo, val);
#endif

}

void resetHif_ResetGpioPull(uint32_t dongle_id)
{
	int i4Status;

	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	if (!resetInfo[dongle_id].isResetGpioReleased) {
		MR_Err("[%d] %s: reset gpio is already pulled\n",
			dongle_id, __func__);
		return;
	}
	if (!resetInfo[dongle_id].resetGpioInfo.flag_inited) {
		MR_Err("[%d] %s: reset gpio is unknown\n",
			dongle_id, __func__);
		return;
	}

	i4Status = resetHif_GpioOutput(
			       resetInfo[dongle_id].resetGpioInfo.gpio_num,
			       resetInfo[dongle_id].resetGpioInfo.action_level);
	if (i4Status < 0) {
		MR_Err("[%d] %s: gpio %d set output %d failed, ret = %d\n",
		       dongle_id, __func__,
		       resetInfo[dongle_id].resetGpioInfo.gpio_num,
		       resetInfo[dongle_id].resetGpioInfo.action_level,
		       i4Status);
		return;
	}

	MR_Warn("[%d] %s: pull reset gpio (%d, %d)\n", dongle_id, __func__,
		resetInfo[dongle_id].resetGpioInfo.gpio_num,
		resetInfo[dongle_id].resetGpioInfo.action_level);
	resetInfo[dongle_id].isResetGpioReleased = false;
}

void resetHif_ResetGpioRelease(uint32_t dongle_id)
{
	int i4Status;

	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	if (resetInfo[dongle_id].isResetGpioReleased) {
		MR_Err("[%d] %s: reset gpio is already released\n",
			dongle_id, __func__);
		return;
	}
	if (!resetInfo[dongle_id].resetGpioInfo.flag_inited) {
		MR_Err("[%d] %s: reset gpio is unknown\n", dongle_id, __func__);
		return;
	}

	i4Status = resetHif_GpioOutput(
			      resetInfo[dongle_id].resetGpioInfo.gpio_num,
			      resetInfo[dongle_id].resetGpioInfo.default_level);
	if (i4Status < 0) {
		MR_Err("[%d] %s: gpio %d set output %d failed, ret = %d\n",
		       dongle_id, __func__,
		       resetInfo[dongle_id].resetGpioInfo.gpio_num,
		       resetInfo[dongle_id].resetGpioInfo.default_level,
		       i4Status);
		return;
	}

	MR_Warn("[%d] %s: release reset gpio (%d, %d)\n", dongle_id, __func__,
		resetInfo[dongle_id].resetGpioInfo.gpio_num,
		resetInfo[dongle_id].resetGpioInfo.default_level);
	resetInfo[dongle_id].isResetGpioReleased = true;
}

bool resetHif_isResetGpioReleased(uint32_t dongle_id)
{
	if (dongle_id >= MAX_DONGLE_NUM)
		return true;
	return resetInfo[dongle_id].isResetGpioReleased;
}

void resetHif_PowerGpioSwitchOn(uint32_t dongle_id)
{
	int i4Status;

	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	if (resetInfo[dongle_id].isPowerSwitchOn) {
		MR_Err("[%d] %s: power is already switched on\n",
			dongle_id, __func__);
		return;
	}
	if (!resetInfo[dongle_id].powerGpioInfo.flag_inited) {
		MR_Err("[%d] %s: power gpio is unknown\n", dongle_id, __func__);
		return;
	}

	i4Status = resetHif_GpioOutput(
			    resetInfo[dongle_id].powerGpioInfo.gpio_num,
			    resetInfo[dongle_id].powerGpioInfo.switch_on_level);
	if (i4Status < 0) {
		MR_Err("[%d] %s: gpio %d set output %d failed, ret = %d\n",
		       dongle_id, __func__,
		       resetInfo[dongle_id].powerGpioInfo.gpio_num,
		       resetInfo[dongle_id].powerGpioInfo.switch_on_level,
		       i4Status);
		return;
	}

	MR_Warn("[%d] %s: power switch on (%d, %d)\n", dongle_id, __func__,
		resetInfo[dongle_id].powerGpioInfo.gpio_num,
		resetInfo[dongle_id].powerGpioInfo.switch_on_level);
	resetInfo[dongle_id].isPowerSwitchOn = true;
}

void resetHif_PowerGpioSwitchOff(uint32_t dongle_id)
{
	int i4Status;

	if (dongle_id >= MAX_DONGLE_NUM)
		return;

	if (!resetInfo[dongle_id].isPowerSwitchOn) {
		MR_Err("[%d] %s: power is already switched off\n",
			dongle_id, __func__);
		return;
	}
	if (!resetInfo[dongle_id].powerGpioInfo.flag_inited) {
		MR_Err("[%d] %s: power gpio is unknown\n", dongle_id, __func__);
		return;
	}

	i4Status = resetHif_GpioOutput(
			   resetInfo[dongle_id].powerGpioInfo.gpio_num,
			   resetInfo[dongle_id].powerGpioInfo.switch_off_level);
	if (i4Status < 0) {
		MR_Err("[%d] %s: gpio %d set output %d failed, ret = %d\n",
		       dongle_id, __func__,
		       resetInfo[dongle_id].powerGpioInfo.gpio_num,
		       resetInfo[dongle_id].powerGpioInfo.switch_off_level,
		       i4Status);
		return;
	}

	MR_Warn("[%d] %s: power switch off (%d, %d)\n", dongle_id, __func__,
		resetInfo[dongle_id].powerGpioInfo.gpio_num,
		resetInfo[dongle_id].powerGpioInfo.switch_off_level);
	resetInfo[dongle_id].isPowerSwitchOn = false;
}

bool resetHif_isPowerSwitchOn(uint32_t dongle_id)
{
	if (dongle_id >= MAX_DONGLE_NUM)
		return true;
	return resetInfo[dongle_id].isPowerSwitchOn;
}

bool resetHif_isReuseSomeHifInfo(uint32_t id_1, uint32_t id_2)
{
	if (id_1 >= MAX_DONGLE_NUM || id_2 >= MAX_DONGLE_NUM)
		return false;

	if (resetInfo[id_1].resetGpioInfo.flag_inited &&
	    resetInfo[id_2].resetGpioInfo.flag_inited &&
	    (resetInfo[id_1].resetGpioInfo.gpio_num ==
	     resetInfo[id_2].resetGpioInfo.gpio_num))
		return true;

	if (resetInfo[id_1].powerGpioInfo.flag_inited &&
	    resetInfo[id_2].powerGpioInfo.flag_inited &&
	    (resetInfo[id_1].powerGpioInfo.gpio_num ==
	     resetInfo[id_2].powerGpioInfo.gpio_num))
		return true;

	if (resetInfo[id_1].prSdioHost != NULL &&
	    resetInfo[id_1].prSdioHost == resetInfo[id_2].prSdioHost)
		return true;

	if (resetInfo[id_1].pciBusId != INVALID_BUS_ID &&
	    resetInfo[id_1].pciBusId == resetInfo[id_2].pciBusId)
		return true;

	return false;
}
