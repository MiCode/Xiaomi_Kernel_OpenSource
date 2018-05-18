/*
 * DSPG DBMDX UART interface driver
 *
 * Copyright (C) 2014 DSP Group
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/* #define DEBUG */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/tty.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>

#include "dbmdx-interface.h"
#include "dbmdx-va-regmap.h"
#include "dbmdx-uart.h"

/* baud rate for wakeup sequence */
#define UART_TTY_WAKEUP_SEQ_BAUD_RATE			2400

#define DEFAULT_UART_WRITE_CHUNK_SIZE	8
#define MAX_UART_WRITE_CHUNK_SIZE	0x20000
#define DEFAULT_UART_READ_CHUNK_SIZE	8
#define MAX_UART_READ_CHUNK_SIZE	4096

#ifndef INIT_COMPLETION
#define INIT_COMPLETION(x) reinit_completion(&x)
#endif

static DECLARE_WAIT_QUEUE_HEAD(dbmdx_wq);

static void uart_transport_enable(struct dbmdx_private *p, bool enable);

#ifndef NEED_FILE_TTY
static inline struct tty_struct *file_tty(struct file *file)
{
	return ((struct tty_file_private *)file->private_data)->tty;
}
#endif

static int uart_open_file(struct dbmdx_uart_private *p)
{
	long err = 0;
	struct file *fp;
	int attempt = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(60000);

	if (p->uart_open)
		goto out_ok;

	/*
	 * Wait for the device node to appear in the filesystem. This can take
	 * some time if the kernel is still booting up and filesystems are
	 * being mounted.
	 */
	do {
		msleep(DBMDX_MSLEEP_UART_PROBE);
		dev_dbg(p->dev,
			"%s(): probing for tty on %s (attempt %d)\n",
			 __func__, p->pdata->uart_dev, ++attempt);

		fp = filp_open(p->pdata->uart_dev,
				O_RDWR | O_NONBLOCK | O_NOCTTY, 0);

		err = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && (err == -ENOENT) &&
		 (atomic_read(&p->stop_uart_probing) == 0));

	if (atomic_read(&p->stop_uart_probing)) {
		dev_dbg(p->dev, "%s: UART probe thread stopped\n", __func__);
		atomic_set(&p->stop_uart_probing, 0);
		err = -EIO;
		goto out;
	}

	if (IS_ERR_OR_NULL(fp)) {
		dev_err(p->dev, "%s: UART device node open failed\n", __func__);
		err = -ENODEV;
		goto out;
	}

	/* set uart_dev members */
	p->fp = fp;
	p->tty = file_tty(fp);
	p->ldisc = tty_ldisc_ref(p->tty);
	p->uart_open = 1;
	err = 0;

	dev_dbg(p->dev, "%s: UART successfully opened\n", __func__);
out_ok:
	/* finish probe */
	complete(&p->uart_done);
out:
	return err;
}

static int uart_open_file_noprobe(struct dbmdx_uart_private *p)
{
	long err = 0;
	struct file *fp;
	int attempt = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	if (p->uart_open)
		goto out;

	/*
	 * Wait for the device node to appear in the filesystem. This can take
	 * some time if the kernel is still booting up and filesystems are
	 * being mounted.
	 */
	do {
		if (attempt > 0)
			msleep(DBMDX_MSLEEP_UART_PROBE);
		dev_dbg(p->dev,
			"%s(): probing for tty on %s (attempt %d)\n",
			 __func__, p->pdata->uart_dev, ++attempt);

		fp = filp_open(p->pdata->uart_dev,
				O_RDWR | O_NONBLOCK | O_NOCTTY, 0);

		err = PTR_ERR(fp);
	} while (time_before(jiffies, timeout) && IS_ERR_OR_NULL(fp));


	if (IS_ERR_OR_NULL(fp)) {
		dev_err(p->dev, "%s: UART device node open failed, err=%d\n",
			__func__,
			(int)err);
		err = -ENODEV;
		goto out;
	}

	/* set uart_dev members */
	p->fp = fp;
	p->tty = file_tty(fp);
	p->ldisc = tty_ldisc_ref(p->tty);
	p->uart_open = 1;

	err = 0;
	dev_dbg(p->dev, "%s: UART successfully opened\n", __func__);

out:
	return err;
}


static void uart_close_file(struct dbmdx_uart_private *p)
{
	if (p->uart_probe_thread) {
		atomic_inc(&p->stop_uart_probing);
		kthread_stop(p->uart_probe_thread);
		p->uart_probe_thread = NULL;
	}
	if (p->uart_open) {
		tty_ldisc_deref(p->ldisc);
		filp_close(p->fp, 0);
		p->uart_open = 0;
	}
	atomic_set(&p->stop_uart_probing, 0);
}

void uart_flush_rx_fifo(struct dbmdx_uart_private *p)
{
	dev_dbg(p->dev, "%s\n", __func__);

	if (!p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return;
	}

	tty_ldisc_flush(p->tty);
}

