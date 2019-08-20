/*
 * File: miniisp_ctrl_intf.h
 * Description: mini ISP control cmd interface.
 * use for handling the control cmds instead of debug cmds
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 *	2018/08/28; PhenixChen; Initial version
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


#ifndef _MINIISP_CTRL_INTF_H_
#define _MINIISP_CTRL_INTF_H_

#define P_F_INTERLEAVE

extern int handle_ControlFlowCmd_II(u16 miniisp_op_code, u8 *param);
extern long handle_ControlFlowCmd(unsigned int cmd, unsigned long arg);
/*AL6100 Kernel Base Solution */
enum miniisp_firmware {
	ECHO_IQ_CODE,
	ECHO_DEPTH_CODE,
	ECHO_OTHER_MAX
};

extern long handle_ControlFlowCmd_Kernel(unsigned int cmd, unsigned long arg);
extern void mini_isp_other_drv_open(char *file_name, u8 type);
extern void mini_isp_other_drv_read(struct file *filp, u8 type);
/*AL6100 Kernel Base Solution */
#endif
