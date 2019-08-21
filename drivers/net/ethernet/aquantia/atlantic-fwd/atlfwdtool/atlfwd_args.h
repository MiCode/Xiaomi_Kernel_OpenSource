/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATLFWD_ARGS_H_
#define _ATLFWD_ARGS_H_

#include <stdbool.h>
#include <stdint.h>

#include "atl_fwdnl.h"

struct atlfwd_args {
	char *devname;
	bool verbose;
	enum atlfwd_nl_command cmd;
	union {
		struct {
			uint32_t flags;
			uint32_t ring_size;
			uint32_t buf_size;
			uint32_t page_order;
		};
		int32_t ring_index;
		uint32_t tx_bunch;
	};
};

struct atlfwd_args *parse_args(const int argc, char **argv);

#endif
