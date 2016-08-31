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
 * $Id: dhd_qmon.h 309265 2012-01-19 02:50:46Z $
 *
 */
#ifndef _dhd_qmon_h_
#define _dhd_qmon_h_


typedef struct dhd_qmon_s {
	uint32	transitq_count;
	uint32  queued_time_thres;
	uint64  queued_time_cumul;
	uint64  queued_time_cumul_last;
	uint64  queued_time_last;
	uint64  queued_time_last_io;
} dhd_qmon_t;


extern void dhd_qmon_reset(dhd_qmon_t* entry);
extern void dhd_qmon_tx(dhd_qmon_t* entry);
extern void dhd_qmon_txcomplete(dhd_qmon_t* entry);
extern int dhd_qmon_getpercent(dhd_pub_t *dhdp);
extern int dhd_qmon_thres(dhd_pub_t *dhdp, int set, int setval);


#endif	/* _dhd_qmon_h_ */
