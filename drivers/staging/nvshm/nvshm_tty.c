/*
 * Copyright (C) 2012-2013 NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include <linux/skbuff.h>

#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_iobuf.h"

/* NVSHM interface */

#define MAX_OUTPUT_SIZE 1500

#define NVSHM_TTY_UP (1)

/*
 * This structure hold per tty line information like
 * nvshm_iobuf queues and back reference to nvshm channel/driver
 */

struct nvshm_tty_line {
	int nvshm_chan; /* nvshm channel */
	int throttled;
	struct tty_port port;
	/* iobuf queues for nvshm flow control support */
	struct nvshm_iobuf *io_queue_head;
	struct nvshm_iobuf *io_queue_tail;
	struct nvshm_channel *pchan;
	int errno;
	spinlock_t lock;
};

struct nvshm_tty_device {
	int up;
	struct tty_driver *tty_driver;
	struct nvshm_handle *handle;
	int nlines;
	struct workqueue_struct *tty_wq;
	struct work_struct tty_worker;
	struct nvshm_tty_line line[NVSHM_MAX_CHANNELS];
};

static struct nvshm_tty_device tty_dev;

static void nvshm_tty_rx_rewrite_line(int l)
{
	struct nvshm_iobuf *list;
	struct tty_struct *tty = NULL;
	int len, nbuff = 0;

	tty = tty_port_tty_get(&tty_dev.line[l].port);

	if (!tty)
		return;

	spin_lock(&tty_dev.line[l].lock);

	while (tty_dev.line[l].io_queue_head) {
		list = tty_dev.line[l].io_queue_head;
		spin_unlock(&tty_dev.line[l].lock);
		len = tty_insert_flip_string(tty,
					     NVSHM_B2A(tty_dev.handle,
						       list->npdu_data)
					     + list->data_offset,
					     list->length);
		tty_flip_buffer_push(tty);
		spin_lock(&tty_dev.line[l].lock);
		if (len < list->length) {
			list->data_offset += len;
			list->length -= len;
			spin_unlock(&tty_dev.line[l].lock);
			tty_kref_put(tty);
			return;
		}
		if (list->sg_next) {
			/* Propagate ->next to the sg_next fragment
			   do not forget to move tail also */
			if (tty_dev.line[l].io_queue_head !=
			    tty_dev.line[l].io_queue_tail) {
				tty_dev.line[l].io_queue_head =
					NVSHM_B2A(tty_dev.handle,
						  list->sg_next);
				tty_dev.line[l].io_queue_head->next =
					list->next;
			} else {
				tty_dev.line[l].io_queue_head =
					NVSHM_B2A(tty_dev.handle,
						  list->sg_next);
				tty_dev.line[l].io_queue_tail =
					tty_dev.line[l].io_queue_head;
				if (list->next != NULL)
					pr_debug("%s:tail->next!=NULL\n",
						 __func__);
			}
		} else {
			if (list->next) {
				if (tty_dev.line[l].io_queue_head !=
				    tty_dev.line[l].io_queue_tail) {
					tty_dev.line[l].io_queue_head =
						NVSHM_B2A(tty_dev.handle,
							  list->next);
				} else {
					tty_dev.line[l].io_queue_head =
						NVSHM_B2A(tty_dev.handle,
							  list->next);
					tty_dev.line[l].io_queue_tail =
						tty_dev.line[l].io_queue_head;
				}
			} else {
				tty_dev.line[l].io_queue_tail = NULL;
				tty_dev.line[l].io_queue_head = NULL;
			}
		}
		nbuff++;
		nvshm_iobuf_free((struct nvshm_iobuf *)list);
	}
	if (!tty_dev.line[l].io_queue_head)
		tty_dev.line[l].throttled = 0;
	spin_unlock(&tty_dev.line[l].lock);
	tty_kref_put(tty);
}

/* Called in a workqueue when unthrottle is called */
static void nvshm_tty_rx_rewrite(struct work_struct *work)
{
	int idx;

	for (idx = 0; idx < tty_dev.nlines; idx++)
		nvshm_tty_rx_rewrite_line(idx);
}

