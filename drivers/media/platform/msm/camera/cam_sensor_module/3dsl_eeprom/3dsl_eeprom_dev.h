#ifndef _CAM_SL_EEPROM_DEV_H_
#define _CAM_SL_EEPROM_DEV_H_
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/unistd.h>
#include <linux/initrd.h>
#include <linux/init.h>
#include <linux/of_gpio.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/cam_sensor.h>
#include <cam_sensor_i2c.h>
#include <cam_sensor_spi.h>
#include <cam_sensor_io.h>
#include <cam_cci_dev.h>
#include <cam_req_mgr_util.h>
#include <cam_req_mgr_interface.h>
#include <cam_mem_mgr.h>
#include <cam_subdev.h>
#include "cam_soc_util.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define PROPERTY_MAXSIZE 32

#define MSM_EEPROM_MEMORY_MAP_MAX_SIZE         80
#define MSM_EEPROM_MAX_MEM_MAP_CNT             8
#define MSM_EEPROM_MEM_MAP_PROPERTIES_CNT      6

#define DL_CLASS_NAME "3DSL_EEPROM"

#define DL_EEPROM_IOC_MAGIC 'Q'
#define DL_EEPROM_PRIVATE    168


enum sl_eeprom_i2c_type {
	SL_EEPROM_I2C_TYPE_INVALID,
	SL_EEPROM_SENSOR_I2C_TYPE_BYTE,
	SL_EEPROM_SENSOR_I2C_TYPE_WORD,
	SL_EEPROM_SENSOR_I2C_TYPE_3B,
	SL_EEPROM_SENSOR_I2C_TYPE_DWORD,
	SL_EEPROM_SENSOR_I2C_TYPE_MAX,
};

struct sl_eeprom_i2c_reg_array {
	uint32_t reg_addr;
	uint32_t reg_data;
	uint32_t delay;
	uint32_t data_mask;
};

struct sl_eeprom_i2c_reg_setting {
	struct sl_eeprom_i2c_reg_array *reg_setting;
	unsigned short size;
	enum sl_eeprom_i2c_type addr_type;
	enum sl_eeprom_i2c_type data_type;
	unsigned short delay;
};

#define DL_IOC_PWR_UP \
	_IO(DL_EEPROM_IOC_MAGIC, DL_EEPROM_PRIVATE + 1)
#define DL_IOC_PWR_DOWN \
	_IO(DL_EEPROM_IOC_MAGIC, DL_EEPROM_PRIVATE + 2)
#define DL_IOC_READ_DATA \
	_IOWR(DL_EEPROM_IOC_MAGIC, DL_EEPROM_PRIVATE + 3,  struct sl_eeprom_memory_block_t)
#define DL_IOC_WRITE_DATA \
	_IOWR(DL_EEPROM_IOC_MAGIC, DL_EEPROM_PRIVATE + 4, struct sl_eeprom_i2c_reg_setting)

enum sl_eeprom_state {
	CAM_SL_EEPROM_INIT,
	CAM_SL_EEPROM_ACQUIRE,
	CAM_SL_EEPROM_CONFIG,
};

/**
 * struct sl_eeprom_map_t - eeprom map
 * @data_type       :   Data type
 * @addr_type       :   Address type
 * @addr            :   Address
 * @data            :   data
 * @delay           :   Delay
 *
 */
struct sl_eeprom_map_t {
	uint32_t valid_size;
	uint32_t addr;
	uint32_t addr_type;
	uint32_t data;
	uint32_t data_type;
	uint32_t delay;
};

/**
 * struct sl_eeprom_memory_map_t - eeprom memory map types
 * @page            :   page memory
 * @pageen          :   pageen memory
 * @poll            :   poll memory
 * @mem             :   mem
 * @saddr           :   slave addr
 *
 */
struct sl_eeprom_memory_map_t {
	uint32_t saddr;
	struct sl_eeprom_map_t mem;
	struct sl_eeprom_map_t page;
	struct sl_eeprom_map_t pageen;
	struct sl_eeprom_map_t poll;
	struct sl_eeprom_map_t delay;
};

/**
 * struct sl_eeprom_memory_block_t - eeprom mem block info
 * @map             :   eeprom memory map
 * @num_map         :   number of map blocks
 * @mapdata         :   map data
 * @cmd_type        :   size of total mapdata
 *
 */
struct sl_eeprom_memory_block_t {
	struct sl_eeprom_memory_map_t *map;
	uint32_t num_map;
	uint8_t *mapdata;
	uint32_t num_data;
};

/**
 * struct sl_eeprom_i2c_info_t - I2C info
 * @slave_addr      :   slave address
 * @i2c_freq_mode   :   i2c frequency mode
 *
 */
struct sl_eeprom_i2c_info_t {
	uint16_t slave_addr;
	uint8_t i2c_freq_mode;
};

/**
 * struct sl_eeprom_soc_private - eeprom soc private data structure
 * @eeprom_name     :   eeprom name
 * @i2c_info        :   i2c info structure
 * @power_info      :   eeprom power info
 * @cmm_data        :   cmm data
 *
 */
struct sl_eeprom_soc_private {
	const char *eeprom_name;
	struct sl_eeprom_i2c_info_t i2c_info;
	struct cam_sensor_power_ctrl_t power_info;

};

/**
 * struct sl_eeprom_intf_params - bridge interface params
 * @device_hdl   : Device Handle
 * @session_hdl  : Session Handle
 * @ops          : KMD operations
 * @crm_cb       : Callback API pointers
 */
struct sl_eeprom_intf_params {
	int32_t device_hdl;
	int32_t session_hdl;
	int32_t link_hdl;
	struct cam_req_mgr_kmd_ops ops;
	struct cam_req_mgr_crm_cb *crm_cb;
};

struct sl_eeprom_device_info_t{
	char device_name[20];
	char class_name[20];
	struct class *chr_class;
	dev_t dev_num;
	struct device *chr_dev;
	struct cdev cdev;
};
/**
 * struct sl_eeprom_ctrl_t - Conditional wait command
 * @pdev            :   platform device
 * @spi             :   spi device
 * @eeprom_mutex    :   eeprom mutex
 * @soc_info        :   eeprom soc related info
 * @io_master_info  :   Information about the communication master
 * @gpio_num_info   :   gpio info
 * @cci_i2c_master  :   I2C structure
 * @v4l2_dev_str    :   V4L2 device structure
 * @bridge_intf     :   bridge interface params
 * @sl_eeprom_state:   eeprom_device_state
 * @userspace_probe :   flag indicates userspace or kernel probe
 * @cal_data        :   Calibration data
 * @device_name     :   Device name
 *
 */
struct sl_eeprom_ctrl_t {
	struct platform_device *pdev;
	struct spi_device *spi;
	struct mutex eeprom_mutex;
	struct cam_hw_soc_info soc_info;
	struct camera_io_master io_master_info;
	struct msm_camera_gpio_num_info *gpio_num_info;
	enum cci_i2c_master_t cci_i2c_master;
	struct cam_subdev v4l2_dev_str;
	struct sl_eeprom_intf_params bridge_intf;
	enum msm_camera_device_type_t eeprom_device_type;
	enum sl_eeprom_state sl_eeprom_state;
	bool userspace_probe;
	struct sl_eeprom_memory_block_t cal_data;
	struct sl_eeprom_device_info_t  dev_info;
};

int32_t sl_eeprom_update_i2c_info(struct sl_eeprom_ctrl_t *e_ctrl,
	struct sl_eeprom_i2c_info_t *i2c_info);

#endif /*_CAM_SL_EEPROM_DEV_H_ */