int uart_configure_tty(struct dbmdx_uart_private *p, u32 bps, int stop,
			      int parity, int flow)
{
	int rc = 0;
	struct ktermios termios;

	if (!p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	memcpy(&termios, &(p->tty->termios), sizeof(termios));

	tty_wait_until_sent(p->tty, 0);
	usleep_range(50, 60);

	/* clear csize, baud */
	termios.c_cflag &= ~(CBAUD | CSIZE | PARENB | CSTOPB);
	termios.c_cflag |= BOTHER; /* allow arbitrary baud */
	termios.c_cflag |= CS8;
	termios.c_cflag |= CREAD;
	if (parity)
		termios.c_cflag |= PARENB;

	if (stop == 2)
		termios.c_cflag |= CSTOPB;

	/* set uart port to raw mode (see termios man page for flags) */
	termios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
		| INLCR | IGNCR | ICRNL | IXON | IXOFF);

	if (flow && p->pdata->software_flow_control)
		termios.c_iflag |= IXOFF; /* enable XON/OFF for input */

	termios.c_oflag &= ~(OPOST);
	termios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

	/* set baud rate */
	termios.c_ospeed = bps;
	termios.c_ispeed = bps;

	rc = tty_set_termios(p->tty, &termios);
	return rc;
}

ssize_t uart_read_data(struct dbmdx_private *p, void *buf, size_t len)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	size_t count = uart_p->pdata->read_chunk_size;
	u8 *d = (u8 *)buf;
	mm_segment_t oldfs;
	int rc;
	int i = 0;
	size_t bytes_to_read = len;
	unsigned long timeout;

	/* if stuck for more than 10s, something is wrong */
	timeout = jiffies + msecs_to_jiffies(1000);

	if (!uart_p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	do {
		if (count > bytes_to_read)
			count = bytes_to_read;

		rc = uart_p->ldisc->ops->read(uart_p->tty,
					      uart_p->fp,
					      uart_p->pdata->read_buf,
					      count);
		if (rc > 0) {
			memcpy(d + i, uart_p->pdata->read_buf, rc);
			bytes_to_read -= rc;
			i += rc;
		} else if (rc == 0 || rc == -EAGAIN) {
			usleep_range(2000, 2100);
		} else
			dev_err(p->dev,
				"%s: Failed to read err= %d bytes to read=%zu\n",
				__func__,
				rc, bytes_to_read);
	} while (time_before(jiffies, timeout) && bytes_to_read);

	/* restore old fs context */
	set_fs(oldfs);

	if (bytes_to_read) {
		dev_err(uart_p->dev,
			"%s: timeout: unread %zu bytes ,requested %zu\n",
			__func__, bytes_to_read, len);
		return -EIO;
	}

	return len;
}

ssize_t uart_write_data_no_sync(struct dbmdx_private *p, const void *buf,
				       size_t len)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;
	const u8 *cmds = (const u8 *)buf;
	size_t to_copy = len;
	size_t max_size = (size_t)(uart_p->pdata->write_chunk_size);
	mm_segment_t oldfs;
	unsigned int count;

	if (!uart_p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	while (to_copy > 0) {
		if (to_copy > max_size)
			count = max_size;
		else
			count = to_copy;
		/* block until tx buffer space is available */
		do {
			ret = tty_write_room(uart_p->tty);
			usleep_range(100, 110);
		} while (ret <= 0);

		if (ret < count)
			count = ret;

		ret = uart_p->ldisc->ops->write(uart_p->tty,
						uart_p->fp,
						cmds,
						min_t(size_t,
							count, max_size));
		if (ret < 0) {
			dev_err(uart_p->dev,
				"%s: uart_write_data_no_sync failed ret=%d\n",
				__func__, ret);
			break;
		}
		to_copy -= ret;
		cmds += ret;
	}

	/* restore old fs context */
	set_fs(oldfs);
	return len - to_copy;
}

ssize_t uart_write_data(struct dbmdx_private *p, const void *buf,
			       size_t len)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	ssize_t bytes_wr;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (!uart_p->uart_open) {
		dev_err(p->dev, "%s: UART is not opened !!!\n", __func__);
		return -EIO;
	}

	bytes_wr = uart_write_data_no_sync(p, buf, len);

	tty_wait_until_sent(uart_p->tty, 0);
	usleep_range(50, 60);

	return bytes_wr;
}

