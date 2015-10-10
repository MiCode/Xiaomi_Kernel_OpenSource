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

#ifndef __IST30XX_TSP_H__
#define __IST30XX_TSP_H__

#define IST30XX_DD_VERSION  (3)

#define IST30XX_MULTIPLE_TSP    (1)

#define TSP_CHIP_VENDOR     ("IMAGIS")
#define TSP_CHIP_NAME       ("IST3038")

#define FLAG_NODE_Y         (0)
#define FLAG_NODE_X         (1)

#define NODE_TX_NUM         (24)
#define NODE_RX_NUM         (13)
#define NODE_TOTAL_NUM      (TSP_TX_NUM * TSP_RX_NUM)

#define TSP_TX_NUM          (23)
#define TSP_RX_NUM          (13)
#define TSP_TOTAL_NUM       (TSP_TX_NUM * TSP_RX_NUM)

#define TSP_THRESHOLD       (30)
#define TKEY_THRESHOLD      (600)

#define IST30XXB_PARSE_TSPTYPE(k)   ((k >> 1) & 0xF)

#define TSP_TYPE_UNKNOWN    (0xF0)

#endif  // __IST30XX_TSP_H__
