/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __Q6CORE_H__
#define __Q6CORE_H__
#include <ipc/apr.h>
#include <dsp/apr_audio-v2.h>



#define AVCS_CMD_ADSP_EVENT_GET_STATE		0x0001290C
#define AVCS_CMDRSP_ADSP_EVENT_GET_STATE	0x0001290D
#define AVCS_API_VERSION_V4		4
#define AVCS_API_VERSION_V5		5
#define APRV2_IDS_SERVICE_ID_ADSP_CORE_V (0x3)

bool q6core_is_adsp_ready(void);

#if IS_ENABLED(CONFIG_MSM_AVTIMER)
int avcs_core_query_timer_offset(int64_t *av_offset, int32_t clock_id);
#else
static inline int avcs_core_query_timer_offset(int64_t *av_offset,
                                                int32_t clock_id) {
        return 0;
}
#endif
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


#define AVCS_CMD_DEREGISTER_TOPOLOGIES                             0x0001292a

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

#define AVCS_LOAD_MODULES 1

#define AVCS_UNLOAD_MODULES 0

#define AVCS_CMD_LOAD_MODULES           0x00012989

#define AVCS_CMD_UNLOAD_MODULES         0x0001298A

#define AVCS_CMD_RSP_LOAD_MODULES       0x0001298B

/*
 * Module is generic, such as a preprocessor,
 * postprocessor, or volume control
 *  module.
 */
#define AMDB_MODULE_TYPE_GENERIC           0

/** Module is a decoder. */
#define AMDB_MODULE_TYPE_DECODER           1

/** Module is an encoder. */
#define AMDB_MODULE_TYPE_ENCODER           2

/** Module is a converter. */
#define AMDB_MODULE_TYPE_CONVERTER         3

/** Module is a packetizer. */
#define AMDB_MODULE_TYPE_PACKETIZER        4

/** Module is a depacketizer. */
#define AMDB_MODULE_TYPE_DEPACKETIZER      5


struct avcs_load_unload_modules_sec_payload {
	uint32_t       module_type;
	/*
	 * < Type of module.

	 * @values
	 * - #AMDB_MODULE_TYPE_GENERIC
	 * - #AMDB_MODULE_TYPE_DECODER
	 * - #AMDB_MODULE_TYPE_ENCODER
	 * - #AMDB_MODULE_TYPE_CONVERTER
	 * - #AMDB_MODULE_TYPE_PACKETIZER
	 * - #AMDB_MODULE_TYPE_DEPACKETIZER
	 */


	uint32_t       id1;
	/*
	 * < One of the following types of IDs:
	 * - Format ID for the encoder, decoder, and packetizer module types
	 * - Module ID for the generic module type
	 * - Source format ID for the converter module type
	 */

	uint32_t       id2;
	/*
	 * < Sink format ID for the converter module type.
	 * Zero for other module types
	 */

	uint32_t handle_lsw;
	/* < LSW of the Handle to the module loaded */

	uint32_t handle_msw;
	/* < MSW of the Handle to the module loaded. */
} __packed;

struct avcs_load_unload_modules_payload {
	uint32_t num_modules;
	/**< Number of modules being registered */

	struct avcs_load_unload_modules_sec_payload load_unload_info[0];
	/*
	 * < Load/unload information for num_modules.
	 * It will be of type avcs_load_unload_info_t[num_modules]
	 */
} __packed;

struct avcs_load_unload_modules_meminfo {
	uint32_t                  data_payload_addr_lsw;
	/**< Lower 32 bits of the 64-bit data payload address. */

	uint32_t                  data_payload_addr_msw;
	/**< Upper 32 bits of the 64-bit data payload address. */

	uint32_t                  mem_map_handle;
	/*
	 * < Unique identifier for an address. The aDSP returns this memory map
	 * handle through the #AVCS_CMD_SHARED_MEM_MAP_REGIONS command.

	 * @values @vertspace{-2}
	 * - NULL -- Parameter data payloads are within the message payload
	 * (in-band).
	 * - Non-NULL -- Parameter data payloads begin at the address specified
	 * in the data_payload_addr_lsw and data_payload_addr_msw fields
	 * (out-of-band).

	 * The client can choose in-band or out-of-band
	 */

	uint32_t                  buffer_size;
	/*
	 * < Actual size (in bytes) of the valid data
	 * in the data payload address.
	 */
} __packed;

struct avcs_cmd_dynamic_modules {
	struct apr_hdr hdr;
	struct avcs_load_unload_modules_meminfo meminfo;
	struct avcs_load_unload_modules_payload payload;
} __packed;


int32_t q6core_avcs_load_unload_modules(struct avcs_load_unload_modules_payload *payload,
				uint32_t preload_type);


