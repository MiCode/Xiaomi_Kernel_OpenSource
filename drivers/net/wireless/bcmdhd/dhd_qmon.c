/*
 * Queue monitoring.
 *
 * The feature allows monitoring the DHD queue utilization to get the percentage of a time period
 * where the number of pending packets is above a configurable theshold.
 * Right now, this is used by a server application, interfacing a Miracast Video Encoder, and
 * doing IOVAR "qtime_percent" at regular interval. Based on IOVAR "qtime_percent" results,
 * the server indicates to the Video Encoder if its bitrate can be increased or must be decreased.
 * Currently, this works only with P2P interfaces and with PROP_TXSTATUS. There is no need to handle
 * concurrent access to the fieds because the existing concurrent accesses are protected
 * by the PROP_TXSTATUS's lock.
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: dhd_qmon.c 309265 2012-01-19 02:50:46Z $
 *
 */
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <dngl_stats.h>
#include <wlioctl.h>
#include <dhd.h>
#include <dhd_qmon.h>
#ifndef PROP_TXSTATUS
#error "PROP_TXSTATUS must be build to build dhd_qmon.c"
#endif
#include <wlfc_proto.h>
#include <dhd_wlfc.h>

#if defined(BCMDRIVER)
#define QMON_SYSUPTIME() ((uint64)(jiffies_to_usecs(jiffies)))
#else
	#error "target not yet supported"
#endif

static dhd_qmon_t *
dhd_qmon_p2p_entry(dhd_pub_t *dhdp)
{
	wlfc_mac_descriptor_t* interfaces = NULL;
	wlfc_mac_descriptor_t* nodes = NULL;
	uint8 i;

	if (dhdp->wlfc_state == NULL)
		return NULL;

	interfaces = ((athost_wl_status_info_t*)dhdp->wlfc_state)->destination_entries.interfaces;
	nodes =  ((athost_wl_status_info_t*)dhdp->wlfc_state)->destination_entries.nodes;

	ASSERT(interfaces != NULL);
	ASSERT(nodes != NULL);

	for (i = 0; i < WLFC_MAC_DESC_TABLE_SIZE; i++) {
		if (nodes[i].occupied &&
		    ((nodes[i].iftype == WLC_E_IF_ROLE_P2P_CLIENT) ||
		     (nodes[i].iftype == WLC_E_IF_ROLE_P2P_GO)))
			return &nodes[i].qmon;
	}

	for (i = 0; i < WLFC_MAX_IFNUM; i++) {
		if (interfaces[i].occupied &&
		    ((interfaces[i].iftype == WLC_E_IF_ROLE_P2P_CLIENT) ||
		     (interfaces[i].iftype == WLC_E_IF_ROLE_P2P_GO)))
		    return &nodes[i].qmon;
	}

	return NULL;
}

void
dhd_qmon_reset(dhd_qmon_t* qmon)
{
	qmon->transitq_count = 0;
	qmon->queued_time_cumul = 0;
	qmon->queued_time_cumul_last = 0;
	qmon->queued_time_last = 0;
	qmon->queued_time_last_io = 0;
}

void
dhd_qmon_tx(dhd_qmon_t* qmon)
{
	if ((++qmon->transitq_count > qmon->queued_time_thres) &&
	    (qmon->queued_time_last == 0)) {
		/* Set timestamp when transit packet above a threshold */
		qmon->queued_time_last = QMON_SYSUPTIME();
	}
}

void
dhd_qmon_txcomplete(dhd_qmon_t* qmon)
{
	uint64 now = QMON_SYSUPTIME();

	qmon->transitq_count--;
	if ((qmon->transitq_count <= qmon->queued_time_thres) &&
	    (qmon->queued_time_last != 0)) {
		/* Set timestamp when transit packet above a threshold */
		qmon->queued_time_cumul += now - qmon->queued_time_last;
		qmon->queued_time_last = 0;
	}
}

int
dhd_qmon_thres(dhd_pub_t *dhdp, int set, int setval)
{
	int val = 0;
	dhd_qmon_t* qmon = dhd_qmon_p2p_entry(dhdp);

	if (qmon == NULL)
		return 0;

	if (set)
		qmon->queued_time_thres = setval;
	else
		val = qmon->queued_time_thres;

	return val;
}


int
dhd_qmon_getpercent(dhd_pub_t *dhdp)
{
	int percent = 0;
	uint64 time_cumul_adjust = 0;
	uint64 now =  QMON_SYSUPTIME();
	dhd_qmon_t* qmon = dhd_qmon_p2p_entry(dhdp);
	uint64 queued_time_cumul = 0;
	uint64 queued_time_last = 0;

	if (qmon == NULL)
		return 0;

	queued_time_cumul = qmon->queued_time_cumul;
	queued_time_last = qmon->queued_time_last;

	if (queued_time_last)
		time_cumul_adjust = now - queued_time_last;

	percent = (uint32)((time_cumul_adjust + queued_time_cumul
	                    - qmon->queued_time_cumul_last) * 100) /
	                    (uint32)(now - qmon->queued_time_last_io);

	qmon->queued_time_cumul_last = queued_time_cumul + time_cumul_adjust;
	qmon->queued_time_last_io = now;

	return percent;
}
