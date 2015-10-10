/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __IST30XX_CMCS_H__
#define __IST30XX_CMCS_H__


#define CMCS_FLAG_CM                (1)
#define CMCS_FLAG_CM_SPEC           (1 << 1)
#define CMCS_FLAG_CM_SLOPE0         (1 << 2)
#define CMCS_FLAG_CM_SLOPE1         (1 << 3)
#define CMCS_FLAG_CS0               (1 << 4)
#define CMCS_FLAG_CS1               (1 << 5)

#define FLAG_ENABLE_CM              (1)
#define FLAG_ENABLE_CS              (2)
#define FLAG_ENABLE_CR              (4)

#define IST30XX_CMCS_LOAD_END       (0x8FFFFCAB)

#define ENABLE_CM_MODE(k)           (k & 1)
#define ENABLE_CS_MODE(k)           ((k >> 1) & 1)

#define IST30XXB_CMCS_NAME          "ist30xxb.cms"

int ist30xx_init_cmcs_sysfs(struct ist30xx_data *data);

#endif  // __IST30XX_CMCS_H__
