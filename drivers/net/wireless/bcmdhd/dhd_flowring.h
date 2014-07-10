/*
 * Header file describing the flow rings DHD interfaces.
 *
 * Provides type definitions and function prototypes used to create, delete and manage
 *
 * flow rings at high level
 *
 * Copyright (C) 1999-2014, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_flowrings.h  jaganlv $
 */

/****************
 * Common types *
 */

#ifndef _dhd_flowrings_h_
#define _dhd_flowrings_h_

/* Max pkts held in a flow ring's backup queue */
#define FLOW_RING_QUEUE_THRESHOLD       (2048)

/* Number of H2D common rings : PCIE Spec Rev? */
#define FLOW_RING_COMMON                2

#define FLOWID_INVALID                  (ID16_INVALID)
#define FLOWID_RESERVED                 (FLOW_RING_COMMON)

#define FLOW_RING_STATUS_OPEN           0
#define FLOW_RING_STATUS_PENDING        1
#define FLOW_RING_STATUS_CLOSED         2
#define FLOW_RING_STATUS_DELETE_PENDING 3
#define FLOW_RING_STATUS_FLUSH_PENDING  4

#define DHD_FLOWRING_RX_BUFPOST_PKTSZ	2048

#define DHD_FLOW_PRIO_AC_MAP		0
#define DHD_FLOW_PRIO_TID_MAP		1


/* Pkttag not compatible with PROP_TXSTATUS or WLFC */
typedef struct dhd_pkttag_fr {
	uint16  flowid;
	int     dataoff;
} dhd_pkttag_fr_t;

#define DHD_PKTTAG_SET_FLOWID(tag, flow)    ((tag)->flowid = (uint16)(flow))
#define DHD_PKTTAG_SET_DATAOFF(tag, offset) ((tag)->dataoff = (int)(offset))

#define DHD_PKTTAG_FLOWID(tag)              ((tag)->flowid)
#define DHD_PKTTAG_DATAOFF(tag)             ((tag)->dataoff)

/* Hashing a MacAddress for lkup into a per interface flow hash table */
#define DHD_FLOWRING_HASH_SIZE    256
#define	DHD_FLOWRING_HASHINDEX(ea, prio) \
	       ((((uint8 *)(ea))[3] ^ ((uint8 *)(ea))[4] ^ ((uint8 *)(ea))[5] ^ ((uint8)(prio))) \
		% DHD_FLOWRING_HASH_SIZE)

#define DHD_IF_ROLE(pub, idx)		(((if_flow_lkup_t *)(pub)->if_flow_lkup)[idx].role)
#define DHD_IF_ROLE_AP(pub, idx)	(DHD_IF_ROLE(pub, idx) == WLC_E_IF_ROLE_AP)
#define DHD_IF_ROLE_P2PGO(pub, idx)	(DHD_IF_ROLE(pub, idx) == WLC_E_IF_ROLE_P2P_GO)
#define DHD_FLOW_RING(dhdp, flowid) \
	(flow_ring_node_t *)&(((flow_ring_node_t *)((dhdp)->flow_ring_table))[flowid])

struct flow_queue;

/* Flow Ring Queue Enqueue overflow callback */
typedef int (*flow_queue_cb_t)(struct flow_queue * queue, void * pkt);

typedef struct flow_queue {
	dll_t  list;                /* manage a flowring queue in a dll */
	void * head;                /* first packet in the queue */
	void * tail;                /* last packet in the queue */
	uint16 len;                 /* number of packets in the queue */
	uint16 max;                 /* maximum number of packets, queue may hold */
	uint32 failures;            /* enqueue failures due to queue overflow */
	flow_queue_cb_t cb;         /* callback invoked on threshold crossing */
	void * lock;		/* OS specific lock handle for Q access protection */
} flow_queue_t;

#define flow_queue_len(queue)   ((int)(queue)->len)
#define flow_queue_max(queue)   ((int)(queue)->max)
#define flow_queue_avail(queue) ((int)((queue)->max - (queue)->len))
#define flow_queue_full(queue)  ((queue)->len >= (queue)->max)
#define flow_queue_empty(queue) ((queue)->len == 0)

typedef struct flow_info {
	uint8		tid;
	uint8		ifindex;
	char		sa[ETHER_ADDR_LEN];
	char		da[ETHER_ADDR_LEN];
} flow_info_t;

typedef struct flow_ring_node {
	dll_t		list; /* manage a constructed flowring in a dll, must be at first place */
	flow_queue_t	queue;
	bool		active;
	uint8		status;
	uint16		flowid;
	flow_info_t	flow_info;
	void		*prot_info;
} flow_ring_node_t;
typedef flow_ring_node_t flow_ring_table_t;

typedef struct flow_hash_info {
	uint16			flowid;
	flow_info_t		flow_info;
	struct flow_hash_info	*next;
} flow_hash_info_t;

typedef struct if_flow_lkup {
	bool		status;
	uint8		role; /* Interface role: STA/AP */
	flow_hash_info_t *fl_hash[DHD_FLOWRING_HASH_SIZE]; /* Lkup Hash table */
} if_flow_lkup_t;

static INLINE flow_ring_node_t *
dhd_constlist_to_flowring(dll_t *item)
{
	return ((flow_ring_node_t *)item);
}

/* Exported API */

/* Flow ring's queue management functions */
extern void dhd_flow_queue_init(dhd_pub_t *dhdp, flow_queue_t *queue, int max);
extern void dhd_flow_queue_register(flow_queue_t *queue, flow_queue_cb_t cb);
extern int  dhd_flow_queue_enqueue(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt);
extern void * dhd_flow_queue_dequeue(dhd_pub_t *dhdp, flow_queue_t *queue);
extern void dhd_flow_queue_reinsert(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt);

extern int  dhd_flow_rings_init(dhd_pub_t *dhdp, uint32 num_flow_rings);

extern void dhd_flow_rings_deinit(dhd_pub_t *dhdp);

extern int dhd_flowid_update(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio,
                void *pktbuf);

extern void dhd_flowid_free(dhd_pub_t *dhdp, uint8 ifindex, uint16 flowid);

extern void dhd_flow_rings_delete(dhd_pub_t *dhdp, uint8 ifindex);

extern void dhd_flow_rings_delete_for_peer(dhd_pub_t *dhdp, uint8 ifindex,
                char *addr);

/* Handle Interface ADD, DEL operations */
extern void dhd_update_interface_flow_info(dhd_pub_t *dhdp, uint8 ifindex,
                uint8 op, uint8 role);

/* Handle a STA interface link status update */
extern int dhd_update_interface_link_status(dhd_pub_t *dhdp, uint8 ifindex,
                uint8 status);
extern int dhd_flow_prio_map(dhd_pub_t *dhd, uint8 *map, bool set);
extern int dhd_update_flow_prio_map(dhd_pub_t *dhdp, uint8 map);

extern uint8 dhd_flow_rings_ifindex2role(dhd_pub_t *dhdp, uint8 ifindex);
#endif /* _dhd_flowrings_h_ */
