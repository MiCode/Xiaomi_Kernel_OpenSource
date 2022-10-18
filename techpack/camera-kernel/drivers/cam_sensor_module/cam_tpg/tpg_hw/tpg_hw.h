/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __TPG_HW_H__
#define __TPG_HW_H__

#include <linux/kernel.h>
#include "cam_debug_util.h"
#include "cam_soc_util.h"
#include <cam_cpas_api.h>
#include <media/cam_sensor.h>
#define TPG_HW_VERSION_1_0 0x10000000
#define TPG_HW_VERSION_1_1 0x10000001
#define TPG_HW_VERSION_1_2 0x10000002
#define TPG_HW_VERSION_1_3 0x10000003


struct tpg_hw;

/**
 * tpg_hw_ops : tpg hw operations
 *
 * @init        : tpg hw layer init
 * @start       : tpg hw start stream
 * @stop        : tpg hw stop  stream
 * @deinit      : tpg hw deinit
 * @read        : tpg hw read register
 * @write       : tpg hw write register
 * @process_cmd : tpg hw process command
 * @dump_status : dump any status registers
 */
struct tpg_hw_ops {
	int (*init)(struct tpg_hw *hw, void *data);
	int (*start)(struct tpg_hw *hw, void *data);
	int (*stop)(struct tpg_hw *hw, void *data);
	int (*deinit)(struct tpg_hw *hw, void *data);
	int (*read)(struct tpg_hw *hw, uint32_t  *addr, uint32_t *val);
	int (*write)(struct tpg_hw *hw, uint32_t *addr, uint32_t *val);
	int (*process_cmd)(struct tpg_hw *hw, uint32_t cmd, void *arg);
	int (*dump_status)(struct tpg_hw *hw, void *data);
};

/**
 * tpg_hw_state : tpg hw states
 *
 * TPG_HW_STATE_HW_DISABLED: tpg hw is not enabled yet
 * TPG_HW_STATE_HW_ENABLED : tpg hw is enabled
 */
enum tpg_hw_state {
	TPG_HW_STATE_HW_DISABLED,
	TPG_HW_STATE_HW_ENABLED,
};

/**
 * @brief tpg_vc_slot_info
 * @slot_id      : slot id of this vc slot
 * @vc           : virtual channel configured
 * @stream_count : number of streams in this slot
 * @head         : head pointing all data types in with this vc
 */
struct tpg_vc_slot_info {
	int    slot_id;
	int    vc;
	int    stream_count;
	struct list_head head;
};

/**
 * tpg_hw_info : tpg hw layer info
 *
 * @version:  version of tpg hw
 * @max_vc_channels: max number of virtual channels supported by tpg
 * @max_dt_channels_per_vc: max dts supported in each vc
 * @ops:   tpg hw operations
 */
struct tpg_hw_info {
	uint32_t          version;
	uint32_t          max_vc_channels;
	uint32_t          max_dt_channels_per_vc;
	struct tpg_hw_ops *ops;
};


/**
 * tpg_hw_stream : tpg hw stream
 *
 * @stream : tpg stream;
 * @list   : entry to tpg stream list
 */
struct tpg_hw_stream {
	struct tpg_stream_config_t stream;
	struct list_head list;
};

/**
 * tpg_hw : tpg hw
 *
 * @hw_idx        : hw id
 * @state         : tpg hw state
 * @cpas_handle   : handle to cpas
 * @hw_info       : tp hw info
 * @soc_info      : soc info
 * @mutex         : lock
 * @stream_list   : list of tpg stream
 * @global_config : global configuration
 */
struct tpg_hw {
	uint32_t                   hw_idx;
	uint32_t                   state;
	uint32_t                   cpas_handle;
	uint32_t                   vc_count;
	struct tpg_hw_info        *hw_info;
	struct cam_hw_soc_info    *soc_info;
	struct mutex               mutex;
	struct tpg_vc_slot_info   *vc_slots;
	struct tpg_global_config_t global_config;
};

/**
 * tpg_hw_acquire_args : tpg hw acquire arguments
 *
 * @resource_list  : list of resources to acquire
 * @count          : number of resources to acquire
 */
struct tpg_hw_acquire_args {
	/* Integer id of resources */
	uint32_t *resource_list;
	ssize_t  count;
};

enum tpg_hw_cmd_t {
	TPG_HW_CMD_INVALID = 0,
	TPG_HW_CMD_INIT_CONFIG,
	TPG_HW_CMD_MAX,
};

#define TPG_HW_CONFIG_BASE 0x4000
#define TPG_CONFIG_CTRL    (TPG_HW_CONFIG_BASE + 0)
#define TPG_CONFIG_VC      (TPG_HW_CONFIG_BASE + 1)
#define TPG_CONFIG_DT      (TPG_HW_CONFIG_BASE + 2)

