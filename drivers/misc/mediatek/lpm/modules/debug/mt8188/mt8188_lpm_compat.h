/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _LPM_MT8188_COMPAT_H_
#define _LPM_MT8188_COMPAT_H_

/* compatible with lpm_dbg_common_v1.h */

int lpm_dbg_plat_ops_register(struct lpm_dbg_plat_ops *lpm_dbg_plat_ops);

/* compatible for mt8188_lpm_compat.c */

int __init lpm_init(void);

#endif
