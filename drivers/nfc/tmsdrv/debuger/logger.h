/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#ifndef _TMS_LOGGER_H_
#define _TMS_LOGGER_H_

/*********** PART0: Head files ***********/
#include <linux/proc_fs.h>

/*********** PART1: Define Area ***********/

/*********** PART2: Struct Area ***********/

/*********** PART3: Function or variables for other files ***********/
int logger_proc_create(struct proc_dir_entry *prEntry);
#ifdef TMS_DEBUGER_LOGGER
int logger_init(void);
void logger_deinit(void);
#endif

#endif /* _TMS_LOGGER_H_ */
