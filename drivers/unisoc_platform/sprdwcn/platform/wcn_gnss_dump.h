/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef __WCN_GNSS_DUMP_H__
#define __WCN_GNSS_DUMP_H__

void gnss_ring_reset(void);
unsigned long gnss_ring_free_space(void);
int gnss_dump_write(char *buf, int len);
int wcn_gnss_dump_init(void);
void wcn_gnss_dump_exit(void);

#endif
