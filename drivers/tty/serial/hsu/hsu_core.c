/*
 * hsu_core.c: driver core for Intel High Speed UART device
 *
 * Refer pxa.c, 8250.c and some other drivers in drivers/serial/
 *
 * (C) Copyright 2010-2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/slab.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/irq.h>

#include "hsu.h"

static int hsu_dma_enable = 0xff;
module_param(hsu_dma_enable, int, 0);
MODULE_PARM_DESC(hsu_dma_enable,
		 "It is a bitmap to set working mode, if bit[x] is 1, then port[x] will work in DMA mode, otherwise in PIO mode.");

static struct hsu_port hsu;
static struct hsu_port *phsu = &hsu;
static struct uart_driver serial_hsu_reg;

static void serial_hsu_command(struct uart_hsu_port *up);

static inline int check_qcmd(struct uart_hsu_port *up, char *cmd)
{
	struct circ_buf *circ = &up->qcirc;
	char *buf;

	buf = circ->buf + circ->tail;
	*cmd = *buf;
	return CIRC_CNT(circ->head, circ->tail, HSU_Q_MAX);
}

static inline void insert_qcmd(struct uart_hsu_port *up, char cmd)
{
	struct circ_buf *circ = &up->qcirc;
	char *buf;
	char last_cmd;

	if (check_qcmd(up, &last_cmd) && last_cmd == cmd &&
		cmd != qcmd_port_irq && cmd != qcmd_dma_irq)
		return;
	up->qcmd_num++;
	buf = circ->buf + circ->head;
	if (CIRC_SPACE(circ->head, circ->tail, HSU_Q_MAX) < 1)
		*buf = qcmd_overflow;
	else {
		*buf = cmd;
		circ->head++;
		if (circ->head == HSU_Q_MAX)
			circ->head = 0;
	}
}

static inline int get_qcmd(struct uart_hsu_port *up, char *cmd)
{
	struct circ_buf *circ = &up->qcirc;
	char *buf;

	if (!CIRC_CNT(circ->head, circ->tail, HSU_Q_MAX))
		return 0;
	buf = circ->buf + circ->tail;
	*cmd = *buf;
	circ->tail++;
	if (circ->tail == HSU_Q_MAX)
		circ->tail = 0;
	up->qcmd_done++;
	return 1;
}

static inline void cl_put_char(struct uart_hsu_port *up, char c)
{
	struct circ_buf *circ = &up->cl_circ;
	char *buf;
	unsigned long flags;

	spin_lock_irqsave(&up->cl_lock, flags);
	buf = circ->buf + circ->head;
	if (CIRC_SPACE(circ->head, circ->tail, HSU_CL_BUF_LEN) > 1) {
		*buf = c;
		circ->head++;
		if (circ->head == HSU_CL_BUF_LEN)
			circ->head = 0;
	}
	spin_unlock_irqrestore(&up->cl_lock, flags);
}

static inline int cl_get_char(struct uart_hsu_port *up, char *c)
{
	struct circ_buf *circ = &up->cl_circ;
	char *buf;
	unsigned long flags;

	spin_lock_irqsave(&up->cl_lock, flags);
	if (!CIRC_CNT(circ->head, circ->tail, HSU_CL_BUF_LEN)) {
		spin_unlock_irqrestore(&up->cl_lock, flags);
		return 0;
	}
	buf = circ->buf + circ->tail;
	*c = *buf;
	circ->tail++;
	if (circ->tail == HSU_CL_BUF_LEN)
		circ->tail = 0;
	spin_unlock_irqrestore(&up->cl_lock, flags);
	return 1;
}



void serial_sched_cmd(struct uart_hsu_port *up, char cmd)
{
	pm_runtime_get(up->dev);
	insert_qcmd(up, cmd);
	if (test_bit(flag_cmd_on, &up->flags)) {
		if (up->use_dma)
			tasklet_schedule(&up->tasklet);
		else
			queue_work(up->workqueue, &up->work);
	}
	pm_runtime_put(up->dev);
}

static inline void serial_sched_sync(struct uart_hsu_port *up)
{
	mutex_lock(&up->q_mutex);
	if (up->q_start > 0) {
		if (up->use_dma) {
			tasklet_disable(&up->tasklet);
			serial_hsu_command(up);
			tasklet_enable(&up->tasklet);
		} else {
			flush_workqueue(up->workqueue);
		}
	}
	mutex_unlock(&up->q_mutex);
}

static inline void serial_sched_start(struct uart_hsu_port *up)
{
	unsigned long flags;

	mutex_lock(&up->q_mutex);
	up->q_start++;
	if (up->q_start == 1) {
		clear_bit(flag_cmd_off, &up->flags);
		spin_lock_irqsave(&up->port.lock, flags);
		set_bit(flag_cmd_on, &up->flags);
		spin_unlock_irqrestore(&up->port.lock, flags);
		if (up->use_dma)
			tasklet_schedule(&up->tasklet);
		else
			queue_work(up->workqueue, &up->work);
	}
	mutex_unlock(&up->q_mutex);
}

static inline void serial_sched_stop(struct uart_hsu_port *up)
{
	unsigned long flags;

	mutex_lock(&up->q_mutex);
	up->q_start--;
	if (up->q_start == 0) {
		spin_lock_irqsave(&up->port.lock, flags);
		clear_bit(flag_cmd_on, &up->flags);
		insert_qcmd(up, qcmd_cmd_off);
		spin_unlock_irqrestore(&up->port.lock, flags);
		if (up->use_dma) {
			tasklet_schedule(&up->tasklet);
			while (!test_bit(flag_cmd_off, &up->flags))
				cpu_relax();
		} else {
			queue_work(up->workqueue, &up->work);
			flush_workqueue(up->workqueue);
		}
	}
	mutex_unlock(&up->q_mutex);
}

#ifdef CONFIG_DEBUG_FS

#define HSU_DBGFS_BUFSIZE	8192

static int hsu_show_regs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t port_show_regs(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct uart_hsu_port *up = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(HSU_DBGFS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	pm_runtime_get_sync(up->dev);
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"HSU port[%d] regs:\n", up->index);

	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"IER: \t\t0x%08x\n", serial_in(up, UART_IER));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"IIR: \t\t0x%08x\n", serial_in(up, UART_IIR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"LCR: \t\t0x%08x\n", serial_in(up, UART_LCR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MCR: \t\t0x%08x\n", serial_in(up, UART_MCR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"LSR: \t\t0x%08x\n", serial_in(up, UART_LSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MSR: \t\t0x%08x\n", serial_in(up, UART_MSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"FOR: \t\t0x%08x\n", serial_in(up, UART_FOR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"PS: \t\t0x%08x\n", serial_in(up, UART_PS));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MUL: \t\t0x%08x\n", serial_in(up, UART_MUL));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"DIV: \t\t0x%08x\n", serial_in(up, UART_DIV));
	pm_runtime_put(up->dev);

	if (len > HSU_DBGFS_BUFSIZE)
		len = HSU_DBGFS_BUFSIZE;

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static ssize_t dma_show_regs(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct hsu_dma_chan *chan = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(HSU_DBGFS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	pm_runtime_get_sync(chan->uport->dev);
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"HSU DMA channel [%d] regs:\n", chan->id);

	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"CR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_CR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"DCR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_DCR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"BSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_BSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"MOTSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_MOTSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D0SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D0TSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D1SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D1TSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D2SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D2TSR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0SAR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D3SAR));
	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"D0TSR: \t\t0x%08x\n", chan_readl(chan, HSU_CH_D3TSR));
	pm_runtime_put(chan->uport->dev);

	if (len > HSU_DBGFS_BUFSIZE)
		len = HSU_DBGFS_BUFSIZE;

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static ssize_t hsu_dump_show(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct uart_hsu_port *up;
	struct hsu_port_cfg *cfg;
	char *buf;
	char cmd;
	int i;
	u32 len = 0;
	ssize_t ret;
	struct irq_desc *port_irqdesc;
	struct circ_buf *xmit;

	buf = kzalloc(HSU_DBGFS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
		"HSU status dump:\n");
	for (i = 0; i < phsu->port_num; i++) {
		up = phsu->port + i;
		cfg = up->port_cfg;
		port_irqdesc = irq_to_desc(up->port.irq);
		xmit = &up->port.state->xmit;

		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"HSU port[%d] %s:\n", up->index, up->name);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"xmit empty[%d] xmit pending[%d]\n",
			uart_circ_empty(xmit),
			(int)uart_circ_chars_pending(xmit));
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tsuspend idle: %d\n", cfg->idle);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tuse_dma: %s\n",
			up->use_dma ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_console: %s\n",
			test_bit(flag_console, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_suspend: %s\n",
			test_bit(flag_suspend, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_active: %s\n",
			test_bit(flag_active, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tflag_startup: %s\n",
			test_bit(flag_startup, &up->flags) ? "yes" : "no");
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd q_start: %d\n", up->q_start);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd total count: %d\n", up->qcmd_num);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd done count: %d\n", up->qcmd_done);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq count: %d\n", up->port_irq_num);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq cmddone: %d\n", up->port_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq no irq pending: %d\n",
			up->port_irq_pio_no_irq_pend);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio line status: %d\n",
			up->port_irq_pio_line_sts);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio rx available: %d\n",
			up->port_irq_pio_rx_avb);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio rx fifo timeout: %d\n",
			up->port_irq_pio_rx_timeout);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq pio tx request: %d\n",
			up->port_irq_pio_tx_req);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tdma irq count: %d\n", up->dma_irq_num);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tdma irq cmddone: %d\n", up->dma_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport&dma rx irq cmddone: %d\n",
			up->dma_rx_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport&dma rx timeout irq cmddone: %d\n",
			up->dma_rx_tmt_irq_cmddone);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\ttasklet done: %d\n", up->tasklet_done);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tworkq done: %d\n", up->workq_done);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tqcmd pending count: %d\n", check_qcmd(up, &cmd));
		if (check_qcmd(up, &cmd))
			len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
				"\tqcmd pending next: %d\n", cmd);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tin tasklet: %d\n", up->in_tasklet);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tin workq: %d\n", up->in_workq);
		len += snprintf(buf + len, HSU_DBGFS_BUFSIZE - len,
			"\tport irq (>0: disable): %d\n",
			port_irqdesc ? port_irqdesc->depth : 0);
	}
	if (len > HSU_DBGFS_BUFSIZE)
		len = HSU_DBGFS_BUFSIZE;

	ret =  simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}


static const struct file_operations port_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= hsu_show_regs_open,
	.read		= port_show_regs,
	.llseek		= default_llseek,
};

static const struct file_operations dma_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= hsu_show_regs_open,
	.read		= dma_show_regs,
	.llseek		= default_llseek,
};

static const struct file_operations hsu_dump_ops = {
	.owner		= THIS_MODULE,
	.read		= hsu_dump_show,
	.llseek		= default_llseek,
};

static int hsu_debugfs_add(struct hsu_port *hsu)
{
	struct hsu_dma_chan *dchan;
	int i;
	char name[32];

	hsu->debugfs = debugfs_create_dir("hsu", NULL);
	if (!hsu->debugfs)
		return -ENOMEM;

	for (i = 0; i < hsu->port_num; i++) {
		snprintf(name, sizeof(name), "port_%d_regs", i);
		debugfs_create_file(name, S_IRUSR,
			hsu->debugfs, (void *)(&hsu->port[i]), &port_regs_ops);
	}

	for (i = 0; i < 6; i++) {
		dchan = &hsu->chans[i];
		if (!dchan->uport)
			break;
		snprintf(name, sizeof(name), "dma_chan_%d_regs", i);
		debugfs_create_file(name, S_IRUSR,
			hsu->debugfs, (void *)dchan, &dma_regs_ops);
	}

	snprintf(name, sizeof(name), "dump_status");
	debugfs_create_file(name, S_IRUSR,
		hsu->debugfs, NULL, &hsu_dump_ops);

	return 0;
}

static void hsu_debugfs_remove(struct hsu_port *hsu)
{
	debugfs_remove_recursive(hsu->debugfs);
}

static int __init hsu_debugfs_init(void)
{
	return hsu_debugfs_add(phsu);
}

static void __exit hsu_debugfs_exit(void)
{
	hsu_debugfs_remove(phsu);
}

late_initcall(hsu_debugfs_init);
module_exit(hsu_debugfs_exit);
#endif /* CONFIG_DEBUG_FS */

