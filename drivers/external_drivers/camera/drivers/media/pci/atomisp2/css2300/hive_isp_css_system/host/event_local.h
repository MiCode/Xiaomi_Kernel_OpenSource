#ifndef __EVENT_LOCAL_H_INCLUDED__
#define __EVENT_LOCAL_H_INCLUDED__

/*
 * All events come from connections mapped on the system
 * bus but do not use a global IRQ
 */
#include "event_global.h"

typedef enum {
	SP0_EVENT_ID,
	ISP0_EVENT_ID,
	STR2MIPI_EVENT_ID,
	N_EVENT_ID
} event_ID_t;

#define	EVENT_QUERY_BIT		0

/* Events are read from FIFO */
static const hrt_address event_source_addr[N_EVENT_ID] = {
	0x10200200UL,
	0x10200204UL,
	0xffffffffUL};

/* Read from FIFO are blocking, query data availability */
static const hrt_address event_source_query_addr[N_EVENT_ID] = {
	0x10200210UL,
	0x10200214UL,
	0xffffffffUL};

/* Events are written to FIFO */
static const hrt_address event_sink_addr[N_EVENT_ID] = {
	0x10200208UL,
	0x1020020CUL,
	0x10200304UL};

/* Writes to FIFO are blocking, query data space */
static const hrt_address event_sink_query_addr[N_EVENT_ID] = {
	0x10200218UL,
	0x1020021CUL,
	0x1020030CUL};

#endif /* __EVENT_LOCAL_H_INCLUDED__ */
