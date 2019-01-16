#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <asm/fiq.h>
#include <asm/fiq_glue.h>
#include <asm/fiq_debugger.h>
#include <mach/irqs.h>
#include <mach/mt_reg_base.h>
#include <linux/uart/mtk_uart.h>
#include <linux/uart/mtk_uart_intf.h>

#define MT_UART_FIQ_ID MT_UART4_IRQ_ID

static void fiq_enable(struct platform_device *pdev, unsigned int fiq, bool enable)
{
	if (enable)
		enable_fiq(fiq);
	else
		disable_fiq(fiq);
}

static void fiq_dbg_force_irq(struct platform_device *pdev, unsigned int irq)
{
	trigger_sw_irq(irq);
}

struct fiq_debugger_pdata fiq_serial_data = {
	.uart_getc = &fiq_uart_getc,
	.uart_putc = &fiq_uart_putc,
	.fiq_enable = &fiq_enable,
	.fiq_ack = 0,
	.force_irq = &fiq_dbg_force_irq,
	.force_irq_ack = 0,
};

struct resource fiq_resource[] = {
	[0] = {
	       .start = FIQ_DBG_SGI,
	       .end = FIQ_DBG_SGI,
	       .flags = IORESOURCE_IRQ,
	       .name = "signal",
	       },
	[1] = {
	       .start = MT_UART_FIQ_ID,
	       .end = MT_UART_FIQ_ID,
	       .flags = IORESOURCE_IRQ,
	       .name = "fiq",
	       },
};

struct platform_device mt_fiq_debugger = {
	.name = "fiq_debugger",
	.id = -1,
	.dev = {
		.platform_data = &fiq_serial_data,
		},
	.num_resources = ARRAY_SIZE(fiq_resource),
	.resource = fiq_resource,
};

void mt_fiq_printf(const char *fmt, ...)
{
	char buf[256];
	va_list ap;
	unsigned c;
	char *s;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	s = (char *)buf;
	while ((c = *s++)) {
#if 0
		if (c == '\n') {
			fiq_uart_putc(NULL, '\r');
		}
#endif
		fiq_uart_putc(NULL, c);
	}
}