static void serial_hsu_enable_ms(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	up->ier |= UART_IER_MSI;
	serial_sched_cmd(up, qcmd_set_ier);
}

/* Protected by spin_lock_irqsave(port->lock) */
static void serial_hsu_start_tx(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	serial_sched_cmd(up, qcmd_start_tx);
}

static void serial_hsu_stop_tx(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	serial_sched_cmd(up, qcmd_stop_tx);
}

/* This is always called in spinlock protected mode, so
 * modify timeout timer is safe here */
void intel_dma_do_rx(struct uart_hsu_port *up, u32 int_sts)
{
	struct hsu_dma_buffer *dbuf = &up->rxbuf;
	struct hsu_dma_chan *chan = up->rxc;
	struct uart_port *port = &up->port;
	struct tty_struct *tty;
	struct tty_port *tport = &port->state->port;
	int count;

	tty = tty_port_tty_get(&up->port.state->port);
	if (!tty)
		return;

	/*
	 * First need to know how many is already transferred,
	 * then check if its a timeout DMA irq, and return
	 * the trail bytes out, push them up and reenable the
	 * channel
	 */

	/* Timeout IRQ, need wait some time, see Errata 2 */
	if (int_sts & 0xf00) {
		up->dma_rx_tmt_irq_cmddone++;
		udelay(2);
	} else
		up->dma_rx_irq_cmddone++;

	/* Stop the channel */
	chan_writel(chan, HSU_CH_CR, 0x0);

	count = chan_readl(chan, HSU_CH_D0SAR) - dbuf->dma_addr;
	if (!count) {
		/* Restart the channel before we leave */
		chan_writel(chan, HSU_CH_CR, 0x3);
		tty_kref_put(tty);
		return;
	}

	dma_sync_single_for_cpu(port->dev, dbuf->dma_addr,
			dbuf->dma_size, DMA_FROM_DEVICE);

	/*
	 * Head will only wrap around when we recycle
	 * the DMA buffer, and when that happens, we
	 * explicitly set tail to 0. So head will
	 * always be greater than tail.
	 */
	tty_insert_flip_string(tport, dbuf->buf, count);
	port->icount.rx += count;

	dma_sync_single_for_device(up->port.dev, dbuf->dma_addr,
			dbuf->dma_size, DMA_FROM_DEVICE);

	/* Reprogram the channel */
	chan_writel(chan, HSU_CH_D0SAR, dbuf->dma_addr);
	chan_writel(chan, HSU_CH_D0TSR, dbuf->dma_size);
	chan_writel(chan, HSU_CH_DCR, 0x1
			| (0x1 << 8)
			| (0x1 << 16)
			| (0x1 << 24) /* timeout bit, see HSU Errata 1 */
			);
	tty_flip_buffer_push(tport);

	chan_writel(chan, HSU_CH_CR, 0x3);
	tty_kref_put(tty);

}

