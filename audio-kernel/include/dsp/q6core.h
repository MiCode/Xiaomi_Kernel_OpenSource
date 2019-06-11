/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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
#include <ipc/apr.h>
#include <dsp/apr_audio-v2.h>



#define AVCS_CMD_ADSP_EVENT_GET_STATE		0x0001290C
#define AVCS_CMDRSP_ADSP_EVENT_GET_STATE	0x0001290D

bool q6core_is_adsp_ready(void);

int q6core_get_service_version(uint32_t service_id,
			       struct avcs_fwk_ver_info *ver_info,
			       size_t size);
size_t q6core_get_fwk_version_size(uint32_t service_id);

struct audio_uevent_data {
	struct kobject kobj;
	struct kobj_type ktype;
};

int q6core_init_uevent_data(struct audio_uevent_data *uevent_data, char *name);
void q6core_destroy_uevent_data(struct audio_uevent_data *uevent_data);
int q6core_send_uevent(struct audio_uevent_data *uevent_data, char *name);
int q6core_get_avcs_api_version_per_service(uint32_t service_id);

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
 * for DS1 license via ADSP generic license mechanism.
 * Please refer AVCS_CMD_SET_LICENSE for more details.
 */
#define DOLBY_DS1_LICENSE_ID	0x00000001

#define AVCS_CMD_SET_LICENSE	0x00012919
struct avcs_cmd_set_license {
	struct apr_hdr hdr;
	uint32_t id; /**< A unique ID used to refer to this license */
	uint32_t overwrite;
	/* 0 = do not overwrite an existing license with this id.
	 * 1 = overwrite an existing license with this id.
	 */
	uint32_t size;
	/**< Size in bytes of the license data following this header. */
	/* uint8_t* data ,  data and padding follows this structure
	 * total packet size needs to be multiple of 4 Bytes
	 */

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
	 * ADSP_ENOTEXIST if there is no license with the given id.
	 * ADSP_ENOTIMPL if there is no validation function for a license
	 * with this id.
	 */
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

/* Commands the AVCS to map multiple shared memory regions with remote
 * processor ID. All mapped regions must be from the same memory pool.
 *
 * Return:
 * ADSP_EOK        : SUCCESS
 * ADSP_EHANDLE    : Failed due to incorrect handle.
 * ADSP_EBADPARAM  : Failed due to bad parameters.
 *
 * Dependencies:
 * The mem_map_handle should be obtained earlier
 * using AVCS_CMD_SHARED_MEM_MAP_REGIONS with pool ID
 * ADSP_MEMORY_MAP_MDF_SHMEM_4K_POOL.
 */
#define AVCS_CMD_MAP_MDF_SHARED_MEMORY                              0x00012930

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

#define AVCS_CMD_LOAD_TOPO_MODULES                 0x0001296C

#define AVCS_CMD_UNLOAD_TOPO_MODULES               0x0001296D

#define CORE_LOAD_TOPOLOGY	0

#define CORE_UNLOAD_TOPOLOGY	1

struct avcs_cmd_load_unload_topo_modules {
	struct apr_hdr hdr;
	uint32_t topology_id;
} __packed;


int q6core_map_memory_regions(phys_addr_t *buf_add, uint32_t mempool_id,
			uint32_t *bufsz, uint32_t bufcnt, uint32_t *map_handle);
int q6core_memory_unmap_regions(uint32_t mem_map_handle);
int q6core_map_mdf_shared_memory(uint32_t map_handle, phys_addr_t *buf_add,
			uint32_t proc_id, uint32_t *bufsz, uint32_t bufcnt);

int32_t core_set_license(uint32_t key, uint32_t module_id);
int32_t core_get_license_status(uint32_t module_id);

int32_t q6core_load_unload_topo_modules(uint32_t topology_id,
			bool preload_type);
int q6core_is_avs_up(int32_t *avs_state);

#if IS_ENABLED(CONFIG_USE_Q6_32CH_SUPPORT)
static inline bool q6core_use_Q6_32ch_support(void)
{
	return true;
}
#else
static inline bool q6core_use_Q6_32ch_support(void)
{
	return false;
}
#endif

#endif /* __Q6CORE_H__ */
