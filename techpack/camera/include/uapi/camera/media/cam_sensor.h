/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef __UAPI_CAM_SENSOR_H__
#define __UAPI_CAM_SENSOR_H__

#include <linux/types.h>
#include <linux/ioctl.h>
#include <camera/media/cam_defs.h>

#define CAM_SENSOR_PROBE_CMD   (CAM_COMMON_OPCODE_MAX + 1)
#define CAM_FLASH_MAX_LED_TRIGGERS 2
#define MAX_OIS_NAME_SIZE 32
#define CAM_CSIPHY_SECURE_MODE_ENABLED 1
/**
 * struct cam_sensor_query_cap - capabilities info for sensor
 *
 * @slot_info        :  Indicates about the slotId or cell Index
 * @secure_camera    :  Camera is in secure/Non-secure mode
 * @pos_pitch        :  Sensor position pitch
 * @pos_roll         :  Sensor position roll
 * @pos_yaw          :  Sensor position yaw
 * @actuator_slot_id :  Actuator slot id which connected to sensor
 * @eeprom_slot_id   :  EEPROM slot id which connected to sensor
 * @ois_slot_id      :  OIS slot id which connected to sensor
 * @flash_slot_id    :  Flash slot id which connected to sensor
 * @csiphy_slot_id   :  CSIphy slot id which connected to sensor
 *
 */
struct  cam_sensor_query_cap {
	__u32        slot_info;
	__u32        secure_camera;
	__u32        pos_pitch;
	__u32        pos_roll;
	__u32        pos_yaw;
	__u32        actuator_slot_id;
	__u32        eeprom_slot_id;
	__u32        ois_slot_id;
	__u32        flash_slot_id;
	__u32        csiphy_slot_id;
} __attribute__((packed));

/**
 * struct cam_csiphy_query_cap - capabilities info for csiphy
 *
 * @slot_info        :  Indicates about the slotId or cell Index
 * @version          :  CSIphy version
 * @clk lane         :  Of the 5 lanes, informs lane configured
 *                      as clock lane
 * @reserved
 */
struct cam_csiphy_query_cap {
	__u32            slot_info;
	__u32            version;
	__u32            clk_lane;
	__u32            reserved;
} __attribute__((packed));

/**
 * struct cam_actuator_query_cap - capabilities info for actuator
 *
 * @slot_info        :  Indicates about the slotId or cell Index
 * @reserved
 */
struct cam_actuator_query_cap {
	__u32            slot_info;
	__u32            reserved;
} __attribute__((packed));

/**
 * struct cam_eeprom_query_cap_t - capabilities info for eeprom
 *
 * @slot_info                  :  Indicates about the slotId or cell Index
 * @eeprom_kernel_probe        :  Indicates about the kernel or userspace probe
 */
struct cam_eeprom_query_cap_t {
	__u32            slot_info;
	__u16            eeprom_kernel_probe;
	__u16            is_multimodule_mode;
} __attribute__((packed));

/**
 * struct cam_ois_query_cap_t - capabilities info for ois
 *
 * @slot_info                  :  Indicates about the slotId or cell Index
 */
struct cam_ois_query_cap_t {
	__u32            slot_info;
	__u16            reserved;
} __attribute__((packed));

/**
 * struct cam_cmd_i2c_info - Contains slave I2C related info
 *
 * @slave_addr      :    Slave address
 * @i2c_freq_mode   :    4 bits are used for I2c freq mode
 * @cmd_type        :    Explains type of command
 */
struct cam_cmd_i2c_info {
	__u32    slave_addr;
	__u8     i2c_freq_mode;
	__u8     cmd_type;
	__u16    reserved;
} __attribute__((packed));

/**
 * struct cam_ois_opcode - Contains OIS opcode
 *
 * @prog             :    OIS FW prog register address
 * @coeff            :    OIS FW coeff register address
 * @pheripheral      :    OIS pheripheral
 * @memory           :    OIS memory
 * @fw_addr_type     :    OIS FW addr type
 * @is_addr_increase :    OIS FW addr increase
 * others            :    OIS FW Upgrade related.
 */