static void serial_hsu_stop_rx(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	serial_sched_cmd(up, qcmd_stop_rx);
}

static inline void receive_chars(struct uart_hsu_port *up, int *status)
{
	struct tty_struct *tty = up->port.state->port.tty;
	struct tty_port *tport = &up->port.state->port;
	unsigned int ch, flag;
	unsigned int max_count = 256;

	if (!tty)
		return;

	do {
		ch = serial_in(up, UART_RX);
		flag = TTY_NORMAL;
		up->port.icount.rx++;

		if (unlikely(*status & (UART_LSR_BI | UART_LSR_PE |
				       UART_LSR_FE | UART_LSR_OE))) {

			dev_warn(up->dev,
				"We really rush into ERR/BI case status = 0x%02x\n",
				*status);
			/* For statistics only */
			if (*status & UART_LSR_BI) {
				*status &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char;
			} else if (*status & UART_LSR_PE)
				up->port.icount.parity++;
			else if (*status & UART_LSR_FE)
				up->port.icount.frame++;
			if (*status & UART_LSR_OE)
				up->port.icount.overrun++;

			/* Mask off conditions which should be ignored. */
			*status &= up->port.read_status_mask;

#ifdef CONFIG_SERIAL_HSU_CONSOLE
			if (up->port.cons &&
				up->port.cons->index == up->port.line) {
				/* Recover the break flag from console xmit */
				*status |= up->lsr_break_flag;
				up->lsr_break_flag = 0;
			}
#endif
			if (*status & UART_LSR_BI)
				flag = TTY_BREAK;
			else if (*status & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (*status & UART_LSR_FE)
				flag = TTY_FRAME;
		}

		if (uart_handle_sysrq_char(&up->port, ch))
			goto ignore_char;

		uart_insert_char(&up->port, *status, UART_LSR_OE, ch, flag);
ignore_char:
		*status = serial_in(up, UART_LSR);
	} while ((*status & UART_LSR_DR) && max_count--);

	tty_flip_buffer_push(tport);
}

static void transmit_chars(struct uart_hsu_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	unsigned long flags;
	int count;

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->port.x_char) {
		serial_out(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		goto out;
	}
	if (uart_circ_empty(xmit) || uart_tx_stopped(&up->port)) {
		serial_hsu_stop_tx(&up->port);
		goto out;
	}

	/* The IRQ is for TX FIFO half-empty */
	count = up->port.fifosize / 2;

	do {
		if (uart_tx_stopped(&up->port)) {
			serial_hsu_stop_tx(&up->port);
			break;
		}
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);

		up->port.icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	if (uart_circ_empty(xmit))
		serial_hsu_stop_tx(&up->port);

out:
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void check_modem_status(struct uart_hsu_port *up)
{
	int status;
	int delta_msr = 0;

	status = serial_in(up, UART_MSR);

	if ((status & UART_MSR_ANY_DELTA)) {
		if (status & UART_MSR_TERI)
			up->port.icount.rng++;
		if (status & UART_MSR_DDSR)
			up->port.icount.dsr++;
		/* We may only get DDCD when HW init and reset */
		if (status & UART_MSR_DDCD)
			uart_handle_dcd_change(&up->port,
					status & UART_MSR_DCD);
		if (status & UART_MSR_DCTS)
			uart_handle_cts_change(&up->port,
					status & UART_MSR_CTS);
		delta_msr = 1;
	}

	if (delta_msr)
		wake_up_interruptible(&up->port.state->port.delta_msr_wait);
}

static void hsu_dma_chan_handler(struct hsu_port *hsu, int index)
{
	unsigned long flags;
	struct uart_hsu_port *up = hsu->chans[index * 2].uport;
	struct hsu_dma_chan *txc = up->txc;
	struct hsu_dma_chan *rxc = up->rxc;

	up->dma_irq_num++;
	if (unlikely(!up->use_dma
		|| !test_bit(flag_startup, &up->flags))) {
		chan_readl(txc, HSU_CH_SR);
		chan_readl(rxc, HSU_CH_SR);
		return;
	}
	disable_irq_nosync(up->dma_irq);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_dma_irq);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

/*
 * This handles the interrupt from one port.
 */
static irqreturn_t hsu_port_irq(int irq, void *dev_id)
{
	struct uart_hsu_port *up = dev_id;
	unsigned long flags;
	u8 lsr;

	up->port_irq_num++;

	if (up->hw_type == hsu_dw) {
		if (unlikely(test_bit(flag_suspend, &up->flags)))
			return IRQ_NONE;

		/* This IRQ may be shared with other HW */
		up->iir = serial_in(up, UART_IIR) & HSU_IIR_INT_MASK;
		if (unlikely(up->iir == HSU_PIO_NO_INT))
			return IRQ_NONE;
	}

	/* DesignWare HW's DMA mode still needs the port irq */
	if (up->use_dma && up->hw_type == hsu_intel) {
		lsr = serial_in(up, UART_LSR);
		spin_lock_irqsave(&up->port.lock, flags);
		check_modem_status(up);
		spin_unlock_irqrestore(&up->port.lock, flags);
		if (unlikely(lsr & (UART_LSR_BI | UART_LSR_PE |
				UART_LSR_FE | UART_LSR_OE)))
			dev_warn(up->dev,
				"Got LSR irq(0x%02x) while using DMA", lsr);
		return IRQ_HANDLED;
	}

	disable_irq_nosync(up->port.irq);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_port_irq);
	spin_unlock_irqrestore(&up->port.lock, flags);

	return IRQ_HANDLED;
}

static irqreturn_t hsu_dma_irq(int irq, void *dev_id)
{
	struct uart_hsu_port *up;
	unsigned long flags;
	unsigned int dmairq;
	int i;

	spin_lock_irqsave(&phsu->dma_lock, flags);
	dmairq = hsu_readl(phsu, HSU_GBL_DMAISR);
	for (i = 0; i < 3; i++)
		if (dmairq & (3 << (i * 2))) {
			up = phsu->chans[i * 2].uport;
			up->port_dma_sts = dmairq;
			hsu_dma_chan_handler(phsu, i);
		}
	spin_unlock_irqrestore(&phsu->dma_lock, flags);

	return IRQ_HANDLED;
}

static unsigned int serial_hsu_tx_empty(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	int ret = 1;

	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);

	if (up->use_dma && test_bit(flag_tx_on, &up->flags))
		ret = 0;
	ret = ret &&
		(serial_in(up, UART_LSR) & UART_LSR_TEMT ? TIOCSER_TEMT : 0);
	serial_sched_start(up);
	pm_runtime_put(up->dev);
	return ret;
}

