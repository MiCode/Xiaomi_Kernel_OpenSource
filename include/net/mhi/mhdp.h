/*
 * File: mhdp.h
 *
 * Modem-Host Interface (MHI) - MHDP kernel interface
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __NET_MHI_MHDP_H
#define __NET_MHI_MHDP_H

struct mhdp_tunnel_parm {
	char name[IFNAMSIZ];
	char master[IFNAMSIZ];
	int  pdn_id;
};

#define SIOCADDPDNID     (SIOCDEVPRIVATE + 1)
#define SIOCDELPDNID     (SIOCDEVPRIVATE + 2)
#define SIOCRESETMHDP    (SIOCDEVPRIVATE + 3)

#endif /* __NET_MHI_MHDP_H */
