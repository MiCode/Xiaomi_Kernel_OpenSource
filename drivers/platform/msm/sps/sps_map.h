/* Copyright (c) 2011,2013, The Linux Foundation. All rights reserved.
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

/* SPS driver mapping table data declarations. */


#ifndef _SPS_MAP_H_
#define _SPS_MAP_H_

#include <linux/types.h>	/* u32 */

/* End point parameters */
struct sps_map_end_point {
	u32 periph_class;	/* Peripheral device enumeration class */
	u32 periph_phy_addr;	/* Peripheral base address */
	u32 pipe_index;		/* Pipe index */
	u32 event_thresh;	/* Pipe event threshold */
};

/* Mapping connection descriptor */
struct sps_map {
	/* Source end point parameters */
	struct sps_map_end_point src;

	/* Destination end point parameters */
	struct sps_map_end_point dest;

	/* Resource parameters */
	u32 config;	 /* Configuration (stream) identifier */
	phys_addr_t desc_base;	 /* Physical address of descriptor FIFO */
	u32 desc_size;	 /* Size (bytes) of descriptor FIFO */
	phys_addr_t data_base;	 /* Physical address of data FIFO */
	u32 data_size;	 /* Size (bytes) of data FIFO */

};

#endif /* _SPS_MAP_H_ */