/* This command allows a remote client(HLOS) creates a client to LPASS NPA
 * resource node. Currently, this command supports only the NPA Sleep resource
 * "/island/core/drivers" node ID.
 */
#define AVCS_CMD_CREATE_LPASS_NPA_CLIENT    0x00012985

#define AVCS_SLEEP_ISLAND_CORE_DRIVER_NODE_ID    0x00000001

struct avcs_cmd_create_lpass_npa_client_t {
	struct apr_hdr hdr;
	uint32_t  node_id;
	/* Unique ID of the NPA node.
	 * @values
	 *   - #AVCS_SLEEP_ISLAND_CORE_DRIVER_NODE_ID
	 */

	char client_name[16];
	/* Client name, up to a maximum of sixteen characters.*/
};

/* In response to the #AVCS_CMD_CREATE_LPASS_NPA_CLIENT command, the AVCS
 * returns the handle to remote HLOS client.
 */
#define AVCS_CMDRSP_CREATE_LPASS_NPA_CLIENT    0x00012986

struct avcs_cmdrsp_create_lpass_npa_client_t {
	uint32_t status;
	/* Status message (error code).
	 * @values
	 *   - ADSP_EOK -- Create was successful
	 *   - ADSP_EFAILED -- Create failed
	 */

	uint32_t  client_handle;
	/* Handle of the client.*/
};

/* The remote HLOS client use this command to issue the request to the npa
 * resource. Remote client(HLOS) must send the valid npa client handle and
 * resource id info.
 */
#define AVCS_CMD_REQUEST_LPASS_NPA_RESOURCES    0x00012987

#define AVCS_SLEEP_NODE_ISLAND_TRANSITION_RESOURCE_ID    0x00000001

#define SLEEP_RESTRICT_ISLAND                0x0
#define SLEEP_ALLOW_ISLAND                   0x1

/* Immediately following this structure is the resource request configuration
 * data payload. Payload varies depend on the resource_id requested.
 * Currently supported only island transition payload.
 */
struct avcs_cmd_request_lpass_npa_resources_t {
	struct apr_hdr hdr;
	uint32_t  client_handle;
	/* Handle of the client.
	 * @values
	 * - Valid uint32 number
	 */

	uint32_t  resource_id;
	/* Unique ID of the NPA resource ID.
	 * @values
	 * - #AVCS_SLEEP_NODE_ISLAND_TRANSITION_RESOURCE_ID
	 */
};

/* This structure contains the sleep node resource payload data.
 */
struct avcs_sleep_node_island_transition_config_t {
	struct avcs_cmd_request_lpass_npa_resources_t req_lpass_npa_rsc;
	uint32_t  island_allow_mode;
	/* Specifies the island state.
	 * @values
	 * - #SLEEP_RESTRICT_ISLAND
	 * - #SLEEP_ALLOW_ISLAND
	 */
};

/* This command allows remote client(HLOS) to destroy the npa node client
 * handle, which is created using the #AVCS_CMD_CREATE_LPASS_NPA_CLIENT command.
 * Remote client(HLOS) must send the valid npa client handle.
 */
#define AVCS_CMD_DESTROY_LPASS_NPA_CLIENT    0x00012988

struct avcs_cmd_destroy_lpass_npa_client_t {
	struct apr_hdr hdr;
	uint32_t  client_handle;
	/* Handle of the client.
	 * @values
	 * - Valid uint32 number
	 */
};

int q6core_map_memory_regions(phys_addr_t *buf_add, uint32_t mempool_id,
			uint32_t *bufsz, uint32_t bufcnt, uint32_t *map_handle);
int q6core_memory_unmap_regions(uint32_t mem_map_handle);

int q6core_map_mdf_memory_regions(uint64_t *buf_add, uint32_t mempool_id,
			uint32_t *bufsz, uint32_t bufcnt, uint32_t *map_handle);

int q6core_map_mdf_shared_memory(uint32_t map_handle, uint64_t *buf_add,
			uint32_t proc_id, uint32_t *bufsz, uint32_t bufcnt);

int32_t core_set_license(uint32_t key, uint32_t module_id);
int32_t core_get_license_status(uint32_t module_id);

int32_t q6core_load_unload_topo_modules(uint32_t topology_id,
			bool preload_type);

int q6core_create_lpass_npa_client(uint32_t node_id, char *client_name,
				   uint32_t *client_handle);
int q6core_destroy_lpass_npa_client(uint32_t client_handle);
int q6core_request_island_transition(uint32_t client_handle,
				     uint32_t island_allow_mode);

int q6core_get_avcs_avs_build_version_info(
	uint32_t *build_major_version, uint32_t *build_minor_version,
					uint32_t *build_branch_version);
#endif /* __Q6CORE_H__ */