static unsigned int serial_hsu_get_mctrl(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	unsigned char status = up->msr;
	unsigned int ret = 0;

	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;
	return ret;
}

static void set_mctrl(struct uart_hsu_port *up, unsigned int mctrl)
{
	up->mcr &= ~(UART_MCR_RTS | UART_MCR_DTR | UART_MCR_OUT1 |
		     UART_MCR_OUT2 | UART_MCR_LOOP);
	if (mctrl & TIOCM_RTS)
		up->mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		up->mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		up->mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		up->mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		up->mcr |= UART_MCR_LOOP;
	serial_out(up, UART_MCR, up->mcr);
	udelay(100);
}

static void serial_hsu_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	up->mcr &= ~(UART_MCR_RTS | UART_MCR_DTR | UART_MCR_OUT1 |
		     UART_MCR_OUT2 | UART_MCR_LOOP);
	if (mctrl & TIOCM_RTS)
		up->mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		up->mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		up->mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		up->mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		up->mcr |= UART_MCR_LOOP;
	serial_sched_cmd(up, qcmd_set_mcr);
}

static void serial_hsu_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	serial_sched_start(up);
	pm_runtime_put(up->dev);
}

/*
 * What special to do:
 * 1. chose the 64B fifo mode
 * 2. start dma or pio depends on configuration
 * 3. we only allocate dma memory when needed
 */
static int serial_hsu_startup(struct uart_port *port)
{
	static int console_first_init = 1;
	int ret = 0;
	unsigned long flags;
	static DEFINE_MUTEX(lock);
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	struct hsu_port_cfg *cfg = up->port_cfg;

	mutex_lock(&lock);

	pm_runtime_get_sync(up->dev);

	/* HW start it */
	if (cfg->hw_reset)
		cfg->hw_reset(up->port.membase);

	if (console_first_init && test_bit(flag_console, &up->flags)) {
		serial_sched_stop(up);
		console_first_init = 0;
	}
	clear_bit(flag_reopen, &up->flags);
	serial_sched_start(up);
	serial_sched_stop(up);

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR |
			UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);

	/* Clear the interrupt registers. */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	/* Now, initialize the UART, default is 8n1 */
	serial_out(up, UART_LCR, UART_LCR_WLEN8);
	up->port.mctrl |= TIOCM_OUT2;
	set_mctrl(up, up->port.mctrl);

	/* DMA init */
	if (up->use_dma) {
		ret = up->dma_ops->init ? up->dma_ops->init(up) : -ENODEV;
		if (ret) {
			dev_warn(up->dev, "Fail to init DMA, will use PIO\n");
			up->use_dma = 0;
		}
	}

	/*
	 * Finally, enable interrupts.  Note: Modem status
	 * interrupts are set via set_termios(), which will
	 *  be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	/* bit 4 for DW is reserved, but SEG need it to be set */
	if (!up->use_dma || up->hw_type == hsu_dw)
		up->ier = UART_IER_RLSI | UART_IER_RDI | UART_IER_RTOIE;
	else
		up->ier = 0;
	serial_out(up, UART_IER, up->ier);

	/* And clear the interrupt registers again for luck. */
	(void) serial_in(up, UART_LSR);
	(void) serial_in(up, UART_RX);
	(void) serial_in(up, UART_IIR);
	(void) serial_in(up, UART_MSR);

	set_bit(flag_startup, &up->flags);
	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);
	pm_runtime_put(up->dev);
	mutex_unlock(&lock);

	return ret;
}

static void serial_hsu_shutdown(struct uart_port *port)
{
	static DEFINE_MUTEX(lock);
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	mutex_lock(&lock);
	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);
	clear_bit(flag_startup, &up->flags);

	/* Disable interrupts from this port */
	up->ier = 0;
	serial_out(up, UART_IER, 0);

	clear_bit(flag_tx_on, &up->flags);

	up->port.mctrl &= ~TIOCM_OUT2;
	set_mctrl(up, up->port.mctrl);

	/* Disable break condition and FIFOs */
	serial_out(up, UART_LCR,
			serial_in(up, UART_LCR) & ~UART_LCR_SBC);
	serial_out(up, UART_FCR, UART_FCR_ENABLE_FIFO |
			UART_FCR_CLEAR_RCVR |
			UART_FCR_CLEAR_XMIT);
	serial_out(up, UART_FCR, 0);

	/* Free allocated dma buffer */
	if (up->use_dma)
		up->dma_ops->exit(up);

	pm_runtime_put_sync(up->dev);
	mutex_unlock(&lock);
}

