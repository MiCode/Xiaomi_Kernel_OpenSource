// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/device.h>       /* needed by device_* */
#include "vcp_ipi_pin.h"
#include "vcp_mbox_layout.h"
#include "vcp_vcpctl.h"

/*
 * A device node to send commands to vcp wit unified interface
 * @magic:	should be 666
 * @type:	a class for different types of commands
 * @op:		the operation specified in a command type
 * @return:	0 if success, -EINVAL if wrong value of number
 *		of parameters
 */
static ssize_t vcpctl_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t n)
{
	int ret;
	int magic, type, op;
	struct vcpctl_cmd_s cmd;
	char *prompt = "[VCPCTL]:";

	if (sscanf(buf, "%d %d %d", &magic, &type, &op) != 3)
		return -EINVAL;
	pr_notice("%s %d %d %d\n", prompt, magic, type, op);

	if (magic != 666)
		return -EINVAL;

	cmd.type = type;
	cmd.op = op;

	ret = mtk_ipi_send(&vcp_ipidev, IPI_OUT_VCPCTL_1, 0, &cmd,
			   PIN_OUT_SIZE_VCPCTL_1, 0);
	if (ret != IPI_ACTION_DONE)
		goto _err;

	return n;

_err:
	pr_notice("%s failed, %d\n", prompt, ret);
	return -EIO;
}
DEVICE_ATTR_WO(vcpctl);