/* pixel bit width */
#define PACK_8_BIT    8
#define PACK_10_BIT   10
#define PACK_12_BIT   12
#define PACK_14_BIT   14
#define PACK_16_BIT   16

/**
 * @vc_config_args : arguments for vc config process cmd
 *
 * @vc_slot : slot to configure this vc
 * @num_dts : number of dts in this vc
 * @stream  : output stream
 */
struct vc_config_args {
	uint32_t vc_slot;
	uint32_t num_dts;
	struct tpg_stream_config_t *stream;
};

/**
 * dt_config_args : arguments for dt config process cmd
 *
 * @vc_slot  : vc slot to configure this dt
 * @dt_slot  : dt slot to configure this dt
 * @stream   : stream packet to configure for this dt
 */
struct dt_config_args {
	uint32_t vc_slot;
	uint32_t dt_slot;
	struct tpg_stream_config_t *stream;
};

/**
 * global_config_args : tpg global config args
 *
 * @num_vcs : number of vcs to be configured
 * @globalconfig: global config cmd
 */
struct global_config_args {
	uint32_t num_vcs;
	struct tpg_global_config_t *globalconfig;
};

/**
 * tpg_hw_initsettings : initial configurations
 *
 * @global_config : global configuration
 * @streamconfigs : array of stream configurations
 * @num_streams   : number of streams in strea config array
 */
struct tpg_hw_initsettings {
	struct tpg_global_config_t globalconfig;
	struct tpg_stream_config_t *streamconfigs;
	uint32_t num_streams;
};

/**
 * @brief dump the tpg memory info
 *
 * @param soc_info: soc info for soc related info
 *
 * @return : 0 on succuss
 */
int32_t cam_tpg_mem_dmp(struct cam_hw_soc_info *soc_info);

/**
 * @brief dump global config command
 *
 * @param idx    : hw index
 * @param global : global config command
 *
 * @return       : 0 on success
 */
int dump_global_configs(int idx, struct tpg_global_config_t *global);

/**
 * @brief : dump stream config command
 *
 * @param hw_idx: hw index
 * @param stream_idx: stream index
 * @param stream: stream config command
 *
 * @return : 0 on success
 */
int dump_stream_configs(int hw_idx, int stream_idx, struct tpg_stream_config_t *stream);

/**
 * @brief : dump any hw status registers
 *
 * @param hw: tpg hw instance
 *
 * @return : 0 on success
 */
int tpg_hw_dump_status(struct tpg_hw *hw);
/**
 * @brief : start tpg hw stream
 *
 * @param hw: tpg hw instance
 *
 * @return : 0 on success
 */
int tpg_hw_start(struct tpg_hw *hw);

/**
 * @brief : stop tpg hw stream
 *
 * @param hw: tpg hw instance
 *
 * @return : 0 on success
 */
int tpg_hw_stop(struct tpg_hw *hw);

/**
 * @brief : tpg hw acquire
 *
 * @param hw: tpg hw instance
 * @param acquire: list of resources to acquire
 *
 * @return : 0 on success
 */
int tpg_hw_acquire(struct tpg_hw *hw,
		struct tpg_hw_acquire_args *acquire);
/**
 * @brief release tpg hw
 *
 * @param hw: tpg hw instance
 *
 * @return : 0 on success
 */
int tpg_hw_release(struct tpg_hw *hw);

/**
 * @brief : configure tpg hw
 *
 * @param hw: tpg hw instance
 * @param cmd: configuration command
 * @param arg: configuration command argument
 *
 * @return : 0 on success
 */
int tpg_hw_config(struct tpg_hw *hw, enum tpg_hw_cmd_t cmd, void *arg);

/**
 * @brief : tpg free streams
 *
 * @param hw: tpg hw instance
 *
 * @return : 0 on success
 */
int tpg_hw_free_streams(struct tpg_hw *hw);

/**
 * @brief : reset the tpg hw instance
 *
 * @param hw: tpg hw instance
 *
 * @return : 0 on success
 */
int tpg_hw_reset(struct tpg_hw *hw);

/**
 * @brief : tp hw add stream
 *
 * @param hw: tpg hw instance
 * @param cmd: tpg hw command
 *
 * @return : 0 on success
 */
int tpg_hw_add_stream(struct tpg_hw *hw, struct tpg_stream_config_t *cmd);

/**
 * @brief : copy global config command
 *
 * @param hw: tpg hw instance
 * @param global: global config command
 *
 * @return : 0 on success
 */
int tpg_hw_copy_global_config(struct tpg_hw *hw, struct tpg_global_config_t *global);

#endif