void
serial_hsu_do_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	struct uart_hsu_port *up =
			container_of(port, struct uart_hsu_port, port);
	struct hsu_port_cfg *cfg = up->port_cfg;
	unsigned char cval, fcr = 0;
	unsigned long flags;
	unsigned int baud, quot, bits;
	u32 ps = 0, mul = 0, m = 0, n = 0;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		bits = 7;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		bits = 8;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		bits = 9;
		break;
	default:
	case CS8:
		cval = UART_LCR_WLEN8;
		bits = 10;
		break;
	}

	/* CMSPAR isn't supported by this driver */
	termios->c_cflag &= ~CMSPAR;

	if (termios->c_cflag & CSTOPB) {
		cval |= UART_LCR_STOP;
		bits++;
	}
	if (termios->c_cflag & PARENB) {
		cval |= UART_LCR_PARITY;
		bits++;
	}
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;

	baud = uart_get_baud_rate(port, termios, old, 0, 4000000);

	if (up->hw_type == hsu_intel) {
		/*
		 * If base clk is 50Mhz, and the baud rate come from:
		 *	baud = 50M * MUL / (DIV * PS * DLAB)
		 *
		 * For those basic low baud rate we can get the direct
		 * scalar from 2746800, like 115200 = 2746800/24. For those
		 * higher baud rate, we handle them case by case, mainly by
		 * adjusting the MUL/PS registers, and DIV register is kept
		 * as default value 0x3d09 to make things simple
		 */

		quot = 1;
		ps = 0x10;
		mul = 0x3600;
		switch (baud) {
		case 3500000:
			mul = 0x3345;
			ps = 0xC;
			break;
		case 1843200:
			mul = 0x2400;
			break;
		case 3000000:
		case 2500000:
		case 2000000:
		case 1500000:
		case 1000000:
		case 500000:
			/*
			 * mul/ps/quot = 0x9C4/0x10/0x1 will make a 500000 bps
			 */
			mul = baud / 500000 * 0x9C4;
			break;
		default:
			/*
			 * Use uart_get_divisor to get quot for other baud rates
			 */
			quot = 0;
		}

		if (!quot)
			quot = uart_get_divisor(port, baud);

		if ((up->port.uartclk / quot) < (2400 * 16))
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_HSU_64_1B;
		else if ((up->port.uartclk / quot) < (230400 * 16))
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_HSU_64_16B;
		else
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_HSU_64_32B;

		fcr |= UART_FCR_HSU_64B_FIFO;
	} else {
		/*
		* For baud rates 0.5M, 1M, 1.5M, 2M, 2.5M, 3M, 3.5M and 4M the
		* dividers must be adjusted.
		*
		* uartclk = (m / n) * 100 MHz, where m <= n
		*/
		switch (baud) {
		case 500000:
		case 1000000:
		case 2000000:
		case 4000000:
			m = 64;
			n = 100;
			up->port.uartclk = 64000000;
			break;
		case 3500000:
			m = 56;
			n = 100;
			up->port.uartclk = 56000000;
			break;
		case 1500000:
		case 3000000:
			m = 48;
			n = 100;
			up->port.uartclk = 48000000;
			break;
		case 2500000:
			m = 40;
			n = 100;
			up->port.uartclk = 40000000;
			break;
		default:
			m = 6912;
			n = 15625;
			up->port.uartclk = 44236800;
		}

		quot = uart_get_divisor(port, baud);

		fcr = UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10 |
			UART_FCR_T_TRIG_11;
		if (baud < 2400) {
			fcr &= ~UART_FCR_TRIGGER_MASK;
			fcr |= UART_FCR_TRIGGER_1;
		}
	}

	/* one byte transfer duration unit microsecond */
	up->byte_delay = (bits * 1000000 + baud - 1) / baud;

	pm_runtime_get_sync(up->dev);
	serial_sched_stop(up);
	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	/* Update the per-port timeout */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (IGNBRK | BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/* Characters to ignore */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/* Ignore all characters if CREAD is not set */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts, disable
	 * MSI by default
	 */
	up->ier &= ~UART_IER_MSI;

	serial_out(up, UART_IER, up->ier);

	if (termios->c_cflag & CRTSCTS) {
		up->mcr |= UART_MCR_AFE | UART_MCR_RTS;
		up->prev_mcr = up->mcr;
	} else
		up->mcr &= ~UART_MCR_AFE;

	up->dll	= quot & 0xff;
	up->dlm	= quot >> 8;
	up->fcr	= fcr;
	up->lcr = cval;					/* Save LCR */

	serial_out(up, UART_LCR, cval | UART_LCR_DLAB);	/* set DLAB */
	serial_out(up, UART_DLL, up->dll);		/* LS of divisor */
	serial_out(up, UART_DLM, up->dlm);		/* MS of divisor */
	serial_out(up, UART_LCR, cval);			/* reset DLAB */

	if (up->hw_type == hsu_intel) {
		up->mul	= mul;
		up->ps	= ps;
		serial_out(up, UART_MUL, up->mul);	/* set MUL */
		serial_out(up, UART_PS, up->ps);	/* set PS */
	} else {
		if (m != up->m || n != up->n) {
			if (cfg->set_clk)
				cfg->set_clk(m, n, up->port.membase);
			up->m = m;
			up->n = n;
		}
	}

	serial_out(up, UART_FCR, fcr);
	set_mctrl(up, up->port.mctrl);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_start(up);
	serial_sched_sync(up);
	pm_runtime_put(up->dev);
}
EXPORT_SYMBOL(serial_hsu_do_set_termios);

static void
serial_hsu_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	if (port->set_termios)
		port->set_termios(port, termios, old);
	else
		serial_hsu_do_set_termios(port, termios, old);
}

static void
serial_hsu_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
}

static void serial_hsu_release_port(struct uart_port *port)
{
}

static int serial_hsu_request_port(struct uart_port *port)
{
	return 0;
}

static void serial_hsu_config_port(struct uart_port *port, int flags)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	up->port.type = PORT_HSU;
}

static int
serial_hsu_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* We don't want the core code to modify any port params */
	return -EINVAL;
}

static const char *
serial_hsu_type(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	return up->name;
}

struct device *intel_mid_hsu_set_wake_peer(int port,
			void (*wake_peer)(struct device *))
{
	struct uart_hsu_port *up = phsu->port + port;
	struct hsu_port_cfg *cfg = up->port_cfg;

	cfg->wake_peer = wake_peer;
	return cfg->dev;
}
EXPORT_SYMBOL(intel_mid_hsu_set_wake_peer);

static void serial_hsu_wake_peer(struct uart_port *port)
{
	struct uart_hsu_port *up =
			container_of(port, struct uart_hsu_port, port);
	struct hsu_port_cfg *cfg = up->port_cfg;

	if (cfg->wake_peer)
		cfg->wake_peer(cfg->dev);
}

#define BOTH_EMPTY (UART_LSR_TEMT | UART_LSR_THRE)
/* Wait for transmitter & holding register to empty */
static inline int wait_for_xmitr(struct uart_hsu_port *up)
{
	unsigned int status, tmout = 10000;

	while (--tmout) {
		status = serial_in(up, UART_LSR);
		if (status & UART_LSR_BI)
			up->lsr_break_flag = UART_LSR_BI;
		udelay(1);
		if (status & BOTH_EMPTY)
			break;
	}
	if (tmout == 0)
		return 0;

	if (up->port.flags & UPF_CONS_FLOW) {
		tmout = 10000;
		while (--tmout &&
		       ((serial_in(up, UART_MSR) & UART_MSR_CTS) == 0))
			udelay(1);
		if (tmout == 0)
			return 0;
	}
	return 1;
}

