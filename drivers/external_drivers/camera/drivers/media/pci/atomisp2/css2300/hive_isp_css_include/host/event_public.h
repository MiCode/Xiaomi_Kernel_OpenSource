#ifndef __EVENT_PUBLIC_H_INCLUDED__
#define __EVENT_PUBLIC_H_INCLUDED__

#include <stdbool.h>
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

#endif /* __EVENT_PUBLIC_H_INCLUDED__ */
