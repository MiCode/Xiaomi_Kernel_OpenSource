/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __EVENT_FIFO_PUBLIC_H
#define __EVENT_FIFO_PUBLIC_H

#include <type_support.h>
#include "system_types.h"

/*! Blocking read from an event source EVENT[ID]
 
 \param	ID[in]				EVENT identifier

 \return none, dequeue(event_queue[ID])
 */
STORAGE_CLASS_EVENT_H void event_wait_for(
	const event_ID_t		ID);

/*! Conditional blocking wait for an event source EVENT[ID]
 
 \param	ID[in]				EVENT identifier
 \param	cnd[in]				predicate

 \return none, if(cnd) dequeue(event_queue[ID])
 */
STORAGE_CLASS_EVENT_H void cnd_event_wait_for(
	const event_ID_t		ID,
	const bool				cnd);

/*! Blocking read from an event source EVENT[ID]
 
 \param	ID[in]				EVENT identifier

 \return dequeue(event_queue[ID])
 */
STORAGE_CLASS_EVENT_H hrt_data event_receive_token(
	const event_ID_t		ID);

/*! Blocking write to an event sink EVENT[ID]
 
 \param	ID[in]				EVENT identifier
 \param	token[in]			token to be written on the event

 \return none, enqueue(event_queue[ID])
 */
STORAGE_CLASS_EVENT_H void event_send_token(
	const event_ID_t		ID,
	const hrt_data			token);

/*! Query an event source EVENT[ID]
 
 \param	ID[in]				EVENT identifier

 \return !isempty(event_queue[ID])
 */
STORAGE_CLASS_EVENT_H bool is_event_pending(
	const event_ID_t		ID);

/*! Query an event sink EVENT[ID]
 
 \param	ID[in]				EVENT identifier

 \return !isfull(event_queue[ID])
 */
STORAGE_CLASS_EVENT_H bool can_event_send_token(
	const event_ID_t		ID);

#endif /* __EVENT_FIFO_PUBLIC_H */