ssize_t send_uart_cmd_vqe(struct dbmdx_private *p, u32 command,
			     u16 *response)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	char tmp[3];
	u8 send[7];
	u8 recv[6] = {0, 0, 0, 0, 0, 0};
	int ret;

	dev_dbg(uart_p->dev, "%s: Send 0x%04x\n", __func__, command);

	if (response)
		uart_flush_rx_fifo(uart_p);

	ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
	if (ret < 0)
		goto out;
	send[0] = tmp[0];
	send[1] = tmp[1];
	send[2] = 'w';

	ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
	if (ret < 0)
		goto out;
	send[3] = tmp[0];
	send[4] = tmp[1];

	ret = snprintf(tmp, 3, "%02x", command & 0xff);
	if (ret < 0)
		goto out;
	send[5] = tmp[0];
	send[6] = tmp[1];

	ret = uart_write_data(p, send, 7);
	if (ret != 7)
		goto out;

	ret = 0;

	/* the sleep command cannot be acked before the device goes to sleep */
	if (command == DBMDX_VA_SET_POWER_STATE_SLEEP)
		goto out;

	if (!response)
		goto out;

	ret = uart_read_data(p, recv, 5);
	if (ret < 0)
		goto out;
	ret = kstrtou16(recv, 16, response);
	if (ret < 0) {
		dev_err(uart_p->dev, "%s: %2.2x:%2.2x:%2.2x:%2.2x\n",
			__func__, recv[0], recv[1], recv[2], recv[3]);
		goto out;
	}
	ret = 0;
out:
	return ret;
}

ssize_t send_uart_cmd_va(struct dbmdx_private *p, u32 command,
				   u16 *response)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	char tmp[3];
	u8 send[7];
	u8 recv[6] = {0, 0, 0, 0, 0, 0};
	int ret;

	dev_dbg(uart_p->dev, "%s: Send 0x%02x\n", __func__, command);

	/*Send wakeup byte*/
	if (p->pdata->send_wakeup_seq) {
		send[0] = 0;
		ret = uart_write_data(p, send, 1);
		if (ret != 1)
			goto out;

		usleep_range(DBMDX_USLEEP_UART_AFTER_WAKEUP_BYTE,
			DBMDX_USLEEP_UART_AFTER_WAKEUP_BYTE + 10);
	}

	if (response) {
		uart_flush_rx_fifo(uart_p);

		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		send[0] = tmp[0];
		send[1] = tmp[1];
		send[2] = 'r';

		ret = uart_write_data(p, send, 3);
		if (ret != 3)
			goto out;

		ret = 0;

		/* the sleep command cannot be ack'ed before the device goes
		 * to sleep */
		if (command == DBMDX_VA_SET_POWER_STATE_SLEEP)
			goto out;

		if (!response)
			goto out;

		ret = uart_read_data(p, recv, 5);
		if (ret < 0)
			goto out;
		ret = kstrtou16(recv, 16, response);
		if (ret < 0) {
			dev_err(uart_p->dev, "%s: %2.2x:%2.2x:%2.2x:%2.2x\n",
				__func__, recv[0], recv[1], recv[2], recv[3]);
			goto out;
		}

		dev_dbg(uart_p->dev,
				"%s: Received 0x%02x\n", __func__, *response);

		ret = 0;
	} else {
		ret = snprintf(tmp, 3, "%02x", (command >> 16) & 0xff);
		if (ret < 0)
			goto out;
		send[0] = tmp[0];
		send[1] = tmp[1];
		send[2] = 'w';

		ret = snprintf(tmp, 3, "%02x", (command >> 8) & 0xff);
		if (ret < 0)
			goto out;
		send[3] = tmp[0];
		send[4] = tmp[1];

		ret = snprintf(tmp, 3, "%02x", command & 0xff);
		if (ret < 0)
			goto out;
		send[5] = tmp[0];
		send[6] = tmp[1];

		ret = uart_write_data(p, send, 7);
		if (ret != 7)
			goto out;
		ret = 0;

	}
out:
	return ret;
}

