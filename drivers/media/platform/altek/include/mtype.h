/*
 * File: mtype.h
 * Description: Global type definition
 *   Please don't add new definition arbitrarily
 *   Only system-member is allowed to modify this file
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */


#ifndef _MTYPE_H_
#define _MTYPE_H_

#include <linux/types.h>
#include <linux/stddef.h>

typedef u32 errcode;

/* Other definition*/
/*#define _Uncached volatile __attribute__ ((section(".ucdata")))*/

#endif