struct cam_ois_opcode {
	__u32 prog;
	__u32 coeff;
	__u32 pheripheral;
	__u32 memory;
	__u8 fw_addr_type;
	__u8 is_addr_increase;
	__u8 is_addr_indata;
	__u8 fwversion;
	__u32 fwchecksumsize;
	__u32 fwchecksum;
} __attribute__((packed));

/**
 * struct cam_cmd_ois_info - Contains OIS slave info
 *
 * @slave_addr            :    OIS i2c slave address
 * @i2c_freq_mode         :    i2c frequency mode
 * @cmd_type              :    Explains type of command
 * @ois_fw_flag           :    indicates if fw is present or not
 * @is_ois_calib          :    indicates the calibration data is available
 * @ois_name              :    OIS name
 * @opcode                :    opcode
 * @is_ois_pre_init       :    indicates the pre initialize data is available
 */
struct cam_cmd_ois_info {
	__u32                 slave_addr;
	__u8                  i2c_freq_mode;
	__u8                  cmd_type;
	__u8                  ois_fw_flag;
	__u8                  is_ois_calib;
	char                  ois_name[MAX_OIS_NAME_SIZE];
	struct cam_ois_opcode opcode;
	uint8_t               is_ois_pre_init; //xiaomi add
} __attribute__((packed));

/**
 * struct cam_cmd_probe - Contains sensor slave info
 *
 * @data_type       :   Slave register data type
 * @addr_type       :   Slave register address type
 * @op_code         :   Don't Care
 * @cmd_type        :   Explains type of command
 * @reg_addr        :   Slave register address
 * @expected_data   :   Data expected at slave register address
 * @data_mask       :   Data mask if only few bits are valid
 * @camera_id       :   Indicates the slot to which camera
 *                      needs to be probed
 * @reserved
 */
struct cam_cmd_probe {
	__u8     data_type;
	__u8     addr_type;
	__u8     op_code;
	__u8     cmd_type;
	__u32    reg_addr;
	__u32    expected_data;
	__u32    data_mask;
	__u16    camera_id;
	__u16    reserved;
} __attribute__((packed));

/**
 * struct cam_power_settings - Contains sensor power setting info
 *
 * @power_seq_type  :   Type of power sequence
 * @reserved
 * @config_val_low  :   Lower 32 bit value configuration value
 * @config_val_high :   Higher 32 bit value configuration value
 *
 */
struct cam_power_settings {
	__u16    power_seq_type;
	__u16    reserved;
	__u32    config_val_low;
	__u32    config_val_high;
} __attribute__((packed));

/**
 * struct cam_cmd_power - Explains about the power settings
 *
 * @count           :    Number of power settings follows
 * @reserved
 * @cmd_type        :    Explains type of command
 * @power_settings  :    Contains power setting info
 */
struct cam_cmd_power {
	__u32                       count;
	__u8                        reserved;
	__u8                        cmd_type;
	__u16                       more_reserved;
	struct cam_power_settings   power_settings[1];
} __attribute__((packed));

/**
 * struct i2c_rdwr_header - header of READ/WRITE I2C command
 *
 * @ count           :   Number of registers / data / reg-data pairs
 * @ op_code         :   Operation code
 * @ cmd_type        :   Command buffer type
 * @ data_type       :   I2C data type
 * @ addr_type       :   I2C address type
 * @ reserved
 */
struct i2c_rdwr_header {
	__u32    count;
	__u8     op_code;
	__u8     cmd_type;
	__u8     data_type;
	__u8     addr_type;
} __attribute__((packed));

/**
 * struct i2c_random_wr_payload - payload for I2C random write
 *
 * @ reg_addr        :   Register address
 * @ reg_data        :   Register data
 *
 */
struct i2c_random_wr_payload {
	__u32     reg_addr;
	__u32     reg_data;
} __attribute__((packed));

/**
 * struct cam_cmd_i2c_random_wr - I2C random write command
 * @ header            :   header of READ/WRITE I2C command
 * @ random_wr_payload :   payload for I2C random write
 */
