/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc. All Rights Reserved.
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

#ifndef __IST30XX_MISC_H__
#define __IST30XX_MISC_H__

#include "ist30xx_tsp.h"


#define IST30XXB_RAW_ADDR           (0x40100200)
#define IST30XXB_FILTER_ADDR        (0x40101000)

#define IST30XX_RX_CNT_ADDR         (0x20000038)
#define IST30XX_CONFIG_ADDR         (0x20000040)

#define NODE_FLAG_RAW               (1)
#define NODE_FLAG_BASE              (1 << 1)
#define NODE_FLAG_FILTER            (1 << 2)
#define NODE_FLAG_DIFF              (1 << 3)
#define NODE_FLAG_ALL               (0xF)
#define NODE_FLAG_NO_CCP            (1 << 7)

int ist30xx_parse_touch_node(struct ist30xx_data *data, u8 flag, struct TSP_NODE_BUF *node);
int ist30xx_read_touch_node(struct ist30xx_data *data, u8 flag, struct TSP_NODE_BUF *node);

int ist30xx_tsp_update_info(struct ist30xx_data *data);
int ist30xx_tkey_update_info(struct ist30xx_data *data);

int ist30xx_get_tsp_info(struct ist30xx_data *data);
int ist30xx_get_tkey_info(struct ist30xx_data *data);

int ist30xx_init_misc_sysfs(struct ist30xx_data *data);

#endif  // __IST30XX_MISC_H__