/*
 * nvshm_tty_rx_event()
 * NVSHM has received data insert them in tty flip buffer.
 * If no data is available, turn flow control on
 */
void nvshm_tty_rx_event(struct nvshm_channel *chan,
			struct nvshm_iobuf *iob)
{
	struct nvshm_tty_line *line = chan->data;
	struct tty_struct *tty = NULL;
	struct nvshm_iobuf *_phy_iob, *tmp;
	int len;

	/* line can be null if TTY install failed or not executed yet */
	if (line)
		tty = tty_port_tty_get(&line->port);

	if (!tty) {
		pr_warn("%s: data received on a closed/non-init TTY\n",
			__func__);
		nvshm_iobuf_free_cluster(iob);
		return;
	}

	spin_lock(&line->lock);

	if (!line->throttled) {
		_phy_iob = iob;
		while (_phy_iob) {
			spin_unlock(&line->lock);
			len = tty_insert_flip_string(tty,
						     NVSHM_B2A(tty_dev.handle,
							       iob->npdu_data)
						     + iob->data_offset,
						     iob->length);
			tty_flip_buffer_push(tty);
			spin_lock(&line->lock);
			if (len < iob->length) {
				line->throttled = 1;
				iob->data_offset += len;
				iob->length -= len;
				goto queue;
			}

			tmp = iob;
			/* Go next element: if ->sg_next follow it
			   and propagate ->next otherwise go next */
			if (iob->sg_next) {
				struct nvshm_iobuf *leaf;
				_phy_iob = iob->sg_next;
				if (_phy_iob) {
					leaf = NVSHM_B2A(tty_dev.handle,
							 _phy_iob);
					leaf->next = iob->next;
				}
			} else {
				_phy_iob = iob->next;
			}
			iob = NVSHM_B2A(tty_dev.handle, _phy_iob);
			nvshm_iobuf_free(tmp);
		}
		spin_unlock(&line->lock);
		tty_kref_put(tty);
		return;
	}
queue:
	/* Queue into FIFO */
	if (line->io_queue_tail) {
		line->io_queue_tail->next =
			NVSHM_A2B(tty_dev.handle, iob);
	} else {
		if (line->io_queue_head) {
			line->io_queue_head->next =
				NVSHM_A2B(tty_dev.handle, iob);
		} else {
			line->io_queue_head = iob;
		}
	}
	line->io_queue_tail = iob;
	spin_unlock(&line->lock);
	queue_work(tty_dev.tty_wq, &tty_dev.tty_worker);
	tty_kref_put(tty);
	return;
}

void nvshm_tty_error_event(struct nvshm_channel *chan,
			   enum nvshm_error_id error)
{
	struct nvshm_tty_line *line = chan->data;
	struct tty_struct *tty;

	tty = tty_port_tty_get(&line->port);
	pr_debug("%s\n", __func__);
	tty_dev.line[tty->index].errno = error;
	tty_hangup(tty);
	tty_kref_put(tty);
}

void nvshm_tty_start_tx(struct nvshm_channel *chan)
{
	struct nvshm_tty_line *line = chan->data;
	struct tty_struct *tty;

	tty = tty_port_tty_get(&line->port);

	pr_debug("%s\n", __func__);
	tty_unthrottle(tty);
	tty_kref_put(tty);
}

static struct nvshm_if_operations nvshm_tty_ops = {
	.rx_event = nvshm_tty_rx_event,
	.error_event = nvshm_tty_error_event,
	.start_tx = nvshm_tty_start_tx
};

/* TTY interface */

static int nvshm_tty_open(struct tty_struct *tty, struct file *f)
{
	struct nvshm_tty_line *line = tty->driver_data;
	if (line)
		return tty_port_open(&line->port, tty, f);
	return -EIO;
}

static void nvshm_tty_close(struct tty_struct *tty, struct file *f)
{
	struct nvshm_tty_line *line = tty->driver_data;
	if (line)
		tty_port_close(&line->port, tty, f);
}

