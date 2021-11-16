/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __EDMA_QUEUE_H__
#define __EDMA_QUEUE_H__

#define DECLARE_VLIST(type) \
struct type ## _list { \
	struct type node; \
	struct list_head link; \
}

/*
 * vlist_node_of - get the pointer to the node which has specific vlist
 * @ptr:	the pointer to struct list_head
 * @type:	the type of list node
 */
#define vlist_node_of(ptr, type) ({ \
	const struct list_head *mptr = (ptr); \
	(type *)((char *)mptr - offsetof(type ## _list, link)); })

/*
 * vlist_link - get the pointer to struct list_head
 * @ptr:	the pointer to struct vlist
 * @type:	the type of list node
 */
#define vlist_link(ptr, type) \
	((struct list_head *)((char *)ptr + offsetof(type ## _list, link)))

/*
 * vlist_type - get the type of struct vlist
 * @type:	the type of list node
 */
#define vlist_type(type) type ## _list

/*
 * vlist_node - get the pointer to the node of vlist
 * @ptr:	the pointer to struct vlist
 * @type:	the type of list node
 */
#define vlist_node(ptr, type) ((type *)ptr)

enum edma_command_type {
	EDMA_PROC_NORMAL,
	EDMA_PROC_FILL,
	EDMA_PROC_NUMERICAL,
	EDMA_PROC_FORMAT,
	EDMA_PROC_COMPRESS,
	EDMA_PROC_DECOMPRESS,
	EDMA_PROC_RAW,
	EDMA_PROC_EXT_MODE,
	EDMA_PROC_MAX,
};


int edma_alloc_request(struct edma_request **rreq);
int edma_free_request(struct edma_request *req);

int edma_create_user(struct edma_user **user, struct edma_device *edma_device);
int edma_delete_user(struct edma_user *user, struct edma_device *edma_device);

int edma_enque_routine_loop(void *arg);
int edma_flush_requests_from_queue(struct edma_user *user);
int edma_free_request(struct edma_request *req);

void edma_setup_normal_request(struct edma_request *req,
			       struct edma_normal *edma_normal,
			       unsigned int type);
void edma_setup_fill_request(struct edma_request *req,
			       struct edma_fill *edma_fill,
			       unsigned int type);
void edma_setup_ext_mode_request(struct edma_request *req,
			       struct edma_ext *edma_ext,
			       unsigned int type);
void edma_setup_numerical_request(struct edma_request *req,
			       struct edma_numerical *edma_numerical,
			       unsigned int type);
void edma_setup_format_request(struct edma_request *req,
			       struct edma_format *edma_format,
			       unsigned int type);
void edma_setup_compress_request(struct edma_request *req,
			       struct edma_compress *edma_compress,
			       unsigned int type);
void edma_setup_decompress_request(struct edma_request *req,
			       struct edma_decompress *edma_decompress,
			       unsigned int type);

void edma_setup_raw_request(struct edma_request *req,
			       struct edma_raw *edma_raw,
			       unsigned int type);
int edma_push_request_to_queue(struct edma_user *user,
			      struct edma_request *req);
int edma_pop_request_from_queue(u64 handle,
			       struct edma_user *user,
			       struct edma_request **rreq);

#endif /* __EDMA_QUEUE_H__ */