struct cam_cmd_i2c_random_wr {
	struct i2c_rdwr_header       header;
	struct i2c_random_wr_payload random_wr_payload[1];
} __attribute__((packed));

/**
 * struct cam_cmd_read - I2C read command
 * @ reg_data        :   Register data
 * @ reserved
 */
struct cam_cmd_read {
	__u32                reg_data;
	__u32                reserved;
} __attribute__((packed));

/**
 * struct cam_cmd_i2c_continuous_wr - I2C continuous write command
 * @ header          :   header of READ/WRITE I2C command
 * @ reg_addr        :   Register address
 * @ data_read       :   I2C read command
 */
struct cam_cmd_i2c_continuous_wr {
	struct i2c_rdwr_header header;
	__u32                  reg_addr;
	struct cam_cmd_read    data_read[1];
} __attribute__((packed));

/**
 * struct cam_cmd_i2c_random_rd - I2C random read command
 * @ header          :   header of READ/WRITE I2C command
 * @ data_read       :   I2C read command
 */
struct cam_cmd_i2c_random_rd {
	struct i2c_rdwr_header header;
	struct cam_cmd_read    data_read[1];
} __attribute__((packed));

/**
 * struct cam_cmd_i2c_continuous_rd - I2C continuous continuous read command
 * @ header          :   header of READ/WRITE I2C command
 * @ reg_addr        :   Register address
 *
 */
struct cam_cmd_i2c_continuous_rd {
	struct i2c_rdwr_header header;
	__u32                  reg_addr;
} __attribute__((packed));

/**
 * struct cam_cmd_conditional_wait - Conditional wait command
 * @data_type       :   Data type
 * @addr_type       :   Address type
 * @op_code         :   Opcode
 * @cmd_type        :   Explains type of command
 * @timeout         :   Timeout for retries
 * @reserved
 * @reg_addr        :   Register Address
 * @reg_data        :   Register data
 * @data_mask       :   Data mask if only few bits are valid
 * @camera_id       :   Indicates the slot to which camera
 *                      needs to be probed
 *
 */
struct cam_cmd_conditional_wait {
	__u8     data_type;
	__u8     addr_type;
	__u16    reserved;
	__u8     op_code;
	__u8     cmd_type;
	__u16    timeout;
	__u32    reg_addr;
	__u32    reg_data;
	__u32    data_mask;
} __attribute__((packed));

/**
 * struct cam_cmd_unconditional_wait - Un-conditional wait command
 * @delay           :   Delay
 * @op_code         :   Opcode
 * @cmd_type        :   Explains type of command
 */
struct cam_cmd_unconditional_wait {
	__s16    delay;
	__s16    reserved;
	__u8     op_code;
	__u8     cmd_type;
	__u16    reserved1;
} __attribute__((packed));

/**
 * cam_csiphy_info       : Provides cmdbuffer structre
 * @lane_assign          : Lane sensor will be using
 * @mipi_flags           : MIPI phy flags
 * @lane_cnt             : Total number of lanes
 * @secure_mode          : Secure mode flag to enable / disable
 * @settle_time          : Settling time in ms
 * @data_rate            : Data rate
 *
 */
struct cam_csiphy_info {
	__u16    reserved;
	__u16    lane_assign;
	__u16    mipi_flags;
	__u8     lane_cnt;
	__u8     secure_mode;
	__u64    settle_time;
	__u64    data_rate;
} __attribute__((packed));

/**
 * cam_csiphy_acquire_dev_info : Information needed for
 *                               csiphy at the time of acquire
 * @combo_mode                 : Indicates the device mode of operation
 * @cphy_dphy_combo_mode       : Info regarding cphy_dphy_combo mode
 * @csiphy_3phase              : Details whether 3Phase / 2Phase operation
 * @reserve
 *
 */
struct cam_csiphy_acquire_dev_info {
	__u32    combo_mode;
	__u16    cphy_dphy_combo_mode;
	__u8     csiphy_3phase;
	__u8     reserve;
} __attribute__((packed));

