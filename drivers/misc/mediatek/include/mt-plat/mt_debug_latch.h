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

#ifndef __MT_DEBUG_LATCH_H__
#define __MT_DEBUG_LATCH_H__

/* public APIs for those want to dump lastbus to buf */
int __attribute__((weak))	mt_lastbus_dump(char *buf);


#endif /* end of __MT_DEBUG_LATCH_H__ */
