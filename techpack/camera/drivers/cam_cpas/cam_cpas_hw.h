/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CPAS_HW_H_
#define _CAM_CPAS_HW_H_

#include <dt-bindings/msm/msm-camera.h>
#include "cam_cpas_api.h"
#include "cam_cpas_hw_intf.h"
#include "cam_common_util.h"

#define CAM_CPAS_INFLIGHT_WORKS              5
#define CAM_CPAS_MAX_CLIENTS                 40
#define CAM_CPAS_MAX_AXI_PORTS               6
#define CAM_CPAS_MAX_TREE_LEVELS             4
#define CAM_CPAS_MAX_GRAN_PATHS_PER_CLIENT   32
#define CAM_CPAS_PATH_DATA_MAX               38
#define CAM_CPAS_TRANSACTION_MAX             2

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
	(cpas_core->cpas_client[indx]->registered))
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
 * @registered: Whether client has registered with cpas
 * @started: Whether client has streamed on
 * @tree_node_valid: Indicates whether tree node has at least one valid node
 * @ahb_level: Determined/Applied ahb level for the client
 * @axi_vote: Determined/Applied axi vote for the client
 * @axi_port: Client's parent axi port
 * @tree_node: All granular path voting nodes for the client
 *
 */
struct cam_cpas_client {
	struct cam_cpas_register_params data;
	bool registered;
	bool started;
	bool tree_node_valid;
	enum cam_vote_level ahb_level;
	struct cam_axi_vote axi_vote;
	struct cam_cpas_axi_port *axi_port;
	struct cam_cpas_tree_node *tree_node[CAM_CPAS_PATH_DATA_MAX]
		[CAM_CPAS_TRANSACTION_MAX];
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
 * @axi_port_name: Name of this AXI port
 * @bus_client: bus client info for this port
 * @ib_bw_voting_needed: if this port can update ib bw dynamically
 * @axi_port_node: Node representing AXI Port info in device tree
 * @ab_bw: AB bw value for this port
 * @ib_bw: IB bw value for this port
 * @additional_bw: Additional bandwidth to cover non-hw cpas clients
 */
struct cam_cpas_axi_port {
	const char *axi_port_name;
	struct cam_cpas_bus_client bus_client;
	bool ib_bw_voting_needed;
	struct device_node *axi_port_node;
	uint64_t ab_bw;
	uint64_t ib_bw;
	uint64_t additional_bw;
};

/**
 * struct cam_cpas : CPAS core data structure info
 *
 * @hw_caps: CPAS hw capabilities
 * @cpas_client: Array of pointers to CPAS clients info
 * @client_mutex: Mutex for accessing client info
 * @tree_lock: Mutex lock for accessing CPAS node tree
 * @num_clients: Total number of clients that CPAS supports
 * @num_axi_ports: Total number of axi ports found in device tree
 * @registered_clients: Number of Clients registered currently
 * @streamon_clients: Number of Clients that are in start state currently
 * @regbase_index: Register base indices for CPAS register base IDs
 * @ahb_bus_client: AHB Bus client info
 * @axi_port: AXI port info for a specific axi index
 * @internal_ops: CPAS HW internal ops
 * @work_queue: Work queue handle
 * @irq_count: atomic irq count
 * @irq_count_wq: wait variable to ensure all irq's are handled
 * @dentry: debugfs file entry
 * @ahb_bus_scaling_disable: ahb scaling based on src clk corner for bus
 */
struct cam_cpas {
	struct cam_cpas_hw_caps hw_caps;
	struct cam_cpas_client *cpas_client[CAM_CPAS_MAX_CLIENTS];
	struct mutex client_mutex[CAM_CPAS_MAX_CLIENTS];
	struct mutex tree_lock;
	uint32_t num_clients;
	uint32_t num_axi_ports;
	uint32_t registered_clients;
	uint32_t streamon_clients;
	int32_t regbase_index[CAM_CPAS_REG_MAX];
	struct cam_cpas_bus_client ahb_bus_client;
	struct cam_cpas_axi_port axi_port[CAM_CPAS_MAX_AXI_PORTS];
	struct cam_cpas_internal_ops internal_ops;
	struct workqueue_struct *work_queue;
	atomic_t irq_count;
	wait_queue_head_t irq_count_wq;
	struct dentry *dentry;
	bool ahb_bus_scaling_disable;
};

int cam_camsstop_get_internal_ops(struct cam_cpas_internal_ops *internal_ops);
int cam_cpastop_get_internal_ops(struct cam_cpas_internal_ops *internal_ops);

int cam_cpas_util_reg_update(struct cam_hw_info *cpas_hw,
	enum cam_cpas_reg_base reg_base, struct cam_cpas_reg *reg_info);

int cam_cpas_util_client_cleanup(struct cam_hw_info *cpas_hw);

#endif /* _CAM_CPAS_HW_H_ */
