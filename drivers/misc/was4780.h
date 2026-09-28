/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2022, The Linux Foundation. All rights reserved.
 */
#ifndef was4780_I2C_H
#define was4780_I2C_H

#include <linux/of.h>
#include <linux/notifier.h>

#if IS_ENABLED(CONFIG_WAS4780_AUDIO_SWITCH)
enum was_function {
	WAS_MIC_GND_SWAP,
	WAS_USBC_ORIENTATION_CC1,
	WAS_USBC_ORIENTATION_CC2,
	WAS_USBC_DISPLAYPORT_DISCONNECTED,
	WAS_EARPIECE_TYPE,
	WAS_EVENT_MAX,
};

enum EARPIECE_TYPE {
	NO_EARPIECE	= 0,
	THREE_POLE,
	CTIA,
	OMTP,
	TYPE_MAX
};

enum NOTIFY_FLAG_BIT_TYPE {
	NOTIFY_REGISTER_BIT_FOR_ACCDET = 0,
	NOTIFY_REGISTER_BIT_MAX
};

enum NOTIFY_REGISTER_TYPE {
	NOTIFY_UNREGISTER = 0,
	NOTIFY_REGISTER
};

int was4780_switch_event(struct device_node *node,
			 enum was_function event, unsigned int param);
int was4780_reg_notifier(struct notifier_block *nb,
			 struct device_node *node, enum NOTIFY_FLAG_BIT_TYPE notify_flag_bit);
int was4780_unreg_notifier(struct notifier_block *nb,
			   struct device_node *node, enum NOTIFY_FLAG_BIT_TYPE notify_flag_bit);
#else
static inline int was4780_switch_event(struct device_node *node,
				       enum was_function event)
{
	return 0;
}

static inline int was4780_reg_notifier(struct notifier_block *nb,
				       struct device_node *node, enum NOTIFY_FLAG_BIT_TYPE notify_flag_bit)
{
	return 0;
}

static inline int was4780_unreg_notifier(struct notifier_block *nb,
					 struct device_node *node, enum NOTIFY_FLAG_BIT_TYPE notify_flag_bit)
{
	return 0;
}
#endif /* CONFIG_WAS4780_AUDIO_SWITCH */

#endif /* was4780_I2C_H */

