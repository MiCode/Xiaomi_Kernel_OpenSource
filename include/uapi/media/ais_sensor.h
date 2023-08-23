#ifndef __UAPI_AIS_SENSOR_H__
#define __UAPI_AIS_SENSOR_H__

#include <media/cam_sensor.h>

#define AIS_SENSOR_OPCODE_START (CAM_COMMON_OPCODE_MAX + 10)
#define AIS_SENSOR_PROBE_CMD   (AIS_SENSOR_OPCODE_START + 1)
#define AIS_SENSOR_POWER_UP    (AIS_SENSOR_OPCODE_START + 2)
#define AIS_SENSOR_POWER_DOWN  (AIS_SENSOR_OPCODE_START + 3)
#define AIS_SENSOR_I2C_READ    (AIS_SENSOR_OPCODE_START + 4)
#define AIS_SENSOR_I2C_WRITE   (AIS_SENSOR_OPCODE_START + 5)
#define AIS_SENSOR_I2C_WRITE_SYNC (AIS_SENSOR_OPCODE_START + 6)
#define AIS_SENSOR_I2C_WRITE_ARRAY_SYNC (AIS_SENSOR_OPCODE_START + 7)
#define AIS_SENSOR_I2C_WRITE_ARRAY (AIS_SENSOR_OPCODE_START + 8)
#define AIS_SENSOR_I2C_POWER_UP    (AIS_SENSOR_OPCODE_START + 9)
#define AIS_SENSOR_I2C_POWER_DOWN  (AIS_SENSOR_OPCODE_START + 10)
#define AIS_SENSOR_INTR_INIT      (AIS_SENSOR_OPCODE_START + 11)
#define AIS_SENSOR_INTR_DEINIT    (AIS_SENSOR_OPCODE_START + 12)
#define AIS_SENSOR_I2C_SET_SYNC_PARMS (AIS_SENSOR_OPCODE_START + 13)
#define AIS_SENSOR_EVENT_BASE      (V4L2_EVENT_PRIVATE_START)
#define AIS_SENSOR_EVENT_TYPE      (AIS_SENSOR_EVENT_BASE + 1)


#define AIS_MAX_POWER_SEQ 12
#define AIS_MAX_INTR_GPIO 3

struct ais_sensor_event_data {
	uint32_t data[16];
};

/**
 * struct ais_power_settings - Contains sensor power setting info
 *
 * @power_seq_type  :   Type of power sequence
 * @reserved
 * @config_val_low  :   Lower 32 bit value configuration value
 * @config_val_high :   Higher 32 bit value configuration value
 * @delay           :   Delay
 *
 */
struct ais_power_settings {
	uint16_t    power_seq_type;
	uint16_t    reserved;
	uint32_t    config_val_low;
	uint32_t    config_val_high;
	uint32_t    delay;
} __attribute__((packed));

/**
 * struct ais_sensor_power_config - power up and down settings
 * @size_up            :    power up size
 * @power_up_setting   :    power up settings
 * @size_down          :    power down size
 * @power_down_setting :    power down settings
 */
struct ais_sensor_power_config {
	uint16_t size_up;
	struct ais_power_settings power_up_setting[AIS_MAX_POWER_SEQ];
	uint16_t size_down;
	struct ais_power_settings power_down_setting[AIS_MAX_POWER_SEQ];
};

/**
 * struct ais_sensor_gpio_intr_config - gpio intr settings
 * @gpio_num    :    gpio number
 * @gpio_cfg0   :    config 0 param
 * @gpio_cfg1   :    config 1 param
 * @reserved    :    reserved
 */
struct ais_sensor_gpio_intr_config {
	uint32_t gpio_num;
	uint32_t gpio_cfg0;
	uint32_t gpio_cfg1;
	uint32_t reserved;
};

/**
 * struct cam_cmd_i2c_cci_cfg - cci debug config
 * @i2c_info        :    i2c config
 * @power_settings  :    power up and down settings
 * @gpio_intr_config:    gpio interrupt config
 */