#ifdef CONFIG_CONSOLE_POLL
static int serial_hsu_get_poll_char(struct uart_port *port)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	u8 lsr;

	lsr = serial_in(up, UART_LSR);
	if (!(lsr & UART_LSR_DR))
		return NO_POLL_CHAR;
	return serial_in(up, UART_RX);
}

static void serial_hsu_put_poll_char(struct uart_port *port,
			unsigned char c)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);

	serial_out(up, UART_IER, 0);
	while (!wait_for_xmitr(up))
		cpu_relax();
	serial_out(up, UART_TX, c);
	while (!wait_for_xmitr(up))
		cpu_relax();
	serial_out(up, UART_IER, up->ier);
}
#endif

#ifdef CONFIG_SERIAL_HSU_CONSOLE
static void serial_hsu_console_putchar(struct uart_port *port, int ch)
{
	struct uart_hsu_port *up =
		container_of(port, struct uart_hsu_port, port);
	cl_put_char(up, ch);
}

/*
 * Print a string to the serial port trying not to disturb
 * any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
serial_hsu_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_hsu_port *up = phsu->port + co->index;
	unsigned long flags;

	uart_console_write(&up->port, s, count, serial_hsu_console_putchar);
	spin_lock_irqsave(&up->cl_lock, flags);
	serial_sched_cmd(up, qcmd_cl);
	spin_unlock_irqrestore(&up->cl_lock, flags);
}

static struct console serial_hsu_console;

static int __init
serial_hsu_console_setup(struct console *co, char *options)
{
	struct uart_hsu_port *up = phsu->port + co->index;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	unsigned long flags;

	if (co->index < 0 || co->index >= HSU_PORT_MAX)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	pm_runtime_get_sync(up->dev);
	set_bit(flag_console, &up->flags);
	set_bit(flag_startup, &up->flags);
	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);
	pm_runtime_put(up->dev);
	up->cl_circ.buf = kzalloc(HSU_CL_BUF_LEN, GFP_KERNEL);
	if (up->cl_circ.buf == NULL)
		return -ENOMEM;
	return uart_set_options(&up->port, co, baud, parity, bits, flow);
}

static struct console serial_hsu_console = {
	.name		= "ttyHSU",
	.write		= serial_hsu_console_write,
	.device		= uart_console_device,
	.setup		= serial_hsu_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &serial_hsu_reg,
};

#define SERIAL_HSU_CONSOLE	(&serial_hsu_console)
#else
#define SERIAL_HSU_CONSOLE	NULL
#endif

struct uart_ops serial_hsu_pops = {
	.tx_empty	= serial_hsu_tx_empty,
	.set_mctrl	= serial_hsu_set_mctrl,
	.get_mctrl	= serial_hsu_get_mctrl,
	.stop_tx	= serial_hsu_stop_tx,
	.start_tx	= serial_hsu_start_tx,
	.stop_rx	= serial_hsu_stop_rx,
	.enable_ms	= serial_hsu_enable_ms,
	.break_ctl	= serial_hsu_break_ctl,
	.startup	= serial_hsu_startup,
	.shutdown	= serial_hsu_shutdown,
	.set_termios	= serial_hsu_set_termios,
	.pm		= serial_hsu_pm,
	.type		= serial_hsu_type,
	.release_port	= serial_hsu_release_port,
	.request_port	= serial_hsu_request_port,
	.config_port	= serial_hsu_config_port,
	.verify_port	= serial_hsu_verify_port,
	.wake_peer	= serial_hsu_wake_peer,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = serial_hsu_get_poll_char,
	.poll_put_char = serial_hsu_put_poll_char,
#endif
};

static struct uart_driver serial_hsu_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= "HSU serial",
	.dev_name	= "ttyHSU",
	.major		= TTY_MAJOR,
	.minor		= 128,
	.nr		= HSU_PORT_MAX,
};

#if defined(CONFIG_PM) || defined(CONFIG_PM_RUNTIME)
static void hsu_regs_context(struct uart_hsu_port *up, int op)
{
	struct hsu_port_cfg *cfg = up->port_cfg;

	if (op == context_load) {
		usleep_range(10, 100);

		serial_out(up, UART_LCR, up->lcr);
		serial_out(up, UART_LCR, up->lcr | UART_LCR_DLAB);
		serial_out(up, UART_DLL, up->dll);
		serial_out(up, UART_DLM, up->dlm);
		serial_out(up, UART_LCR, up->lcr);

		if (up->hw_type == hsu_intel) {
			serial_out(up, UART_MUL, up->mul);
			serial_out(up, UART_DIV, up->div);
			serial_out(up, UART_PS, up->ps);
		} else {
			if (cfg->set_clk)
				cfg->set_clk(up->m, up->n, up->port.membase);
		}

		serial_out(up, UART_MCR, up->mcr);
		serial_out(up, UART_FCR, up->fcr);
		serial_out(up, UART_IER, up->ier);
	} else	/* Disable interrupt mask bits  */
		serial_out(up, UART_IER, 0);

	if (up->use_dma && up->dma_ops->context_op)
		up->dma_ops->context_op(up, op);
}

int serial_hsu_do_suspend(struct uart_hsu_port *up)
{
	struct hsu_port_cfg *cfg = up->port_cfg;
	struct uart_port *uport = &up->port;
	struct tty_port *tport = &uport->state->port;
	struct tty_struct *tty = tport->tty;
	struct circ_buf *xmit = &up->port.state->xmit;
	char cmd;
	unsigned long flags;

	/* Should check the RX FIFO is not empty */
	if (test_bit(flag_startup, &up->flags) && (up->hw_type == hsu_dw)
			&& serial_in(up, UART_DW_USR) & UART_DW_USR_RFNE)
			goto busy;

	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up, 1);

	disable_irq(up->port.irq);
	disable_irq(up->dma_irq);

	serial_sched_stop(up);
	set_bit(flag_suspend, &up->flags);

	if (test_bit(flag_startup, &up->flags) && check_qcmd(up, &cmd)) {
		dev_info(up->dev, "ignore suspend cmd: %d\n", cmd);
		goto err;
	}

	if (test_bit(flag_tx_on, &up->flags)) {
		dev_info(up->dev, "ignore suspend for tx on\n");
		dev_info(up->dev,
			"xmit pending:%d, stopped:%d, hw_stopped:%d, MSR:%x\n",
			(int)uart_circ_chars_pending(xmit), tty->stopped,
			tty->hw_stopped, serial_in(up, UART_MSR));
		goto err;
	}

	if (test_bit(flag_startup, &up->flags) && !uart_circ_empty(xmit) &&
		!uart_tx_stopped(&up->port)) {
		dev_info(up->dev, "ignore suspend for xmit\n");
		dev_info(up->dev,
			"xmit pending:%d, stopped:%d, hw_stopped:%d, MSR:%x\n",
			(int)uart_circ_chars_pending(xmit),
			tty->stopped,
			tty->hw_stopped,
			serial_in(up, UART_MSR));
		goto err;
	}

	if (up->use_dma && up->dma_ops->suspend(up))
		goto err;

	if (cfg->hw_suspend)
		cfg->hw_suspend(up);

	if (cfg->hw_context_save)
		hsu_regs_context(up, context_save);

	enable_irq(up->dma_irq);
	if (up->hw_type == hsu_dw)
		enable_irq(up->port.irq);

	return 0;
