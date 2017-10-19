/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _QMP_H_
#define _QMP_H_

#include <linux/types.h>

/**
 * struct qmp_pkt - Packet structure to be used for TX and RX with QMP
 * @size	size of data
 * @data	Buffer holding data of this packet
 */
struct qmp_pkt {
	u32 size;
	void *data;
};

#endif /* _QMP_H_ */
