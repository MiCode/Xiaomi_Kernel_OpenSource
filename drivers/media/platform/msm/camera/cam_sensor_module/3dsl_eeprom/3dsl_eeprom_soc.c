#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>

#include "3dsl_eeprom_soc.h"
#include "cam_debug_util.h"

/*
 * sl_eeprom_parse_dt_memory_map() - parse memory map in device node
 * @of:         device node
 * @data:       memory block for output
 *
 * This functions parses @of to fill @data.  It allocates map itself, parses
 * the @of node, calculate total data length, and allocates required buffer.
 * It only fills the map, but does not perform actual reading.
 */
int sl_eeprom_parse_dt_memory_map(struct device_node *node,
	struct sl_eeprom_memory_block_t *data)
{
	int       i, rc = 0;
	char      property[PROPERTY_MAXSIZE];
	uint32_t  count = MSM_EEPROM_MEM_MAP_PROPERTIES_CNT;
	struct    sl_eeprom_memory_map_t *map;
	snprintf(property, PROPERTY_MAXSIZE, "num-blocks");
	rc = of_property_read_u32(node, property, &data->num_map);
	if (rc < 0) {
		CAM_ERR(CAM_SL_EEPROM, "failed: num-blocks not available rc %d",
			rc);
		return rc;
	}
	map = kzalloc((sizeof(*map) * data->num_map), GFP_KERNEL);
	if (!map) {
		rc = -ENOMEM;
		return rc;
	}
	data->map = map;
	for (i = 0; i < data->num_map; i++) {
		snprintf(property, PROPERTY_MAXSIZE, "page%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].page, count);
		if (rc < 0) {
			CAM_ERR(CAM_SL_EEPROM, "failed: page not available rc %d",
				rc);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE, "pageen%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].pageen, count);
		if (rc < 0)
			CAM_DBG(CAM_SL_EEPROM, "pageen not needed");

		snprintf(property, PROPERTY_MAXSIZE, "saddr%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].saddr, 1);
		if (rc < 0)
			CAM_DBG(CAM_SL_EEPROM, "saddr not needed - block %d", i);

		snprintf(property, PROPERTY_MAXSIZE, "poll%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].poll, count);
		if (rc < 0) {
			CAM_ERR(CAM_SL_EEPROM, "failed: poll not available rc %d",
				rc);
			goto ERROR;
		}

		snprintf(property, PROPERTY_MAXSIZE, "mem%d", i);
		rc = of_property_read_u32_array(node, property,
			(uint32_t *) &map[i].mem, count);
		if (rc < 0) {
			CAM_ERR(CAM_SL_EEPROM, "failed: mem not available rc %d",
				rc);
			goto ERROR;
		}
		data->num_data += map[i].mem.valid_size;
	}

	data->mapdata = kzalloc(data->num_data, GFP_KERNEL);
	if (!data->mapdata) {
		rc = -ENOMEM;
		goto ERROR;
	}
	return rc;

ERROR:
	kfree(data->map);
	memset(data, 0, sizeof(*data));
	return rc;
}

int sl_eeprom_parse_memory_map(struct device_node *node,
	struct sl_eeprom_memory_block_t *data,  void *source, uint8_t** user_addr){
	int rc = 0;
	int i;
	struct    sl_eeprom_memory_map_t *map;
	rc = copy_from_user(data, (void __user*)source, sizeof(struct sl_eeprom_memory_block_t));
	if (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "sl_eeprom_parse_memory_map copy from user failed\n");
		return rc;
	}
	map = kzalloc((sizeof(*map) * data->num_map), GFP_KERNEL);
	if (!map) {
		rc = -ENOMEM;
		return rc;
	}
	rc = copy_from_user(map, data->map, sizeof(struct sl_eeprom_memory_map_t) * data->num_map);
	if (rc < 0){
		CAM_ERR(CAM_SL_EEPROM, "sl_eeprom_parse_memory_map copy from user failed\n");
		goto ERROR;
	}
	data->map = map;
	for (i = 0; i < data->num_map; i++) {
		CAM_DBG(CAM_SL_EEPROM,
			"sl_eeprom_parse_memory_map saddr 0X%4x\n",
			data->map[i].saddr);
		CAM_DBG(CAM_SL_EEPROM,
			"sl_eeprom_parse_memory_map mem 0X%4x %d 0X%2x %d %d %d\n",
			data->map[i].mem.addr, data->map[i].mem.addr_type,
			data->map[i].mem.data, data->map[i].mem.data_type,
			data->map[i].mem.valid_size, data->map[i].mem.delay);
		CAM_DBG(CAM_SL_EEPROM,
			"sl_eeprom_parse_memory_map page 0X%4x %d 0X%2x %d %d %d\n",
			data->map[i].page.addr, data->map[i].page.addr_type,
			data->map[i].page.data, data->map[i].page.data_type,
			data->map[i].page.valid_size, data->map[i].page.delay);
		CAM_DBG(CAM_SL_EEPROM,
			"sl_eeprom_parse_memory_map pageen 0X%4x %d 0X%2x %d %d %d\n",
			data->map[i].pageen.addr, data->map[i].pageen.addr_type,
			data->map[i].pageen.data, data->map[i].pageen.data_type,
			data->map[i].pageen.valid_size, data->map[i].pageen.delay);
		CAM_DBG(CAM_SL_EEPROM,
			"sl_eeprom_parse_memory_map poll 0X%4x %d 0X%2x %d %d %d\n",
			data->map[i].poll.addr, data->map[i].poll.addr_type,
			data->map[i].poll.data, data->map[i].poll.data_type,
			data->map[i].poll.valid_size, data->map[i].poll.delay);
		CAM_DBG(CAM_SL_EEPROM,
			"sl_eeprom_parse_memory_map delay 0X%4x %d 0X%2x %d %d %d\n",
			data->map[i].delay.addr, data->map[i].delay.addr_type,
			data->map[i].delay.data, data->map[i].delay.data_type,
			data->map[i].delay.valid_size, data->map[i].delay.delay);
	}
	*user_addr = data->mapdata;
	data->mapdata = kzalloc(data->num_data, GFP_KERNEL);
	if (!data->mapdata) {
		rc = -ENOMEM;
		goto ERROR;
	}
	return rc;
ERROR:
	kfree(data->map);
	memset(data, 0, sizeof(*data));
	return rc;
}
/**
*sl_eeprom_get_dt_data
 * @e_ctrl: ctrl structure
 *
 * Parses eeprom dt
 */
static int sl_eeprom_get_dt_data(struct sl_eeprom_ctrl_t *e_ctrl)
{
	int                             rc = 0;
	struct cam_hw_soc_info         *soc_info = &e_ctrl->soc_info;
	struct sl_eeprom_soc_private  *soc_private =
		(struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;
	struct device_node             *of_node = NULL;
	of_node = soc_info->dev->of_node;
	if (e_ctrl->userspace_probe == false) {
		rc = cam_get_dt_power_setting_data(of_node,
			soc_info, power_info);
		if (rc < 0) {
			CAM_ERR(CAM_SL_EEPROM, "failed in getting power settings");
			return rc;
		}
	}
	if (!soc_info->gpio_data) {
		CAM_INFO(CAM_SL_EEPROM, "No GPIO found");
		return 0;
	}
	if (!soc_info->gpio_data->cam_gpio_common_tbl_size) {
		CAM_INFO(CAM_SL_EEPROM, "No GPIO found");
		return -EINVAL;
	}
	rc = cam_sensor_util_init_gpio_pin_tbl(soc_info,
		&power_info->gpio_num_info);
	if ((rc < 0) || (!power_info->gpio_num_info)) {
		CAM_ERR(CAM_SL_EEPROM, "No/Error EEPROM GPIOs");
		return -EINVAL;
	}
	return rc;
}

/**
*sl_eeprom_parse_dt
 * @e_ctrl: ctrl structure
 *
 * This function is called from cam_eeprom_platform/i2c/spi_driver_probe
 * it parses the eeprom dt node and decides for userspace or kernel probe.
 */
int sl_eeprom_parse_dt(struct sl_eeprom_ctrl_t *e_ctrl)
{
	int	i;
	int rc = 0;
	struct cam_hw_soc_info         *soc_info = &e_ctrl->soc_info;
	struct device_node             *of_node = NULL;
	struct sl_eeprom_soc_private  *soc_private =
		(struct sl_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	uint32_t                        temp;
	if (!soc_info->dev) {
		CAM_ERR(CAM_SL_EEPROM, "Dev is NULL");
		return -EINVAL;
	}
	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_SL_EEPROM, "Failed to read DT properties rc : %d", rc);
		return rc;
	}
	of_node = soc_info->dev->of_node;
	rc = of_property_read_string(of_node, "sl-eeprom-name",
		&soc_private->eeprom_name);
	if (rc < 0) {
		CAM_ERR(CAM_SL_EEPROM, "kernel probe is not enabled");
		e_ctrl->userspace_probe = true;
	}
	if (e_ctrl->io_master_info.master_type == CCI_MASTER) {
		rc = of_property_read_u32(of_node, "cci-master",
			&e_ctrl->cci_i2c_master);
		CAM_ERR(CAM_SL_EEPROM, "cci master");
		if (rc < 0 || (e_ctrl->cci_i2c_master >= MASTER_MAX)) {
			CAM_ERR(CAM_SL_EEPROM, "failed rc %d", rc);
			rc = -EFAULT;
			return rc;
		}
	}
	rc = sl_eeprom_get_dt_data(e_ctrl);
	if (rc < 0)
		CAM_ERR(CAM_SL_EEPROM, "failed: eeprom get dt data rc %d", rc);
	if ((e_ctrl->userspace_probe == false) &&
			(e_ctrl->io_master_info.master_type != SPI_MASTER)) {
		rc = of_property_read_u32(of_node, "slave-addr", &temp);
		if (rc < 0)
			CAM_ERR(CAM_SL_EEPROM, "failed: no slave-addr rc %d", rc);
		soc_private->i2c_info.slave_addr = temp;
		rc = of_property_read_u32(of_node, "i2c-freq-mode", &temp);
		soc_private->i2c_info.i2c_freq_mode = temp;
		if (rc < 0) {
			CAM_ERR(CAM_SL_EEPROM,
				"i2c-freq-mode read fail %d", rc);
			soc_private->i2c_info.i2c_freq_mode = 0;
		}
		if (soc_private->i2c_info.i2c_freq_mode >= I2C_MAX_MODES) {
			CAM_ERR(CAM_SL_EEPROM, "invalid i2c_freq_mode = %d",
				soc_private->i2c_info.i2c_freq_mode);
			soc_private->i2c_info.i2c_freq_mode = 0;
		}
	}
	for (i = 0; i < soc_info->num_clk; i++) {
		soc_info->clk[i] = devm_clk_get(soc_info->dev,
			soc_info->clk_name[i]);
		if (!soc_info->clk[i]) {
			CAM_ERR(CAM_SL_EEPROM, "get failed for %s",
				soc_info->clk_name[i]);
			rc = -ENOENT;
			return rc;
		}
	}
	return rc;
}

