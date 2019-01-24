
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(VSERVICES_BLOCK_TYPES_H)
#define VSERVICES_BLOCK_TYPES_H

#define VSERVICE_BLOCK_IO_READ_MAX_PENDING 1024
#define VSERVICE_BLOCK_IO_WRITE_MAX_PENDING 1024

typedef enum vservice_block_block_io_error {
	VSERVICE_BLOCK_INVALID_INDEX,
	VSERVICE_BLOCK_MEDIA_FAILURE,
	VSERVICE_BLOCK_MEDIA_TIMEOUT,
	VSERVICE_BLOCK_UNSUPPORTED_COMMAND,
	VSERVICE_BLOCK_SERVICE_RESET
} vservice_block_block_io_error_t;

typedef enum {
/* state closed */
	VSERVICE_BASE_STATE_CLOSED = 0,
	VSERVICE_BASE_STATE_CLOSED__OPEN,
	VSERVICE_BASE_STATE_CLOSED__CLOSE,
	VSERVICE_BASE_STATE_CLOSED__REOPEN,

/* state running */
	VSERVICE_BASE_STATE_RUNNING,
	VSERVICE_BASE_STATE_RUNNING__OPEN,
	VSERVICE_BASE_STATE_RUNNING__CLOSE,
	VSERVICE_BASE_STATE_RUNNING__REOPEN,

	VSERVICE_BASE__RESET = VSERVICE_BASE_STATE_CLOSED
} vservice_base_statenum_t;

typedef struct {
	vservice_base_statenum_t statenum;
} vservice_base_state_t;

#define VSERVICE_BASE_RESET_STATE (vservice_base_state_t) { \
.statenum = VSERVICE_BASE__RESET}

#define VSERVICE_BASE_STATE_IS_CLOSED(state) (\
((state).statenum == VSERVICE_BASE_STATE_CLOSED) || \
((state).statenum == VSERVICE_BASE_STATE_CLOSED__OPEN) || \
((state).statenum == VSERVICE_BASE_STATE_CLOSED__CLOSE) || \
((state).statenum == VSERVICE_BASE_STATE_CLOSED__REOPEN))

#define VSERVICE_BASE_STATE_IS_RUNNING(state) (\
((state).statenum == VSERVICE_BASE_STATE_RUNNING) || \
((state).statenum == VSERVICE_BASE_STATE_RUNNING__OPEN) || \
((state).statenum == VSERVICE_BASE_STATE_RUNNING__CLOSE) || \
((state).statenum == VSERVICE_BASE_STATE_RUNNING__REOPEN))

#define VSERVICE_BASE_STATE_VALID(state) ( \
VSERVICE_BASE_STATE_IS_CLOSED(state) ? true : \
VSERVICE_BASE_STATE_IS_RUNNING(state) ? true : \
false)

static inline const char *vservice_base_get_state_string(vservice_base_state_t
							 state)
{
	static const char *names[] =
	    { "closed", "closed__open", "closed__close", "closed__reopen",
		"running", "running__open", "running__close", "running__reopen"
	};
	if (!VSERVICE_BASE_STATE_VALID(state)) {
		return "INVALID";
	}
	return names[state.statenum];
}

typedef struct {
	DECLARE_BITMAP(read_bitmask, VSERVICE_BLOCK_IO_READ_MAX_PENDING);
	void *read_tags[VSERVICE_BLOCK_IO_READ_MAX_PENDING];
	 DECLARE_BITMAP(write_bitmask, VSERVICE_BLOCK_IO_WRITE_MAX_PENDING);
	void *write_tags[VSERVICE_BLOCK_IO_WRITE_MAX_PENDING];
} vservice_block_io_state_t;

#define VSERVICE_BLOCK_IO_RESET_STATE (vservice_block_io_state_t) { \
.read_bitmask = {0}, \
.read_tags = {NULL}, \
.write_bitmask = {0}, \
.write_tags = {NULL}}

#define VSERVICE_BLOCK_IO_STATE_VALID(state) true

typedef struct {

	vservice_base_state_t base;

	vservice_block_io_state_t io;
} vservice_block_state_t;

#define VSERVICE_BLOCK_RESET_STATE (vservice_block_state_t) {\
.base = VSERVICE_BASE_RESET_STATE,\
.io = VSERVICE_BLOCK_IO_RESET_STATE }

#define VSERVICE_BLOCK_IS_STATE_RESET(state) \
            ((state).base.statenum == VSERVICE_BASE__RESET)
#endif				/* ! VSERVICES_BLOCK_TYPES_H */
