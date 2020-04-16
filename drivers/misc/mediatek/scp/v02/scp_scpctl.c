/*
 * Copyright (C) 2011-2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details
 * .
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/device.h>       /* needed by device_* */
#include "scp_ipi_pin.h"
#include "scp_mbox_layout.h"
#include "scp_scpctl.h"

/*
 * A device node to send commands to scp wit unified interface
 * @magic:	should be 666
 * @type:	a class for different types of commands
 * @op:		the operation specified in a command type
 * @return:	0 if success, -EINVAL if wrong value of number
 *		of parameters
 */
static ssize_t scpctl_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t n)
{
	int ret;
	int magic, type, op;
	struct scpctl_cmd_s cmd;
	char *prompt = "[SCPCTL]:";

	if (sscanf(buf, "%d %d %d", &magic, &type, &op) != 3)
		return -EINVAL;
	pr_notice("%s %d %d %d\n", prompt, magic, type, op);

	if (magic != 666)
		return -EINVAL;

	cmd.type = type;
	cmd.op = op;

	switch (type) {
	case SCPCTL_TYPE_TMON:
		ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_SCPCTL_0, 0, &cmd,
				   PIN_OUT_SIZE_SCPCTL_0, 0);
		if (ret != IPI_ACTION_DONE)
			goto _err;
		break;
	default:
		return -EINVAL;
	}

	return n;

_err:
	pr_notice("%s failed, %d\n", prompt, ret);
	return -EIO;
}
DEVICE_ATTR(scpctl, 0200, NULL, scpctl_store);