int send_uart_cmd_boot(struct dbmdx_private *p,  u32 command)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	u8 send[3];
	int ret;

	dev_info(uart_p->dev, "%s: send_uart_cmd_boot = %x\n",
		__func__, command);

	send[0] = (command >> 16) & 0xff;
	send[1] = (command >>  8) & 0xff;

	uart_flush_rx_fifo(uart_p);
	ret = uart_write_data(p, send, 2);

	if (ret != 2) {
		dev_err(uart_p->dev, "%s: send_uart_cmd_boot ret = %d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

int uart_verify_boot_checksum(struct dbmdx_private *p,
	const void *checksum, size_t chksum_len)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;
	u8 rx_checksum[10];

	if (!checksum)
		return 0;

	if (chksum_len > 8) {
		dev_err(uart_p->dev, "%s: illegal checksum length\n", __func__);
		return -EINVAL;
	}

	uart_flush_rx_fifo(uart_p);

	ret = send_uart_cmd_boot(p, DBMDX_READ_CHECKSUM);

	if (ret < 0) {
		dev_err(uart_p->dev, "%s: could not read checksum\n", __func__);
		return -EIO;
	}

	ret = uart_read_data(p, (void *)rx_checksum, chksum_len + 2);

	if (ret < 0) {
		dev_err(uart_p->dev, "%s: could not read checksum data\n",
			__func__);
		return -EIO;
	}

	ret = p->verify_checksum(p, checksum, &rx_checksum[2], chksum_len);
	if (ret) {
		dev_err(uart_p->dev, "%s: checksum mismatch\n", __func__);
		return -EILSEQ;
	}

	return 0;
}

int uart_verify_chip_id(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	int ret;
	u8 idr_read_cmd[] = {0x5A, 0x07, 0x68, 0x00, 0x00, 0x03};
	u8 idr_read_result[7] = {0};
	u8 chip_rev_id_low_a = 0;
	u8 chip_rev_id_low_b = 0;
	u8 chip_rev_id_high = 0;

	u8 recv_chip_rev_id_high = 0;
	u8 recv_chip_rev_id_low = 0;

	if (p->cur_firmware_id == DBMDX_FIRMWARE_ID_DBMD2) {
		idr_read_cmd[2] = 0x68;
		chip_rev_id_high = 0x0d;
		chip_rev_id_low_a = 0xb0;
		chip_rev_id_low_b = 0xb1;
	} else if (p->cur_firmware_id == DBMDX_FIRMWARE_ID_DBMD4) {
		idr_read_cmd[2] = 0x74;
		chip_rev_id_high = 0xdb;
		chip_rev_id_low_a = 0x40;
		chip_rev_id_low_b = 0x40;
	} else {
		idr_read_cmd[2] = 0x74;
		chip_rev_id_high = 0xdb;
		chip_rev_id_low_a = 0x60;
		chip_rev_id_low_b = 0x60;
	}

	ret = uart_write_data(p, idr_read_cmd, 6);
	if (ret != sizeof(idr_read_cmd)) {
		dev_err(uart_p->dev, "%s: idr_read_cmd ret = %d\n",
			__func__, ret);
		return ret;
	}

	usleep_range(1000, 2000);

	ret = uart_read_data(p, (void *)idr_read_result, 6);

	if (ret < 0) {
		dev_err(uart_p->dev, "%s: could not idr register data\n",
			__func__);
		return -EIO;
	}
	/* Verify answer */
	if ((idr_read_result[0] != idr_read_cmd[0]) ||
		(idr_read_result[1] != idr_read_cmd[1]) ||
		(idr_read_result[4] != 0x00) ||
		(idr_read_result[5] != 0x00)) {
		dev_err(uart_p->dev, "%s: Wrong IDR resp: %x:%x:%x:%x:%x:%x\n",
				__func__,
				idr_read_result[0],
				idr_read_result[1],
				idr_read_result[2],
				idr_read_result[3],
				idr_read_result[4],
				idr_read_result[5]);
		return -EIO;
	}
	recv_chip_rev_id_high = idr_read_result[3];
	recv_chip_rev_id_low = idr_read_result[2];

	if ((recv_chip_rev_id_high != chip_rev_id_high) ||
		((recv_chip_rev_id_low != chip_rev_id_low_a) &&
		(recv_chip_rev_id_low != chip_rev_id_low_b))) {

		dev_err(uart_p->dev,
			"%s: Wrong chip ID: Recieved 0x%2x%2x Expected: 0x%2x%2x | 0x%2x%2x\n",
				__func__,
				recv_chip_rev_id_high,
				recv_chip_rev_id_low,
				chip_rev_id_high,
				chip_rev_id_low_a,
				chip_rev_id_high,
				chip_rev_id_low_b);
		return -EILSEQ;
	}

	dev_info(uart_p->dev,
			"%s: Chip ID was successfully verified: 0x%2x%2x\n",
				__func__,
				recv_chip_rev_id_high,
				recv_chip_rev_id_low);
	return 0;
}

static int uart_can_boot(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	unsigned long remaining_time;
	int retries = RETRY_COUNT;
	int ret = -EBUSY;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	/*
	 * do additional waiting until UART device is really
	 * available
	 */
	do {
		remaining_time =
			wait_for_completion_timeout(&uart_p->uart_done, HZ);
	} while (!remaining_time && retries--);

	if (uart_p->uart_probe_thread) {
		atomic_inc(&uart_p->stop_uart_probing);
		kthread_stop(uart_p->uart_probe_thread);
		uart_p->uart_probe_thread = NULL;
	}

	INIT_COMPLETION(uart_p->uart_done);

	if (retries == 0) {
		dev_err(p->dev, "%s: UART not available\n", __func__);
		goto out;
	}

	uart_transport_enable(p, true);

	ret = 0;

out:
	return ret;
}

static int uart_prepare_boot(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

int uart_wait_for_ok(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	u8 resp[5] = {0, 0, 0, 0, 0};
	const char match[] = "OK\n\r";
	int ret;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	ret = uart_read_data(p, resp, 3);
	if (ret < 0) {
		dev_err(uart_p->dev, "%s: failed to read OK from uart: %d\n",
			__func__, ret);
		goto out;
	}
	ret = strncmp(match , resp, 2);
	if (ret)
		dev_err(uart_p->dev,
			"%s: result = %d : %2.2x:%2.2x:%2.2x\n",
			__func__, ret, resp[0], resp[1], resp[2]);
	if (ret)
		ret = strncmp(match + 1, resp, 2);
	if (ret)
		ret = strncmp(match, resp + 1, 2);
out:
	return ret;
}

static int uart_boot(const void *fw_data, size_t fw_size,
		struct dbmdx_private *p, const void *checksum,
		size_t chksum_len, int load_fw)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);


	return 0;
}