err:
	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up, 0);
	clear_bit(flag_suspend, &up->flags);
	enable_irq(up->port.irq);
	enable_irq(up->dma_irq);
	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);
busy:
	pm_schedule_suspend(up->dev, cfg->idle);
	return -EBUSY;
}
EXPORT_SYMBOL(serial_hsu_do_suspend);

int serial_hsu_do_resume(struct uart_hsu_port *up)
{
	struct hsu_port_cfg *cfg = up->port_cfg;
	unsigned long flags;

	if (!test_and_clear_bit(flag_suspend, &up->flags))
		return 0;

	if (up->hw_type == hsu_dw)
		disable_irq(up->port.irq);

	if (cfg->hw_context_save)
		hsu_regs_context(up, context_load);
	if (cfg->hw_resume)
		cfg->hw_resume(up);
	if (up->use_dma)
		up->dma_ops->resume(up);
	if (cfg->hw_set_rts)
		cfg->hw_set_rts(up, 0);

	enable_irq(up->port.irq);

	serial_sched_start(up);
	spin_lock_irqsave(&up->port.lock, flags);
	serial_sched_cmd(up, qcmd_get_msr);
	spin_unlock_irqrestore(&up->port.lock, flags);
	serial_sched_sync(up);
	return 0;
}
EXPORT_SYMBOL(serial_hsu_do_resume);
#endif

#ifdef CONFIG_PM_RUNTIME
int serial_hsu_do_runtime_idle(struct uart_hsu_port *up)
{
	struct hsu_port_cfg *cfg = up->port_cfg;

	if (!test_and_clear_bit(flag_active, &up->flags))
		pm_schedule_suspend(up->dev, 20);
	else
		pm_schedule_suspend(up->dev, cfg->idle);

	return -EBUSY;
}
EXPORT_SYMBOL(serial_hsu_do_runtime_idle);
#endif