static void nvshm_tty_hangup(struct tty_struct *tty)
{
	struct nvshm_tty_line *line = tty->driver_data;
	if (line)
		tty_port_hangup(&line->port);
}


static int nvshm_tty_write_room(struct tty_struct *tty)
{
	return MAX_OUTPUT_SIZE;
}

static int nvshm_tty_write(struct tty_struct *tty, const unsigned char *buf,
			   int len)
{
	struct nvshm_iobuf *iob, *leaf = NULL, *list = NULL;
	int to_send = 0, remain, idx = tty->index, ret;

	if (!tty_dev.up)
		return -EIO;

	remain = len;
	while (remain) {
		to_send = remain < MAX_OUTPUT_SIZE ? remain : MAX_OUTPUT_SIZE;
		iob = nvshm_iobuf_alloc(tty_dev.line[idx].pchan, to_send);
		if (!iob) {
			if (tty_dev.line[idx].errno) {
				pr_err("%s iobuf alloc failed\n", __func__);
				if (list)
					nvshm_iobuf_free_cluster(list);
				return -ENOMEM;
			} else {
				pr_err("%s: Xoff condition\n", __func__);
				return 0;
			}
		}

		iob->length = to_send;
		iob->chan = tty_dev.line[idx].pchan->index;
		remain -= to_send;
		memcpy(NVSHM_B2A(tty_dev.handle,
				 iob->npdu_data +
				 iob->data_offset),
		       buf,
		       to_send);
		buf += to_send;

		if (!list) {
			leaf = list = iob;
		} else {
			leaf->sg_next = NVSHM_A2B(tty_dev.handle, iob);
			leaf = iob;
		}
	}
	ret = nvshm_write(tty_dev.line[idx].pchan, list);

	if (ret == 1)
		tty_throttle(tty);

	return len;
}

static void nvshm_tty_unthrottle(struct tty_struct *tty)
{
	int idx = tty->index;

	pr_debug("%s\n", __func__);

	if (!tty_dev.up)
		return;

	spin_lock(&tty_dev.line[idx].lock);
	if (tty_dev.line[idx].throttled) {
		spin_unlock(&tty_dev.line[idx].lock);
		queue_work(tty_dev.tty_wq, &tty_dev.tty_worker);
		return;
	}
	spin_unlock(&tty_dev.line[idx].lock);
}

static void nvshm_tty_dtr_rts(struct tty_port *tport, int onoff)
{
	pr_debug("%s\n", __func__);
}

static int nvshm_tty_carrier_raised(struct tty_port *tport)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int  nvshm_tty_activate(struct tty_port *tport, struct tty_struct *tty)
{
	int idx = tty->index;

	/* Set TTY flags */
	set_bit(TTY_IO_ERROR, &tty->flags);
	set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);
	set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);
	tty->low_latency = 1;

	pr_debug("%s line %d\n", __func__, idx);
	tty_dev.line[idx].throttled = 0;
	tty_dev.line[idx].pchan =
		nvshm_open_channel(tty_dev.line[idx].nvshm_chan,
				   &nvshm_tty_ops,
				   &tty_dev.line[idx]);
	if (!tty_dev.line[idx].pchan) {
		pr_err("%s fail to open SHM chan\n", __func__);
		return -EIO;
	}
	clear_bit(TTY_IO_ERROR, &tty->flags);
	return 0;
}

static void  nvshm_tty_shutdown(struct tty_port *tport)
{
	struct nvshm_tty_line *line =
			container_of(tport, struct nvshm_tty_line, port);

	if (line) {
		pr_debug("%s\n", __func__);
		nvshm_close_channel(line->pchan);
		line->pchan = NULL;
	}
}

static int nvshm_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	int idx = tty->index;
	struct nvshm_tty_line *line = &tty_dev.line[idx];
	int ret = tty_standard_install(driver, tty);

	pr_debug("%s\n", __func__);
	if (ret == 0)
		tty->driver_data = line;
	return ret;
}

