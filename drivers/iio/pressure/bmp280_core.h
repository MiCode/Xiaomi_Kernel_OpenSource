/*!
 * @section LICENSE
 * (C) Copyright 2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename bmp280_core.h
 * @date     "Fri Jan 3 15:29:18 2014 +0800"
 * @id       "3e78836"
 *
 * @brief
 * The head file of BMP280 device driver core code
*/
#ifndef _BMP280_CORE_H
#define _BMP280_CORE_H

#include "BMP280.h"
#include "bs_log.h"

/*! @defgroup bmp280_core_inc
 *  @brief The head file of BMP280 device driver core code
 @{*/
/*! define BMP device name */
#define BMP_NAME "bmp280"
#define BMP_INPUT_NAME "bmpX80"

/*! define BMP register name according to API */
#define BMP_REG_NAME(name) BMP280_##name
/*! define BMP value name according to API */
#define BMP_VAL_NAME(name) BMP280_##name
/*! define BMP hardware-related function according to API */
#define BMP_CALL_API(name) bmp280_##name
/*! only for debug */
/*#define DEBUG_BMP280*/

/*!
 * @brief bus communication operation
*/
struct bmp_bus_ops {
	/*!write pointer */
	BMP280_WR_FUNC_PTR;
	/*!read pointer */
	BMP280_RD_FUNC_PTR;
};

/*!
 * @brief bus data client
*/
struct bmp_data_bus {
	/*!bus communication operation */
	const struct bmp_bus_ops	*bops;
	/*!bmp client */
	void	*client;
};

int bmp_probe(struct device *dev, struct bmp_data_bus *data_bus);
int bmp_remove(struct device *dev);
#ifdef CONFIG_PM
int bmp_enable(struct device *dev);
int bmp_disable(struct device *dev);
#endif

#endif/*_BMP280_CORE_H*/
/*@}*/
