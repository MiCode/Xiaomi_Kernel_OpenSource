/*
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/tegra_caif.h>
#include <mach/spi.h>
#include <net/caif/caif_spi.h>

MODULE_LICENSE("GPL");

#define SPI_CAIF_PAD_TRANSACTION_SIZE(x) \
	(((x) > 4) ? ((((x) + 15) / 16) * 16) : (x))

struct sspi_struct {
	struct cfspi_dev sdev;
	struct cfspi_xfer *xfer;
};

static struct sspi_struct slave;
static struct platform_device slave_device;
static struct spi_device *tegra_caif_spi_slave_device;
int tegra_caif_sspi_gpio_spi_int;
int tegra_caif_sspi_gpio_spi_ss;
int tegra_caif_sspi_gpio_reset;
int tegra_caif_sspi_gpio_power;
int tegra_caif_sspi_gpio_awr;
int tegra_caif_sspi_gpio_cwr;


static int __devinit tegra_caif_spi_slave_probe(struct spi_device *spi);

static int tegra_caif_spi_slave_remove(struct spi_device *spi)
{
	return 0;
}

#ifdef CONFIG_PM
static int tegra_caif_spi_slave_suspend(struct spi_device *spi
			, pm_message_t mesg)
{
	return 0;
}
#endif /* CONFIG_PM */

#ifdef CONFIG_PM
static int tegra_caif_spi_slave_resume(struct spi_device *spi)
{
	return 0;
}
#endif /* CONFIG_PM */

static struct spi_driver tegra_caif_spi_slave_driver = {
	.driver = {
		.name = "baseband_spi_slave0.0",
		.owner = THIS_MODULE,
	},
	.probe = tegra_caif_spi_slave_probe,
	.remove = __devexit_p(tegra_caif_spi_slave_remove),
#ifdef CONFIG_PM
	.suspend = tegra_caif_spi_slave_suspend,
	.resume = tegra_caif_spi_slave_resume,
#endif /* CONFIG_PM */
};

void tegra_caif_modem_power(int on)
{
	static int u3xx_on;
	int err;
	int cnt = 0;
	int val = 0;

	if (u3xx_on == on)
		return;
	u3xx_on = on;

	if (u3xx_on) {
		/* turn on u3xx modem */
		err = gpio_request(tegra_caif_sspi_gpio_reset
				, "caif_sspi_reset");
		if (err < 0)
			goto err1;

		err = gpio_request(tegra_caif_sspi_gpio_power
				, "caif_sspi_power");
		if (err < 0)
			goto err2;

		err = gpio_request(tegra_caif_sspi_gpio_awr
				, "caif_sspi_awr");
		if (err < 0)
			goto err3;

		err = gpio_request(tegra_caif_sspi_gpio_cwr
				, "caif_sspi_cwr");
		if (err < 0)
			goto err4;

		err = gpio_direction_output(tegra_caif_sspi_gpio_reset
						, 0 /* asserted */);
		if (err < 0)
			goto err5;

		err = gpio_direction_output(tegra_caif_sspi_gpio_power
						, 0 /* off */);
		if (err < 0)
			goto err6;

		err = gpio_direction_output(tegra_caif_sspi_gpio_awr
						, 0);
		if (err < 0)
			goto err7;

		err = gpio_direction_input(tegra_caif_sspi_gpio_cwr);
		if (err < 0)
			goto err8;

		gpio_set_value(tegra_caif_sspi_gpio_power, 0);
		gpio_set_value(tegra_caif_sspi_gpio_reset, 0);

		msleep(800);

		/* pulse modem power on for 300 ms */
		gpio_set_value(tegra_caif_sspi_gpio_reset
				, 1 /* deasserted */);
		msleep(300);
		gpio_set_value(tegra_caif_sspi_gpio_power, 1);
		msleep(300);
		gpio_set_value(tegra_caif_sspi_gpio_power, 0);
		msleep(100);

		/* set awr high */
		gpio_set_value(tegra_caif_sspi_gpio_awr, 1);
		val = gpio_get_value(tegra_caif_sspi_gpio_cwr);
		while (!val) {
			/* wait for cwr to go high */
			val = gpio_get_value(tegra_caif_sspi_gpio_cwr);
			pr_info(".");
			msleep(100);
			cnt++;
			if (cnt > 200) {
				pr_err("\nWaiting for CWR timed out - ERROR\n");
				break;
			}
		}
	}
	return;
err8:
err7:
err6:
err5:
	gpio_free(tegra_caif_sspi_gpio_cwr);
err4:
	gpio_free(tegra_caif_sspi_gpio_awr);
err3:
	gpio_free(tegra_caif_sspi_gpio_power);
err2:
	gpio_free(tegra_caif_sspi_gpio_reset);
err1:
	return;
}

