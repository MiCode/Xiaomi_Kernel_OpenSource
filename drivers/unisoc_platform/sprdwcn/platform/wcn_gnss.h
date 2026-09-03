/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _WCN_GNSS_H
#define _WCN_GNSS_H

struct sprdwcn_gnss_ops {
	int (*file_judge)(char *buff, int type);
	int (*backup_data)(void);
	int (*write_data)(void);
	void (*set_file_path)(char *buf);
	int (*wait_gnss_boot)(void);
};

int wcn_gnss_ops_register(struct sprdwcn_gnss_ops *ops);
void wcn_gnss_ops_unregister(void);

#endif
