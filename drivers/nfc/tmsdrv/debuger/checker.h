/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 TsingTeng MicroSystem Co., Ltd.
 */

#ifndef _TMS_CHECKER_H_
#define _TMS_CHECKER_H_

/*********** PART0: Head files ***********/
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/*********** PART1: Define Area ***********/

/*********** PART2: Struct Area ***********/

/*********** PART3: Function or variables for other files ***********/
#ifdef TMS_DEBUGER_CHECKER
int checker_init(void);
void checker_deinit(void);
void checker_show(struct seq_file *m);
#endif
int checker_proc_create(struct proc_dir_entry *prEntry);

#endif /* _TMS_CHECKER_H_ */
