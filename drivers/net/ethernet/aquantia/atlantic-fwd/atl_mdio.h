/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2019 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_MDIO_H_
#define _ATL_MDIO_H_

#include "atl_hw.h"

int atl_mdio_hwsem_get(struct atl_hw *hw);
void atl_mdio_hwsem_put(struct atl_hw *hw);
int __atl_mdio_read(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t *val);
int atl_mdio_read(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t *val);
int __atl_mdio_write(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t val);
int atl_mdio_write(struct atl_hw *hw, uint8_t prtad, uint8_t mmd,
	uint16_t addr, uint16_t val);

#endif /* _ATL_MDIO_H_ */
