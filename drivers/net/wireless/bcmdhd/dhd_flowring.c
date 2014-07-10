/*
 * Broadcom Dongle Host Driver (DHD), Flow ring specific code at top level
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
 * $Id: dhd_flowrings.c jaganlv $
 */

#include <typedefs.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <bcmdevs.h>

#include <proto/ethernet.h>
#include <proto/bcmevent.h>
#include <dngl_stats.h>

#include <dhd.h>

#include <dhd_flowring.h>
#include <dhd_bus.h>
#include <dhd_proto.h>
#include <dhd_dbg.h>
#include <proto/802.1d.h>

static INLINE uint16 dhd_flowid_find(dhd_pub_t *dhdp, uint8 ifindex,
                                     uint8 prio, char *sa, char *da);

static INLINE uint16 dhd_flowid_alloc(dhd_pub_t *dhdp, uint8 ifindex,
                                      uint8 prio, char *sa, char *da);

static INLINE int dhd_flowid_lookup(dhd_pub_t *dhdp, uint8 ifindex,
                                uint8 prio, char *sa, char *da, uint16 *flowid);
int BCMFASTPATH dhd_flow_queue_overflow(flow_queue_t *queue, void *pkt);

#define FLOW_QUEUE_PKT_NEXT(p)          PKTLINK(p)
#define FLOW_QUEUE_PKT_SETNEXT(p, x)    PKTSETLINK((p), (x))