static const struct tty_operations nvshm_tty_ttyops = {
	.open = nvshm_tty_open,
	.close = nvshm_tty_close,
	.hangup = nvshm_tty_hangup,
	.write = nvshm_tty_write,
	.write_room = nvshm_tty_write_room,
	.unthrottle = nvshm_tty_unthrottle,
	.install = nvshm_tty_install,
};

static const struct tty_port_operations nvshm_tty_port_ops = {
	.dtr_rts = nvshm_tty_dtr_rts,
	.carrier_raised = nvshm_tty_carrier_raised,
	.shutdown = nvshm_tty_shutdown,
	.activate = nvshm_tty_activate,
};

int nvshm_tty_init(struct nvshm_handle *handle)
{
	int ret, chan;

	pr_debug("%s\n", __func__);

	memset(&tty_dev, 0, sizeof(tty_dev));

	tty_dev.tty_wq = create_singlethread_workqueue("NVSHM_tty");
	INIT_WORK(&tty_dev.tty_worker, nvshm_tty_rx_rewrite);

	tty_dev.tty_driver = alloc_tty_driver(NVSHM_MAX_CHANNELS);

	if (tty_dev.tty_driver == NULL)
		return -ENOMEM;

	tty_dev.tty_driver->owner = THIS_MODULE;
	tty_dev.tty_driver->driver_name = "nvshm_tty";
	tty_dev.tty_driver->name = "ttySHM";
	tty_dev.tty_driver->major = 0;
	tty_dev.tty_driver->minor_start = 0;
	tty_dev.tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	tty_dev.tty_driver->subtype = SERIAL_TYPE_NORMAL;
	tty_dev.tty_driver->init_termios = tty_std_termios;
	tty_dev.tty_driver->init_termios.c_iflag = 0;
	tty_dev.tty_driver->init_termios.c_oflag = 0;
	tty_dev.tty_driver->init_termios.c_cflag =
		B115200 | CS8 | CREAD | CLOCAL;
	tty_dev.tty_driver->init_termios.c_lflag = 0;
	tty_dev.tty_driver->flags =
		TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW |
		TTY_DRIVER_DYNAMIC_DEV;

	tty_set_operations(tty_dev.tty_driver, &nvshm_tty_ttyops);

	ret = tty_register_driver(tty_dev.tty_driver);

	tty_dev.handle = handle;

	for (chan = 0; chan < NVSHM_MAX_CHANNELS; chan++) {
		if ((handle->chan[chan].map.type == NVSHM_CHAN_TTY)
			|| (handle->chan[chan].map.type == NVSHM_CHAN_LOG)) {
			tty_dev.line[tty_dev.nlines].nvshm_chan = chan;
			tty_dev.nlines++;
		}
	}

	for (chan = 0; chan < tty_dev.nlines; chan++) {
		pr_debug("%s: register tty#%d cha %d\n",
			 __func__, chan, tty_dev.line[chan].nvshm_chan);
		spin_lock_init(&tty_dev.line[tty_dev.nlines].lock);
		tty_port_init(&tty_dev.line[chan].port);
		tty_dev.line[chan].port.ops = &nvshm_tty_port_ops;
		tty_port_register_device(&tty_dev.line[chan].port,
					 tty_dev.tty_driver, chan, 0);
	}

	tty_dev.up = NVSHM_TTY_UP;
	return 0;
}

void nvshm_tty_cleanup(void)
{
	int chan;

	pr_debug("%s\n", __func__);
	tty_dev.up = 0;
	for (chan = 0; chan < tty_dev.nlines; chan++) {
		struct tty_struct *tty;

		tty = tty_port_tty_get(&tty_dev.line[chan].port);
		if (tty) {
			tty_vhangup(tty);
			tty_kref_put(tty);
		}
		/* No need to cleanup data as iobufs are invalid now */
		/* Next nvshm_tty_init will do it */
		pr_debug("%s unregister tty device %d\n", __func__, chan);
		tty_unregister_device(tty_dev.tty_driver, chan);
	}
	destroy_workqueue(tty_dev.tty_wq);
	tty_unregister_driver(tty_dev.tty_driver);
	put_tty_driver(tty_dev.tty_driver);
}

