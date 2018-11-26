#ifndef _sl_EEPROM_SOC_H_
#define _sl_EEPROM_SOC_H_

#include "3dsl_eeprom_dev.h"

int sl_eeprom_parse_dt_memory_map(struct device_node *of,
	struct sl_eeprom_memory_block_t *data);
int sl_eeprom_parse_memory_map(struct device_node *node,
	struct sl_eeprom_memory_block_t *data,  void *source,  uint8_t** user_addr);
int sl_eeprom_parse_dt(struct sl_eeprom_ctrl_t *e_ctrl);
#endif/* _sl_EEPROM_SOC_H_ */

