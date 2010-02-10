/*
 * drivers/serial/msm_serial_debuger.c
 *
 * Serial Debugger Interface for MSM7K
 *
 * Copyright (C) 2008 Google, Inc.
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

#include <stdarg.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/console.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/kernel_debugger.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include <mach/system.h>
#include <mach/fiq.h>

#include "msm_serial.h"

static unsigned int debug_port_base;
static int debug_signal_irq;
static struct clk *debug_clk;
static int debug_enable;
static int debugger_enable;
static struct {
	unsigned int	base;
	int		irq;
	struct device	*clk_device;
	int		signal_irq;
} init_data;

static inline void msm_write(unsigned int val, unsigned int off)
{
	__raw_writel(val, debug_port_base + off);
}

static inline unsigned int msm_read(unsigned int off)
{
	return __raw_readl(debug_port_base + off);
}

static void debug_port_init(void)
{
	/* reset everything */
	msm_write(UART_CR_CMD_RESET_RX, UART_CR);
	msm_write(UART_CR_CMD_RESET_TX, UART_CR);
	msm_write(UART_CR_CMD_RESET_ERR, UART_CR);
	msm_write(UART_CR_CMD_RESET_BREAK_INT, UART_CR);
	msm_write(UART_CR_CMD_RESET_CTS, UART_CR);
	msm_write(UART_CR_CMD_SET_RFR, UART_CR);

	/* setup clock dividers */
	if (clk_get_rate(debug_clk) == 19200000) {
		/* clock is TCXO (19.2MHz) */
		msm_write(0x06, UART_MREG);
		msm_write(0xF1, UART_NREG);
		msm_write(0x0F, UART_DREG);
		msm_write(0x1A, UART_MNDREG);
	} else {
		/* clock must be TCXO/4 */
		msm_write(0x18, UART_MREG);
		msm_write(0xF6, UART_NREG);
		msm_write(0x0F, UART_DREG);
		msm_write(0x0A, UART_MNDREG);
	}

	msm_write(UART_CSR_115200, UART_CSR);

	/* rx interrupt on every character -- keep it simple */
	msm_write(0, UART_RFWR);

	/* enable TX and RX */
	msm_write(0x05, UART_CR);

	/* enable RX interrupt */
	msm_write(UART_IMR_RXLEV, UART_IMR);
}

static inline int debug_getc(void)
{
	if (msm_read(UART_SR) & UART_SR_RX_READY) {
		return msm_read(UART_RF);
	} else {
		return -1;
	}
}

static inline void debug_putc(unsigned int c)
{
	while (!(msm_read(UART_SR) & UART_SR_TX_READY)) ;
	msm_write(c, UART_TF);
}

static inline void debug_flush(void)
{
	while (!(msm_read(UART_SR) & UART_SR_TX_EMPTY)) ;
}

static void debug_puts(char *s)
{
	unsigned c;
	while ((c = *s++)) {
		if (c == '\n')
			debug_putc('\r');
		debug_putc(c);
	}
}

static void debug_prompt(void)
{
	debug_puts("debug> ");
}

int log_buf_copy(char *dest, int idx, int len);
static void dump_kernel_log(void)
{
	char buf[1024];
	int idx = 0;
	int ret;
	int saved_oip;

	/* setting oops_in_progress prevents log_buf_copy()
	 * from trying to take a spinlock which will make it
	 * very unhappy in some cases...
	 */
	saved_oip = oops_in_progress;
	oops_in_progress = 1;
	for (;;) {
		ret = log_buf_copy(buf, idx, 1023);
		if (ret <= 0)
			break;
		buf[ret] = 0;
		debug_puts(buf);
		idx += ret;
	}
	oops_in_progress = saved_oip;
}

static char *mode_name(unsigned cpsr)
{
	switch (cpsr & MODE_MASK) {
	case USR_MODE: return "USR";
	case FIQ_MODE: return "FIQ";
	case IRQ_MODE: return "IRQ";
	case SVC_MODE: return "SVC";
	case ABT_MODE: return "ABT";
	case UND_MODE: return "UND";
	case SYSTEM_MODE: return "SYS";
	default: return "???";
	}
}

