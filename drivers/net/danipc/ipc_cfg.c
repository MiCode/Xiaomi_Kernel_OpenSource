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
 * MACRO (define) section
 * -----------------------------------------------------------
 */

/* -----------------------------------------------------------
 * Type definition section
 * -----------------------------------------------------------
 */
/* Entry of Route-How table providing information how
 * information is to pass when Source and destination Node location are known
 */
struct IPC_route_how {
	uint8_t srcNodeLoc;
	uint8_t destNodeLoc;
	struct IPC_transport_func const *utilVectorPtr;
};


/* -----------------------------------------------------------
 * Global data section
 * -----------------------------------------------------------
 */

const struct IPC_transport_func IPC_fifo_utils = {
	IPC_trns_fifo_buffer_alloc,
	IPC_trns_fifo_buffer_free,
	IPC_trns_fifo_buf_send
};

static const struct IPC_transport_func IPC_fifo2eth_utils = {
	IPC_trns_fifo2eth_buffer_alloc,
	IPC_trns_fifo2eth_buffer_free,
	IPC_trns_fifo2eth_buffer_send
};

static const struct IPC_transport_func IPC_proxy2eth_utils = {
	NULL,
	NULL,
	NULL,
};

static const struct IPC_route_how routeHow[] = {
	{dan3400_e,		dan3400_e,	&IPC_fifo_utils},
	{dan3400_e,		extEth_e,	&IPC_fifo2eth_utils},
	{dan3400_eth_e,	extEth_e,	&IPC_proxy2eth_utils}
};

/* The static constant table contains information about the location
 * of each node.
 */
static const enum IPC_node_type nodeAllocation[PLATFORM_MAX_NUM_OF_NODES] = {
		dan3400_e,		/* Node #0 */
		dan3400_e,		/* Node #1 */
		dan3400_e,		/* Node #2 */
		dan3400_e,		/* Node #3 */
		extEth_e,		/* Node #4 */
		extFser_e,		/* Node #5 */
		extUart_e,		/* Node #6 */
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




static const uint8_t numOfRouteHows = sizeof(routeHow) /
						sizeof(struct IPC_route_how);

/* Routing table is maintained globaly per 'board' */
static struct IPC_transport_func const *IPC_routing[PLATFORM_MAX_NUM_OF_NODES];

struct agentNameEntry __iomem	*agentTable;


/* -----------------------------------------------------------
 * Global prototypes section
 * -----------------------------------------------------------
 */

/* ===========================================================================
 * IPC_getOwnNode
 * ===========================================================================
 * Description:  Get Node ID for current node
 *
 * Parameters: none
 *
 * Returns: IPC node ID (0:31)
 *
 */
uint8_t IPC_getOwnNode(void)
{
	return PLATFORM_my_ipc_id;
}


/* ===========================================================================
 * IPC_cfg_get_util_vec
 * ===========================================================================
 * Description:	Fetch the node util function vector
 *
 * Parameters:		srcNode		- source Node Id
 *			destNode	- destination Node Id
 *
 * Returns: Location Type
 *
 */
static struct IPC_transport_func const *IPC_cfg_get_util_vec(uint8_t srcNode,
							 uint8_t destNode)
{
	int i;

	for (i = 0; i < numOfRouteHows; i++) {
		if ((routeHow[i].srcNodeLoc == nodeAllocation[srcNode]) &&
			(routeHow[i].destNodeLoc == nodeAllocation[destNode]))
			return routeHow[i].utilVectorPtr;
	}

	return NULL;
}

/* ===========================================================================
 * IPC_setAgentName
 * ===========================================================================
 * Description:	Set agent name in Agent Table
 *
 * Parameters:		name		- Agent Name
 *			inx		- Index in table
 *
 * Returns: n/a
 *
 */
void IPC_setAgentName(const char *name, uint8_t inx)
{
	if (name) {
		const uint32_t	len = strlen(name);
		if (len < MAX_AGENT_NAME_LEN)
			strlcpy(agentTable[inx].agentName, name,
					sizeof(agentTable[0].agentName));
		else
			pr_err("%s: agent name is too long: %u bytes\n",
				__func__, len);
	}
}

/* ===========================================================================
 * IPC_agent_table_clean
 * ===========================================================================
 * Description:  Clear agent table, no registered agents on this stage
 *
 * Parameters:   inx  - Index in table
 *
 * Returns: name
 *
 */
void IPC_agent_table_clean(void)
{
	int		len = (sizeof(struct agentNameEntry) * MAX_LOCAL_AGENT)
					/ sizeof(uint32_t);
	const unsigned	aid = __IPC_AGENT_ID(PLATFORM_my_ipc_id, 0);
	uint32_t	*p = (uint32_t *)&agentTable[aid];

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
 * getUtilFuncVector
 * ===========================================================================
 * Description:	Fetch the utility function's vector from the IPC
 *		routing table.
 *
 * Parameters:	CPU-Id
 *
 * Returns: Pointer to function's pointers structure
 *
 */
struct IPC_transport_func const *IPC_getUtilFuncVector(uint8_t nodeId)
{
	return (nodeId < PLATFORM_MAX_NUM_OF_NODES) ?
						IPC_routing[nodeId] : NULL;
}

/* ===========================================================================
 * IPC_routeTableInit
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
void IPC_routeTableInit(struct IPC_transport_func const *defTranVecPtr)
{
	int i;

	/* For every potential destination fill the associated function
	 * vector or set to the node default transport functions
	 */
	for (i = 0; i < PLATFORM_MAX_NUM_OF_NODES; i++) {
		IPC_routing[i] = (struct IPC_transport_func *)
				IPC_cfg_get_util_vec(IPC_OwnNode, i);
		if (IPC_routing[i] == NULL)
			IPC_routing[i] = defTranVecPtr;
	}
}
