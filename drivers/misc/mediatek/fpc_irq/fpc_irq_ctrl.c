/* Fingerprint Cards, Hybrid Touch sensor driver
 *
 * Copyright (c) 2014,2015 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 *
 * Software license : "Dual BSD/GPL"
 * see <linux/module.h> and ./Documentation
 * for  details.
 *
*/

#define DEBUG

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>

#include "fpc_irq_common.h"
#include "fpc_irq_ctrl.h"

/* -------------------------------------------------------------------------- */
/* function prototypes                                                        */
/* -------------------------------------------------------------------------- */
static void fpc_irq_ctrl_reset_out(fpc_irq_data_t *fpc_irq_data);


/* -------------------------------------------------------------------------- */
/* fpc_irq driver constants                                                   */
/* -------------------------------------------------------------------------- */
#define FPC1020_RESET_RETRIES	2
#define FPC1020_RESET_RETRY_US	1250

#define FPC1020_RESET_LOW_US	1000
#define FPC1020_RESET_HIGH1_US	100
#define FPC1020_RESET_HIGH2_US	1250


/* -------------------------------------------------------------------------- */
/* function definitions                                                       */
/* -------------------------------------------------------------------------- */
int fpc_irq_ctrl_init(fpc_irq_data_t *fpc_irq_data, fpc_irq_pdata_t *pdata)
{

	int error = 0;

	fpc_irq_data->pdata.rst_gpio = pdata->rst_gpio;
	{


		error = mt_set_gpio_mode(fpc_irq_data->pdata.rst_gpio, GPIO_MODE_00);

		if (error != 0) {
		printk("[FPC]gpio_request (reset) failed.\n");
		return error;
		}

		error = mt_set_gpio_dir(fpc_irq_data->pdata.rst_gpio, GPIO_DIR_OUT);

		if (error != 0) {
		printk("[FPC]gpio_direction_output(reset) failed.\n");
		return error;
		}
	}
	return error;

}


/* -------------------------------------------------------------------------- */
int fpc_irq_ctrl_destroy(fpc_irq_data_t *fpc_irq_data)
{
	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);


	return 0;
}


/* -------------------------------------------------------------------------- */
int fpc_irq_ctrl_hw_reset(fpc_irq_data_t *fpc_irq_data)
{
	int ret = 0;
	int counter = FPC1020_RESET_RETRIES;

	dev_dbg(fpc_irq_data->dev, "%s\n", __func__);

	while (counter) {

		--counter;

		fpc_irq_ctrl_reset_out(fpc_irq_data);

		ret = mt_get_gpio_in(fpc_irq_data->pdata.irq_gpio) ? 0 : -EIO;

		if (!ret) {
			dev_dbg(fpc_irq_data->dev, "%s OK\n", __func__);
			return 0;
		} else {
			dev_err(fpc_irq_data->dev,
				"%s timed out, retrying\n",
				__func__);

			udelay(FPC1020_RESET_RETRY_US);
		}
	}
	return ret;
}


/* -------------------------------------------------------------------------- */
static void fpc_irq_ctrl_reset_out(fpc_irq_data_t *fpc_irq_data)
{
	mt_set_gpio_out(fpc_irq_data->pdata.rst_gpio, 1);
	udelay(FPC1020_RESET_HIGH1_US);

	mt_set_gpio_out(fpc_irq_data->pdata.rst_gpio, 0);
	udelay(FPC1020_RESET_LOW_US);

	mt_set_gpio_out(fpc_irq_data->pdata.rst_gpio, 1);
	udelay(FPC1020_RESET_HIGH2_US);
}


/* -------------------------------------------------------------------------- */

