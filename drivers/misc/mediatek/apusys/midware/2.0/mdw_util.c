// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_ioctl.h"
#include "mdw_cmn.h"

int mdw_util_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_util_args *args = (union mdw_util_args *)data;
	struct mdw_util_in *in = (struct mdw_util_in *)args;
	struct mdw_device *mdev = mpriv->mdev;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);

	switch (args->in.op) {
	case MDW_UTIL_IOCTL_SETPOWER:
		ret = mdev->dev_funcs->set_power(mdev, in->power.dev_type,
			in->power.core_idx, in->power.boost);
		break;

	default:
		mdw_drv_err("invalid util op code(%d)\n", args->in.op);
		ret = -EINVAL;
		break;
	}

	return ret;
}
