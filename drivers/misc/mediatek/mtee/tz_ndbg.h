/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __TZ_NDBG_T__
#define __TZ_NDBG_T__

/* enable ndbg implementation */
/* #ifndef CONFIG_TRUSTY */ /* disable ndbg if trusty is on for now. */
#if 0 /* fix me, gki build error for module double init. */
#define CC_ENABLE_NDBG
#endif

#endif				/* __TZ_NDBG_T__ */