const uint8 prio2ac[8] = { 0, 1, 1, 0, 2, 2, 3, 3 };
const uint8 prio2tid[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

int BCMFASTPATH
dhd_flow_queue_overflow(flow_queue_t *queue, void *pkt)
{
	return BCME_NORESOURCE;
}

/* Flow ring's queue management functions */

void /* Initialize a flow ring's queue */
dhd_flow_queue_init(dhd_pub_t *dhdp, flow_queue_t *queue, int max)
{
	ASSERT((queue != NULL) && (max > 0));

	dll_init(&queue->list);
	queue->head = queue->tail = NULL;
	queue->len = 0;
	queue->max = max - 1;
	queue->failures = 0U;
	queue->cb = &dhd_flow_queue_overflow;
	queue->lock = dhd_os_spin_lock_init(dhdp->osh);

	if (queue->lock == NULL)
		DHD_ERROR(("%s: Failed to init spinlock for queue!\n", __FUNCTION__));
}

void /* Register an enqueue overflow callback handler */
dhd_flow_queue_register(flow_queue_t *queue, flow_queue_cb_t cb)
{
	ASSERT(queue != NULL);
	queue->cb = cb;
}


int BCMFASTPATH /* Enqueue a packet in a flow ring's queue */
dhd_flow_queue_enqueue(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt)
{
	int ret = BCME_OK;

	ASSERT(queue != NULL);

	if (queue->len >= queue->max) {
		queue->failures++;
		ret = (*queue->cb)(queue, pkt);
		goto done;
	}

	if (queue->head) {
		FLOW_QUEUE_PKT_SETNEXT(queue->tail, pkt);
	} else {
		queue->head = pkt;
	}

	FLOW_QUEUE_PKT_SETNEXT(pkt, NULL);

	queue->tail = pkt; /* at tail */

	queue->len++;

done:
	return ret;
}

void * BCMFASTPATH /* Dequeue a packet from a flow ring's queue, from head */
dhd_flow_queue_dequeue(dhd_pub_t *dhdp, flow_queue_t *queue)
{
	void * pkt;

	ASSERT(queue != NULL);

	pkt = queue->head; /* from head */

	if (pkt == NULL) {
		ASSERT((queue->len == 0) && (queue->tail == NULL));
		goto done;
	}

	queue->head = FLOW_QUEUE_PKT_NEXT(pkt);
	if (queue->head == NULL)
		queue->tail = NULL;

	queue->len--;

	FLOW_QUEUE_PKT_SETNEXT(pkt, NULL); /* dettach packet from queue */

done:
	return pkt;
}

void BCMFASTPATH /* Reinsert a dequeued packet back at the head */
dhd_flow_queue_reinsert(dhd_pub_t *dhdp, flow_queue_t *queue, void *pkt)
{
	if (queue->head == NULL) {
		queue->tail = pkt;
	}

	FLOW_QUEUE_PKT_SETNEXT(pkt, queue->head);
	queue->head = pkt;
	queue->len++;
}


/* Init Flow Ring specific data structures */
int
dhd_flow_rings_init(dhd_pub_t *dhdp, uint32 num_flow_rings)
{
	uint32 idx;
	uint32 flow_ring_table_sz;
	uint32 if_flow_lkup_sz;
	void * flowid_allocator;
	flow_ring_table_t *flow_ring_table;
	if_flow_lkup_t *if_flow_lkup;

	DHD_INFO(("%s\n", __FUNCTION__));

	/* Construct a 16bit flow1d allocator */
	flowid_allocator = id16_map_init(dhdp->osh,
	                       num_flow_rings - FLOW_RING_COMMON, FLOWID_RESERVED);
	if (flowid_allocator == NULL) {
		DHD_ERROR(("%s: flowid allocator init failure\n", __FUNCTION__));
		return BCME_ERROR;
	}

	/* Allocate a flow ring table, comprising of requested number of rings */
	flow_ring_table_sz = (num_flow_rings * sizeof(flow_ring_node_t));
	flow_ring_table = (flow_ring_table_t *)MALLOC(dhdp->osh, flow_ring_table_sz);
	if (flow_ring_table == NULL) {
		DHD_ERROR(("%s: flow ring table alloc failure\n", __FUNCTION__));
		id16_map_fini(dhdp->osh, flowid_allocator);
		return BCME_ERROR;
	}

	/* Initialize flow ring table state */
	bzero((uchar *)flow_ring_table, flow_ring_table_sz);
	for (idx = 0; idx < num_flow_rings; idx++) {
		flow_ring_table[idx].status = FLOW_RING_STATUS_CLOSED;
		flow_ring_table[idx].flowid = (uint16)idx;
		dll_init(&flow_ring_table[idx].list);

		/* Initialize the per flow ring backup queue */
		dhd_flow_queue_init(dhdp, &flow_ring_table[idx].queue,
		                    FLOW_RING_QUEUE_THRESHOLD);
	}

	/* Allocate per interface hash table */
	if_flow_lkup_sz = sizeof(if_flow_lkup_t) * DHD_MAX_IFS;
	if_flow_lkup = (if_flow_lkup_t *)MALLOC(dhdp->osh, if_flow_lkup_sz);
	if (if_flow_lkup == NULL) {
		DHD_ERROR(("%s: if flow lkup alloc failure\n", __FUNCTION__));
		MFREE(dhdp->osh, flow_ring_table, flow_ring_table_sz);
		id16_map_fini(dhdp->osh, flowid_allocator);
		return BCME_ERROR;
	}

	/* Initialize per interface hash table */
	bzero((uchar *)if_flow_lkup, if_flow_lkup_sz);
	for (idx = 0; idx < DHD_MAX_IFS; idx++) {
		int hash_ix;
		if_flow_lkup[idx].status = 0;
		if_flow_lkup[idx].role = 0;
		for (hash_ix = 0; hash_ix < DHD_FLOWRING_HASH_SIZE; hash_ix++)
			if_flow_lkup[idx].fl_hash[hash_ix] = NULL;
	}

	/* Now populate into dhd pub */
	dhdp->num_flow_rings = num_flow_rings;
	dhdp->flowid_allocator = (void *)flowid_allocator;
	dhdp->flow_ring_table = (void *)flow_ring_table;
	dhdp->if_flow_lkup = (void *)if_flow_lkup;

	dhdp->flow_prio_map_type = DHD_FLOW_PRIO_AC_MAP;
	bcopy(prio2ac, dhdp->flow_prio_map, sizeof(uint8) * NUMPRIO);

	DHD_INFO(("%s done\n", __FUNCTION__));
	return BCME_OK;
}

/* Deinit Flow Ring specific data structures */
void dhd_flow_rings_deinit(dhd_pub_t *dhdp)
{
	uint16 idx;
	uint32 flow_ring_table_sz;
	uint32 if_flow_lkup_sz;
	flow_ring_table_t *flow_ring_table;
	DHD_INFO(("dhd_flow_rings_deinit\n"));

	if (dhdp->flow_ring_table != NULL) {

		ASSERT(dhdp->num_flow_rings > 0);

		flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;
		for (idx = 0; idx < dhdp->num_flow_rings; idx++) {
			if (flow_ring_table[idx].active) {
				dhd_bus_clean_flow_ring(dhdp->bus, idx);
			}
			ASSERT(flow_queue_empty(&flow_ring_table[idx].queue));

			/* Deinit flow ring queue locks before destroying flow ring table */
			dhd_os_spin_lock_deinit(dhdp->osh, flow_ring_table[idx].queue.lock);
			flow_ring_table[idx].queue.lock = NULL;
		}

		/* Destruct the flow ring table */
		flow_ring_table_sz = dhdp->num_flow_rings * sizeof(flow_ring_table_t);
		MFREE(dhdp->osh, dhdp->flow_ring_table, flow_ring_table_sz);
		dhdp->flow_ring_table = NULL;
	}

	/* Destruct the per interface flow lkup table */
	if (dhdp->if_flow_lkup != NULL) {
		if_flow_lkup_sz = sizeof(if_flow_lkup_t) * DHD_MAX_IFS;
		MFREE(dhdp->osh, dhdp->if_flow_lkup, if_flow_lkup_sz);
		dhdp->if_flow_lkup = NULL;
	}

	/* Destruct the flowid allocator */
	if (dhdp->flowid_allocator != NULL)
		dhdp->flowid_allocator = id16_map_fini(dhdp->osh, dhdp->flowid_allocator);

	dhdp->num_flow_rings = 0U;
}

uint8
dhd_flow_rings_ifindex2role(dhd_pub_t *dhdp, uint8 ifindex)
{
	if_flow_lkup_t *if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
	ASSERT(if_flow_lkup);
	return if_flow_lkup[ifindex].role;
}

#ifdef WLTDLS
bool is_tdls_destination(dhd_pub_t *dhdp, uint8 *da)
{
	tdls_peer_node_t *cur = dhdp->peer_tbl.node;
	while (cur != NULL) {
		if (!memcmp(da, cur->addr, ETHER_ADDR_LEN)) {
			return TRUE;
		}
		cur = cur->next;
	}
	return FALSE;
}
#endif /* WLTDLS */

/* For a given interface, search the hash table for a matching flow */
static INLINE uint16
dhd_flowid_find(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio, char *sa, char *da)
{
	int hash;
	bool ismcast = FALSE;
	flow_hash_info_t *cur;
	if_flow_lkup_t *if_flow_lkup;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

	if (if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_STA) {
#ifdef WLTDLS
		if (dhdp->peer_tbl.tdls_peer_count && !(ETHER_ISMULTI(da)) &&
			is_tdls_destination(dhdp, da)) {
			hash = DHD_FLOWRING_HASHINDEX(da, prio);
			cur = if_flow_lkup[ifindex].fl_hash[hash];
			while (cur != NULL) {
				if (!memcmp(cur->flow_info.da, da, ETHER_ADDR_LEN))
					return cur->flowid;
				cur = cur->next;
			}
			return FLOWID_INVALID;
		}
#endif /* WLTDLS */
		cur = if_flow_lkup[ifindex].fl_hash[prio];
		if (cur) {
			return cur->flowid;
		}

	} else {

		if (ETHER_ISMULTI(da)) {
			ismcast = TRUE;
			hash = 0;
		} else {
			hash = DHD_FLOWRING_HASHINDEX(da, prio);
		}

		cur = if_flow_lkup[ifindex].fl_hash[hash];

		while (cur) {
			if ((ismcast && ETHER_ISMULTI(cur->flow_info.da)) ||
				(!memcmp(cur->flow_info.da, da, ETHER_ADDR_LEN) &&
				(cur->flow_info.tid == prio))) {
				return cur->flowid;
			}
			cur = cur->next;
		}
	}

	return FLOWID_INVALID;
}

/* Allocate Flow ID */
static INLINE uint16
dhd_flowid_alloc(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio, char *sa, char *da)
{
	flow_hash_info_t *fl_hash_node, *cur;
	if_flow_lkup_t *if_flow_lkup;
	int hash;
	uint16 flowid;

	fl_hash_node = (flow_hash_info_t *) MALLOC(dhdp->osh, sizeof(flow_hash_info_t));
	memcpy(fl_hash_node->flow_info.da, da, sizeof(fl_hash_node->flow_info.da));

	ASSERT(dhdp->flowid_allocator != NULL);
	flowid = id16_map_alloc(dhdp->flowid_allocator);

	if (flowid == FLOWID_INVALID) {
		MFREE(dhdp->osh, fl_hash_node,  sizeof(flow_hash_info_t));
		DHD_ERROR(("%s: cannot get free flowid \n", __FUNCTION__));
		return FLOWID_INVALID;
	}

	fl_hash_node->flowid = flowid;
	fl_hash_node->flow_info.tid = prio;
	fl_hash_node->flow_info.ifindex = ifindex;
	fl_hash_node->next = NULL;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
	if (if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_STA) {
		/* For STA non TDLS dest we allocate entry based on prio only */
#ifdef WLTDLS
		if (dhdp->peer_tbl.tdls_peer_count &&
			(is_tdls_destination(dhdp, da))) {
			hash = DHD_FLOWRING_HASHINDEX(da, prio);
			cur = if_flow_lkup[ifindex].fl_hash[hash];
			if (cur) {
				while (cur->next) {
					cur = cur->next;
				}
				cur->next = fl_hash_node;
			} else {
				if_flow_lkup[ifindex].fl_hash[hash] = fl_hash_node;
			}
		} else
#endif /* WLTDLS */
			if_flow_lkup[ifindex].fl_hash[prio] = fl_hash_node;
	} else {

		/* For bcast/mcast assign first slot in in interface */
		hash = ETHER_ISMULTI(da) ? 0 : DHD_FLOWRING_HASHINDEX(da, prio);
		cur = if_flow_lkup[ifindex].fl_hash[hash];
		if (cur) {
			while (cur->next) {
				cur = cur->next;
			}
			cur->next = fl_hash_node;
		} else
			if_flow_lkup[ifindex].fl_hash[hash] = fl_hash_node;
	}

	DHD_INFO(("%s: allocated flowid %d\n", __FUNCTION__, fl_hash_node->flowid));

	return fl_hash_node->flowid;
}

/* Get flow ring ID, if not present try to create one */
static INLINE int
dhd_flowid_lookup(dhd_pub_t *dhdp, uint8 ifindex,
                  uint8 prio, char *sa, char *da, uint16 *flowid)
{
	uint16 id;
	flow_ring_node_t *flow_ring_node;
	flow_ring_table_t *flow_ring_table;

	DHD_INFO(("%s\n", __FUNCTION__));

	flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;

	id = dhd_flowid_find(dhdp, ifindex, prio, sa, da);

	if (id == FLOWID_INVALID) {

		if_flow_lkup_t *if_flow_lkup;
		if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

		if (!if_flow_lkup[ifindex].status)
			return BCME_ERROR;

		id = dhd_flowid_alloc(dhdp, ifindex, prio, sa, da);
		if (id == FLOWID_INVALID) {
			DHD_ERROR(("%s: alloc flowid ifindex %u status %u\n",
			           __FUNCTION__, ifindex, if_flow_lkup[ifindex].status));
			return BCME_ERROR;
		}

		/* register this flowid in dhd_pub */
		dhd_add_flowid(dhdp, ifindex, prio, da, id);
	}

	ASSERT(id < dhdp->num_flow_rings);

	flow_ring_node = (flow_ring_node_t *) &flow_ring_table[id];
	if (flow_ring_node->active) {
		*flowid = id;
		return BCME_OK;
	}

	/* flow_ring_node->flowid = id; */

	/* Init Flow info */
	memcpy(flow_ring_node->flow_info.sa, sa, sizeof(flow_ring_node->flow_info.sa));
	memcpy(flow_ring_node->flow_info.da, da, sizeof(flow_ring_node->flow_info.da));
	flow_ring_node->flow_info.tid = prio;
	flow_ring_node->flow_info.ifindex = ifindex;

	/* Create and inform device about the new flow */
	if (dhd_bus_flow_ring_create_request(dhdp->bus, (void *)flow_ring_node)
	        != BCME_OK) {
		DHD_ERROR(("%s: create error %d\n", __FUNCTION__, id));
		return BCME_ERROR;
	}
	flow_ring_node->active = TRUE;

	*flowid = id;
	return BCME_OK;
}

/* Update flowid information on the packet */
int BCMFASTPATH
dhd_flowid_update(dhd_pub_t *dhdp, uint8 ifindex, uint8 prio, void *pktbuf)
{
	uint8 *pktdata = (uint8 *)PKTDATA(dhdp->osh, pktbuf);
	struct ether_header *eh = (struct ether_header *)pktdata;
	uint16 flowid;

	if (dhd_bus_is_txmode_push(dhdp->bus))
		return BCME_OK;

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS) {
		return BCME_BADARG;
	}

	if (!dhdp->flowid_allocator) {
		DHD_ERROR(("%s: Flow ring not intited yet  \n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (dhd_flowid_lookup(dhdp, ifindex, prio, eh->ether_shost, eh->ether_dhost,
		&flowid) != BCME_OK) {
		return BCME_ERROR;
	}

	DHD_INFO(("%s: prio %d flowid %d\n", __FUNCTION__, prio, flowid));

	/* Tag the packet with flowid */
	DHD_PKTTAG_SET_FLOWID((dhd_pkttag_fr_t *)PKTTAG(pktbuf), flowid);
	return BCME_OK;
}

void
dhd_flowid_free(dhd_pub_t *dhdp, uint8 ifindex, uint16 flowid)
{
	int hashix;
	bool found = FALSE;
	flow_hash_info_t *cur, *prev;
	if_flow_lkup_t *if_flow_lkup;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

	for (hashix = 0; hashix < DHD_FLOWRING_HASH_SIZE; hashix++) {

		cur = if_flow_lkup[ifindex].fl_hash[hashix];

		if (cur) {
			if (cur->flowid == flowid) {
				found = TRUE;
			}

			prev = NULL;
			while (!found && cur) {
				if (cur->flowid == flowid) {
					found = TRUE;
					break;
				}
				prev = cur;
				cur = cur->next;
			}
			if (found) {
				if (!prev) {
					if_flow_lkup[ifindex].fl_hash[hashix] = cur->next;
				} else {
					prev->next = cur->next;
				}

				/* deregister flowid from dhd_pub. */
				dhd_del_flowid(dhdp, ifindex, flowid);

				id16_map_free(dhdp->flowid_allocator, flowid);
				MFREE(dhdp->osh, cur, sizeof(flow_hash_info_t));

				return;
			}
		}
	}

	DHD_ERROR(("%s: could not free flow ring hash entry flowid %d\n",
	           __FUNCTION__, flowid));
}


/* Delete all Flow rings assocaited with the given Interface */
void
dhd_flow_rings_delete(dhd_pub_t *dhdp, uint8 ifindex)
{
	uint32 id;
	flow_ring_table_t *flow_ring_table;

	DHD_INFO(("%s: ifindex %u\n", __FUNCTION__, ifindex));

	ASSERT(ifindex < DHD_MAX_IFS);
	if (!dhdp->flow_ring_table)
		return;

	flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;
	for (id = 0; id < dhdp->num_flow_rings; id++) {
		if (flow_ring_table[id].active &&
		    (flow_ring_table[id].flow_info.ifindex == ifindex)) {
			dhd_bus_flow_ring_delete_request(dhdp->bus,
			                                 (void *) &flow_ring_table[id]);
		}
	}
}

/* Delete flow/s for given peer address */
void
dhd_flow_rings_delete_for_peer(dhd_pub_t *dhdp, uint8 ifindex, char *addr)
{
	uint32 id;
	flow_ring_table_t *flow_ring_table;

	DHD_ERROR(("%s: ifindex %u\n", __FUNCTION__, ifindex));

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS)
		return;

	if (!dhdp->flow_ring_table)
		return;

	flow_ring_table = (flow_ring_table_t *)dhdp->flow_ring_table;
	for (id = 0; id < dhdp->num_flow_rings; id++) {
		if (flow_ring_table[id].active &&
		    (flow_ring_table[id].flow_info.ifindex == ifindex) &&
		    (!memcmp(flow_ring_table[id].flow_info.da, addr, ETHER_ADDR_LEN)) &&
		    (flow_ring_table[id].status != FLOW_RING_STATUS_DELETE_PENDING)) {
			DHD_INFO(("%s: deleting flowid %d\n",
			          __FUNCTION__, flow_ring_table[id].flowid));
			dhd_bus_flow_ring_delete_request(dhdp->bus,
			                                 (void *) &flow_ring_table[id]);
		}
	}
}