static void serial_hsu_command(struct uart_hsu_port *up)
{
	char cmd, c;
	unsigned long flags;
	unsigned int iir, lsr;
	int status;
	struct hsu_dma_chan *txc = up->txc;
	struct hsu_dma_chan *rxc = up->rxc;

	if (unlikely(test_bit(flag_cmd_off, &up->flags)))
		return;

	if (unlikely(test_bit(flag_suspend, &up->flags))) {
		dev_err(up->dev,
			"Error to handle cmd while port is suspended\n");
		if (check_qcmd(up, &cmd))
			dev_err(up->dev, "Command pending: %d\n", cmd);
		return;
	}
	set_bit(flag_active, &up->flags);
	spin_lock_irqsave(&up->port.lock, flags);
	while (get_qcmd(up, &cmd)) {
		spin_unlock_irqrestore(&up->port.lock, flags);
		switch (cmd) {
		case qcmd_overflow:
			dev_err(up->dev, "queue overflow!!\n");
			break;
		case qcmd_set_mcr:
			serial_out(up, UART_MCR, up->mcr);
			break;
		case qcmd_set_ier:
			serial_out(up, UART_IER, up->ier);
			break;
		case qcmd_start_rx:
			/* use for DW DMA RX only */
			if (test_and_clear_bit(flag_rx_pending, &up->flags)) {
				if (up->use_dma)
					up->dma_ops->start_rx(up);
			}
			break;
		case qcmd_stop_rx:
			if (!up->use_dma || up->hw_type == hsu_dw) {
				up->ier &= ~UART_IER_RLSI;
				up->port.read_status_mask &= ~UART_LSR_DR;
				serial_out(up, UART_IER, up->ier);
			}

			if (up->use_dma)
				up->dma_ops->stop_rx(up);
			break;
		case qcmd_start_tx:
			if (up->use_dma) {
				if (!test_bit(flag_tx_on, &up->flags))
					up->dma_ops->start_tx(up);
			} else if (!(up->ier & UART_IER_THRI)) {
				up->ier |= UART_IER_THRI;
				serial_out(up, UART_IER, up->ier);
			}
			break;
		case qcmd_stop_tx:
			if (up->use_dma) {
				spin_lock_irqsave(&up->port.lock, flags);
				up->dma_ops->stop_tx(up);
				clear_bit(flag_tx_on, &up->flags);
				spin_unlock_irqrestore(&up->port.lock, flags);
			} else if (up->ier & UART_IER_THRI) {
				up->ier &= ~UART_IER_THRI;
				serial_out(up, UART_IER, up->ier);
			}
			break;
		case qcmd_cl:
			serial_out(up, UART_IER, 0);
			while (cl_get_char(up, &c)) {
				while (!wait_for_xmitr(up))
					schedule();
				serial_out(up, UART_TX, c);
			}
			serial_out(up, UART_IER, up->ier);
			break;
		case qcmd_port_irq:
			up->port_irq_cmddone++;

			/* if use shared IRQ and need more care */
			if (up->hw_type == hsu_intel) {
				iir = serial_in(up, UART_IIR);
			} else {
				if (up->iir == HSU_PIO_NO_INT)
					up->iir = serial_in(up, UART_IIR)
							& HSU_IIR_INT_MASK;
				iir = up->iir;
				up->iir = HSU_PIO_NO_INT;
			}

			iir &= HSU_IIR_INT_MASK;

			if (iir == UART_IIR_NO_INT) {
				enable_irq(up->port.irq);
				up->port_irq_pio_no_irq_pend++;
				break;
			}

			if (iir == HSU_PIO_LINE_STS)
				up->port_irq_pio_line_sts++;
			if (iir == HSU_PIO_RX_AVB)
				up->port_irq_pio_rx_avb++;
			if (iir == HSU_PIO_RX_TMO)
				up->port_irq_pio_rx_timeout++;
			if (iir == HSU_PIO_TX_REQ)
				up->port_irq_pio_tx_req++;

			lsr = serial_in(up, UART_LSR);

			/* We need to judge it's timeout or data available */
			if (lsr & UART_LSR_DR) {
				if (!up->use_dma) {
					receive_chars(up, &lsr);
				} else if (up->hw_type == hsu_dw) {
					if (iir == HSU_PIO_RX_TMO) {
						/*
						 * RX timeout IRQ, the DMA
						 * channel may be stalled
						 */
						up->dma_ops->stop_rx(up);
						receive_chars(up, &lsr);
					} else
						up->dma_ops->start_rx(up);
				}
			}

			/* lsr will be renewed during the receive_chars */
			if (!up->use_dma && (lsr & UART_LSR_THRE))
				transmit_chars(up);

			spin_lock_irqsave(&up->port.lock, flags);
			enable_irq(up->port.irq);
			spin_unlock_irqrestore(&up->port.lock, flags);
			break;
		case qcmd_dma_irq:
			/* Only hsu_intel has this irq */
			up->dma_irq_cmddone++;
			if (up->port_dma_sts & (1 << txc->id)) {
				status = chan_readl(txc, HSU_CH_SR);
				up->dma_ops->start_tx(up);
			}

			if (up->port_dma_sts & (1 << rxc->id)) {
				status = chan_readl(rxc, HSU_CH_SR);
				intel_dma_do_rx(up, status);
			}
			enable_irq(up->dma_irq);
			break;
		case qcmd_cmd_off:
			set_bit(flag_cmd_off, &up->flags);
			break;
		case qcmd_get_msr:
			break;
		default:
			dev_err(up->dev, "invalid command!!\n");
			break;
		}
		spin_lock_irqsave(&up->port.lock, flags);
		if (unlikely(test_bit(flag_cmd_off, &up->flags)))
			break;
	}
	up->msr = serial_in(up, UART_MSR);
	check_modem_status(up);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

static void serial_hsu_tasklet(unsigned long data)
{
	struct uart_hsu_port *up = (struct uart_hsu_port *)data;

	up->in_tasklet = 1;
	serial_hsu_command(up);
	up->tasklet_done++;
	up->in_tasklet = 0;
}

static void serial_hsu_work(struct work_struct *work)
{
	struct uart_hsu_port *uport =
		container_of(work, struct uart_hsu_port, work);

	uport->in_workq = 1;
	serial_hsu_command(uport);
	uport->workq_done++;
	uport->in_workq = 0;
}

static int serial_port_setup(struct uart_hsu_port *up, int index)
{
	int ret;

	up->port.line = index;
	snprintf(up->name, sizeof(up->name) - 1, "hsu_port%d", index);
	up->index = index;

	if ((hsu_dma_enable & (1 << index)) && up->dma_ops)
		up->use_dma = 1;
	else
		up->use_dma = 0;

	mutex_init(&up->q_mutex);
	tasklet_init(&up->tasklet, serial_hsu_tasklet,
				(unsigned long)up);
	up->workqueue =
		create_singlethread_workqueue(up->name);
	INIT_WORK(&up->work, serial_hsu_work);
	up->qcirc.buf = (char *)up->qbuf;
	spin_lock_init(&up->cl_lock);
	set_bit(flag_cmd_off, &up->flags);
	uart_add_one_port(&serial_hsu_reg, &up->port);

	up->dma_irq = phsu->dma_irq;
	ret = request_irq(up->port.irq, hsu_port_irq, IRQF_SHARED,
			up->name, up);
	if (ret) {
		dev_err(up->dev, "can not get port IRQ\n");
		return ret;
	}

	return 0;
}

struct uart_hsu_port *serial_hsu_port_setup(struct device *pdev, int index,
	resource_size_t start, resource_size_t len,
	int irq, struct hsu_port_cfg *cfg)
{
	struct uart_hsu_port *up;

	pr_info("Found a %s HSU\n", cfg->hw_ip ? "Designware" : "Intel");

	up = phsu->port + index;
	up->port_cfg = cfg;

	up->dev = pdev;
	up->port.type = PORT_HSU;
	up->port.iotype = UPIO_MEM;
	up->port.mapbase = start;
	up->port.membase = ioremap_nocache(up->port.mapbase, len);
	up->port.fifosize = 64;
	up->port.ops = &serial_hsu_pops;
	up->port.flags = UPF_IOREMAP;
	up->hw_type = cfg->hw_ip;
	if (cfg->get_uartclk)
		up->port.uartclk = cfg->get_uartclk(up);
	else	/* set the scalable maxim support rate to 2746800 bps */
		up->port.uartclk = 115200 * 24 * 16;

	up->port.irq = irq;
	up->port.dev = pdev;

	if (up->hw_type == hsu_intel) {
		up->txc = &phsu->chans[index * 2];
		up->rxc = &phsu->chans[index * 2 + 1];
		up->dma_ops = &intel_hsu_dma_ops;
	} else
		up->dma_ops = &dw_dma_ops;

	serial_port_setup(up, index);

	phsu->port_num++;

	return up;
}
EXPORT_SYMBOL(serial_hsu_port_setup);

void serial_hsu_port_free(struct uart_hsu_port *up)
{
	uart_remove_one_port(&serial_hsu_reg, &up->port);
	free_irq(up->port.irq, up);
}
EXPORT_SYMBOL(serial_hsu_port_free);

void serial_hsu_port_shutdown(struct uart_hsu_port *up)
{
	uart_suspend_port(&serial_hsu_reg, &up->port);
}
EXPORT_SYMBOL(serial_hsu_port_shutdown);

int serial_hsu_dma_setup(struct device *pdev,
	resource_size_t start, resource_size_t len, unsigned int irq)
{
	struct hsu_dma_chan *dchan;
	int i, ret;

	phsu->reg = ioremap_nocache(start, len);
	dchan = phsu->chans;
	for (i = 0; i < 6; i++) {
		dchan->id = i;
		dchan->dirt = (i & 0x1) ? DMA_FROM_DEVICE :
			DMA_TO_DEVICE;
		dchan->uport = &phsu->port[i/2];
		dchan->reg = phsu->reg + HSU_DMA_CHANS_REG_OFFSET +
			i * HSU_DMA_CHANS_REG_LENGTH;

		dchan++;
	}

	phsu->dma_irq = irq;
	ret = request_irq(irq, hsu_dma_irq, 0, "hsu dma", phsu);
	if (ret) {
		dev_err(pdev, "can not get dma IRQ\n");
		goto err;
	}

	dev_set_drvdata(pdev, phsu);

	return 0;
err:
	iounmap(phsu->reg);
	return ret;
}
EXPORT_SYMBOL(serial_hsu_dma_setup);

void serial_hsu_dma_free(void)
{
	free_irq(phsu->dma_irq, phsu);
}
EXPORT_SYMBOL(serial_hsu_dma_free);

static int __init hsu_init(void)
{
	int ret;

	ret = uart_register_driver(&serial_hsu_reg);
	if (ret)
		return ret;

	spin_lock_init(&phsu->dma_lock);

	return ret;
}

static void __exit hsu_exit(void)
{
	uart_unregister_driver(&serial_hsu_reg);
}

module_init(hsu_init);
module_exit(hsu_exit);

MODULE_AUTHOR("Yang Bin <bin.yang@intel.com>");
MODULE_LICENSE("GPL v2");
