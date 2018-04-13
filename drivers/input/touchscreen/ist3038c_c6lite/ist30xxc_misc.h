/*
 *  Copyright (C) 2010, Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2018 XiaoMi, Inc.
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

#ifndef __IST30XXC_MISC_H__
#define __IST30XXC_MISC_H__

#define NODE_FLAG_RAW       (1 << 0)
#define NODE_FLAG_BASE      (1 << 1)
#define NODE_FLAG_FILTER    (1 << 2)
#define NODE_FLAG_DIFF      (1 << 3)
#define NODE_FLAG_ALL       (0xF)
#define NODE_FLAG_NO_CCP    (1 << 7)

#define NODE_FLAG_CP_LOWER  (1 << 0)
#define NODE_FLAG_CP_UPPER  (1 << 1)

#define TSP_RAW_ALL         (0)
#define TSP_RAW_SCREEN      (1 << 0)
#define TSP_RAW_KEY         (1 << 1)

#define IST30XX_REC_FILENAME    "ist30xxc.res"

int ist30xx_check_valid_ch(struct ist30xx_data *data, int ch_tx, int ch_rx);
int ist30xx_parse_cp_node(struct ist30xx_data *data, struct TSP_NODE_BUF *node);
int parse_cp_node(struct ist30xx_data *data, u8 flag,
		 struct TSP_NODE_BUF *node, s16 *buf16, int mode);
int ist30xx_read_cp_node(struct ist30xx_data *data, struct TSP_NODE_BUF *node);
int ist30xx_parse_touch_node(struct ist30xx_data *data, u8 flag,
		 struct TSP_NODE_BUF *node);
int parse_tsp_node(struct ist30xx_data *data, u8 flag,
		 struct TSP_NODE_BUF *node, s16 *buf16, int mode);
int ist30xx_read_touch_node(struct ist30xx_data *data, u8 flag,
		 struct TSP_NODE_BUF *node);

int ist30xx_put_frame(struct ist30xx_data *data, u32 ms, u32 *touch, u32 *frame,
		 int frame_cnt);

int ist30xx_init_misc_sysfs(struct ist30xx_data *data);

#endif
