#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>
#include "3dsl_eeprom_core.h"
#include "3dsl_eeprom_soc.h"
#include "cam_debug_util.h"

/**
 * sl_eeprom_read_memory() - read map data into buffer
 * @e_ctrl:     eeprom control struct
 * @block:      block to be read
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */
#define TEST_READ 0
#define WRITE_TO_FILE 0
#define CONTINUS_WRITE 0
static int sl_eeprom_read_memory(struct sl_eeprom_ctrl_t *e_ctrl,
	struct sl_eeprom_memory_block_t *block)
{
	int rc = 0;
	int j;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings;
	struct cam_sensor_i2c_reg_array    i2c_reg_array;
	struct sl_eeprom_memory_map_t    *emap = block->map;
	struct sl_eeprom_soc_private     *eb_info;
	uint8_t *memptr = block->mapdata;

	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}

	eb_info = (struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	for (j = 0; j < block->num_map; j++) {
		if (emap[j].saddr) {
			eb_info->i2c_info.slave_addr = emap[j].saddr;
			rc = sl_eeprom_update_i2c_info(e_ctrl,
				&eb_info->i2c_info);
			if (rc) {
				CAM_ERR(CAM_SL_EEPROM,
					"failed: to update i2c info rc %d",
					rc);
				return rc;
			}
		}
#if 0
			i2c_reg_settings.addr_type = 2;
			i2c_reg_settings.data_type = 1;
			i2c_reg_settings.size = 1;
			i2c_reg_settings.delay = 0;
			i2c_reg_array.reg_addr = 0x1778;
			i2c_reg_array.reg_data = 0xaa;
			i2c_reg_array.delay =0;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);

#endif

		if (emap[j].page.valid_size) {
			i2c_reg_settings.addr_type = emap[j].page.addr_type;
			i2c_reg_settings.data_type = emap[j].page.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_settings.delay = emap[j].page.delay;
			i2c_reg_array.reg_addr = emap[j].page.addr;
			i2c_reg_array.reg_data = emap[j].page.data;
			i2c_reg_array.delay = emap[j].page.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_SL_EEPROM, "page write failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].pageen.valid_size) {
			i2c_reg_settings.addr_type = emap[j].pageen.addr_type;
			i2c_reg_settings.data_type = emap[j].pageen.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].pageen.addr;
			i2c_reg_array.reg_data = emap[j].pageen.data;
			i2c_reg_array.delay = emap[j].pageen.delay;
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_SL_EEPROM, "page enable failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].poll.valid_size) {
			rc = camera_io_dev_poll(&e_ctrl->io_master_info,
				emap[j].poll.addr, emap[j].poll.data,
				0, emap[j].poll.addr_type,
				emap[j].poll.data_type,
				emap[j].poll.delay);
			if (rc) {
				CAM_ERR(CAM_SL_EEPROM, "poll failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].delay.valid_size)
			msleep(emap[j].delay.delay);

		if (emap[j].mem.valid_size) {
			rc = camera_io_dev_read_seq(&e_ctrl->io_master_info,
				emap[j].mem.addr, memptr,
				emap[j].mem.addr_type,
				emap[j].mem.valid_size);
			if (rc) {
				CAM_ERR(CAM_SL_EEPROM, "read failed rc %d",
					rc);
				return rc;
			}
			memptr += emap[j].mem.valid_size;
		}
	}
	return rc;
}

/**
 * sl_eeprom_power_up - Power up eeprom hardware
 * @e_ctrl:     ctrl structure
 * @power_info: power up/down info for eeprom
 *
 * Returns success or failure
 */
static int sl_eeprom_power_up(struct sl_eeprom_ctrl_t *e_ctrl,
	struct cam_sensor_power_ctrl_t *power_info)
{
	int32_t                 rc = 0;
	struct cam_hw_soc_info *soc_info = &e_ctrl->soc_info;
	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&e_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM,
			"failed to fill power up vreg params rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&e_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM,
			"failed to fill power down vreg params  rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;
	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "failed in eeprom power up rc %d", rc);
		return rc;
	}

	if (e_ctrl->io_master_info.master_type == CCI_MASTER) {
		rc = camera_io_init(&(e_ctrl->io_master_info));
		if (rc) {
			CAM_ERR(CAM_SL_EEPROM, "cci_init failed");
			return -EINVAL;
		}
	}
	return rc;
}