#define DEBUG_MAX 64
static char debug_cmd[DEBUG_MAX];
static int debug_busy;
static int debug_abort;

static int debug_printf(void *cookie, const char *fmt, ...)
{
	char buf[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, 128, fmt, ap);
	va_end(ap);

	debug_puts(buf);
	return debug_abort;
}

/* Safe outside fiq context */
static int debug_printf_nfiq(void *cookie, const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	unsigned long irq_flags;

	va_start(ap, fmt);
	vsnprintf(buf, 128, fmt, ap);
	va_end(ap);

	local_irq_save(irq_flags);
	debug_puts(buf);
	debug_flush();
	local_irq_restore(irq_flags);
	return debug_abort;
}

#define dprintf(fmt...) debug_printf(0, fmt)

unsigned int last_irqs[NR_IRQS];

static void dump_irqs(void)
{
	int n;
	dprintf("irqnr       total  since-last   status  name\n");
	for (n = 1; n < NR_IRQS; n++) {
		struct irqaction *act = irq_desc[n].action;
		if (!act && !kstat_cpu(0).irqs[n])
			continue;
		dprintf("%5d: %10u %11u %8x  %s\n", n,
			kstat_cpu(0).irqs[n],
			kstat_cpu(0).irqs[n] - last_irqs[n],
			irq_desc[n].status,
			(act && act->name) ? act->name : "???");
		last_irqs[n] = kstat_cpu(0).irqs[n];
	}
}

static void debug_exec(const char *cmd, unsigned *regs)
{
	if (!strcmp(cmd, "pc")) {
		dprintf(" pc %08x cpsr %08x mode %s\n",
			regs[15], regs[16], mode_name(regs[16]));
	} else if (!strcmp(cmd, "regs")) {
		dprintf(" r0 %08x  r1 %08x  r2 %08x  r3 %08x\n",
			regs[0], regs[1], regs[2], regs[3]);
		dprintf(" r4 %08x  r5 %08x  r6 %08x  r7 %08x\n",
			regs[4], regs[5], regs[6], regs[7]);
		dprintf(" r8 %08x  r9 %08x r10 %08x r11 %08x  mode %s\n",
			regs[8], regs[9], regs[10], regs[11],
			mode_name(regs[16]));
		dprintf(" ip %08x  sp %08x  lr %08x  pc %08x  cpsr %08x\n",
			regs[10], regs[13], regs[14], regs[15], regs[16]);
	} else if (!strcmp(cmd, "reboot")) {
		if (msm_hw_reset_hook)
			msm_hw_reset_hook();
	} else if (!strcmp(cmd, "irqs")) {
		dump_irqs();
	} else if (!strcmp(cmd, "kmsg")) {
		dump_kernel_log();
	} else if (!strcmp(cmd, "version")) {
		dprintf("%s\n", linux_banner);
	} else {
		if (debug_busy) {
			dprintf("command processor busy. trying to abort.\n");
			debug_abort = -1;
		} else {
			strcpy(debug_cmd, cmd);
			debug_busy = 1;
		}
		msm_trigger_irq(debug_signal_irq);
		return;
	}
	debug_prompt();
}

static irqreturn_t debug_irq(int irq, void *dev)
{
	if (debug_busy) {
		struct kdbg_ctxt ctxt;

		ctxt.printf = debug_printf_nfiq;
		kernel_debugger(&ctxt, debug_cmd);
		debug_prompt();

		debug_busy = 0;
	}
	return IRQ_HANDLED;
}

static char debug_buf[DEBUG_MAX];
static int debug_count;

