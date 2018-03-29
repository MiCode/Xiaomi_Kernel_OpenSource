/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   Copyright (C) 2009-2013 John Crispin <blogic@openwrt.org>
 */

#ifndef _RALINK_GSW_MT7620_H__
#define _RALINK_GSW_MT7620_H__

#ifdef CONFIG_SUPPORT_OPENWRT
#include <asm/mach-ralink/ralink_regs.h>
#include <ralink_regs.h>
#include <asm/mach-ralink/mt7620.h>
#else
#include "eth_reg.h"
#include "ra_ioctl.h"
#endif

int mt7620_gsw_config(struct fe_priv *priv);
int mt7621_gsw_config(struct fe_priv *priv);
int mt7620_gsw_probe(struct fe_priv *priv);
void mt7620_set_mac(struct fe_priv *priv, unsigned char *mac);
int mt7620_mdio_write(struct mii_bus *bus, int phy_addr, int phy_reg,
		      u16 val);
int mt7620_mdio_read(struct mii_bus *bus, int phy_addr, int phy_reg);
void mt7620_mdio_link_adjust(struct fe_priv *priv, int port);
void mt7620_port_init(struct fe_priv *priv, struct device_node *np);
int mt7620a_has_carrier(struct fe_priv *priv);
int mt7623_has_carrier(struct fe_priv *priv);
void mii_mgr_read_combine(struct fe_priv *priv, u32 phy_addr,
			  u32 phy_register, u32 *read_data);
void mii_mgr_write_combine(struct fe_priv *priv, u32 phy_addr,
			   u32 phy_register, u32 write_data);
void mii_mgr_read_cl45(struct fe_priv *priv, u32 port_num, u32 dev_addr,
		       u32 reg_addr, u32 *read_data);
u32 mii_mgr_write_cl45(struct fe_priv *priv, u32 port_num, u32 dev_addr,
		       u32 reg_addr, u32 write_data);
u32 __gsw_r32(struct fe_priv *priv, unsigned reg);
void __gsw_w32(struct fe_priv *priv, u32 val, unsigned reg);
int debug_proc_init(struct fe_priv *priv);
void debug_proc_exit(void);

#endif
