#ifndef _sl_EEPROM_CORE_H_
#define _sl_EEPROM_CORE_H_

#include "3dsl_eeprom_dev.h"




/**
 * @e_ctrl: EEPROM ctrl structure
 *
 * This API handles the shutdown ioctl/close
 */

int32_t sl_eeprom_power_up_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg);
int32_t sl_eeprom_read_eeprom_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg);
int32_t sl_eeprom_power_down_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg);
int32_t sl_eeprom_write_eeprom_wrapper(struct sl_eeprom_ctrl_t *e_ctrl, void *arg);
#endif
/* _sl_EEPROM_CORE_H_ */
