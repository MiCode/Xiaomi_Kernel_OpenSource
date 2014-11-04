#ifndef __EVENT_PRIVATE_H_INCLUDED__
#define __EVENT_PRIVATE_H_INCLUDED__

#include "event_public.h"

#include "device_access.h"

#include <bits.h>

#include "assert_support.h"

STORAGE_CLASS_EVENT_C void event_wait_for(
	const event_ID_t		ID)
{
assert(ID < N_EVENT_ID);
assert(event_source_addr[ID] != ((hrt_address)-1));
	(void)device_load_uint32(event_source_addr[ID]);
return;
}

STORAGE_CLASS_EVENT_C void cnd_event_wait_for(
	const event_ID_t		ID,
	const bool				cnd)
{
	if (cnd) {
		event_wait_for(ID);
	}
return;
}

STORAGE_CLASS_EVENT_C hrt_data event_receive_token(
	const event_ID_t		ID)
{
assert(ID < N_EVENT_ID);
assert(event_source_addr[ID] != ((hrt_address)-1));
return device_load_uint32(event_source_addr[ID]);
}

STORAGE_CLASS_EVENT_C void event_send_token(
	const event_ID_t		ID,
	const hrt_data			token)
{
assert(ID < N_EVENT_ID);
assert(event_sink_addr[ID] != ((hrt_address)-1));
	device_store_uint32(event_sink_addr[ID], token);
return;
}

STORAGE_CLASS_EVENT_C bool is_event_pending(
	const event_ID_t		ID)
{
	hrt_data	value;
assert(ID < N_EVENT_ID);
assert(event_source_query_addr[ID] != ((hrt_address)-1));
	value = device_load_uint32(event_source_query_addr[ID]);
return !_hrt_get_bit(value, EVENT_QUERY_BIT);
}

STORAGE_CLASS_EVENT_C bool can_event_send_token(
	const event_ID_t		ID)
{
	hrt_data	value;
assert(ID < N_EVENT_ID);
assert(event_sink_query_addr[ID] != ((hrt_address)-1));
	value = device_load_uint32(event_sink_query_addr[ID]);
return !_hrt_get_bit(value, EVENT_QUERY_BIT);
}

#endif /* __EVENT_PRIVATE_H_INCLUDED__ */
