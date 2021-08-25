/*
 * LIONSEMI LN2702 voltage regulator driver
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef _LN2702_REGULATOR_H_
#define _LN2702_REGULATOR_H_

// (high-level) operation mode
enum {
    LN2702_OPMODE_UNKNOWN = -1,
    LN2702_OPMODE_STANDBY = 0,
    LN2702_OPMODE_BYPASS  = 1,
    LN2702_OPMODE_SWITCHING = 2,
    LN2702_OPMODE_SWITCHING_ALT = 3,
};

// Forward declarations
struct ln2702_info;

void ln2702_use_ext_5V(struct ln2702_info *info, unsigned int enable);
void ln2702_set_infet(struct ln2702_info *info, unsigned int enable);
void ln2702_set_powerpath(struct ln2702_info *info, bool forward_path);
bool ln2702_change_opmode(struct ln2702_info *info, unsigned int target_mode);
int ln2702_hw_init(struct ln2702_info *info);

#endif
