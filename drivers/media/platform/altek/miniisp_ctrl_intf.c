/*
 * File: miniisp_ctrl_intf.c
 * Description: mini ISP control cmd interface.
 * use for handling the control cmds instead of debug cmds
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 * 018/08/28 PhenixChen; Initial version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */



/******Include File******/
#include <miniISP/miniISP_ioctl.h>
#include <linux/uaccess.h> /* copy_*_user() */
#include "include/miniisp.h"
#include "include/miniisp_ctrl.h" /* mini_isp_drv_setting() */
#include "include/isp_camera_cmd.h"
#include "include/ispctrl_if_master.h" /* ispctrl_if_mast_execute_cmd() */
#include "include/miniisp_customer_define.h"
#include "include/miniisp_ctrl_intf.h"

/******Private Constant Definition******/
#define MINI_ISP_LOG_TAG	"[miniisp_ctrl_intf]"
#define MAX_DATA_SIZE		(64 * 1024)

/******Private Function Prototype******/

/******Public Function Prototype******/

/******Private Global Variable******/



long handle_ControlFlowCmd(unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	struct miniISP_cmd_config *config = NULL;
	u8 *param = NULL;

	misp_info("%s - enter", __func__);

	/* step1: allocate & receive cmd struct from user space */
	config = kzalloc(sizeof(struct miniISP_cmd_config), GFP_KERNEL);
	if (NULL == config) {
		retval = -ENOMEM;
		goto done;
	}
	if (copy_from_user(config, (void __user *)arg,
		sizeof(struct miniISP_cmd_config))) {
		retval = -EFAULT;
		goto done;
	}

	/* step2: allocate & receive cmd parameter from user space if needed*/
	if ((config->size > 0) && (config->size < MAX_DATA_SIZE)) {
		param = kzalloc(config->size, GFP_KERNEL);
		if (NULL == param) {
			retval = -ENOMEM;
			goto done;
		}
		if (copy_from_user((void *)param,
			(void __user *)(config->param), config->size)) {
			retval = -EFAULT;
			goto done;
		}
	}

	switch (cmd) {
	case IOCTL_ISP_LOAD_FW:
		misp_info("%s - IOCTL_ISP_LOAD_FW", __func__);
		/* open boot and FW file then write boot code and FW code */
		mini_isp_poweron();
		mini_isp_drv_setting(MINI_ISP_MODE_GET_CHIP_ID);
#ifndef AL6100_SPI_NOR
		/* if boot form SPI NOR, do not call this */
		mini_isp_drv_setting(MINI_ISP_MODE_CHIP_INIT);
#endif
		mini_isp_drv_setting(MINI_ISP_MODE_E2A);
		mini_isp_drv_setting(MINI_ISP_MODE_NORMAL);
		break;
	case IOCTL_ISP_PURE_BYPASS:
		misp_info("%s - IOCTL_ISP_PURE_BYPASS", __func__);
		mini_isp_poweron();
		mini_isp_drv_setting(MINI_ISP_MODE_GET_CHIP_ID);
		mini_isp_drv_set_bypass_mode(1);
		break;
	case IOCTL_ISP_CTRL_CMD:
		if (param == NULL) {
			misp_info("%s - cmd parameter is NULL", __func__);
			break;
		}
		retval = ispctrl_if_mast_execute_cmd(config->opcode, param);
		break;
	case IOCTL_ISP_DEINIT:
		misp_info("%s - IOCTL_ISP_DEINIT", __func__);
		mini_isp_poweroff();
		break;
	default:
		misp_info("%s - UNKNOWN CMD[0x%x]", __func__, cmd);
		retval = -ENOTTY;
		break;
	}

done:
	if (param != NULL)
		kfree(param);
	if (config != NULL)
		kfree(config);

	misp_info("%s - leave", __func__);
	return retval;
}
