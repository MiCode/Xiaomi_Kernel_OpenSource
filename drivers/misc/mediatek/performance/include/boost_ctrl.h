/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
int eas_ctrl_init(struct proc_dir_entry *parent);

/*topology controller*/
int topo_ctrl_init(struct proc_dir_entry *parent);
void topo_ctrl_exit(void);
int get_min_clstr_cap(void);
int get_max_clstr_cap(void);

#endif /* _BOOST_CTRL_H */
