/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef TZ_SERVICE_H
#define TZ_SERVICE_H

#ifdef VFS_RDWR_SEM
extern struct semaphore VFS_rd_sem;
extern struct semaphore VFS_wr_sem;
#else
extern struct completion VFS_rd_comp;
extern struct completion VFS_wr_comp;
#endif

int wait_for_service_done(void);
int register_switch_irq_handler(void);
int teei_service_init(void);

void set_sch_nq_cmd(void);
void set_sch_load_img_cmd(void);
long create_cmd_buff(void);
void set_cancel_command(unsigned long memory_size);
#endif
