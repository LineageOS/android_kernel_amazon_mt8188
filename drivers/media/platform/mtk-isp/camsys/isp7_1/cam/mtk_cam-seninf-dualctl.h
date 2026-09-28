/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_SENINF_DUALCTLH__
#define __MTK_CAM_SENINF_DUALCTLH__

#ifdef MTK_CAM_DUAL_PIPE_DPTZ_SUPPORT
#include "mtk_cam-seninf.h"

#define SENINF_GET	0
#define SENINF_RELEASE	1
#define SENINF_NOCTL	2
#define SENINF_ACTIVE	1
#define SENINF_PASS	0

enum SENINFSESSION {
	SENINF_INIT,
	SENINF_OPEN_CLOSE,
	SENINF_STREAM_ONOFF,
	SENINF_SUSPEND_RESUME,
	SENINF_PORT_ASSIGN,
	SENINF_SESSION_MAX,
};

int
dualSeninfCtl(enum SENINFSESSION session, int op, struct seninf_ctx *ctx);

int
dualSeninfUser(enum SENINFSESSION session, struct seninf_ctx *ctx);

#endif
#endif
