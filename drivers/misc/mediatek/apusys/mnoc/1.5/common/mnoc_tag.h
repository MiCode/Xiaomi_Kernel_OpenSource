/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MNOC_TAG_H__
#define __MNOC_TAG_H__

#include "mnoc_option.h"

/* Tags: Count of Tags */
#define MNOC_TAGS_CNT (3000)

/* The tag entry of MNOC */
struct mnoc_tag {
	int type;

	union mnoc_tag_data {
		struct probe_mnoc_excep {
			unsigned int rt_id;
			unsigned int sw_irq;
			unsigned int mni_qos_irq;
			unsigned int addr_dec_err;
			unsigned int mst_parity_err;
			unsigned int mst_misro_err;
			unsigned int mst_crdt_err;
			unsigned int slv_parity_err;
			unsigned int slv_misro_err;
			unsigned int slv_crdt_err;
			unsigned int req_misro_err;
			unsigned int rsp_misro_err;
			unsigned int req_to_err;
			unsigned int rsp_to_err;
			unsigned int req_cbuf_err;
			unsigned int rsp_cbuf_err;
			unsigned int req_crdt_err;
			unsigned int rsp_crdt_err;
		} excep;
	} d;
};

#ifdef MNOC_TAG_TP
int mnoc_init_drv_tags(void);
void mnoc_exit_drv_tags(void);
void mnoc_tags_show(struct seq_file *s);
#else
static inline int mnoc_init_drv_tags(void)
{
	return 0;
}

static inline void mnoc_exit_drv_tags(void)
{
}

static inline void mnoc_tags_show(struct seq_file *s)
{
}
#endif /* MNOC_TAG_TP */

#endif /* __MNOC_TAG_H__ */
