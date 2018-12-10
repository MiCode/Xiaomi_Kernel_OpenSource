
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(VSERVICES_CORE_TYPES_H)
#define VSERVICES_CORE_TYPES_H

#define VSERVICE_CORE_SERVICE_NAME_SIZE (uint32_t)16

#define VSERVICE_CORE_PROTOCOL_NAME_SIZE (uint32_t)32

typedef enum {
/* state offline */
	VSERVICE_CORE_STATE_OFFLINE = 0,
	VSERVICE_CORE_STATE_OFFLINE__CONNECT,
	VSERVICE_CORE_STATE_OFFLINE__DISCONNECT,

/* state disconnected */
	VSERVICE_CORE_STATE_DISCONNECTED,
	VSERVICE_CORE_STATE_DISCONNECTED__CONNECT,
	VSERVICE_CORE_STATE_DISCONNECTED__DISCONNECT,

/* state connected */
	VSERVICE_CORE_STATE_CONNECTED,
	VSERVICE_CORE_STATE_CONNECTED__CONNECT,
	VSERVICE_CORE_STATE_CONNECTED__DISCONNECT,

	VSERVICE_CORE__RESET = VSERVICE_CORE_STATE_OFFLINE
} vservice_core_statenum_t;

typedef struct {
	vservice_core_statenum_t statenum;
} vservice_core_state_t;

#define VSERVICE_CORE_RESET_STATE (vservice_core_state_t) { \
.statenum = VSERVICE_CORE__RESET}

#define VSERVICE_CORE_STATE_IS_OFFLINE(state) (\
((state).statenum == VSERVICE_CORE_STATE_OFFLINE) || \
((state).statenum == VSERVICE_CORE_STATE_OFFLINE__CONNECT) || \
((state).statenum == VSERVICE_CORE_STATE_OFFLINE__DISCONNECT))

#define VSERVICE_CORE_STATE_IS_DISCONNECTED(state) (\
((state).statenum == VSERVICE_CORE_STATE_DISCONNECTED) || \
((state).statenum == VSERVICE_CORE_STATE_DISCONNECTED__CONNECT) || \
((state).statenum == VSERVICE_CORE_STATE_DISCONNECTED__DISCONNECT))

#define VSERVICE_CORE_STATE_IS_CONNECTED(state) (\
((state).statenum == VSERVICE_CORE_STATE_CONNECTED) || \
((state).statenum == VSERVICE_CORE_STATE_CONNECTED__CONNECT) || \
((state).statenum == VSERVICE_CORE_STATE_CONNECTED__DISCONNECT))

#define VSERVICE_CORE_STATE_VALID(state) ( \
VSERVICE_CORE_STATE_IS_OFFLINE(state) ? true : \
VSERVICE_CORE_STATE_IS_DISCONNECTED(state) ? true : \
VSERVICE_CORE_STATE_IS_CONNECTED(state) ? true : \
false)

static inline const char *vservice_core_get_state_string(vservice_core_state_t
							 state)
{
	static const char *names[] =
	    { "offline", "offline__connect", "offline__disconnect",
		"disconnected", "disconnected__connect",
		    "disconnected__disconnect",
		"connected", "connected__connect", "connected__disconnect"
	};
	if (!VSERVICE_CORE_STATE_VALID(state)) {
		return "INVALID";
	}
	return names[state.statenum];
}

typedef struct {

	vservice_core_state_t core;
} vservice_core_protocol_state_t;

#define VSERVICE_CORE_PROTOCOL_RESET_STATE (vservice_core_protocol_state_t) {\
.core = VSERVICE_CORE_RESET_STATE }
#endif				/* ! VSERVICES_CORE_TYPES_H */
