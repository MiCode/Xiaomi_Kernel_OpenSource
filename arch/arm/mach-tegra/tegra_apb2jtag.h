/*
 * arch/arm/mach-tegra/tegra_apb2jtag.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MACH_TEGRA_APB2JTAG_H_
#define _MACH_TEGRA_APB2JTAG_H_

/*
 * Reads len bits from apb2jtag to buffer.
 * Assumes buf is at least len bits wide.
 */
int apb2jtag_read(u32 instr_id, u32 len, u32 chiplet, u32 *buf);

/*
 * Writes len bits from buffer to apb2jtag.
 * Assumes buf is at least len bits wide.
 */
int apb2jtag_write(u32 instr_id, u32 len, u32 chiplet, const u32 *buf);

/*
 * Accquire/Release the apb2jtag lock.
 */
void apb2jtag_get(void);
void apb2jtag_put(void);

/*
 * Locked versions of apb2jtag_read/write. Must be called with the
 * apb2jtag_lock held using get/put.
 */
int apb2jtag_read_locked(u32 instr_id, u32 len, u32 chiplet, u32 *buf);
int apb2jtag_write_locked(u32 instr_id, u32 len, u32 chiplet, const u32 *buf);

#endif /* _MACH_TEGRA_APB2JTAG_H_ */