/**
 * cam_sensor_acquire_dev : Updates sensor acuire cmd
 * @device_handle  :    Updates device handle
 * @session_handle :    Session handle for acquiring device
 * @handle_type    :    Resource handle type
 * @reserved
 * @info_handle    :    Handle to additional info
 *                      needed for sensor sub modules
 *
 */
struct cam_sensor_acquire_dev {
	__u32    session_handle;
	__u32    device_handle;
	__u32    handle_type;
	__u32    reserved;
	__u64    info_handle;
} __attribute__((packed));

/**
 * cam_sensor_streamon_dev : StreamOn command for the sensor
 * @session_handle :    Session handle for acquiring device
 * @device_handle  :    Updates device handle
 * @handle_type    :    Resource handle type
 * @reserved
 * @info_handle    :    Information Needed at the time of streamOn
 *
 */
struct cam_sensor_streamon_dev {
	__u32    session_handle;
	__u32    device_handle;
	__u32    handle_type;
	__u32    reserved;
	__u64    info_handle;
} __attribute__((packed));

/**
 * struct cam_flash_init : Init command for the flash
 * @flash_type  :    flash hw type
 * @reserved
 * @cmd_type    :    command buffer type
 */
struct cam_flash_init {
	__u32    flash_type;
	__u8     reserved;
	__u8     cmd_type;
	__u16    reserved1;
} __attribute__((packed));

/**
 * struct cam_flash_set_rer : RedEyeReduction command buffer
 *
 * @count             :   Number of flash leds
 * @opcode            :   Command buffer opcode
 *			CAM_FLASH_FIRE_RER
 * @cmd_type          :   command buffer operation type
 * @num_iteration     :   Number of led turn on/off sequence
 * @reserved
 * @led_on_delay_ms   :   flash led turn on time in ms
 * @led_off_delay_ms  :   flash led turn off time in ms
 * @led_current_ma    :   flash led current in ma
 *
 */
struct cam_flash_set_rer {
	__u32    count;
	__u8     opcode;
	__u8     cmd_type;
	__u16    num_iteration;
	__u32    led_on_delay_ms;
	__u32    led_off_delay_ms;
	__u32    led_current_ma[CAM_FLASH_MAX_LED_TRIGGERS];
} __attribute__((packed));

/**
 * struct cam_flash_set_on_off : led turn on/off command buffer
 *
 * @count                  : Number of Flash leds
 * @opcode                 : Command buffer opcodes
 *			     CAM_FLASH_FIRE_LOW
 *			     CAM_FLASH_FIRE_HIGH
 *			     CAM_FLASH_OFF
 * @cmd_type               : Command buffer operation type
 * @led_current_ma         : Flash led current in ma
 * @time_on_duration_ms    : Flash time on duration in ns
 *
 */
struct cam_flash_set_on_off {
	__u32    count;
	__u8     opcode;
	__u8     cmd_type;
	__u16    reserved;
	__u32    led_current_ma[CAM_FLASH_MAX_LED_TRIGGERS];
	__u64    time_on_duration_ns;
	__u8     is_trigger_eof; // xiaomi add
} __attribute__((packed));

/**
 * struct cam_flash_query_curr : query current command buffer
 *
 * @reserved
 * @opcode            :   command buffer opcode
 * @cmd_type          :   command buffer operation type
 * @query_current_ma  :   battery current in ma
 *
 */
struct cam_flash_query_curr {
	__u16    reserved;
	__u8     opcode;
	__u8     cmd_type;
	__u32    query_current_ma;
} __attribute__ ((packed));

/**
 * struct cam_flash_query_cap  :  capabilities info for flash
 *
 * @slot_info           :  Indicates about the slotId or cell Index
 * @max_current_flash   :  max supported current for flash
 * @max_duration_flash  :  max flash turn on duration
 * @max_current_torch   :  max supported current for torch
 *
 */
struct cam_flash_query_cap_info {
	__u32    slot_info;
	__u32    max_current_flash[CAM_FLASH_MAX_LED_TRIGGERS];
	__u32    max_duration_flash[CAM_FLASH_MAX_LED_TRIGGERS];
	__u32    max_current_torch[CAM_FLASH_MAX_LED_TRIGGERS];
} __attribute__ ((packed));

#endif
