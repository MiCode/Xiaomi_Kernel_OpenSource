/*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MI_SHA256_H_
#define __MI_SHA256_H_

#define USBPD_EVENT_POWER_SUPPLY_READY          0x10
#define USBPD_EVENT_CABLE_DETACH                0x01

extern bool verify_fg_digest(const u8 *digest, int digest_len, u8 *rand_num, int num_len);

#endif
