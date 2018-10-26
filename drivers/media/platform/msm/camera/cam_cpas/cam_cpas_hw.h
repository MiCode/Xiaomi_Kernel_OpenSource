/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_CPAS_HW_H_
#define _CAM_CPAS_HW_H_

#include "cam_cpas_api.h"
#include "cam_cpas_hw_intf.h"
#include "cam_common_util.h"

#define CAM_CPAS_MAX_CLIENTS 30
#define CAM_CPAS_INFLIGHT_WORKS 5

#define CAM_CPAS_AXI_MIN_MNOC_AB_BW   (2048 * 1024)
#define CAM_CPAS_AXI_MIN_MNOC_IB_BW   (2048 * 1024)
#define CAM_CPAS_AXI_MIN_CAMNOC_AB_BW (2048 * 1024)
#define CAM_CPAS_AXI_MIN_CAMNOC_IB_BW (3000000000UL)

#define CAM_CPAS_GET_CLIENT_IDX(handle) (handle)
#define CAM_CPAS_GET_CLIENT_HANDLE(indx) (indx)

#define CAM_CPAS_CLIENT_VALID(indx) \
	((indx >= 0) && (indx < CAM_CPAS_MAX_CLIENTS))
#define CAM_CPAS_CLIENT_REGISTERED(cpas_core, indx)        \
	((CAM_CPAS_CLIENT_VALID(indx)) && \
	(cpas_core->cpas_client[indx]))
#define CAM_CPAS_CLIENT_STARTED(cpas_core, indx)          \
	((CAM_CPAS_CLIENT_REGISTERED(cpas_core, indx)) && \
	(cpas_core->cpas_client[indx]->started))

/**
 * enum cam_cpas_access_type - Enum for Register access type
 */
enum cam_cpas_access_type {
	CAM_REG_TYPE_READ,
	CAM_REG_TYPE_WRITE,
	CAM_REG_TYPE_READ_WRITE,
};

/**
 * struct cam_cpas_internal_ops - CPAS Hardware layer internal ops
 *
 * @get_hw_info: Function pointer for get hw info
 * @init_hw_version: Function pointer for hw init based on version
 * @handle_irq: Function poniter for irq handling
 * @setup_regbase: Function pointer for setup rebase indices
 * @power_on: Function pointer for hw core specific power on settings
 * @power_off: Function pointer for hw core specific power off settings
 *
 */
struct cam_cpas_internal_ops {
	int (*get_hw_info)(struct cam_hw_info *cpas_hw,
		struct cam_cpas_hw_caps *hw_caps);
	int (*init_hw_version)(struct cam_hw_info *cpas_hw,
		struct cam_cpas_hw_caps *hw_caps);
	irqreturn_t (*handle_irq)(int irq_num, void *data);
	int (*setup_regbase)(struct cam_hw_soc_info *soc_info,
		int32_t regbase_index[], int32_t num_reg_map);
	int (*power_on)(struct cam_hw_info *cpas_hw);
	int (*power_off)(struct cam_hw_info *cpas_hw);
};

/**
 * struct cam_cpas_reg : CPAS register info
 *
 * @enable: Whether this reg info need to be enabled
 * @access_type: Register access type
 * @masked_value: Whether this register write/read is based on mask, shift
 * @mask: Mask for this register value
 * @shift: Shift for this register value
 * @value: Register value
 *
 */
struct cam_cpas_reg {
	bool enable;
	enum cam_cpas_access_type access_type;
	bool masked_value;
	uint32_t offset;
	uint32_t mask;
	uint32_t shift;
	uint32_t value;
};

/**
 * struct cam_cpas_client : CPAS Client structure info
 *
 * @data: Client register params
 * @started: Whether client has streamed on
 * @ahb_level: Determined/Applied ahb level for the client
 * @axi_vote: Determined/Applied axi vote for the client
 * @axi_port: Client's parent axi port
 * @axi_sibling_client: Client's sibllings sharing the same axi port
 *
 */
