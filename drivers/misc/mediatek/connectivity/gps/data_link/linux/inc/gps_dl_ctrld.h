/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _GPS_DL_CTRLD_H
#define _GPS_DL_CTRLD_H
#include "gps_dl_osal.h"
#include "gps_dl_config.h"

#define GPS_DL_OP_BUF_SIZE (16)

enum gps_dl_ctrld_opid {
	GPS_DL_OPID_LINK_EVENT_PROC,
	GPS_DL_OPID_HAL_EVENT_PROC,
	GPS_DL_OPID_MAX
};

struct gps_dl_ctrld_context {
	struct gps_dl_osal_event rgpsdlWq;  /* rename */
	struct gps_dl_osal_lxop_q rOpQ;     /* active op queue */
	struct gps_dl_osal_lxop_q rFreeOpQ; /* free op queue */
	struct gps_dl_osal_lxop arQue[GPS_DL_OP_BUF_SIZE]; /* real op instances */
	struct gps_dl_osal_thread thread;
};

typedef int(*GPS_DL_OPID_FUNC) (struct gps_dl_osal_op_dat *);

unsigned int gps_dl_wait_event_checker(struct gps_dl_osal_thread *pThread);
int gps_dl_put_act_op(struct gps_dl_osal_lxop *pOp);
struct gps_dl_osal_lxop *gps_dl_get_free_op(void);
int gps_dl_put_op_to_free_queue(struct gps_dl_osal_lxop *pOp);
int gps_dl_ctrld_init(void);
int gps_dl_ctrld_deinit(void);

#endif /* _GPS_DL_CTRLD_H */

