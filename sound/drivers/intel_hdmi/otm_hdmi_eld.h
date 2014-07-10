/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2011 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:

  Intel Corporation
  2200 Mission College Blvd.
  Santa Clara, CA  95054

  BSD LICENSE

  Copyright(c) 2011 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
#ifndef _OTM_HDMI_ELD_H
#define _OTM_HDMI_ELD_H

#include <linux/types.h>
#define OTM_HDMI_ELD_SIZE 84

union otm_hdmi_eld_t {
	uint8_t eld_data[OTM_HDMI_ELD_SIZE];
	#pragma pack(1)
	struct {
		/* Byte[0] = ELD Version Number */
		union {
			uint8_t   byte0;
			struct {
				uint8_t reserved:3; /* Reserf */
				uint8_t eld_ver:5; /* ELD Version Number */
						/* 00000b - reserved
						 * 00001b - first rev, obsoleted
						 * 00010b - version 2,
						 * supporting CEA version
						 * 861D or below.
						 * 00011b:11111b - reserved
						 * for future
						 */
			};
		};

		/* Byte[1] = Vendor Version Field */
		union {
			uint8_t vendor_version;
			struct {
				uint8_t reserved1:3;
				uint8_t veld_ver:5; /* Version number of the ELD
						     * extension. This value is
						     * provisioned and unique to
						     * each vendor.
						     */
			};
		};

		/* Byte[2] = Baseline Lenght field */
		uint8_t baseline_eld_length; /* Length of the Baseline structure
					      *	divided by Four.
					      */

		/* Byte [3] = Reserved for future use */
		uint8_t byte3;

		/* Starting of the BaseLine EELD structure
		 * Byte[4] = Monitor Name Length
		 */
		union {
			uint8_t byte4;
			struct {
				uint8_t mnl:5;
				uint8_t cea_edid_rev_id:3;
			};
		};

		/* Byte[5] = Capabilities */
		union {
			uint8_t capabilities;
			struct {
				uint8_t hdcp:1; /* HDCP support */
				uint8_t ai_support:1;   /* AI support */
				uint8_t connection_type:2; /* Connection type
							    * 00 - HDMI
							    * 01 - DP
							    * 10 -11  Reserved
							    * for future
							    * connection types
							    */
				uint8_t sadc:4; /* Indicates number of 3 bytes
						 * Short Audio Descriptors.
						 */
			};
		};

		/* Byte[6] = Audio Synch Delay */
		uint8_t audio_synch_delay; /* Amount of time reported by the
					    * sink that the video trails audio
					    * in milliseconds.
					    */

		/* Byte[7] = Speaker Allocation Block */
		union {
			uint8_t speaker_allocation_block;
			struct {
				uint8_t flr:1; /*Front Left and Right channels*/
				uint8_t lfe:1; /*Low Frequency Effect channel*/
				uint8_t fc:1;  /*Center transmission channel*/
				uint8_t rlr:1; /*Rear Left and Right channels*/
				uint8_t rc:1; /*Rear Center channel*/
				uint8_t flrc:1; /*Front left and Right of Center
						 *transmission channels
						 */
				uint8_t rlrc:1; /*Rear left and Right of Center
						 *transmission channels
						 */
				uint8_t reserved3:1; /* Reserved */
			};
		};

		/* Byte[8 - 15] - 8 Byte port identification value */
		uint8_t port_id_value[8];

		/* Byte[16 - 17] - 2 Byte Manufacturer ID */
		uint8_t manufacturer_id[2];

		/* Byte[18 - 19] - 2 Byte Product ID */
		uint8_t product_id[2];

		/* Byte [20-83] - 64 Bytes of BaseLine Data */
		uint8_t mn_sand_sads[64]; /* This will include
					   * - ASCII string of Monitor name
					   * - List of 3 byte SADs
					   * - Zero padding
					   */

		/* Vendor ELD Block should continue here!
		 * No Vendor ELD block defined as of now.
		 */
	};
	#pragma pack()
};

#endif /* _OTM_HDMI_ELD_H */