/**
 * sl_eeprom_power_down - Power down eeprom hardware
 * @e_ctrl:    ctrl structure
 *
 * Returns success or failure
 */
static int sl_eeprom_power_down(struct sl_eeprom_ctrl_t *e_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info         *soc_info;
	struct sl_eeprom_soc_private  *soc_private;
	int                             rc = 0;

	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "failed: e_ctrl %pK", e_ctrl);
		return -EINVAL;
	}
	soc_private =
		(struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &e_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_SL_EEPROM, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = msm_camera_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "power down the core is failed:%d", rc);
		return rc;
	}

	if (e_ctrl->io_master_info.master_type == CCI_MASTER)
		camera_io_release(&(e_ctrl->io_master_info));
	return rc;
}


/**
 * sl_eeprom_parse_read_memory_map - Parse memory map
 * @of_node:    device node
 * @e_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
int32_t sl_eeprom_parse_read_memory_map(struct device_node *of_node,
	struct sl_eeprom_ctrl_t *e_ctrl)
{
	int32_t                         rc = 0;
	struct sl_eeprom_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;
	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "failed: e_ctrl is NULL");
		return -EINVAL;
	}
	soc_private =
		(struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	rc = sl_eeprom_parse_dt_memory_map(of_node, &e_ctrl->cal_data);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "failed: eeprom dt parse rc %d", rc);
		return rc;
	}
	rc = sl_eeprom_power_up(e_ctrl, power_info);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "failed: eeprom power up rc %d", rc);
		goto data_mem_free;
	}

	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_CONFIG;
	rc = sl_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "read_eeprom_memory failed");
		goto power_down;
	}

	rc = sl_eeprom_power_down(e_ctrl);
	if (rc)
		CAM_ERR(CAM_SL_EEPROM, "failed: eeprom power down rc %d", rc);

	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_ACQUIRE;
	return rc;
power_down:
	sl_eeprom_power_down(e_ctrl);
data_mem_free:
	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
	e_ctrl->cal_data.num_data = 0;
	e_ctrl->cal_data.num_map = 0;
	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_ACQUIRE;
	return rc;
}

/**
 * sl_eeprom_power_up_wrapper - main power up func
 * @e_ctrl:     ctrl structure
 *@arg: power up arg
 *
 * Returns success or failure
 */
int32_t sl_eeprom_power_up_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int32_t                         rc = 0;
	struct sl_eeprom_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;
           if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "failed: e_ctrl is NULL");
		return -EINVAL;
	}
	if (e_ctrl->sl_eeprom_state != CAM_SL_EEPROM_INIT) {
		CAM_ERR(CAM_SL_EEPROM, "failed: state error %d",
			e_ctrl->sl_eeprom_state);
		return -EINVAL;
	}
	soc_private = (struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	rc = sl_eeprom_power_up(e_ctrl, power_info);
	if (rc) {
		CAM_ERR(CAM_SL_EEPROM, "failed: eeprom power up rc %d", rc);
		goto error;
	}
	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_CONFIG;
	return rc;
error:
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_INIT;
	return rc;
}

/**
 * sl_eeprom_read_eeprom_wrapper - main mem read  func
 * @e_ctrl:     ctrl structure
 *@arg: read arg
 *
 * Returns success or failure
 */
int32_t sl_eeprom_read_eeprom_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int32_t	rc = 0;
	uint8_t *user_addr = NULL;
#if TEST_READ
	int32_t	i = 0;
#endif
#if  WRITE_TO_FILE
	struct file *fp = 0;
	mm_segment_t fs;
	loff_t pos = 0;
	int count;
#endif
	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "failed: e_ctrl is NULL");
		return -EINVAL;
	}
	if (e_ctrl->sl_eeprom_state != CAM_SL_EEPROM_CONFIG) {
		CAM_ERR(CAM_SL_EEPROM, "failed: state error %d",
			e_ctrl->sl_eeprom_state);
		return -EINVAL;
	}
#if WRITE_TO_FILE
	fp = filp_open("/sdcard/DCIM/sl_cal.txt", O_RDWR | O_CREAT, 0777);
	if (IS_ERR(fp)){
		CAM_ERR(CAM_SL_EEPROM, "create file error");
		return -ENOENT;
	}
