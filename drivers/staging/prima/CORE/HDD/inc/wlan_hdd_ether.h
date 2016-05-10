/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_HDD_ETHER_H
#define _WLAN_HDD_ETHER_H
/*============================================================================
  @file wlan_hdd_ether.h

  This module describes Ethernet packet formats for processing by HDD.

               Copyright (c) 2009 QUALCOMM Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary
============================================================================*/
/* $Header$ */

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include <linux/version.h>
#include <linux/byteorder/generic.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define WLAN_SNAP_OUI_LEN 3
#define WLAN_SNAP_DSAP 0xAAU
#define WLAN_SNAP_SSAP 0xAAU
#define WLAN_SNAP_CTRL 0x03
#define WLAN_MIN_PROTO 0x0600

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
struct wlan_snap_hdr {
   unsigned char dsap;
   unsigned char ssap;
   unsigned char ctrl;
   unsigned char oui[WLAN_SNAP_OUI_LEN];
} __packed;

struct wlan_8023 {
   unsigned char        h_dest[ETH_ALEN];
   unsigned char        h_source[ETH_ALEN];
   __be16               h_len;
   struct wlan_snap_hdr h_snap;
   __be16               h_proto;
} __packed;

struct wlan_8023_vlan {
   unsigned char        h_dest[ETH_ALEN];
   unsigned char        h_source[ETH_ALEN];
   __be16               h_vlan_proto;
   __be16               h_vlan_TCI;
   __be16               h_len;
   struct wlan_snap_hdr h_snap;
   __be16               h_proto;
} __packed;

union generic_ethhdr {
   struct ethhdr eth_II;
   struct vlan_ethhdr eth_IIv;
   struct wlan_8023 eth_8023;
   struct wlan_8023_vlan eth_8023v;
};

#endif /* #ifndef _WLAN_HDD_ETHER_H */
