/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#ifndef LINUX_SPI_FPC1020_REGULATOR_H
#define LINUX_SPI_FPC1020_REGULATOR_H

#define SUPPLY_1V8		1800000UL
#define SUPPLY_3V3		3300000UL
#define SUPPLY_SPI_MIN		SUPPLY_1V8
#define SUPPLY_SPI_MAX		SUPPLY_1V8

#define SUPPLY_IO_MIN		SUPPLY_1V8
#define SUPPLY_IO_MAX		SUPPLY_1V8

#define SUPPLY_ANA_MIN		SUPPLY_1V8
#define SUPPLY_ANA_MAX		SUPPLY_1V8

#define SUPPLY_TX_MIN		SUPPLY_3V3
#define SUPPLY_TX_MAX		SUPPLY_3V3

#define SUPPLY_SPI_REQ_CURRENT	10U
#define SUPPLY_IO_REQ_CURRENT	6000U
#define SUPPLY_ANA_REQ_CURRENT	6000U

extern int fpc1020_regulator_configure(fpc1020_data_t *fpc1020);

extern int fpc1020_regulator_release(fpc1020_data_t *fpc1020);

extern int fpc1020_regulator_set(fpc1020_data_t *fpc1020, bool enable);

#endif /* LINUX_SPI_FPC1020_REGULATOR_H */

