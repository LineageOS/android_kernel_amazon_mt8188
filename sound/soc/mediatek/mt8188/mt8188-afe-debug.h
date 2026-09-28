/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt8188-afe-debug.h  --  Mediatek 8188 afe debug definition
 *
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Bicycle Tsai <bicycle.tsai@mediatek.com>
 *         Trevor Wu <trevor.wu@mediatek.com>
 *         Chun-Chia Chiu <chun-chia.chiu@mediatek.com>
 */

#ifndef _MT8188_AFE_DEBUG_H_
#define _MT8188_AFE_DEBUG_H_

int mt8188_afe_add_debug_controls(struct snd_soc_component *component);
int mt8188_afe_debugfs_init(struct snd_soc_component *component);
void mt8188_afe_debugfs_exit(struct snd_soc_component *component);

#endif
