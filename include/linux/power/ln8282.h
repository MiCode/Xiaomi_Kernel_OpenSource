/*
 * LIONSEMI LN8282 voltage regulator driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _LN8282_REGULATOR_H_
#define _LN8282_REGULATOR_H_

// (high-level) operation mode
enum {
    LN8282_OPMODE_UNKNOWN = -1,
    LN8282_OPMODE_STANDBY = 0,
    LN8282_OPMODE_BYPASS  = 1,
    LN8282_OPMODE_SWITCHING = 2,
    LN8282_OPMODE_SWITCHING_ALT = 3,
};

// Forward declarations
struct ln8282_info;

void ln8282_use_ext_5V(struct ln8282_info *info, unsigned int enable);
void ln8282_set_infet(struct ln8282_info *info, unsigned int enable);
void ln8282_set_powerpath(struct ln8282_info *info, bool forward_path);
bool ln8282_change_opmode(struct ln8282_info *info, unsigned int target_mode);
int ln8282_hw_init(struct ln8282_info *info);
/*add for other module run*/
/*
void ln8282_set_powerpath_ext(bool forward_path);
bool ln8282_change_opmode_ext(unsigned int target_mode);
unsigned int ln8282_get_opmode_ext(void);
*/

#endif
