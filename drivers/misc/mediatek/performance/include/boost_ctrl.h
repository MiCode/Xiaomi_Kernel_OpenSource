/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _BOOST_CTRL_H
#define _BOOST_CTRL_H

/*boost controller parent*/
int init_boostctrl(struct proc_dir_entry *parent);

/*cpu controller*/
int cpu_ctrl_init(struct proc_dir_entry *parent);
void cpu_ctrl_exit(void);

/*dram controller*/
int dram_ctrl_init(struct proc_dir_entry *parent);

/*eas controller*/
int uclamp_ctrl_init(struct proc_dir_entry *parent);
int eas_ctrl_init(struct proc_dir_entry *parent);

/*topology controller*/
int topo_ctrl_init(struct proc_dir_entry *parent);
void topo_ctrl_exit(void);
int get_min_clstr_cap(void);
int get_max_clstr_cap(void);

#endif /* _BOOST_CTRL_H */
