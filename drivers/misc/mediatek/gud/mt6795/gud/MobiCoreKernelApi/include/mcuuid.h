/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MCUUID_H_
#define _MCUUID_H_

#define UUID_TYPE

/* Universally Unique Identifier (UUID) according to ISO/IEC 11578. */
struct mc_uuid_t {
	uint8_t		value[16];	/* Value of the UUID. */
};

#endif /* _MCUUID_H_ */
