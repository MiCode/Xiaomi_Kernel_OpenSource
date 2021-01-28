/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Zhiyong Tao <zhiyong.tao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _STAR_PHY_H_
#define _STAR_PHY_H_

/* Reg3: PHY Identifier 2 */
#define PHY_REG_IDENTFIR2 (3)
#define PHYID2_SMSC8710A (0xC0F1)
#define PHYID2_DM9162_XMII (0xB8A0)
#define PHYID2_KSZ8081MNX (0x1560)
#define PHYID2_IP101G (0x0c54)
#define PHYID2_INTVITESSE (0x0430)
#define PHYID2_RTL8201 (0x8201)
#define PHYID2_RTL8211 (0xC912)
#define PHYID2_IP101A (0x0C50)
#define PHYID2_SMSC7100 (0xC0B1)
#define PHYID2_SMSC8700 (0xC0C4)
#define PHYID2_DM8710A (0x0011)
#define PHYID2_RTL8201FR (0xC816)

int star_detect_phyid(star_dev *dev);

#endif
