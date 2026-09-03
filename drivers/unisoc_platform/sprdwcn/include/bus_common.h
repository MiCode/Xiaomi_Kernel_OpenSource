/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Defines for the sdio device driver
 */
#ifndef __BUS_COMMON_H__
#define __BUS_COMMON_H__

#include <misc/wcn_bus.h>

int bus_chn_init(struct mchn_ops_t *ops, int hif_type);
int bus_chn_deinit(struct mchn_ops_t *ops);
int buf_list_alloc(int chn, struct mbuf_t **head,
		   struct mbuf_t **tail, int *num);
int buf_list_free(int chn, struct mbuf_t *head,
		  struct mbuf_t *tail, int num);
int buf_list_is_empty(int chn);
int buf_list_is_full(int chn);
struct mchn_ops_t *chn_ops(int channel);
int module_ops_register(struct sprdwcn_bus_ops *ops);
void module_ops_unregister(void);
struct sprdwcn_bus_ops *get_wcn_bus_ops(void);
#endif
