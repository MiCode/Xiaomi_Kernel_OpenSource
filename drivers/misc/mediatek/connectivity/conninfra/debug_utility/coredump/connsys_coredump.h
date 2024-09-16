/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _CONNSYS_COREDUMP_H_
#define _CONNSYS_COREDUMP_H_

#include <linux/types.h>
#include <linux/compiler.h>


/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/* Coredump property */
#define CONNSYS_DUMP_PROPERTY_NO_WAIT	0x1

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

enum connsys_coredump_mode {
	DUMP_MODE_RESET_ONLY = 0,
	DUMP_MODE_AEE = 1,
	DUMP_MODE_DAEMON = 2,
	DUMP_MODE_MAX,
};

struct coredump_event_cb {
	int (*reg_readable)(void);
	void (*poll_cpupcr)(unsigned int times, unsigned int sleep);
};

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/* subsys usage */
void* connsys_coredump_init(
	int conn_type,
	struct coredump_event_cb* cb);
void connsys_coredump_deinit(void* handler);

phys_addr_t connsys_coredump_get_start_offset(int conn_type);

int connsys_coredump_setup_dump_region(void* handler);
int connsys_coredump_start(
	void* handler,
	unsigned int dump_property,
	int drv,
	char *reason);
void connsys_coredump_clean(void* handler);


/* config relative */
enum connsys_coredump_mode connsys_coredump_get_mode(void);
void connsys_coredump_set_dump_mode(enum connsys_coredump_mode mode);

#endif