static void debug_fiq(void *data, void *regs)
{
	int c;
	static int last_c;

	while ((c = debug_getc()) != -1) {
		if (!debug_enable) {
			if ((c == 13) || (c == 10)) {
				debug_enable = true;
				debug_count = 0;
				debug_prompt();
			}
		} else if ((c >= ' ') && (c < 127)) {
			if (debug_count < (DEBUG_MAX - 1)) {
				debug_buf[debug_count++] = c;
				debug_putc(c);
			}
		} else if ((c == 8) || (c == 127)) {
			if (debug_count > 0) {
				debug_count--;
				debug_putc(8);
				debug_putc(' ');
				debug_putc(8);
			}
		} else if ((c == 13) || (c == 10)) {
			if (c == '\r' || (c == '\n' && last_c != '\r')) {
				debug_putc('\r');
				debug_putc('\n');
			}
			if (debug_count) {
				debug_buf[debug_count] = 0;
				debug_count = 0;
				debug_exec(debug_buf, regs);
			} else {
				debug_prompt();
			}
		}
		last_c = c;
	}
	debug_flush();
}

#if defined(CONFIG_MSM_SERIAL_DEBUGGER_CONSOLE)
static void debug_console_write(struct console *co,
				const char *s, unsigned int count)
{
	unsigned long irq_flags;

	/* disable irq's while TXing outside of FIQ context */
	local_irq_save(irq_flags);
	while (count--) {
		if (*s == '\n')
			debug_putc('\r');
		debug_putc(*s++);
	}
	debug_flush();
	local_irq_restore(irq_flags);
}

static struct console msm_serial_debug_console = {
	.name = "debug_console",
	.write = debug_console_write,
	.flags = CON_PRINTBUFFER | CON_ANYTIME | CON_ENABLED,
};
#endif

void msm_serial_debug_enable(int enable) {
	debug_enable = enable;
}

void msm_serial_debug_init(unsigned int base, int irq,
			   struct device *clk_device, int signal_irq)
{
	int ret;
	void *port;

	debug_clk = clk_get(clk_device, "uart_clk");
	if (debug_clk)
		clk_enable(debug_clk);

	port = ioremap(base, 4096);
	if (!port)
		return;

	init_data.base = base;
	init_data.irq = irq;
	init_data.clk_device = clk_device;
	init_data.signal_irq = signal_irq;
	debug_port_base = (unsigned int) port;
	debug_signal_irq = signal_irq;
	debug_port_init();

	debug_prompt();

	msm_fiq_select(irq);
	msm_fiq_set_handler(debug_fiq, 0);
	msm_fiq_enable(irq);

	ret = request_irq(signal_irq, debug_irq,
			  IRQF_TRIGGER_RISING, "debug", 0);
	if (ret)
		printk(KERN_ERR
		       "serial_debugger: could not install signal_irq");

#if defined(CONFIG_MSM_SERIAL_DEBUGGER_CONSOLE)
	register_console(&msm_serial_debug_console);
#endif
	debugger_enable = 1;
}
static int msm_serial_debug_remove(const char *val, struct kernel_param *kp)
{
	int ret;
	static int pre_stat = 1;
	ret = param_set_bool(val, kp);
	if (ret)
		return ret;

	if (pre_stat == *(int *)kp->arg)
		return 0;

	pre_stat = *(int *)kp->arg;

	if (*(int *)kp->arg) {
		msm_serial_debug_init(init_data.base, init_data.irq,
				init_data.clk_device, init_data.signal_irq);
		printk(KERN_INFO "enable FIQ serial debugger\n");
		return 0;
	}

#if defined(CONFIG_MSM_SERIAL_DEBUGGER_CONSOLE)
	unregister_console(&msm_serial_debug_console);
#endif
	free_irq(init_data.signal_irq, 0);
	msm_fiq_set_handler(NULL, 0);
	msm_fiq_disable(init_data.irq);
	msm_fiq_unselect(init_data.irq);
	clk_disable(debug_clk);
	printk(KERN_INFO "disable FIQ serial debugger\n");
	return 0;
}
module_param_call(enable, msm_serial_debug_remove, param_get_bool,
		&debugger_enable, S_IWUSR | S_IRUGO);