#endif
	e_ctrl->userspace_probe = true;
	if (e_ctrl->userspace_probe == false) {
		rc = sl_eeprom_parse_dt_memory_map(e_ctrl->soc_info.dev->of_node, &e_ctrl->cal_data);
		if (rc) {
			CAM_ERR(CAM_SL_EEPROM, "failed: eeprom dt parse rc %d", rc);
			return rc;
		}

		rc = sl_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
		if (rc) {
			CAM_ERR(CAM_SL_EEPROM, "read_eeprom_memory failed");
			goto free_map;
		}
	}else{
		sl_eeprom_parse_memory_map(e_ctrl->soc_info.dev->of_node, &e_ctrl->cal_data, arg, &user_addr);
		if (rc) {
			CAM_ERR(CAM_SL_EEPROM, "failed: sl_eeprom_parse_memory_map rc %d", rc);
			return rc;
		}
		rc = sl_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
		if (rc) {
			CAM_ERR(CAM_SL_EEPROM, "read_eeprom_memory failed");
			goto free_map;
		}
		if (!user_addr){
			CAM_ERR(CAM_SL_EEPROM,
				"read_eeprom_memory failed, user_addr error");
			goto free_map;
		}
		rc = copy_to_user((void __user *)user_addr,
			e_ctrl->cal_data.mapdata, e_ctrl->cal_data.num_data);

		 if (rc < 0){
			CAM_ERR(CAM_SL_EEPROM, "sl_eeprom_parse_memory_map copy to user failed\n");
		}

	}
	e_ctrl->userspace_probe = false;
#if TEST_READ
	for (i = 0; i < e_ctrl->cal_data.num_data; i ++){
		CAM_ERR(CAM_SL_EEPROM,
			"sl_eeprom_read_eeprom_wrapper e_ctrl->cal_data.mapdata, i %d  byte 0x%x ",
			i,  e_ctrl->cal_data.mapdata[i]);
	}
#endif
#if WRITE_TO_FILE
	fs = get_fs();
	set_fs(KERNEL_DS);
	count = vfs_write(fp, e_ctrl->cal_data.mapdata, e_ctrl->cal_data.num_data,  &pos);
	set_fs(fs);
	if (count !=e_ctrl->cal_data.num_data){
		CAM_ERR(CAM_SL_EEPROM, "write to file error");
		rc = -EIO;
	}
#endif
free_map:
	kfree(e_ctrl->cal_data.mapdata);
	kfree(e_ctrl->cal_data.map);
	e_ctrl->cal_data.num_data = 0;
	e_ctrl->cal_data.num_map = 0;
#if WRITE_TO_FILE
file_close:
	filp_close(fp, NULL);
#endif
	return rc;
}

/**
 * sl_eeprom_write_eeprom_wrapper - main write  func
 * @e_ctrl:     ctrl structure
 *@arg: write arg
 *
 * Returns success or failure
 */
int32_t sl_eeprom_write_eeprom_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int32_t rc = 0;
	unsigned short i = 0;

	struct cam_sensor_i2c_reg_setting reg_data_map;
	struct cam_sensor_i2c_reg_array *reg_settings ;
	uint8_t *map_data;
	struct cam_sensor_i2c_reg_setting reg_data_map_w;
	struct cam_sensor_i2c_reg_array regsetting_w;
	struct sl_eeprom_soc_private     *eb_info;
	uint32_t data_r;
	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "failed: e_ctrl is NULL");
		return -EINVAL;
	}
	if (e_ctrl->sl_eeprom_state != CAM_SL_EEPROM_CONFIG) {
		CAM_ERR(CAM_SL_EEPROM, "failed: state error %d",
			e_ctrl->sl_eeprom_state);
		return -EINVAL;
	}
	rc = copy_from_user(&reg_data_map, (void __user *)arg, sizeof(struct cam_sensor_i2c_reg_setting));
	if  (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "sl_eeprom_write_eeprom_wrapper copy from user failed\n");
		return rc;
	}
	reg_settings = kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * reg_data_map.size, GFP_KERNEL);
	if (!reg_settings){
		CAM_ERR(CAM_SL_EEPROM, "reg_settings alloc error");
	}
	rc =  copy_from_user(reg_settings, (void __user*)reg_data_map.reg_setting, sizeof(struct cam_sensor_i2c_reg_array) * reg_data_map.size);
	if (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "sl_eeprom_write_eeprom_wrapper copy from user setting failed\n");
		goto	free_setting;
	}
	reg_data_map.reg_setting = reg_settings;

	eb_info = (struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	rc = sl_eeprom_update_i2c_info(e_ctrl, &eb_info->i2c_info);
	if (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "update i2c info fail rc %d", rc);
		goto free_setting;
	}
