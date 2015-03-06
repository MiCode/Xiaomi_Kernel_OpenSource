/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _API_H_
#define _API_H_

#include "public/mc_linux.h"

#include "client.h"
#include "session.h"

struct tbase_client *api_open_device(bool is_from_kernel);

int api_freeze_device(struct tbase_client	*client);

int api_close_device(struct tbase_client	*client);

int api_open_session(struct tbase_client	*client,
		     uint32_t		*session_id,
		     const struct mc_uuid_t *uuid,
		     uintptr_t		tci,
		     size_t		tci_len,
		     bool		is_gp_uuid);

int api_open_trustlet(struct tbase_client	*client,
		      uint32_t		*p_sid,
		      uint32_t		spid,
		      uintptr_t		trustlet,
		      size_t		trustlet_len,
		      uintptr_t		tci,
		      size_t		tci_len);

int api_close_session(struct tbase_client *client, uint32_t session_id);

int api_notify(struct tbase_client *client, uint32_t session_id);

int api_wait_notification(struct tbase_client *client, uint32_t session_id,
			  int32_t timeout);

int api_malloc_cbuf(struct tbase_client *client, uint32_t len, void **p_addr,
		    struct vm_area_struct *vmarea);

int api_free_cbuf(struct tbase_client *client, void *addr);

int api_map_wsm(struct tbase_client *client, uint32_t session_id, void *buf,
		uint32_t len, uint32_t *p_sva, uint32_t *p_slen);

int api_unmap_wsm(struct tbase_client *client, uint32_t session_id, void *buf,
		  uint32_t sva, uint32_t slen);

int api_get_session_error(struct tbase_client *client, uint32_t session_id,
			  int32_t *last_error);

#endif  /* _API_H_ */