static int uart_finish_boot(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static int uart_dump_state(struct chip_interface *chip, char *buf)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)chip->pdata;
	int off = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	off += snprintf(buf + off, PAGE_SIZE - off,
				"\t===UART Interface  Dump====\n");

	off += snprintf(buf + off, PAGE_SIZE - off, "\tUart Interface:\t%s\n",
				uart_p->uart_open ? "Open" : "Closed");

	off += snprintf(buf + off, PAGE_SIZE - off, "\tUart Device:\t%s\n",
				uart_p->pdata->uart_dev);

	off += snprintf(buf + off, PAGE_SIZE - off,
				"\tUART Write Chunk Size:\t\t%d\n",
				uart_p->pdata->write_chunk_size);
	off += snprintf(buf + off, PAGE_SIZE - off,
				"\tUART Read Chunk Size:\t\t%d\n",
				uart_p->pdata->read_chunk_size);
	off += snprintf(buf + off, PAGE_SIZE - off,
					"\tInterface resumed:\t%s\n",
			uart_p->interface_enabled ? "ON" : "OFF");

	return off;
}

static int uart_set_va_firmware_ready(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret = 0;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	if (p->pdata->uart_low_speed_enabled)
		ret = uart_set_speed(p, DBMDX_VA_SPEED_NORMAL);
	else
		ret = uart_set_speed(p,	DBMDX_VA_SPEED_BUFFERING);

	if (ret) {
		dev_err(p->dev, "%s: failed to send change speed command\n",
				__func__);
		return -EIO;
	}

	return 0;
}

static int uart_set_vqe_firmware_ready(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static void uart_transport_enable(struct dbmdx_private *p, bool enable)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;
	u32 uart_baud;

	dev_dbg(uart_p->dev, "%s (%s)\n", __func__, enable ? "ON" : "OFF");

	if (enable) {

#ifdef CONFIG_PM_WAKELOCKS
		wake_lock(&uart_p->ps_nosuspend_wl);
#endif
		ret = wait_event_interruptible(dbmdx_wq,
			uart_p->interface_enabled);

		if (ret)
			dev_dbg(uart_p->dev,
				"%s, waiting for interface was interrupted",
				__func__);
		else
			dev_dbg(uart_p->dev, "%s, interface is active\n",
				__func__);
	}

	if (enable) {

		p->wakeup_set(p);

		if (uart_p->uart_open)
			return;
		ret = uart_open_file_noprobe(uart_p);
		if (ret < 0) {
			dev_err(uart_p->dev, "%s: failed to enable UART: %d\n",
			__func__, ret);
			return;
		}
		if (p->pdata->uart_low_speed_enabled)
			uart_baud = p->pdata->va_speed_cfg[0].uart_baud;
		else
			uart_baud = p->pdata->va_speed_cfg[1].uart_baud;

		/* Send wakeup byte */
		if (p->pdata->send_wakeup_seq &&
			p->power_mode == DBMDX_PM_SLEEPING) {

			u8 send[2] = {0, 0};

			ret = uart_configure_tty(uart_p,
						UART_TTY_WAKEUP_SEQ_BAUD_RATE,
						uart_p->normal_stop_bits,
						uart_p->normal_parity,
						0);

			if (ret) {
				dev_err(uart_p->dev,
					"%s: cannot configure tty to: %us%up%uf%u\n",
					__func__,
					UART_TTY_WAKEUP_SEQ_BAUD_RATE,
					uart_p->normal_parity,
					uart_p->normal_stop_bits, 0);
					return;
			}
			uart_write_data(p, send, 1);
		}

		if (p->power_mode == DBMDX_PM_SLEEPING)
			/* It takes up to 100ms
			to PLL to stabilize after hibernation */
			msleep(DBMDX_MSLEEP_UART_WAKEUP);

		ret = uart_configure_tty(uart_p,
					uart_baud,
					uart_p->normal_stop_bits,
					uart_p->normal_parity,
					0);
		if (ret) {
			dev_err(uart_p->dev,
				"%s: cannot configure tty to: %us%up%uf%u\n",
				__func__,
				uart_baud,
				uart_p->normal_parity,
				uart_p->normal_stop_bits, 0);
			return;
		}
		/* Send wakeup in detection mode byte */
		if (p->pdata->send_wakeup_seq &&
			p->va_flags.mode == DBMDX_DETECTION) {

			u8 send[2] = {0, 0};

			uart_write_data(p, send, 1);

		}

	} else {
#ifdef CONFIG_PM_WAKELOCKS
		wake_unlock(&uart_p->ps_nosuspend_wl);
#endif
		p->wakeup_release(p);

		if (!uart_p->uart_open)
			return;
		uart_close_file(uart_p);
	}
}

