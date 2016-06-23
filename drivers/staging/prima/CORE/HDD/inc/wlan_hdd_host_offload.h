/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#ifndef __WLAN_HDD_HOST_OFFLOAD_H__
#define __WLAN_HDD_HOST_OFFLOAD_H__

/**===========================================================================

  \file  wlan_hdd_host_offload.h

  \brief Android WLAN HDD Host Offload API

  Copyright 2011 (c) QUALCOMM Incorporated. All Rights Reserved.
  QUALCOMM Proprietary and Confidential.

  ==========================================================================*/

/* Offload types. */
#define WLAN_IPV4_ARP_REPLY_OFFLOAD           0
#define WLAN_IPV6_NEIGHBOR_DISCOVERY_OFFLOAD  1

/* Enable or disable offload. */
#define WLAN_OFFLOAD_DISABLE                     0
#define WLAN_OFFLOAD_ENABLE                      0x1
#define WLAN_OFFLOAD_BC_FILTER_ENABLE            0x2
#define WLAN_OFFLOAD_ARP_AND_BC_FILTER_ENABLE    (WLAN_OFFLOAD_ENABLE | WLAN_OFFLOAD_BC_FILTER_ENABLE)

/* Offload request. */
typedef struct
{
    v_U8_t offloadType;
    v_U8_t enableOrDisable;
    union
    {
        v_U8_t hostIpv4Addr [4];
        v_U8_t hostIpv6Addr [16];
    } params;
    v_MACADDR_t bssId;
} tHostOffloadRequest, *tpHostOffloadRequest;

#endif // __WLAN_HDD_HOST_OFFLOAD_H__