struct ais_sensor_probe_cmd {
	struct cam_cmd_i2c_info i2c_config;
	struct ais_sensor_power_config power_config;
	struct ais_sensor_gpio_intr_config gpio_intr_config[AIS_MAX_INTR_GPIO];
};

/**
 * struct ais_sensor_cmd_i2c_read - i2c read
 * @slave_addr      :    slave address
 * @addr_type       :    address type
 * @data_type       :    data type
 * @reg_addr        :    reg address
 * @reg_data        :    reg data
 * @reserved        :    reserved
 */
struct ais_sensor_cmd_i2c_read {
	struct cam_cmd_i2c_info i2c_config;
	uint8_t     addr_type;
	uint8_t     data_type;
	uint32_t    reg_addr;
	uint32_t    reg_data;
	uint16_t    reserved;
} __attribute__((packed));

/**
 * struct ais_sensor_i2c_wr_payload - I2C random write array entry
 *
 * @ reg_addr        :   Register address
 * @ reg_data        :   Register data
 * @ delay           :   Delay
 *
 */
struct ais_sensor_i2c_wr_payload {
	uint32_t    reg_addr;
	uint32_t    reg_data;
	uint32_t    delay;
};

/**
 * struct ais_sensor_cmd_i2c_wr - i2c single write
 * @slave_addr      :    slave address
 * @addr_type       :    address type
 * @data_type       :    data type
 * @reserved        :    reserved
 * @wr_payload      :    write payload
 */
struct ais_sensor_cmd_i2c_wr {
	struct cam_cmd_i2c_info i2c_config;
	uint8_t     addr_type;
	uint8_t     data_type;
	uint32_t    reserved;
	struct ais_sensor_i2c_wr_payload wr_payload;
} __attribute__((packed));

/**
 * struct ais_sensor_cmd_i2c_wr_array - i2c write array
 * @slave_addr      :    slave address
 * @addr_type       :    address type
 * @data_type       :    data type
 * @reserved        :    reserved
 * @count           :    count of elements in wr_array
 * @wr_array        :    write array payload
 */
struct ais_sensor_cmd_i2c_wr_array {
	struct cam_cmd_i2c_info i2c_config;
	uint8_t     addr_type;
	uint8_t     data_type;
	uint16_t    reserved;
	uint16_t    count;
	struct ais_sensor_i2c_wr_payload *wr_array;
} __attribute__((packed));

/**
 * struct ais_sensor_cmd_i2c_pwrup - i2c power up
 * @master          :    logical master
 * @retries         :    number of retries
 * @reserved        :    reserved
 */
struct ais_sensor_cmd_i2c_pwrup {
	uint8_t     master;
	uint8_t     retries;
	uint16_t     reserved;
} __attribute__((packed));

struct ais_sensor_cmd_i2c_pwrdown {
	uint8_t     master;
	uint8_t     retries;
	uint16_t     reserved;
} __attribute__((packed));


#define MAX_NUM_I2C_CMDS 32

struct ais_sensor_cmd_i2c_sync_cfg {
	uint16_t cid;
	uint16_t csid;
	uint16_t line;
	uint16_t delay;
} __attribute__((packed));

struct ais_sensor_cmd_i2c_cci_client {
	uint8_t i2c_freq_mode;
	uint8_t cci_i2c_master;
	uint32_t timeout;
	uint16_t retries;
} __attribute__((packed));


struct ais_sensor_wr_sync {
	struct ais_sensor_cmd_i2c_sync_cfg sync_cfg;
	struct ais_sensor_cmd_i2c_wr_array wr_cfg[MAX_NUM_I2C_CMDS];
	uint8_t num_wr_cfg;
};

struct ais_cci_cmd_t {
	struct ais_sensor_cmd_i2c_cci_client cci_client;
	union {
	struct ais_sensor_cmd_i2c_wr_array wr_cfg;
	struct ais_sensor_wr_sync wr_sync;
	struct ais_sensor_cmd_i2c_wr i2c_write;
	struct ais_sensor_cmd_i2c_read i2c_read;
	} cmd;
};


#endif