/* Handle Interface ADD, DEL operations */
void
dhd_update_interface_flow_info(dhd_pub_t *dhdp, uint8 ifindex,
                               uint8 op, uint8 role)
{
	if_flow_lkup_t *if_flow_lkup;

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS)
		return;

	DHD_INFO(("%s: ifindex %u op %u role is %u \n",
	          __FUNCTION__, ifindex, op, role));
	if (!dhdp->flowid_allocator) {
		DHD_ERROR(("%s: Flow ring not intited yet  \n", __FUNCTION__));
		return;
	}

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;

	if (op == WLC_E_IF_ADD || op == WLC_E_IF_CHANGE) {

		if_flow_lkup[ifindex].role = role;

		if (role != WLC_E_IF_ROLE_STA) {
			if_flow_lkup[ifindex].status = TRUE;
			DHD_INFO(("%s: Mcast Flow ring for ifindex %d role is %d \n",
			          __FUNCTION__, ifindex, role));
			/* Create Mcast Flow */
		}
	} else	if (op == WLC_E_IF_DEL) {
		if_flow_lkup[ifindex].status = FALSE;
		DHD_INFO(("%s: cleanup all Flow rings for ifindex %d role is %d \n",
		          __FUNCTION__, ifindex, role));
	}
}

/* Handle a STA interface link status update */
int
dhd_update_interface_link_status(dhd_pub_t *dhdp, uint8 ifindex, uint8 status)
{
	if_flow_lkup_t *if_flow_lkup;

	ASSERT(ifindex < DHD_MAX_IFS);
	if (ifindex >= DHD_MAX_IFS)
		return BCME_BADARG;

	if_flow_lkup = (if_flow_lkup_t *)dhdp->if_flow_lkup;
	DHD_INFO(("%s: ifindex %d status %d\n", __FUNCTION__, ifindex, status));

	if (if_flow_lkup[ifindex].role == WLC_E_IF_ROLE_STA) {
		if (status)
			if_flow_lkup[ifindex].status = TRUE;
		else
			if_flow_lkup[ifindex].status = FALSE;
	}
	return BCME_OK;
}
/* Update flow priority mapping */
int dhd_update_flow_prio_map(dhd_pub_t *dhdp, uint8 map)
{
	uint16 flowid;
	flow_ring_node_t *flow_ring_node;

	if (map > DHD_FLOW_PRIO_TID_MAP)
		return BCME_BADOPTION;

	/* Check if we need to change prio map */
	if (map == dhdp->flow_prio_map_type)
		return BCME_OK;

	/* If any ring is active we cannot change priority mapping for flow rings */
	for (flowid = 0; flowid < dhdp->num_flow_rings; flowid++) {
		flow_ring_node = DHD_FLOW_RING(dhdp, flowid);
		if (flow_ring_node->active)
			return BCME_EPERM;
	}
	/* Infor firmware about new mapping type */
	if (BCME_OK != dhd_flow_prio_map(dhdp, &map, TRUE))
		return BCME_ERROR;

	/* update internal structures */
	dhdp->flow_prio_map_type = map;
	if (dhdp->flow_prio_map_type == DHD_FLOW_PRIO_TID_MAP)
		bcopy(prio2tid, dhdp->flow_prio_map, sizeof(uint8) * NUMPRIO);
	else
		bcopy(prio2ac, dhdp->flow_prio_map, sizeof(uint8) * NUMPRIO);

	return BCME_OK;
}

/* Set/Get flwo ring priority map */
int dhd_flow_prio_map(dhd_pub_t *dhd, uint8 *map, bool set)
{
	uint8 iovbuf[24];
	if (!set) {
		bcm_mkiovar("bus:fl_prio_map", NULL, 0, (char*)iovbuf, sizeof(iovbuf));
		if (dhd_wl_ioctl_cmd(dhd, WLC_GET_VAR, iovbuf, sizeof(iovbuf), FALSE, 0) < 0) {
			DHD_ERROR(("%s: failed to get fl_prio_map\n", __FUNCTION__));
			return BCME_ERROR;
		}
		*map = iovbuf[0];
		return BCME_OK;
	}
	bcm_mkiovar("bus:fl_prio_map", (char *)map, 4, (char*)iovbuf, sizeof(iovbuf));
	if (dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0) < 0) {
		DHD_ERROR(("%s: failed to set fl_prio_map \n",
			__FUNCTION__));
		return BCME_ERROR;
	}
	return BCME_OK;
}