#if TEST_READ
	map_data = kzalloc( sizeof(uint8_t)*reg_data_map.size, GFP_KERNEL);
	rc = camera_io_dev_read_seq(&e_ctrl->io_master_info, reg_data_map.reg_setting[0].reg_addr, map_data,
				 reg_data_map.addr_type, reg_data_map.size);
	if (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "read eeprom error rc %d", rc);
	}
	for (i = 0 ; i <  reg_data_map.size; i ++){
		CAM_ERR(CAM_SL_EEPROM, "data [%d] [0x%4x] [0x%2x]", i, reg_data_map.reg_setting[0].reg_addr + i,
			map_data[i]);
	}
#endif

	for (i = 0 ; i < reg_data_map.size; i ++){
		reg_data_map_w.addr_type = reg_data_map.addr_type;
		reg_data_map_w.size = 1;
		reg_data_map_w.data_type = reg_data_map.data_type;
		reg_data_map_w.delay = reg_data_map.delay;
		memcpy(&regsetting_w, &reg_data_map.reg_setting[i], sizeof(struct cam_sensor_i2c_reg_array));
		reg_data_map_w.reg_setting = & regsetting_w;
		rc = camera_io_dev_write(&e_ctrl->io_master_info, &reg_data_map_w);
		if (rc < 0){
			CAM_ERR(CAM_SL_EEPROM, "write eeprom error, rc %d", rc);
			goto	free_setting;
		}
		usleep_range(3000, 4000);
	}

#if CONTINUS_WRITE
	rc = camera_io_dev_write_continuous(&e_ctrl->io_master_info, &reg_data_map, 1);
	if (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "write eeprom error, rc %d", rc);
	}
#endif
#if 1
	usleep_range(10000, 11000);
	map_data = kzalloc( sizeof(uint8_t)*reg_data_map.size, GFP_KERNEL);
	memset(map_data, 0,  reg_data_map.size);
	rc = camera_io_dev_read_seq(&e_ctrl->io_master_info,
		reg_data_map.reg_setting[1].reg_addr, map_data + 1,
		reg_data_map.addr_type, reg_data_map.size - 1);
	if (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "read eeprom error rc %d", rc);
			goto	free_data;
	}
	rc =  camera_io_dev_read(&e_ctrl->io_master_info,
		reg_data_map.reg_setting[0].reg_addr,
		&data_r, reg_data_map.addr_type,
		SL_EEPROM_SENSOR_I2C_TYPE_BYTE);
	if (rc < 0) {
		CAM_ERR(CAM_SL_EEPROM, "read eeprom error, rc %d", rc);
		goto	free_setting;
	}
	*map_data = data_r;
	for (i = 0 ; i <  reg_data_map.size - 1; i++) {
		CAM_ERR(CAM_SL_EEPROM, "data [%d] [0x%4x] [0x%2x]",
			i, reg_data_map.reg_setting[i].reg_addr,
			map_data[i]);
	}
free_data:
#endif
	kfree(map_data);
free_setting:
	kfree(reg_settings);
	return rc;
}

/**
 * sl_eeprom_power_down_wrapper - main power down  func
 * @e_ctrl:     ctrl structure
 *@arg: power down arg
 *
 * Returns success or failure
 */
int32_t sl_eeprom_power_down_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int32_t                         rc = 0;
	struct sl_eeprom_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;
	if (!e_ctrl) {
		CAM_ERR(CAM_SL_EEPROM, "failed: e_ctrl is NULL");
		return -EINVAL;
	}
	if (e_ctrl->sl_eeprom_state == CAM_SL_EEPROM_INIT) {
		CAM_ERR(CAM_SL_EEPROM, "failed: state error %d",
			e_ctrl->sl_eeprom_state);
		return -EINVAL;
	}
	soc_private = (struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	rc =sl_eeprom_power_down(e_ctrl);
	if (rc){
		CAM_ERR(CAM_SL_EEPROM, "failed: eeprom power down rc %d", rc);
		goto error;
	}
	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_INIT;
	return rc;
error:
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	e_ctrl->sl_eeprom_state = CAM_SL_EEPROM_INIT;
	return rc;
}

