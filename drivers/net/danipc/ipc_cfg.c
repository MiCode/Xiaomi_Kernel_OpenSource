/*
	All files except if stated otherwise in the begining of the file
	are under the ISC license:
	----------------------------------------------------------------------

	Copyright (c) 2010-2012 Design Art Networks Ltd.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


/* -----------------------------------------------------------
 * Include section
 * -----------------------------------------------------------
 */

#include <linux/string.h>

#include "ipc_api.h"
#include "ipc_cfg.h"
#include "danipc_lowlevel.h"


/* -----------------------------------------------------------
 * Type definition section
 * -----------------------------------------------------------
 */
/* Entry of Route-How table providing information how
 * information passes when Source and Destination Nodes are known.
 */
struct ipc_route {
	uint8_t src_node;
	uint8_t dst_node;
	struct ipc_trns_func const *trns_funs;
};


/* -----------------------------------------------------------
 * Global data section
 * -----------------------------------------------------------
 */

const struct ipc_trns_func ipc_fifo_utils = {
	ipc_trns_fifo_buf_alloc,
	ipc_trns_fifo_buf_free,
	ipc_trns_fifo_buf_send
};

static const struct ipc_trns_func ipc_fifo2eth_utils = {
	ipc_trns_fifo2eth_buf_alloc,
	ipc_trns_fifo2eth_buf_free,
	ipc_trns_fifo2eth_buf_send
};

static const struct ipc_trns_func ipc_proxy2eth_utils = {
	NULL,
	NULL,
	NULL,
};

static const struct ipc_route ipc_routes[] = {
	{dan3400_e,		dan3400_e,	&ipc_fifo_utils},
	{dan3400_e,		ext_eth_e,	&ipc_fifo2eth_utils},
	{dan3400_eth_e,		ext_eth_e,	&ipc_proxy2eth_utils}
};

/* The static constant table contains information about the location
 * of each node.
 */
static const enum ipc_node_type node_type[PLATFORM_MAX_NUM_OF_NODES] = {
		dan3400_e,		/* Node #0 */
		dan3400_e,		/* Node #1 */
		dan3400_e,		/* Node #2 */
		dan3400_e,		/* Node #3 */
		ext_eth_e,		/* Node #4 */
		ext_fser_e,		/* Node #5 */
		ext_uart_e,		/* Node #6 */
		undef_e,		/* Node #7 */
		dan3400_e,		/* Node #8 */
		dan3400_e,		/* Node #9 */
		dan3400_e,		/* Node #10 */
		dan3400_e,		/* Node #11 */
		dan3400_e,		/* Node #12 */
		dan3400_e,		/* Node #13 */
		dan3400_e,		/* Node #14 */
		dan3400_e,		/* Node #15 */
};


/* Routing table is maintained globaly per 'board' */
static struct ipc_trns_func const *ipc_routing[PLATFORM_MAX_NUM_OF_NODES];

struct agent_entry __iomem	*agent_table;


/* -----------------------------------------------------------
 * Global prototypes section
 * -----------------------------------------------------------
 */

/* ===========================================================================
 * ipc_get_own_node
 * ===========================================================================
 * Description:  Get Node ID for current node
 *
 * Parameters: none
 *
 * Returns: IPC node ID (0:31)
 *
 */
uint8_t ipc_get_own_node(void)
{
	return LOCAL_IPC_ID;
}


/* ===========================================================================
 * ipc_cfg_get_util_vec
 * ===========================================================================
 * Description:	Fetch the node util function vector
 *
 * Parameters:		srcNode		- source Node Id
 *			destNode	- destination Node Id
 *
 * Returns: Location Type
 *
 */
static struct ipc_trns_func const *ipc_cfg_get_util_vec(uint8_t srcNode,
							 uint8_t destNode)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(ipc_routes); i++) {
		if ((ipc_routes[i].src_node == node_type[srcNode]) &&
			(ipc_routes[i].dst_node == node_type[destNode]))
			return ipc_routes[i].trns_funs;
	}

	return NULL;
}


void ipc_agent_table_clean(void)
{
	int		len = (sizeof(struct agent_entry) * MAX_LOCAL_AGENT)
					/ sizeof(uint32_t);
	const unsigned	aid = __IPC_AGENT_ID(LOCAL_IPC_ID, 0);
	uint32_t	*p = (uint32_t *)&agent_table[aid];

	/* Clean only my part of the global table. This is necessary so I may
	 * register agents but do not hurt other cores.
	 * Vladik, 18.08,2011
	 */
	while (--len >= 0) {
		if (*p)
			*p = 0;
		p++;
	}
}

/* ===========================================================================
 * ipc_trns_func
 * ===========================================================================
 * Description:	Fetch the utility function's vector from the IPC
 *		routing table.
 *
 * Parameters:	CPU-Id
 *
 * Returns: Pointer to function's pointers structure
 *
 */
struct ipc_trns_func const *get_trns_funcs(uint8_t cpuid)
{
	return (cpuid < PLATFORM_MAX_NUM_OF_NODES) ?
						ipc_routing[cpuid] : NULL;
}

/* ===========================================================================
 * ipc_route_table_init
 * ===========================================================================
 * Description:  Initialize routing table .
 *
 * Parameters: pointer to default transport function
 *
 * Returns:
 *
 *   Note: The routing table
 *   is unique per CPU as it maintains the information how to communicate
 *   with any other CPU from this CPU.
 *
 *   This is just a preliminary implementation. Need to decide how each CPU
 *   maintains the correct routing table
 */
void ipc_route_table_init(struct ipc_trns_func const *def_trns_funcs)
{
	unsigned i;

	/* For every potential destination fill the associated function
	 * vector or set to the node default transport functions
	 */
	for (i = 0; i < PLATFORM_MAX_NUM_OF_NODES; i++) {
		ipc_routing[i] = (struct ipc_trns_func *)
				ipc_cfg_get_util_vec(ipc_own_node, i);
		if (ipc_routing[i] == NULL)
			ipc_routing[i] = def_trns_funcs;
	}
}
