/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MSM_MERCURY_COMMON_H
#define MSM_MERCURY_COMMON_H

#define MSM_MERCURY_DEBUG
#ifdef MSM_MERCURY_DEBUG
#define MCR_DBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define MCR_DBG(fmt, args...) do { } while (0)
#endif

#define MCR_PR_ERR   pr_err
#endif /* MSM_MERCURY_COMMON_H */
