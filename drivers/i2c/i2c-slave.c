/*
 * i2c-slave.c - a device driver for the iic-slave bus interface.
 *
 * Copyright (c) 2009-2011, NVIDIA Corporation.
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
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/i2c-slave.h>
struct i2c_slave_priv {
	struct i2c_adapter master_adap;
	struct i2c_slave_adapter *slave_adap;
	struct i2c_algorithm master_algo;
};

/**
 * i2c_slave_send - Sends data to master. When master issues a read cycle, the
 * data is sent by the slave.
 * This function copies the client data into the slave tx buffer and return to
 * client. This is not a blocking call. Data will be sent to master later once
 * slave got the master-ready cycle transfer.
 * if there is no sufficient space to write the client buffer, it will return
 * error. it will not write partial data.
 * @client: Handle to i2c-slave client.
 * @buf: Data that will be written to the master
 * @count: How many bytes to write.
 *
 * Returns negative errno, or else the number of bytes written.
 */
int i2c_slave_send(struct i2c_client *client, const char *buf, int count)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_slave_priv *priv = adap->algo_data;

	if (!(adap->algo->functionality(adap) & I2C_FUNC_I2C_SLAVE_SUPPORT))
		BUG();

	if (priv->slave_adap->slv_algo->slave_send)
		return priv->slave_adap->slv_algo->slave_send(priv->slave_adap,
			 buf, count);
	return -ENODEV;
}
EXPORT_SYMBOL(i2c_slave_send);

/**
 * i2c_slave_get_tx_status - Get amount of data available in tx buffer. If there
 * is still data in tx buffer then wait for given time to transfer complete
 * for a give timeout.
 * @client: Handle to i2c-slave client.
 * @timeout_ms: Time to wait for transfer to complete.
 *
 * Returns negative errno, or else the number of bytes remaining in tx buffer.
 */
int i2c_slave_get_tx_status(struct i2c_client *client, int timeout_ms)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_slave_priv *priv = adap->algo_data;

	if (!(adap->algo->functionality(adap) & I2C_FUNC_I2C_SLAVE_SUPPORT))
		BUG();

	if (priv->slave_adap->slv_algo->slave_get_tx_status)
		return priv->slave_adap->slv_algo->slave_get_tx_status(
			priv->slave_adap, timeout_ms);
	return -ENODEV;
}
EXPORT_SYMBOL(i2c_slave_get_tx_status);

/**
 * i2c_slave_recv - Receive data from master. The data receive from master is
 * stored on slave rx buffer. When this api will be called, the data will be
 * copied from the slave rx buffer to client buffer. If requested amount (count)
 * of data is not available then it will wait for either min_count to be receive
 * or timeout whatever first.
 *
 * if timeout_ms = 0, then wait for min_count data to be read.
 * if timoue_ms non zero then wait for the data till timeout happen.
 * @client: Handle to i2c-slave client.
 * @buf: Data that will be read from the master
 * @count: How many bytes to read.
 * @min_count: Block till read min_count of data.
 * @timeout_ms: Time to wait for read to be complete.
 *
 * Returns negative errno, or else the number of bytes read.
 */
int i2c_slave_recv(struct i2c_client *client, char *buf, int count,
			int min_count, int timeout_ms)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_slave_priv *priv = adap->algo_data;

	if (!(adap->algo->functionality(adap) & I2C_FUNC_I2C_SLAVE_SUPPORT))
		BUG();

	if (priv->slave_adap->slv_algo->slave_recv)
		return priv->slave_adap->slv_algo->slave_recv(priv->slave_adap,
			 buf, count, min_count,	timeout_ms);

	return -ENODEV;
}
EXPORT_SYMBOL(i2c_slave_recv);

/**
 * i2c_slave_start - Start the i2c slave to receive/transmit data.
 * After this i2c controller starts responding master.
 * The dummy-char will send to master if there is no data to send on slave tx
 * buffer.
 * @client: Handle to i2c-slave client.
 * @dummy_char: Data which will be send to master if there is no data to be send
 * in slave tx buffer.
 *
 * Returns negative errno, or else 0 for success.
 */
int i2c_slave_start(struct i2c_client *client, unsigned char dummy_char)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_slave_priv *priv = adap->algo_data;
	int slave_add;
	int is_10bit_addr;

	if (!(adap->algo->functionality(adap) & I2C_FUNC_I2C_SLAVE_SUPPORT))
		BUG();
	slave_add = client->addr;
	is_10bit_addr = (client->flags & I2C_CLIENT_TEN) ? 1 : 0;
	if (priv->slave_adap->slv_algo->slave_start)
		return priv->slave_adap->slv_algo->slave_start(priv->slave_adap,
	slave_add, is_10bit_addr, dummy_char);
	return -ENODEV;
}
EXPORT_SYMBOL(i2c_slave_start);

