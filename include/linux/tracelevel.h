/*
 * include/linux/tracelevel.c
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _TRACELEVEL_H
#define _TRACELEVEL_H

/* tracelevel allows a subsystem author to add priorities to
 * trace_events. For usage details, see tracelevel.txt.
 */

#define TRACELEVEL_ERR 3
#define TRACELEVEL_WARN 2
#define TRACELEVEL_INFO 1
#define TRACELEVEL_DEBUG 0

#define TRACELEVEL_MAX TRACELEVEL_ERR
#define TRACELEVEL_DEFAULT TRACELEVEL_ERR

int __tracelevel_register(char *name, unsigned int level);
int tracelevel_set_level(int level);

#define tracelevel_register(name, level)	\
	__tracelevel_register(#name, level)

#endif /* _TRACELEVEL_H */