struct cam_cpas_client {
	struct cam_cpas_register_params data;
	bool started;
	enum cam_vote_level ahb_level;
	struct cam_axi_vote axi_vote;
	struct cam_cpas_axi_port *axi_port;
	struct list_head axi_sibling_client;
};

/**
 * struct cam_cpas_bus_client : Bus client information
 *
 * @src: Bus master/src id
 * @dst: Bus slave/dst id
 * @pdata: Bus pdata information
 * @client_id: Bus client id
 * @num_usecases: Number of use cases for this client
 * @num_paths: Number of paths for this client
 * @curr_vote_level: current voted index
 * @dyn_vote: Whether dynamic voting enabled
 * @lock: Mutex lock used while voting on this client
 * @valid: Whether bus client is valid
 * @name: Name of the bus client
 *
 */
struct cam_cpas_bus_client {
	int src;
	int dst;
	struct msm_bus_scale_pdata *pdata;
	uint32_t client_id;
	int num_usecases;
	int num_paths;
	unsigned int curr_vote_level;
	bool dyn_vote;
	struct mutex lock;
	bool valid;
	const char *name;
};

/**
 * struct cam_cpas_axi_port : AXI port information
 *
 * @sibling_port: Sibling AXI ports
 * @clients_list_head: List head pointing to list of clients sharing this port
 * @lock: Mutex lock for accessing this port
 * @camnoc_bus: CAMNOC bus client info for this port
 * @mnoc_bus: MNOC bus client info for this port
 * @axi_port_name: Name of this AXI port
 * @axi_port_node: Node representing this AXI Port
 * @axi_port_mnoc_node: Node representing mnoc in this AXI Port
 * @axi_port_camnoc_node: Node representing camnoc in this AXI Port
 * @consolidated_axi_vote: Consolidated axi bw values for this AXI port
 */
struct cam_cpas_axi_port {
	struct list_head sibling_port;
	struct list_head clients_list_head;
	struct mutex lock;
	struct cam_cpas_bus_client camnoc_bus;
	struct cam_cpas_bus_client mnoc_bus;
	const char *axi_port_name;
	struct device_node *axi_port_node;
	struct device_node *axi_port_mnoc_node;
	struct device_node *axi_port_camnoc_node;
	struct cam_axi_vote consolidated_axi_vote;
};

/**
 * struct cam_cpas : CPAS core data structure info
 *
 * @hw_caps: CPAS hw capabilities
 * @cpas_client: Array of pointers to CPAS clients info
 * @client_mutex: Mutex for accessing client info
 * @num_clients: Total number of clients that CPAS supports
 * @registered_clients: Number of Clients registered currently
 * @streamon_clients: Number of Clients that are in start state currently
 * @regbase_index: Register base indices for CPAS register base IDs
 * @ahb_bus_client: AHB Bus client info
 * @axi_ports_list_head: Head pointing to list of AXI ports
 * @internal_ops: CPAS HW internal ops
 * @work_queue: Work queue handle
 *
 */
struct cam_cpas {
	struct cam_cpas_hw_caps hw_caps;
	struct cam_cpas_client *cpas_client[CAM_CPAS_MAX_CLIENTS];
	struct mutex client_mutex[CAM_CPAS_MAX_CLIENTS];
	uint32_t num_clients;
	uint32_t registered_clients;
	uint32_t streamon_clients;
	int32_t regbase_index[CAM_CPAS_REG_MAX];
	struct cam_cpas_bus_client ahb_bus_client;
	struct list_head axi_ports_list_head;
	struct cam_cpas_internal_ops internal_ops;
	struct workqueue_struct *work_queue;
	atomic_t irq_count;
	wait_queue_head_t irq_count_wq;
};

int cam_camsstop_get_internal_ops(struct cam_cpas_internal_ops *internal_ops);
int cam_cpastop_get_internal_ops(struct cam_cpas_internal_ops *internal_ops);

int cam_cpas_util_reg_update(struct cam_hw_info *cpas_hw,
	enum cam_cpas_reg_base reg_base, struct cam_cpas_reg *reg_info);

#endif /* _CAM_CPAS_HW_H_ */