static irqreturn_t sspi_irq(int irq, void *arg)
{
	/* You only need to trigger on an edge to the active state of the
	 * SS signal. Once a edge is detected, the ss_cb() function should
	 * be called with the parameter assert set to true. It is OK
	 * (and even advised) to call the ss_cb() function in IRQ context
	 * in order not to add any delay.
	 */
	int val;
	struct cfspi_dev *sdev = (struct cfspi_dev *)arg;
	val = gpio_get_value(tegra_caif_sspi_gpio_spi_ss);
	if (val)
		return IRQ_HANDLED;
	sdev->ifc->ss_cb(true, sdev->ifc);
	return IRQ_HANDLED;
}

static int sspi_callback(void *arg)
{
	/* for each spi_sync() call
	 * - sspi_callback() called before spi transfer
	 * - sspi_complete() called after spi transfer
	 */

	/* set master interrupt gpio pin active (tells master to
	 * start spi clock)
	 */
	udelay(MIN_TRANSITION_TIME_USEC);
	gpio_set_value(tegra_caif_sspi_gpio_spi_int, 1);
	return 0;
}

static void sspi_complete(void *context)
{
	/* Normally the DMA or the SPI framework will call you back
	 * in something similar to this. The only thing you need to
	 * do is to call the xfer_done_cb() function, providing the pointer
	 * to the CAIF SPI interface. It is OK to call this function
	 * from IRQ context.
	 */

	struct cfspi_dev *sdev = (struct cfspi_dev *)context;
	sdev->ifc->xfer_done_cb(sdev->ifc);
}

static void swap_byte(unsigned char *buf, unsigned int bufsiz)
{
	unsigned int i;
	unsigned char tmp;
	for (i = 0; i < bufsiz; i += 2) {
		tmp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = tmp;
	}
}

static int sspi_init_xfer(struct cfspi_xfer *xfer, struct cfspi_dev *dev)
{
	/* Store transfer info. For a normal implementation you should
	 * set up your DMA here and make sure that you are ready to
	 * receive the data from the master SPI.
	 */

	struct sspi_struct *sspi = (struct sspi_struct *)dev->priv;
	struct spi_message m;
	struct spi_transfer t;
	int err;

	sspi->xfer = xfer;

	if (!tegra_caif_spi_slave_device)
		return -ENODEV;

	err = spi_tegra_register_callback(tegra_caif_spi_slave_device,
		sspi_callback, sspi);
	if (err < 0) {
		pr_err("\nspi_tegra_register_callback() failed\n");
		return -ENODEV;
	}
	memset(&t, 0, sizeof(t));
	t.tx_buf = xfer->va_tx;
	swap_byte(xfer->va_tx, xfer->tx_dma_len);
	t.rx_buf = xfer->va_rx;
	t.len = max(xfer->tx_dma_len, xfer->rx_dma_len);
	t.len = SPI_CAIF_PAD_TRANSACTION_SIZE(t.len);
	t.bits_per_word = 16;
	/* SPI controller clock should be 4 times the spi_clk */
	t.speed_hz = (SPI_MASTER_CLK_MHZ * 4 * 1000000);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	dmb();
	err = spi_sync(tegra_caif_spi_slave_device, &m);
	dmb();
	swap_byte(xfer->va_tx, xfer->tx_dma_len);
	swap_byte(xfer->va_rx, xfer->rx_dma_len);
	sspi_complete(&sspi->sdev);
	if (err < 0) {
		pr_err("spi_init_xfer - spi_sync() err %d\n", err);
		return err;
	}
	return 0;
}

void sspi_sig_xfer(bool xfer, struct cfspi_dev *dev)
{
	/* If xfer is true then you should assert the SPI_INT to indicate to
	 * the master that you are ready to recieve the data from the master
	 * SPI. If xfer is false then you should de-assert SPI_INT to indicate
	 * that the transfer is done.
	 */
	if (xfer)
		gpio_set_value(tegra_caif_sspi_gpio_spi_int, 1);
	else
		gpio_set_value(tegra_caif_sspi_gpio_spi_int, 0);
}