static void uart_resume(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	uart_interface_resume(uart_p);
}


void uart_interface_resume(struct dbmdx_uart_private *uart_p)
{
	dev_dbg(uart_p->dev, "%s\n", __func__);

	uart_p->interface_enabled = 1;
	wake_up_interruptible(&dbmdx_wq);
}

static void uart_suspend(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	uart_interface_suspend(uart_p);
}


void uart_interface_suspend(struct dbmdx_uart_private *uart_p)
{
	dev_dbg(uart_p->dev, "%s\n", __func__);

	uart_p->interface_enabled = 0;
}

int uart_wait_till_alive(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	int ret = 0;
	u16 response;
	unsigned long stimeout = jiffies + msecs_to_jiffies(1000);
#if 0
	msleep(DBMDX_MSLEEP_UART_WAIT_TILL_ALIVE);
#endif
	uart_flush_rx_fifo(uart_p);

	/* Poll to wait for firmware completing its wakeup procedure:
	 * Read the firmware ID number () */
	do {
		/* check if chip is alive */
		ret = send_uart_cmd_va(p, DBMDX_VA_FW_ID, &response);
		if (ret)
			continue;

		if (response == (u16)(p->pdata->firmware_id))
			ret = 0;
		else
			ret = -1;
	} while (time_before(jiffies, stimeout) && ret != 0);

	if (ret != 0)
		dev_err(p->dev, "%s: failed to read firmware id\n", __func__);
	ret = (ret >= 0 ? 1 : 0);

	if (!ret)
		dev_err(p->dev, "%s(): failed = 0x%d\n", __func__,
			ret);

	return ret;
}

/* This function sets the uart speed and also can set the software flow
 * control according to the define */
int uart_set_speed_host_only(struct dbmdx_private *p, int index)
{
	struct dbmdx_uart_private *uart_p =
			(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;

	ret = uart_configure_tty(uart_p,
			p->pdata->va_speed_cfg[index].uart_baud,
			uart_p->normal_stop_bits,
			uart_p->normal_parity,
			0);
	if (ret) {
		dev_err(p->dev, "%s: cannot configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			uart_p->normal_parity,
			uart_p->normal_stop_bits,
			0);
		goto out;
	}

	dev_info(p->dev, "%s: Configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			uart_p->normal_parity,
			uart_p->normal_stop_bits,
			0);

	uart_p->normal_baud_rate = p->pdata->va_speed_cfg[index].uart_baud;
	uart_flush_rx_fifo(uart_p);
out:
	return ret;
}

/* this set the uart speed no flow control  */

int uart_set_speed(struct dbmdx_private *p, int index)
{
	struct dbmdx_uart_private *uart_p =
			(struct dbmdx_uart_private *)p->chip->pdata;
	int ret;

	ret = send_uart_cmd_va(p,
				DBMDX_VA_UART_SPEED |
				p->pdata->va_speed_cfg[index].uart_baud/100,
				NULL);
	if (ret) {
		dev_err(p->dev,
			"%s: failed to send UART change speed command\n",
			__func__);
		goto out;
	}

	/* set baudrate to FW baud (common case) */
	ret = uart_configure_tty(uart_p,
			p->pdata->va_speed_cfg[index].uart_baud,
			uart_p->normal_stop_bits,
			uart_p->normal_parity,
			0);
	if (ret) {
		dev_err(p->dev, "%s: cannot configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			uart_p->normal_parity,
			uart_p->normal_stop_bits,
			0);
		goto out;
	}

	dev_info(p->dev, "%s: Configure tty to: %us%up%uf%u\n",
			__func__,
			p->pdata->va_speed_cfg[index].uart_baud,
			uart_p->normal_parity,
			uart_p->normal_stop_bits,
			0);

	uart_p->normal_baud_rate = p->pdata->va_speed_cfg[index].uart_baud;
	uart_flush_rx_fifo(uart_p);

	ret = uart_wait_till_alive(p);

	if (!ret) {
		dev_err(p->dev, "%s: device not responding\n", __func__);
		goto out;
	}
	ret = 0;
	goto out;

out:
	return ret;
}

static int uart_prepare_buffering(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static int uart_read_audio_data(struct dbmdx_private *p,
	void *buf,
	size_t samples,
	bool to_read_metadata,
	size_t *available_samples,
	size_t *data_offset)
{
	size_t bytes_to_read;
	int ret;
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;
	mm_segment_t oldfs;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	ret = send_uart_cmd_va(p, DBMDX_VA_READ_AUDIO_BUFFER | samples, NULL);

	if (ret) {
		dev_err(p->dev, "%s: failed to request %zu audio samples\n",
			__func__, samples);
		ret = -1;
		goto out;
	}

	*available_samples = 0;

	if (to_read_metadata)
		*data_offset = 8;
	else
		*data_offset = 0;

	bytes_to_read = samples * 8 * p->bytes_per_sample + *data_offset;

	ret = uart_read_data(p, buf, bytes_to_read);

	if (ret != bytes_to_read) {
		dev_err(p->dev,
			"%s: read audio failed, %zu bytes to read, res(%d)\n",
			__func__,
			bytes_to_read,
			ret);
		ret = -1;
		goto out;
	}

	/* Word #4 contains current number of available samples */
	if (to_read_metadata)
		*available_samples = (size_t)(((u16 *)buf)[3]);
	else
		*available_samples = samples;

	ret = samples;

out:
	/* restore old fs context */
	set_fs(oldfs);
	return ret;
}

static int uart_finish_buffering(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static int uart_prepare_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	return 0;
}

static int uart_finish_amodel_loading(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s\n", __func__);

	/* do the same as for finishing buffering */
	return uart_finish_buffering(p);
}

static int uart_open_thread(void *data)
{
	int ret;
	struct dbmdx_uart_private *p = (struct dbmdx_uart_private *)data;

	ret = uart_open_file(p);
	while (!kthread_should_stop())
		usleep_range(10000, 11000);
	return ret;
}

static u32 uart_get_read_chunk_size(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s UART read chunk is %u\n",
		__func__, uart_p->pdata->read_chunk_size);

	return uart_p->pdata->read_chunk_size;
}

static u32 uart_get_write_chunk_size(struct dbmdx_private *p)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	dev_dbg(uart_p->dev, "%s UART write chunk is %u\n",
		__func__, uart_p->pdata->write_chunk_size);

	return uart_p->pdata->write_chunk_size;
}

