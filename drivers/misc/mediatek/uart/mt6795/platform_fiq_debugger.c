#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <asm/thread_info.h>
#include <asm/fiq.h>
#include <asm/fiq_glue.h>
#include <asm/fiq_debugger.h>
#include <mach/irqs.h>
#include <mach/mt_reg_base.h>
#include <linux/uart/mtk_uart.h>
#include <linux/uart/mtk_uart_intf.h>

#define THREAD_INFO(sp) ((struct thread_info *) \
		((unsigned long)(sp) & ~(THREAD_SIZE - 1)))
#define REG_UART_BASE   *((volatile unsigned int*)(console_base_addr + 0x00))
#define REG_UART_STATUS *((volatile unsigned int*)(console_base_addr + 0x14))
#define REG_UART_IIR	*((volatile unsigned int*)(console_base_addr + 0x08))
#define FIQ_DEBUGGER_BREAK_CH 6	/* CTRL + F */
#define MAX_FIQ_DBG_EVENT 1024

static struct fiq_dbg_event fiq_dbg_events[MAX_FIQ_DBG_EVENT];
static int fiq_dbg_event_rd, fiq_dbg_event_wr;
static unsigned int fiq_dbg_event_ov;
static int console_base_addr = AP_UART1_BASE;
static int ret_FIQ_DEBUGGER_BREAK;
static int uart_irq_number = -1;

extern struct mtk_uart *mt_console_uart;
extern int is_fiq_debug_console_enable(void *argv);
extern bool debug_handle_uart_interrupt(void *state, int this_cpu, void *regs, void *svc_sp);
extern void mtk_uart_tx_handler(struct mtk_uart *uart);
extern void mtk_uart_get_modem_status(struct mtk_uart *uart);
extern void debug_handle_irq_context(void *arg);

int fiq_uart_getc(struct platform_device *pdev)
{
	int ch;

	if (ret_FIQ_DEBUGGER_BREAK) {
		ret_FIQ_DEBUGGER_BREAK = 0;
		return FIQ_DEBUGGER_BREAK;
	}

	if (!(REG_UART_STATUS & 0x01))
		return FIQ_DEBUGGER_NO_CHAR;

	ch = REG_UART_BASE & 0xFF;

	if (ch == FIQ_DEBUGGER_BREAK_CH)
		return FIQ_DEBUGGER_BREAK;

	return ch;
}

void fiq_uart_putc(struct platform_device *pdev, unsigned int c)
{
	while (!(REG_UART_STATUS & 0x20));

	REG_UART_BASE = c & 0xFF;
}

void fiq_uart_fixup(int uart_port)
{
	switch (uart_port) {
	case 0:
		console_base_addr = AP_UART0_BASE;
		fiq_resource[1].start = UART0_IRQ_BIT_ID;
		fiq_resource[1].end = UART0_IRQ_BIT_ID;
		uart_irq_number = UART0_IRQ_BIT_ID;
		break;
	case 1:
		console_base_addr = AP_UART1_BASE;
		fiq_resource[1].start = UART1_IRQ_BIT_ID;
		fiq_resource[1].end = UART1_IRQ_BIT_ID;
		uart_irq_number = UART1_IRQ_BIT_ID;
		break;
	case 2:
		console_base_addr = AP_UART2_BASE;
		fiq_resource[1].start = UART2_IRQ_BIT_ID;
		fiq_resource[1].end = UART2_IRQ_BIT_ID;
		uart_irq_number = UART2_IRQ_BIT_ID;
		break;
	case 3:
		console_base_addr = AP_UART3_BASE;
		fiq_resource[1].start = UART3_IRQ_BIT_ID;
		fiq_resource[1].end = UART3_IRQ_BIT_ID;
		uart_irq_number = UART3_IRQ_BIT_ID;
		break;
	default:
		break;
	}
}

static void __push_event(u32 iir, int data)
{
	if (((fiq_dbg_event_wr + 1) % MAX_FIQ_DBG_EVENT) == fiq_dbg_event_rd) {
		/* full */
		fiq_dbg_event_ov++;
	} else {
		fiq_dbg_events[fiq_dbg_event_wr].iir = iir;
		fiq_dbg_events[fiq_dbg_event_wr].data = data;
		fiq_dbg_event_wr++;
		fiq_dbg_event_wr %= MAX_FIQ_DBG_EVENT;
	}
}

static int __pop_event(u32 *iir, int *data)
{
	if (fiq_dbg_event_rd == fiq_dbg_event_wr) {
		/* empty */
		return -1;
	} else {
		*iir = fiq_dbg_events[fiq_dbg_event_rd].iir;
		*data = fiq_dbg_events[fiq_dbg_event_rd].data;
		fiq_dbg_event_rd++;
		fiq_dbg_event_rd %= MAX_FIQ_DBG_EVENT;
		return 0;
	}
}

static void mt_debug_fiq(void *arg, void *regs, void *svc_sp)
{
	u32 iir;
	int data = -1;
	int max_count = UART_FIFO_SIZE;
	unsigned int this_cpu;
	int need_irq = 1;

	iir = REG_UART_IIR;
	iir &= UART_IIR_INT_MASK;
	if (iir == UART_IIR_NO_INT_PENDING)
		return;
	if (iir == UART_IIR_THRE) {
	}
	__push_event(iir, data);

	while (max_count-- > 0) {
		if (!(REG_UART_STATUS & 0x01)) {
			break;
		}

		if (is_fiq_debug_console_enable(arg)) {
			data = mt_console_uart->read_byte(mt_console_uart);
			if (data == FIQ_DEBUGGER_BREAK_CH) {
				/* enter FIQ debugger mode */
				ret_FIQ_DEBUGGER_BREAK = 1;
				this_cpu = THREAD_INFO(svc_sp)->cpu;
				debug_handle_uart_interrupt(arg, this_cpu, regs, svc_sp);
				return;
			}
			__push_event(UART_IIR_NO_INT_PENDING, data);
			/*why need_irq? */
			need_irq = 1;
		} else {
			this_cpu = THREAD_INFO(svc_sp)->cpu;
			need_irq = debug_handle_uart_interrupt(arg, this_cpu, regs, svc_sp);
		}
	}

	if (need_irq) {
		mt_disable_fiq(uart_irq_number);
		trigger_sw_irq(FIQ_DBG_SGI);
	}
}

irqreturn_t mt_debug_signal_irq(int irq, void *dev_id)
{
	struct tty_struct *tty = mt_console_uart->port.state->port.tty;
	u32 iir;
	int data;

	while (__pop_event(&iir, &data) >= 0) {
		if (iir == UART_IIR_MS) {
			mtk_uart_get_modem_status(mt_console_uart);
		} else if (iir == UART_IIR_THRE) {
			mtk_uart_tx_handler(mt_console_uart);
		}
		if (data != -1) {
			if (!tty_insert_flip_char(tty->port, data, TTY_NORMAL)) {
			}
		}
	}
	tty_flip_buffer_push(tty->port);

	/* handle commands which can only be handled in the IRQ context */
	debug_handle_irq_context(dev_id);

	mt_enable_fiq(uart_irq_number);

	return IRQ_HANDLED;
}

int mt_fiq_init(void *arg)
{
	return request_fiq(uart_irq_number, (fiq_isr_handler) mt_debug_fiq, IRQF_TRIGGER_LOW, arg);
}