static void sspi_release(struct device *dev)
{
	/*
	 * Here you should release your SPI device resources.
	 */
}

static int __init sspi_init(void)
{
	/* Here you should initialize your SPI device by providing the
	 * necessary functions, clock speed, name and private data. Once
	 * done, you can register your device with the
	 * platform_device_register() function. This function will return
	 * with the CAIF SPI interface initialized. This is probably also
	 * the place where you should set up your GPIOs, interrupts and SPI
	 * resources.
	 */

	int res = 0;

	/* Register Tegra SPI protocol driver. */
	res = spi_register_driver(&tegra_caif_spi_slave_driver);
	if (res < 0)
		return res;

	/* Initialize slave device. */
	slave.sdev.init_xfer = sspi_init_xfer;
	slave.sdev.sig_xfer = sspi_sig_xfer;
	slave.sdev.clk_mhz = SPI_MASTER_CLK_MHZ;
	slave.sdev.priv = &slave;
	slave.sdev.name = "spi_sspi";
	slave_device.dev.release = sspi_release;

	/* Initialize platform device. */
	slave_device.name = "cfspi_sspi";
	slave_device.dev.platform_data = &slave.sdev;

	/* Register platform device. */
	res = platform_device_register(&slave_device);
	if (res)
		return -ENODEV;

	return res;
}

static void __exit sspi_exit(void)
{
	/* Delete platfrom device. */
	platform_device_del(&slave_device);

	/* Free Tegra SPI protocol driver. */
	spi_unregister_driver(&tegra_caif_spi_slave_driver);

	/* Free Tegra GPIO interrupts. */
	disable_irq(gpio_to_irq(tegra_caif_sspi_gpio_spi_ss));
	free_irq(gpio_to_irq(tegra_caif_sspi_gpio_spi_ss), &slave_device);

	/* Free Tegra GPIOs. */
	gpio_free(tegra_caif_sspi_gpio_spi_ss);
	gpio_free(tegra_caif_sspi_gpio_spi_int);
}

static int __devinit tegra_caif_spi_slave_probe(struct spi_device *spi)
{
	struct tegra_caif_platform_data *pdata;
	int res;

	if (!spi)
		return -ENODEV;

	pdata = spi->dev.platform_data;
	if (!pdata)
		return -ENODEV;

	tegra_caif_sspi_gpio_spi_int = pdata->spi_int;
	tegra_caif_sspi_gpio_spi_ss = pdata->spi_ss;
	tegra_caif_sspi_gpio_reset = pdata->reset;
	tegra_caif_sspi_gpio_power = pdata->power;
	tegra_caif_sspi_gpio_awr = pdata->awr;
	tegra_caif_sspi_gpio_cwr = pdata->cwr;

	tegra_caif_spi_slave_device = spi;

	/* Initialize Tegra GPIOs. */
	res = gpio_request(tegra_caif_sspi_gpio_spi_int, "caif_sspi_spi_int");
	if (res < 0)
		goto err1;

	res = gpio_request(tegra_caif_sspi_gpio_spi_ss, "caif_sspi_ss");
	if (res < 0)
		goto err2;

	res = gpio_direction_output(tegra_caif_sspi_gpio_spi_int, 0);
	if (res < 0)
		goto err3;

	res = gpio_direction_input(tegra_caif_sspi_gpio_spi_ss);
	if (res < 0)
		goto err4;

	tegra_caif_modem_power(1);
	msleep(2000);

	/* Initialize Tegra GPIO interrupts. */
	res = request_irq(gpio_to_irq(tegra_caif_sspi_gpio_spi_ss),
		sspi_irq, IRQF_TRIGGER_FALLING, "caif_sspi_ss_irq",
		&slave.sdev);
	if (res < 0)
		goto err5;

	return 0;
err5:
	free_irq(gpio_to_irq(tegra_caif_sspi_gpio_spi_ss), &slave_device);
err4:
err3:
	gpio_free(tegra_caif_sspi_gpio_spi_ss);
err2:
	gpio_free(tegra_caif_sspi_gpio_spi_int);
err1:
	return res;
}

module_init(sspi_init);
module_exit(sspi_exit);