static int uart_set_read_chunk_size(struct dbmdx_private *p, u32 size)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	if (size > MAX_UART_READ_CHUNK_SIZE) {
		dev_err(uart_p->dev,
			"%s Error setting UART read chunk. Max chunk size: %u\n",
		__func__, MAX_UART_READ_CHUNK_SIZE);
		return -EINVAL;
	} else if ((size % 2) != 0) {
		dev_err(uart_p->dev,
			"%s Error setting UART read chunk. Uneven size\n",
		__func__);
		return -EINVAL;
	} else if (size == 0)
		uart_p->pdata->read_chunk_size = DEFAULT_UART_READ_CHUNK_SIZE;
	else
		uart_p->pdata->read_chunk_size = size;

	dev_dbg(uart_p->dev, "%s UART read chunk was set to %u\n",
		__func__, uart_p->pdata->read_chunk_size);

	return 0;
}

static int uart_set_write_chunk_size(struct dbmdx_private *p, u32 size)
{
	struct dbmdx_uart_private *uart_p =
				(struct dbmdx_uart_private *)p->chip->pdata;

	if (size > MAX_UART_WRITE_CHUNK_SIZE) {
		dev_err(uart_p->dev,
			"%s Error setting UART write chunk. Max chunk size: %u\n",
		__func__, MAX_UART_WRITE_CHUNK_SIZE);
		return -EINVAL;
	} else if ((size % 2) != 0) {
		dev_err(uart_p->dev,
			"%s Error setting UART write chunk. Uneven size\n",
		__func__);
		return -EINVAL;
	} else if (size == 0)
		uart_p->pdata->write_chunk_size = DEFAULT_UART_WRITE_CHUNK_SIZE;
	else
		uart_p->pdata->write_chunk_size = size;

	dev_dbg(uart_p->dev, "%s UART write chunk was set to %u\n",
		__func__, uart_p->pdata->write_chunk_size);

	return 0;
}

int uart_common_probe(struct platform_device *pdev, const char threadnamefmt[])
{
#ifdef CONFIG_OF
	struct device_node *np;
#endif
	int ret;
	struct dbmdx_uart_private *p;
	struct dbmdx_uart_data *pdata;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL)
		return -ENOMEM;

	p->pdev = pdev;
	p->dev = &pdev->dev;

	p->chip.pdata = p;
#ifdef CONFIG_OF
	np = p->dev->of_node;
	if (!np) {
		dev_err(p->dev, "%s: no devicetree entry\n", __func__);
		ret = -EINVAL;
		goto out_err_kfree;
	}

	pdata = kzalloc(sizeof(struct dbmdx_uart_data), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto out_err_kfree;
	}

	ret = of_property_read_string(np, "uart_device", &pdata->uart_dev);
	if (ret && ret != -EINVAL) {
		dev_err(p->dev, "%s: invalid 'uart_device'\n", __func__);
		ret = -EINVAL;
		goto out_err_kfree;
	}

	/* check for software flow control option */
	if (of_find_property(np, "software-flow-control", NULL)) {
		dev_info(p->dev, "%s: Software flow control enabled\n",
			__func__);
		pdata->software_flow_control = 1;
	} else
		dev_info(p->dev, "%s: Software flow control disabled\n",
			__func__);
#else
	pdata = dev_get_platdata(&pdev->dev);
