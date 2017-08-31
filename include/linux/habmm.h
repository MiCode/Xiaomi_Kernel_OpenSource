/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <uapi/linux/habmm.h>

#ifndef _HABMM_H
#define _HABMM_H

int32_t habmm_socket_open(int32_t *handle, uint32_t mm_ip_id,
			  uint32_t timeout, uint32_t flags);
int32_t habmm_socket_close(int32_t handle);
int32_t habmm_socket_send(int32_t handle, void *src_buff, uint32_t size_bytes,
			  uint32_t flags);
int32_t habmm_socket_recv(int32_t handle, void *dst_buff, uint32_t *size_bytes,
			  uint32_t timeout, uint32_t flags);
int32_t habmm_socket_sendto(int32_t handle, void *src_buff, uint32_t size_bytes,
			    int32_t remote_handle, uint32_t flags);
int32_t habmm_socket_recvfrom(int32_t handle, void *dst_buff,
			      uint32_t *size_bytes, uint32_t timeout,
			      int32_t *remote_handle, uint32_t flags);
int32_t habmm_export(int32_t handle, void *buff_to_share, uint32_t size_bytes,
		     uint32_t *export_id, uint32_t flags);
int32_t habmm_unexport(int32_t handle, uint32_t export_id, uint32_t flags);
int32_t habmm_import(int32_t handle, void **buff_shared, uint32_t size_bytes,
		     uint32_t export_id, uint32_t flags);
int32_t habmm_unimport(int32_t handle, uint32_t export_id, void *buff_shared,
		       uint32_t flags);

#endif
