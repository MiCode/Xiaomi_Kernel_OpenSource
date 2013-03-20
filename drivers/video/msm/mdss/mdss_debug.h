/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDSS_DEBUG_H
#define MDSS_DEBUG_H

#include "mdss.h"

#define MISR_POLL_SLEEP			2000
#define MISR_POLL_TIMEOUT		32000
#define MISR_CRC_BATCH_SIZE		32
#define MISR_CRC_BATCH_CFG		0x101

#ifdef CONFIG_DEBUG_FS
int mdss_debugfs_init(struct mdss_data_type *mdata);
int mdss_debugfs_remove(struct mdss_data_type *mdata);
int mdss_debug_register_base(const char *name, void __iomem *base,
				    size_t max_offset);
int mdss_misr_crc_set(struct mdss_data_type *mdata, struct mdp_misr *req);
int mdss_misr_crc_get(struct mdss_data_type *mdata, struct mdp_misr *resp);
void mdss_misr_crc_collect(struct mdss_data_type *mdata, int block_id);
#else
static inline int mdss_debugfs_init(struct mdss_data_type *mdata) { return 0; }
static inline int mdss_debugfs_remove(struct mdss_data_type *mdata)
{ return 0; }
static inline int mdss_debug_register_base(const char *name, void __iomem *base,
			     size_t max_offset) { return 0; }
static inline int mdss_misr_crc_set(struct mdss_data_type *mdata,
		      struct mdp_misr *reg) { return 0; }
static inline int mdss_misr_crc_get(struct mdss_data_type *mdata,
		      struct mdp_misr *resp) { return 0; }
static inline void mdss_misr_crc_collect(struct mdss_data_type *mdata,
			   int block_id) { }
#endif
#endif /* MDSS_DEBUG_H */