#endif

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "read-chunk-size",
		&pdata->read_chunk_size);
	if (ret != 0) {
		/*
		 * read-chunk-size not set, set it to default
		 */
		pdata->read_chunk_size = DEFAULT_UART_READ_CHUNK_SIZE;
		dev_info(p->dev,
			"%s: Setting uart read chunk to default val: %u bytes\n",
			__func__, pdata->read_chunk_size);
	}
#endif
	if (pdata->read_chunk_size > MAX_UART_READ_CHUNK_SIZE)
		pdata->read_chunk_size = MAX_UART_READ_CHUNK_SIZE;
	if (pdata->read_chunk_size == 0)
		pdata->read_chunk_size = DEFAULT_UART_READ_CHUNK_SIZE;

	dev_info(p->dev, "%s: Setting uart read chunk to %u bytes\n",
			__func__, pdata->read_chunk_size);

#ifdef CONFIG_OF
	ret = of_property_read_u32(np, "write-chunk-size",
		&pdata->write_chunk_size);
	if (ret != 0) {
		/*
		 * write-chunk-size not set, set it to default
		 */
		pdata->write_chunk_size = DEFAULT_UART_WRITE_CHUNK_SIZE;
		dev_info(p->dev,
			"%s: Setting uart write chunk to default val: %u bytes\n",
			__func__, pdata->write_chunk_size);
	}
#endif
	if (pdata->write_chunk_size > MAX_UART_WRITE_CHUNK_SIZE)
		pdata->write_chunk_size = MAX_UART_WRITE_CHUNK_SIZE;
	if (pdata->write_chunk_size == 0)
		pdata->write_chunk_size = DEFAULT_UART_WRITE_CHUNK_SIZE;

	dev_info(p->dev, "%s: Setting uart write chunk to %u bytes\n",
			__func__, pdata->write_chunk_size);

	p->pdata = pdata;

	init_completion(&p->uart_done);
	atomic_set(&p->stop_uart_probing, 0);

#ifdef CONFIG_PM_WAKELOCKS
	wake_lock_init(&p->ps_nosuspend_wl, WAKE_LOCK_SUSPEND,
		"dbmdx_nosuspend_wakelock_uart");
#endif

	/* fill in chip interface functions */
	p->chip.can_boot = uart_can_boot;
	p->chip.prepare_boot = uart_prepare_boot;
	p->chip.boot = uart_boot;
	p->chip.finish_boot = uart_finish_boot;
	p->chip.dump = uart_dump_state;
	p->chip.set_va_firmware_ready = uart_set_va_firmware_ready;
	p->chip.set_vqe_firmware_ready = uart_set_vqe_firmware_ready;
	p->chip.transport_enable = uart_transport_enable;
	p->chip.read = uart_read_data;
	p->chip.write = uart_write_data;
	p->chip.send_cmd_vqe = send_uart_cmd_vqe;
	p->chip.send_cmd_va = send_uart_cmd_va;
	p->chip.send_cmd_boot = send_uart_cmd_boot;
	p->chip.verify_boot_checksum = uart_verify_boot_checksum;
	p->chip.prepare_buffering = uart_prepare_buffering;
	p->chip.read_audio_data = uart_read_audio_data;
	p->chip.finish_buffering = uart_finish_buffering;
	p->chip.prepare_amodel_loading = uart_prepare_amodel_loading;
	p->chip.finish_amodel_loading = uart_finish_amodel_loading;
	p->chip.get_write_chunk_size = uart_get_write_chunk_size;
	p->chip.get_read_chunk_size = uart_get_read_chunk_size;
	p->chip.set_write_chunk_size = uart_set_write_chunk_size;
	p->chip.set_read_chunk_size = uart_set_read_chunk_size;
	p->chip.resume = uart_resume;
	p->chip.suspend = uart_suspend;

	p->interface_enabled = 1;

	dev_set_drvdata(p->dev, &p->chip);

	p->uart_probe_thread = kthread_run(uart_open_thread,
					   (void *)p,
					   threadnamefmt);
	if (IS_ERR_OR_NULL(p->uart_probe_thread)) {
		dev_err(p->dev,
			"%s(): can't create dbmd uart probe thread = %p\n",
			__func__, p->uart_probe_thread);
		ret = -ENOMEM;
		goto out_err_kfree;
	}

	dev_info(p->dev, "%s: successfully probed\n", __func__);

	ret = 0;
	goto out;

out_err_kfree:
	kfree(p);
out:
	return ret;
}

int uart_common_remove(struct platform_device *pdev)
{
	struct chip_interface *ci = dev_get_drvdata(&pdev->dev);
	struct dbmdx_uart_private *p = (struct dbmdx_uart_private *)ci->pdata;

	dev_set_drvdata(p->dev, NULL);

#ifdef CONFIG_PM_WAKELOCKS
	wake_lock_destroy(&p->ps_nosuspend_wl);
#endif

	uart_close_file(p);
	kfree(p);

	return 0;
}
