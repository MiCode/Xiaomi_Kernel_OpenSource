/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/
#ifndef __AP_THERMAL_LIMIT_H__
#define __AP_THERMAL_LIMIT_H__

struct apthermolmt_user {
	char *log;
	unsigned int cpu_limit;
	unsigned int gpu_limit;
	void *ptr;
};

/*
 *	apthermolmt_register_user
 *	@return 0 success, < 0 with errors
 *	@handle ptr to memory
 *	@log ptr to log string
 */
extern
int apthermolmt_register_user
(struct apthermolmt_user *handle, char *log);

extern
int apthermolmt_unregister_user
(struct apthermolmt_user *handle);

/*
 *	@limit 0x7FFFFFFF for unlimit
 */
extern
void apthermolmt_set_cpu_power_limit
(struct apthermolmt_user *handle, unsigned int limit);

/*
 *	@limit 0x7FFFFFFF for unlimit
 */
extern
void apthermolmt_set_gpu_power_limit
(struct apthermolmt_user *handle, unsigned int limit);

/*
 *	@limit 0 for unlimit
 */
extern
void apthermolmt_set_general_cpu_power_limit
(unsigned int limit);

/*
 *	@limit 0 for unlimit
 */
extern
void apthermolmt_set_general_gpu_power_limit
(unsigned int limit);

extern
unsigned int apthermolmt_get_cpu_power_limit(void);

extern
unsigned int apthermolmt_get_gpu_power_limit(void);

#endif	/* __AP_THERMAL_LIMIT_H__ */
