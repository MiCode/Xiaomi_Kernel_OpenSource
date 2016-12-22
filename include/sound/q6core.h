/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __Q6CORE_H__
#define __Q6CORE_H__
#include <linux/qdsp6v2/apr.h>
#include <sound/apr_audio-v2.h>



#define AVCS_CMD_ADSP_EVENT_GET_STATE		0x0001290C
#define AVCS_CMDRSP_ADSP_EVENT_GET_STATE	0x0001290D

bool q6core_is_adsp_ready(void);

int q6core_get_avcs_fwk_ver_info(uint32_t service_id,
				 struct avcs_fwk_ver_info *ver_info);

#define ADSP_CMD_SET_DTS_EAGLE_DATA_ID 0x00012919
#define DTS_EAGLE_LICENSE_ID           0x00028346
struct adsp_dts_eagle {
	struct apr_hdr hdr;
	uint32_t id;
	uint32_t overwrite;
	uint32_t size;
	char data[];
};
int core_dts_eagle_set(int size, char *data);
int core_dts_eagle_get(int id, int size, char *data);

#define ADSP_CMD_SET_DOLBY_MANUFACTURER_ID 0x00012918

struct adsp_dolby_manufacturer_id {
	struct apr_hdr hdr;
	int manufacturer_id;
};

uint32_t core_set_dolby_manufacturer_id(int manufacturer_id);

/* Dolby Surround1 Module License ID. This ID is used as an identifier
   for DS1 license via ADSP generic license mechanism.
   Please refer AVCS_CMD_SET_LICENSE for more details.
*/
#define DOLBY_DS1_LICENSE_ID	0x00000001

#define AVCS_CMD_SET_LICENSE	0x00012919
struct avcs_cmd_set_license {
	struct apr_hdr hdr;
	uint32_t id; /**< A unique ID used to refer to this license */
	uint32_t overwrite;
	/**< 0 = do not overwrite an existing license with this id.
		1 = overwrite an existing license with this id. */
	uint32_t size;
	/**< Size in bytes of the license data following this header. */
	/* uint8_t* data ,  data and padding follows this structure
		total packet size needs to be multiple of 4 Bytes*/

};

#define AVCS_CMD_GET_LICENSE_VALIDATION_RESULT        0x0001291A
struct avcs_cmd_get_license_validation_result {
	struct apr_hdr hdr;
	uint32_t id; /**< A unique ID used to refer to this license */
};

#define AVCS_CMDRSP_GET_LICENSE_VALIDATION_RESULT        0x0001291B
struct avcs_cmdrsp_get_license_validation_result {
	uint32_t result;
	/* ADSP_EOK if the license validation result was successfully retrieved.
		ADSP_ENOTEXIST if there is no license with the given id.
		ADSP_ENOTIMPL if there is no validation function for a license
		with this id. */
	uint32_t size;
	/* Length in bytes of the result that follows this structure*/
};

/* Set Q6 topologies */
/*
 *	Registers custom topologies in the aDSP for
 *	use in audio, voice, AFE and LSM.
 */


#define AVCS_CMD_SHARED_MEM_MAP_REGIONS                             0x00012924
#define AVCS_CMDRSP_SHARED_MEM_MAP_REGIONS                          0x00012925
#define AVCS_CMD_SHARED_MEM_UNMAP_REGIONS                           0x00012926


#define AVCS_CMD_REGISTER_TOPOLOGIES                                0x00012923

/* The payload for the AVCS_CMD_REGISTER_TOPOLOGIES command */
struct avcs_cmd_register_topologies {
	struct apr_hdr hdr;
	uint32_t                  payload_addr_lsw;
	/* Lower 32 bits of the topology buffer address. */

	uint32_t                  payload_addr_msw;
	/* Upper 32 bits of the topology buffer address. */

	uint32_t                  mem_map_handle;
	/* Unique identifier for an address.
	 * -This memory map handle is returned by the aDSP through the
	 * memory map command.
	 * -NULL mem_map_handle is interpreted as in-band parameter
	 * passing.
	 * -Client has the flexibility to choose in-band or out-of-band.
	 * -Out-of-band is recommended in this case.
	 */

	uint32_t                  payload_size;
	/* Size in bytes of the valid data in the topology buffer. */
} __packed;


#define AVCS_CMD_DEREGISTER_TOPOLOGIES                                0x0001292a

/* The payload for the AVCS_CMD_DEREGISTER_TOPOLOGIES command */
struct avcs_cmd_deregister_topologies {
	struct apr_hdr hdr;
	uint32_t                  payload_addr_lsw;
	/* Lower 32 bits of the topology buffer address. */

	uint32_t                  payload_addr_msw;
	/* Upper 32 bits of the topology buffer address. */

	uint32_t                  mem_map_handle;
	/* Unique identifier for an address.
	 * -This memory map handle is returned by the aDSP through the
	 * memory map command.
	 * -NULL mem_map_handle is interpreted as in-band parameter
	 * passing.
	 * -Client has the flexibility to choose in-band or out-of-band.
	 * -Out-of-band is recommended in this case.
	 */

	uint32_t                  payload_size;
	/* Size in bytes of the valid data in the topology buffer. */

	uint32_t                  mode;
	/* 1: Deregister selected topologies
	 * 2: Deregister all topologies
	 */
} __packed;

#define AVCS_MODE_DEREGISTER_ALL_CUSTOM_TOPOLOGIES	2


int32_t core_set_license(uint32_t key, uint32_t module_id);
int32_t core_get_license_status(uint32_t module_id);

#define AVCS_GET_VERSIONS	0x00012905
struct avcs_cmd_get_version_result {
	struct apr_hdr hdr;
	uint32_t id;
};
#define AVCS_GET_VERSIONS_RSP	0x00012906

#define AVCS_CMDRSP_Q6_ID_2_6	0x00040000
#define AVCS_CMDRSP_Q6_ID_2_7	0x00040001
#define AVCS_CMDRSP_Q6_ID_2_8   0x00040002

enum q6_subsys_image {
	Q6_SUBSYS_AVS2_6 = 1,
	Q6_SUBSYS_AVS2_7,
	Q6_SUBSYS_AVS2_8,
	Q6_SUBSYS_INVALID,
};
enum q6_subsys_image q6core_get_avs_version(void);
int core_get_adsp_ver(void);
#endif /* __Q6CORE_H__ */
