/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */


extern unsigned int ccci_debug_enable;
extern int curr_ubin_id;
int get_dump_buf_usage(char buf[], int size);
extern void spm_ap_mdsrc_req(unsigned char set);
extern void inject_pin_status_event(int pin_value, char pin_name[]);
