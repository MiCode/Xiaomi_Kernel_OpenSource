// SPDX-License-Identifier: GPL-2.0-only
/*
 * unisoc_userlog.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef UNISOC_USERLOG_H
#define UNISOC_USERLOG_H

#include <linux/types.h>
#include <linux/ioctl.h>

struct userlog_entry {
	__u16		len;
};

#define USERLOG_SYSTEM	"userlog_point"	/* system user point messages */
#define USERLOG_ENTRY_MAX_PAYLOAD	4076

#endif