/**
 * i2c_slave_stop - Stop slave to receive/transmit data.
 * After this i2c controller stops responding master.
 * @client: Handle to i2c-slave client.
 * @is_buffer_clear: Reset the tx and rx slave buffer or not.
 */
void i2c_slave_stop(struct i2c_client *client, int is_buffer_clear)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_slave_priv *priv = adap->algo_data;

	if (!(adap->algo->functionality(adap) & I2C_FUNC_I2C_SLAVE_SUPPORT))
		BUG();

	if (priv->slave_adap->slv_algo->slave_stop)
		return priv->slave_adap->slv_algo->slave_stop(priv->slave_adap,
				 is_buffer_clear);
}
EXPORT_SYMBOL(i2c_slave_stop);

/**
 * i2c_slave_flush_buffer - Flush the receive and transmit buffer.
 * @client: Handle to i2c-slave client.
 * @is_flush_tx_buffer: Reset the tx slave buffer or not.
 * @is_flush_rx_buffer: Reset the rx slave buffer or not.
 *
 * Returns negative errno, or else 0 for success.
 */
int i2c_slave_flush_buffer(struct i2c_client *client,
			int is_flush_tx_buffer, int is_flush_rx_buffer)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_slave_priv *priv = adap->algo_data;

	if (!(adap->algo->functionality(adap) & I2C_FUNC_I2C_SLAVE_SUPPORT))
		BUG();

	if (priv->slave_adap->slv_algo->slave_flush_buffer)
		return priv->slave_adap->slv_algo->slave_flush_buffer(
			priv->slave_adap, is_flush_tx_buffer,
			is_flush_rx_buffer);

	return -ENODEV;
}
EXPORT_SYMBOL(i2c_slave_flush_buffer);

/**
 * i2c_slave_get_nack_cycle - Get the number of master read cycle on which
 * dummy char sent. This is the way to find that how much cycle slave sent the
 * NACK packet.
 *
 * @client: Handle to i2c-slave client.
 * @is_cout_reset: Reset the nack count or not.
 *
 * Returns negative errno, or else 0 for success.
 */
int i2c_slave_get_nack_cycle(struct i2c_client *client,
			int is_cout_reset)
{
	struct i2c_adapter *adap = client->adapter;
	struct i2c_slave_priv *priv = adap->algo_data;

	if (!(adap->algo->functionality(adap) & I2C_FUNC_I2C_SLAVE_SUPPORT))
		BUG();

	if (priv->slave_adap->slv_algo->slave_get_nack_cycle)
		return priv->slave_adap->slv_algo->slave_get_nack_cycle(
			priv->slave_adap, is_cout_reset);

	return -ENODEV;
}
EXPORT_SYMBOL(i2c_slave_get_nack_cycle);

static u32 i2c_slave_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C_SLAVE_SUPPORT;
}

int i2c_add_slave_adapter(struct i2c_slave_adapter *slv_adap, bool force_nr)
{
	struct i2c_slave_priv *priv;
	int ret;

	priv = kzalloc(sizeof(struct i2c_slave_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/* Set up private adapter data */
	priv->slave_adap = slv_adap;
	slv_adap->parent_data = priv;

	priv->master_algo.functionality = i2c_slave_func;

	/* Now fill out new adapter structure */
	snprintf(priv->master_adap.name, sizeof(priv->master_adap.name),
		"i2c-%d-slave", slv_adap->nr);
	priv->master_adap.owner = THIS_MODULE;
	priv->master_adap.class = slv_adap->class;
	priv->master_adap.algo = &priv->master_algo;
	priv->master_adap.algo_data = priv;
	priv->master_adap.dev.parent = slv_adap->parent_dev;

	if (force_nr) {
		priv->master_adap.nr = slv_adap->nr;
		ret = i2c_add_numbered_adapter(&priv->master_adap);
	} else {
		ret = i2c_add_adapter(&priv->master_adap);
	}
	if (ret < 0) {
		dev_err(slv_adap->parent_dev,
			"failed to add slave-adapter (error=%d)\n", ret);
		kfree(priv);
		return ret;
	}
	slv_adap->dev = &priv->master_adap.dev;
	dev_info(slv_adap->parent_dev, "Added slave i2c bus %d\n",
		 i2c_adapter_id(&priv->master_adap));

	return 0;
}
EXPORT_SYMBOL_GPL(i2c_add_slave_adapter);

int i2c_del_slave_adapter(struct i2c_slave_adapter *slv_adap)
{
	struct i2c_slave_priv *priv = slv_adap->parent_data;
	int ret;

	ret = i2c_del_adapter(&priv->master_adap);
	if (ret < 0)
		return ret;
	kfree(priv);
	return 0;
}
EXPORT_SYMBOL_GPL(i2c_del_slave_adapter);
