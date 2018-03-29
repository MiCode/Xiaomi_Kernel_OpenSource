/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __TZ_NDBG_T__
#define __TZ_NDBG_T__

/* enable ndbg implementation */
#ifndef CONFIG_TRUSTY /* disable ndbg if trusty is on for now. */
#define CC_ENABLE_NDBG
#endif

#endif				/* __TZ_NDBG_T__ */
