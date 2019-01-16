/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG


#include <linux/compiler.h>	/* "__must_check", used by regulator API*/
#include <linux/types.h>	/* "bool", used by regulator API	*/
//#include <linux/regulator/consumer.h>

#ifndef CONFIG_OF
#include <linux/spi/fpc1020.h>
#include <linux/spi/fpc1020_common.h>
#include <linux/spi/fpc1020_regulator.h>
#else
#include <linux/of.h>
#include "fpc1020.h"
#include "fpc1020_common.h"
#include "fpc1020_regulator.h"
#endif


#ifdef MTK_PLATFORM

 int fpc1020_regulator_configure(fpc1020_data_t *fpc1020)
{
	return 0;
}
int fpc1020_regulator_release(fpc1020_data_t *fpc1020)
{
	return 0;
}

int fpc1020_regulator_set(fpc1020_data_t *fpc1020, bool enable)
{
	return 0;
}
#else
/* -------------------------------------------------------------------- */
int fpc1020_regulator_configure(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->vcc_spi = regulator_get(&fpc1020->spi->dev, "vcc_spi");
	if (IS_ERR(fpc1020->vcc_spi)) {
		error = PTR_ERR(fpc1020->vcc_spi);
		dev_err(&fpc1020->spi->dev,
			"Regulator get failed, vcc_spi, error=%d\n", error);
		goto supply_err;
	}

	if (regulator_count_voltages(fpc1020->vcc_spi) > 0) {
		error = regulator_set_voltage(fpc1020->vcc_spi,
						SUPPLY_SPI_MIN, SUPPLY_SPI_MAX);
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"regulator set(spi) failed, error=%d\n", error);
			goto supply_err;
		}
	}

	fpc1020->vdd_io = regulator_get(&fpc1020->spi->dev, "vdd_io");
	if (IS_ERR(fpc1020->vdd_io)) {
		error = PTR_ERR(fpc1020->vdd_io);
		dev_err(&fpc1020->spi->dev,
			"Regulator get failed, vdd_io, error=%d\n", error);
		goto supply_err;
	}

	if (regulator_count_voltages(fpc1020->vdd_io) > 0) {
		error = regulator_set_voltage(fpc1020->vdd_io,
						SUPPLY_IO_MIN, SUPPLY_IO_MAX);
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"regulator set(io) failed, error=%d\n", error);
			goto supply_err;
		}
	}

	fpc1020->vdd_ana = regulator_get(&fpc1020->spi->dev, "vdd_ana");
	if (IS_ERR(fpc1020->vdd_ana)) {
		error = PTR_ERR(fpc1020->vdd_ana);
		dev_err(&fpc1020->spi->dev,
			"Regulator get failed, vdd_ana error=%d\n", error);
		goto supply_err;
	}

	if (regulator_count_voltages(fpc1020->vdd_ana) > 0) {
		error = regulator_set_voltage(fpc1020->vdd_ana,
						SUPPLY_ANA_MIN, SUPPLY_ANA_MAX);
		if (error) {
			dev_err(&fpc1020->spi->dev,
				"regulator set(ana) failed, error=%d\n", error);
			goto supply_err;
		}
	}

	return 0;

supply_err:
	fpc1020_regulator_release(fpc1020);
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_regulator_release(fpc1020_data_t *fpc1020)
{
	if (fpc1020->vcc_spi != NULL) {
		regulator_put(fpc1020->vcc_spi);
		fpc1020->vcc_spi = NULL;
	}

	if (fpc1020->vdd_io != NULL) {
		regulator_put(fpc1020->vdd_io);
		fpc1020->vdd_io = NULL;
	}

	if (fpc1020->vdd_ana != NULL) {
		regulator_put(fpc1020->vdd_ana);
		fpc1020->vdd_ana = NULL;
	}

	fpc1020->power_enabled = false;

	return 0;
}


/* -------------------------------------------------------------------- */
int fpc1020_regulator_set(fpc1020_data_t *fpc1020, bool enable)
{
	int error = 0;

	if ((fpc1020->vcc_spi == NULL)    ||
		(fpc1020->vdd_io == NULL) ||
		(fpc1020->vdd_ana ==  NULL)) {

		dev_err(&fpc1020->spi->dev,
			"Regulators not set\n");
			return -1;
	}

	if (enable) {
		dev_dbg(&fpc1020->spi->dev, "%s on\n", __func__);

		regulator_set_optimum_mode(fpc1020->vcc_spi,
					SUPPLY_SPI_REQ_CURRENT);

		error = (regulator_is_enabled(fpc1020->vcc_spi) == 0) ?
					regulator_enable(fpc1020->vcc_spi) : 0;

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"Regulator vcc_spi enable failed, error=%d\n",
				error);
			goto out_err;
		}

		regulator_set_optimum_mode(fpc1020->vdd_io,
					SUPPLY_IO_REQ_CURRENT);

		error = (regulator_is_enabled(fpc1020->vdd_io) == 0) ?
					regulator_enable(fpc1020->vdd_io) : 0;

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"Regulator vdd_io enable failed, error=%d\n",
				error);
			goto out_err;
		}

		regulator_set_optimum_mode(fpc1020->vdd_ana,
					 SUPPLY_ANA_REQ_CURRENT);

		error = (regulator_is_enabled(fpc1020->vdd_ana) == 0) ?
					regulator_enable(fpc1020->vdd_ana) : 0;

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"Regulator vdd_ana enable failed, error=%d\n",
				 error);
			goto out_err;
		}
	} else {
		dev_dbg(&fpc1020->spi->dev, "%s off\n", __func__);

		error = (fpc1020->power_enabled &&
			regulator_is_enabled(fpc1020->vcc_spi) > 0) ?
				 regulator_disable(fpc1020->vcc_spi) : 0;

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"Regulator vcc_spi disable failed, error=%d\n",
				error);
			goto out_err;
		}

		error = (fpc1020->power_enabled &&
			regulator_is_enabled(fpc1020->vdd_io) > 0) ?
				 regulator_disable(fpc1020->vdd_io) : 0;

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"Regulator vdd_io disable failed, error=%d\n",
				 error);
			goto out_err;
		}

		error = (fpc1020->power_enabled &&
			regulator_is_enabled(fpc1020->vdd_ana) > 0) ?
				 regulator_disable(fpc1020->vdd_ana) : 0;

		if (error) {
			dev_err(&fpc1020->spi->dev,
				"Regulator vdd_ana disable failed error=%d\n",
				 error);
			goto out_err;
		}
	}

	fpc1020->power_enabled = enable;

	return 0;

out_err:
	fpc1020_regulator_release(fpc1020);
	return error;
}
#endif


