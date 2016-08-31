/*
 * i2c-slave.h - definitions for the i2c-slave-bus interface
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
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* ------------------------------------------------------------------------- */

#ifndef _LINUX_I2C_SLAVE_H
#define _LINUX_I2C_SLAVE_H

#include <linux/types.h>
#ifdef __KERNEL__
/* --- General options ------------------------------------------------	*/

struct i2c_client;
struct i2c_slave_algorithm;
struct i2c_slave_adapter;
#if defined(CONFIG_I2C_SLAVE) && defined(CONFIG_I2C)

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
extern int i2c_slave_send(struct i2c_client *client, const char *buf,
			int count);

/**
 * i2c_slave_get_tx_status - Get amount of data available in tx buffer. If there
 * is still data in tx buffer then wait for given time to transfer complete
 * for a give timeout.
 * @client: Handle to i2c-slave client.
 * @timeout_ms: Time to wait for transfer to complete.
 *
 * Returns negative errno, or else the number of bytes remaining in tx buffer.
 */
extern int i2c_slave_get_tx_status(struct i2c_client *client, int timeout_ms);

/**
 * i2c_slave_recv - Receive data from master. The data received from master is
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
extern int i2c_slave_recv(struct i2c_client *client, char *buf, int count,
			int min_count, int timeout_ms);

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
extern int i2c_slave_start(struct i2c_client *client, unsigned char dummy_char);

/**
 * i2c_slave_stop - Stop slave to receive/transmit data.
 * After this i2c controller stops responding master.
 * @client: Handle to i2c-slave client.
 * @is_buffer_clear: Reset the tx and rx slave buffer or not.
 */
extern void i2c_slave_stop(struct i2c_client *client, int is_buffer_clear);

/**
 * i2c_slave_flush_buffer - Flush the receive and transmit buffer.
 * @client: Handle to i2c-slave client.
 * @is_flush_tx_buffer: Reset the tx slave buffer or not.
 * @is_flush_rx_buffer: Reset the rx slave buffer or not.
 *
 * Returns negative errno, or else 0 for success.
 */
extern int i2c_slave_flush_buffer(struct i2c_client *client,
			int is_flush_tx_buffer,	int is_flush_rx_buffer);

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
extern int i2c_slave_get_nack_cycle(struct i2c_client *client,
			int is_cout_reset);


/**
 * i2c_add_slave_adapter - Add slave adapter.
 *
 * @slv_adap: Slave adapter.
 * @force_nr: Adapter number.
 *
 * Returns negative errno, or else 0 for success.
 */
extern int i2c_add_slave_adapter(struct i2c_slave_adapter *slv_adap,
		bool force_nr);

/**
 * i2c_del_slave_adapter - Delete slave adapter.
 *
 * @slv_adap: Slave adapter.
 *
 * Returns negative errno, or else 0 for success.
 */
extern int i2c_del_slave_adapter(struct i2c_slave_adapter *slv_adap);

#endif /* I2C_SLAVE */

/*
 * i2c_slave_adapter is the structure used to identify a physical i2c bus along
 * with the access algorithms necessary to access it.
 */
struct i2c_slave_adapter {
	struct module *owner;
	unsigned int id;
	unsigned int class;		  /* classes to allow probing for */
	/* the algorithm to access the i2c-slave bus */
	const struct i2c_slave_algorithm *slv_algo;
	void *algo_data;
	void *parent_data;

	/* data fields that are valid for all devices	*/
	u8 level;			/* nesting level for lockdep */
	struct mutex bus_lock;

	int timeout;			/* in jiffies */
	int retries;
	struct device *dev;		/* the adapter device */
	struct device *parent_dev;	/* the adapter device */

	int nr;
	char name[48];
	struct completion dev_released;
};

static inline void *i2c_get_slave_adapdata(const struct i2c_slave_adapter *dev)
{
	return dev_get_drvdata(dev->dev);
}

static inline void i2c_set_slave_adapdata(struct i2c_slave_adapter *dev,
		void *data)
{
	dev_set_drvdata(dev->dev, data);
}

/*
 * The following struct are for those who like to implement new i2c slave
 * bus drivers:
 * i2c_slave_algorithm is the interface to a class of hardware solutions which
 * can be addressed using the same bus algorithms.
 */
struct i2c_slave_algorithm {
	/* Start the slave to receive/transmit data.
	 * The dummy-char will send to master if there is no data to send on
	 * slave tx buffer.
	 */
	int (*slave_start)(struct i2c_slave_adapter *slv_adap, int addr,
		int is_ten_bit_addr, unsigned char dummy_char);

	/* Stop slave to receive/transmit data.
	 * Required information to reset the slave rx and tx buffer to reset
	 * or not.
	 */
	void (*slave_stop)(struct i2c_slave_adapter *slv_adap,
			int is_buffer_clear);

	/*
	 * Send data to master. The data will be copied on the slave tx buffer
	 * and will send to master once master initiates the master-read cycle.
	 * Function will return immediately once the buffer copied into slave
	 * tx buffer.
	 * Client will not wait till data is sent to master.
	 * This function will not copy data partially. If sufficient space is
	 * not available, it will return error.
	 */
	int (*slave_send)(struct i2c_slave_adapter *slv_adap, const char *buf,
			int count);

	/*
	 * Get amount of data available in tx buffer. If there is still data in
	 * tx buffer wait for given time to get slave tx buffer emptied.
	 * returns number of data available in slave tx buffer.
	 */
	int (*slave_get_tx_status)(struct i2c_slave_adapter *slv_adap,
			int timeout_ms);

	/*
	 * Receive data to master. The data received from master is stored on
	 * slave rx buffer. When this api will be called, the data will be
	 * coped from the slave rx buffer to client buffer. If requested (count)
	 * data is not available then it will wait for either min_count to be
	 * receive or timeout whatever first.
	 *
	 * if timeout_ms = 0, then wait for min_count data to be read.
	 * if timoue_ms non zero then wait for the data till timeout happen.
	 * returns number of bytes read as positive integer otherwise error.
	 */
	int (*slave_recv)(struct i2c_slave_adapter *slv_adap, char *buf,
			int count, int min_count, int timeout_ms);

	/* Flush the receive and transmit buffer.
	 */
	int (*slave_flush_buffer)(struct i2c_slave_adapter *slv_adap,
			int is_flush_tx_buffer, int is_flush_rx_buffer);

	/* Get the number of dummy char cycle.
	 * Get the number of master read cycle on which dummy character has
	 * been sent.
	 * This can be treat as NACK cycle from slave side.
	 * Pass option whether count need to be reset or not.
	 */
	int (*slave_get_nack_cycle)(struct i2c_slave_adapter *slv_adap,
			int is_cout_reset);
};
#endif /* __KERNEL__ */
#endif /* _LINUX_I2C_SLAVE_H */
