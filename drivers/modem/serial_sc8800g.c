/*
 * drivers/serial/serial_sc8800g.c
 *
 * Serial driver over SPI for Spreadtrum SC8800G modem
 *
 * Copyright (C) 2012 NVIDIA Corporation
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
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

#include <linux/module.h>
#include <linux/serial.h>
#include <linux/termios.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/serial_sc8800g.h>
#include <linux/kthread.h>
#include <linux/ctype.h>
#include <mach/dma.h>
#include <mach/clk.h>
#include <linux/types.h>
#include <linux/wakelock.h>
#include <linux/circ_buf.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <mach/pinmux.h>
#include <mach/pinmux-t11.h>

/*
 * build options
 */
/* IPC related gpio debug */

/* ap get package with err, request modem resend */
#define HAS_AP_RESEND

/*  modem get package with err, request ap resend*/
#define HAS_MDM_RESEND

#ifdef HAS_AP_RESEND
#define AP_RESEND_MAX_RETRY		(5)
#endif

/*
 * WAR: finally, we need fix all the issues from modem side
 * and remove all these workaround.
 */

/*
 * Modem still doesn't pull MDM_ALIVE pin in a correct way, we
 * need to workaround for its abnormal behaviors, ask Spreadtrum
 * to fix these issues.
 */
#define MDM_ALIVE_TIMEOUT		(30 * HZ)


#if 0
/*
 * Self Testing Case
 */
#ifdef HAS_AP_RESEND
#define TEST_AP_RESEND
#endif

#ifdef HAS_MDM_RESEND
#define TEST_MDM_RESEND
#endif
#endif

/*
 *constants
 */
#define SC8800G_TRANSFER_BITS_PER_WORD	(32)
#define SEND_SIZE_ONE_LOOP	(4096)
#define RECV_SIZE_ONE_LOOP	(4096)
#define	PACKET_HEADER_SIZE	(16)
#define	PACKET_TX_ALIGNMENT	(64)
#define	PACKET_RX_ALIGNMENT	(64)
#define RX_FIRST_PACKET_LENGTH	(64)
#define RX_FIRST_PAYLOAD_MAX	(RX_FIRST_PACKET_LENGTH - PACKET_HEADER_SIZE)

#define SC8800G_TRANSFER_SIZE	(8192 + PACKET_HEADER_SIZE)
#define RECV_BUFFER_SIZE	(SC8800G_TRANSFER_SIZE)
#define	MAX_RX_PACKET_LENGTH	(RECV_BUFFER_SIZE - PACKET_HEADER_SIZE)
#define BP_RDY_TIMEOUT		(3000)
#define FORCE_ASSERT_TIMEOUT (3000)

#define FLAG_BP_RDY		(0)
#define FLAG_BP_RTS		(1)

/* header magic */
#define	HEADER_TAG		(0x7E7F)
#define	HEADER_TYPE		(0xAA55)

#define	HEADER_VALID		1
#define	HEADER_INVALID		0

#define TX_END_WAIT_TIMEOUT	(4 * HZ)

#define SC8800G_CIRCBUF_RESET(circ_buf) \
		((circ_buf)->head = (circ_buf)->tail = 0)

#define SC8800G_CIRCBUF_EMPTY(circ_buf) \
		((circ_buf)->head == (circ_buf)->tail)

#define AP_RTS_RETRY_TIMEOUT	(3 * HZ)	/* 3 second timeout */
#define AP_RTS_RETRY_PERIOD	(HZ / 20)	/* 50ms retry interval */

#define SC8800G_CLOSE_TIMEOUT	(5 * HZ)

/* private ioctls need to be communicated to user space */
#define SC8800G_IO_IGNORE_MDM_ALIVE		0x54F0
#define SC8800G_IO_IGNORE_ALL_IRQ		0x54F1
#define SC8800G_IO_IGNORE_ALL_IRQ_BUT_MDM2AP2	0x54F2
#define SC8800G_IO_ENABLE_ALL_IRQ		0x54F3

#define SUSPEND_TIMEOUT		(3 * HZ)

/* packet header. */
struct packet_header {
	u16 tag;
	u16 type;
	u32 length;
	u32 index;
	u32 checksum;
};

struct sc8800g_mdm_packet {
	struct packet_header head;
	u32 data[0] __attribute__ ((aligned(sizeof(unsigned long))));
};

struct sc8800g_mdm_dev {
	struct serial_sc8800g_offline_log_state *log_state;
	struct serial_sc8800g_pwr_state *state;
	spinlock_t lock;
	struct serial_sc8800g_platform_data *pdata;
	struct spi_device *spi;
	struct tty_struct *tty;

	unsigned long flags;

	struct timer_list ap_rts_request_timer;
	struct timer_list bp_rdy_timer;
	struct timer_list force_assert_timer;

	/* rx transfer stuffs */
	struct tasklet_struct rx_tasklet;
	struct spi_transfer rx_t;
	struct spi_message rx_m;
	int rx_look_for_header;	/* first rx packet (64B) or not */
	unsigned rx_bytes_left;	/* bytes left for the trailing payload */
	unsigned rx_packet_size;/* packet size of current rx */
#ifdef HAS_AP_RESEND
	int rx_retry;		/* resend counter */
#endif
	unsigned char *recvbuf;
#ifdef	RX_HEADER_OFFSET_WORKAROUND
	unsigned header_offset;
#endif

	/* tx transfer stuffs */
	struct tasklet_struct tx_tasklet;
	struct spi_transfer tx_t;
	struct spi_message tx_m;
	struct circ_buf tx_cirbuf;	/* tty circular buffer */
	unsigned char *sendbuf;
	unsigned long last_ap_rts_jiffies;
	unsigned long tx_data_len;	/* only for debug purpose */

#ifdef CONFIG_PM
	int suspending;
	int suspended;
#endif
	int tty_closed;
	int tty_closing;

	/*
	 * modem can't manage its own gpio pins properly during power sequence.
	 * We need the following flags to manage ISRs accordingly
	 */
	int ignore_mdm_rts;
	int ignore_mdm_rdy;
	int ignore_mdm_alive;
	int ignore_mdm2ap1;
	int ignore_mdm2ap2;

	/* prevent system from suspending for awhile */
	struct wake_lock wakeup_wake_lock;
	wait_queue_head_t continue_close;
	wait_queue_head_t continue_suspend;

	/* IRQs which used to handshake with modem */
	int rdy_irq;
	int rts_irq;
	int alive_irq;
	int mdm2ap1_irq;
	int mdm2ap2_irq;
#ifdef HAS_MDM_RESEND
	int resend_irq;
#endif
};

static void sc8800g_state_prase(struct work_struct *work);
DECLARE_WORK(uevent_update_work, sc8800g_state_prase);

static int bp_is_powered = 0;
static int enable_bp_interrupt = 0;
static unsigned int mdm_alive_state = 0;
static unsigned int mdm2ap1_state = 0;
static unsigned int mdm2ap2_state = 0;

static unsigned SC8800G_TX_BUFSIZE = PAGE_SIZE * 8;

static unsigned DOWNLOAD_SEND_BUFFER_SIZE = PAGE_SIZE * 8 + sizeof(struct packet_header);
static unsigned AVTIVE_SEND_BUFFER_SIZE = PAGE_SIZE * 2 + sizeof(struct packet_header);
unsigned char *download_sendbuf = NULL;
unsigned char *active_sendbuf = NULL;
unsigned char *download_cirbuf = NULL;
unsigned char *avtive_cirbuf = NULL;

/*UARTA related pingroups*/
#define UARTA_GPIO_PU0          TEGRA_PINGROUP_GPIO_PU0
#define UARTA_GPIO_PU1          TEGRA_PINGROUP_GPIO_PU1
#define UARTA_GPIO_PU2          TEGRA_PINGROUP_GPIO_PU2
#define UARTA_GPIO_PU3          TEGRA_PINGROUP_GPIO_PU3
int uarta_pin_group[4] = {
	UARTA_GPIO_PU0,
	UARTA_GPIO_PU1,
	UARTA_GPIO_PU2,
	UARTA_GPIO_PU3
};

/*SPI1 related pingroups */
#define SPI1_ULPI_CLK          TEGRA_PINGROUP_ULPI_CLK
#define SPI1_ULPI_DIR          TEGRA_PINGROUP_ULPI_DIR
#define SPI1_ULPI_NXT          TEGRA_PINGROUP_ULPI_NXT
#define SPI1_ULPI_STP          TEGRA_PINGROUP_ULPI_STP
int spi1_pin_group[4] = {
	SPI1_ULPI_CLK,
	SPI1_ULPI_DIR,
	SPI1_ULPI_NXT,
	SPI1_ULPI_STP
};

/* SC8800G modem related pingroup */
#define SC8800G_PINGROUP_AP_RTS         TEGRA_PINGROUP_SDMMC3_CLK
#define SC8800G_PINGROUP_AP_RDY         TEGRA_PINGROUP_KB_ROW1
#define SC8800G_PINGROUP_AP_RESEND      TEGRA_PINGROUP_SDMMC3_CLK_LB_OUT
#define SC8800G_PINGROUP_AP_TO_MDM1     TEGRA_PINGROUP_GMI_AD8
#define SC8800G_PINGROUP_AP_TO_MDM2     TEGRA_PINGROUP_GMI_A18
#define SC8800G_PINGROUP_MDM_EXTRSTN    TEGRA_PINGROUP_KB_ROW5
#define SC8800G_PINGROUP_MDM_PWRON      TEGRA_PINGROUP_GPIO_X1_AUD
#define SC8800G_PINGROUP_BP_PWRON       TEGRA_PINGROUP_GPIO_PV1
#define SC8800G_PINGROUP_MDM_RTS        TEGRA_PINGROUP_GPIO_PBB6
#define SC8800G_PINGROUP_MDM_RDY        TEGRA_PINGROUP_KB_ROW4
#define SC8800G_PINGROUP_MDM_RESEND     TEGRA_PINGROUP_KB_ROW2
#define SC8800G_PINGROUP_MDM_TO_AP1     TEGRA_PINGROUP_KB_ROW10
#define SC8800G_PINGROUP_MDM_TO_AP2     TEGRA_PINGROUP_GPIO_X4_AUD
#define SC8800G_PINGROUP_MDM_ALIVE      TEGRA_PINGROUP_GPIO_PV0

int control_pin_group[16] = {
	SC8800G_PINGROUP_AP_RTS,
	SC8800G_PINGROUP_AP_RDY,
	SC8800G_PINGROUP_AP_RESEND,
	SC8800G_PINGROUP_AP_TO_MDM1,
	SC8800G_PINGROUP_AP_TO_MDM2,
	SC8800G_PINGROUP_MDM_EXTRSTN,
	SC8800G_PINGROUP_MDM_PWRON,
	SC8800G_PINGROUP_BP_PWRON,
	SC8800G_PINGROUP_MDM_RTS,
	SC8800G_PINGROUP_MDM_RDY,
	SC8800G_PINGROUP_MDM_RESEND,
	SC8800G_PINGROUP_MDM_TO_AP1,
	SC8800G_PINGROUP_MDM_TO_AP2,
	SC8800G_PINGROUP_MDM_ALIVE
};

static void bp_special_pg_init(struct serial_sc8800g_platform_data *pdata);
static void control_pg_init(void);
static void control_pg_deinit(void);
static int sc8800g_gpio_init(struct serial_sc8800g_platform_data *pdata);
static void sc8800g_gpio_deinit(struct serial_sc8800g_platform_data *pdata);
static void spi1_pg_init(void);
static void spi1_pg_deinit(void);
static void sc8800g_enable_interrupt (struct sc8800g_mdm_dev *mdv);

static void uarta_pg_init(void);
static void uarta_pg_deinit(void);

static void reset_download_send_buffer(struct sc8800g_mdm_dev *mdv);
static void reset_avtive_send_buffer(struct sc8800g_mdm_dev *mdv);
static int sc8800g_force_assert(struct sc8800g_mdm_dev *mdv);
static irqreturn_t sc8800g_irq_resend(int irq, void *data);
static void sc8800g_force_assert_timer_expired(unsigned long data);

/*
 * debug options
 */

#define DRIVER_NAME "sc8800g: "

#define LEVEL_VERBOSE   5
#define LEVEL_DEBUG     4
#define LEVEL_INFO      3
#define LEVEL_WARNING   2
#define LEVEL_ERROR     1

static int sc8800g_debug = LEVEL_WARNING;
module_param(sc8800g_debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(sc8800g_debug, "level 0~5");

#define VDBG(fmt, args...) \
	do { \
		if (sc8800g_debug >= LEVEL_VERBOSE) \
			pr_info(DRIVER_NAME fmt, ## args); \
	} while (0);

#define DBG(fmt, args...) \
	do { \
		if (sc8800g_debug >= LEVEL_DEBUG) \
			pr_info(DRIVER_NAME fmt, ## args); \
	} while (0);

#define INFO(fmt, args...) \
	do { \
		if (sc8800g_debug >= LEVEL_INFO) \
			pr_info(DRIVER_NAME fmt, ## args); \
	} while (0);

#define WARNING(fmt, args...) \
	do { \
		if (sc8800g_debug >= LEVEL_WARNING) \
			pr_warn(DRIVER_NAME fmt, ## args); \
	} while (0);

#define ERROR(fmt, args...) \
	do { \
		if (sc8800g_debug >= LEVEL_ERROR) \
			pr_err(DRIVER_NAME fmt, ## args); \
	} while (0);


#define PREFIX_SIZE	6 /* in "%04x: " format */
#define DATA_PER_LINE   16
#define CHAR_PER_DATA   3 /* in "%02X " or " %c " format */

static int dumpsize = 1024;
module_param(dumpsize, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dumpsize, "max data length to dump");

static void hexdump(const unsigned char *data, int len)
{
	char linebuf[PREFIX_SIZE + DATA_PER_LINE * CHAR_PER_DATA + 1];
	const int linesize = PREFIX_SIZE + DATA_PER_LINE * CHAR_PER_DATA;
	char *ptr = &linebuf[0];
	int i;

	if (sc8800g_debug < LEVEL_VERBOSE)
		return;

	if (len > dumpsize)
		len = dumpsize;

	for (i = 0; i < len; i++) {
		if ((i % DATA_PER_LINE) == 0) {
			ptr += snprintf(ptr, linesize - (ptr - linebuf),
					"%04x: ", i);
		}

		if ((data[i] > 0x20) && (data[i] < 0x7F)) {
			ptr += snprintf(ptr, linesize - (ptr - linebuf),
				" %c ", data[i]);

		} else {
			ptr += snprintf(ptr, linesize - (ptr - linebuf),
				"%02x ", data[i]);
		}

		if (((i + 1) % DATA_PER_LINE) == 0) {
			*ptr = '\0';
			VDBG("%s\n", linebuf);
			ptr = &linebuf[0];
		}

	}

	/* print the last line */
	if (ptr != &linebuf[0]) {
		*ptr = '\0';
		VDBG("%s\n", linebuf);
	}
}

static void error_hexdump(const unsigned char *data, int len)
{
	char linebuf[PREFIX_SIZE + DATA_PER_LINE * CHAR_PER_DATA + 1];
	const int linesize = PREFIX_SIZE + DATA_PER_LINE * CHAR_PER_DATA;
	char *ptr = &linebuf[0];
	int i;
	if (len <= 512) {
		for (i = 0; i < len; i++) {
			if ((i % DATA_PER_LINE) == 0) {
				ptr += snprintf(ptr, linesize - (ptr - linebuf),
						"%04x: ", i);
			}

			ptr += snprintf(ptr, linesize - (ptr - linebuf),
					"%02x ", data[i]);

			if (((i + 1) % DATA_PER_LINE) == 0) {
				*ptr = '\0';
				ERROR("%s\n", linebuf);
				ptr = &linebuf[0];
			}

		}
		/* print the last line */
		if (ptr != &linebuf[0]) {
			*ptr = '\0';
			ERROR("%s\n", linebuf);
		}
	} else {
		for (i = 0; i < 256; i++) {
			if ((i % DATA_PER_LINE) == 0) {
				ptr += snprintf(ptr, linesize - (ptr - linebuf),
						"%04x: ", i);
			}

			ptr += snprintf(ptr, linesize - (ptr - linebuf),
					"%02x ", data[i]);

			if (((i + 1) % DATA_PER_LINE) == 0) {
				*ptr = '\0';
				ERROR("%s\n", linebuf);
				ptr = &linebuf[0];
			}

		}
		for (i = (len - 256); i < len; i++) {
			if ((i % DATA_PER_LINE) == 0) {
				ptr += snprintf(ptr, linesize - (ptr - linebuf),
						"%04x: ", i);
			}

			ptr += snprintf(ptr, linesize - (ptr - linebuf),
					"%02x ", data[i]);

			if (((i + 1) % DATA_PER_LINE) == 0) {
				*ptr = '\0';
				ERROR("%s\n", linebuf);
				ptr = &linebuf[0];
			}
		}
		/* print the last line */
		if (ptr != &linebuf[0]) {
			*ptr = '\0';
			ERROR("%s\n", linebuf);
		}
	}
}

#ifdef IPC_DEBUG
struct timeval mdm_rts_time;
struct timeval assert_aprdy_time;
struct timeval rx_complete_time;
struct timeval deassert_aprdy_time;
struct timeval modem_active_time;
struct timeval mdm_alive_time;
struct timeval modem_crash_time;

static int mdm_irq_rdy = 0;
static int mdm_irq_rts = 0;

static int assert_ap_rts = 0;
#define sc8800g_assert_ap_rts(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RTS].gpio, 1); \
	assert_ap_rts++; \
} while (0)

static int deassert_ap_rts = 0;
#define sc8800g_deassert_ap_rts(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RTS].gpio, 0); \
	deassert_ap_rts++;\
} while (0)

static int assert_ap_rdy = 0;
#define sc8800g_assert_ap_rdy(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RDY].gpio, 0); \
	assert_ap_rdy++;\
} while (0)

static int deassert_ap_rdy = 0;
#define sc8800g_deassert_ap_rdy(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RDY].gpio, 1); \
	deassert_ap_rdy++;\
} while (0)

static void ipc_debug_clear(void)
{
	assert_ap_rts = 0;
	deassert_ap_rts = 0;
	assert_ap_rdy = 0;
	deassert_ap_rdy = 0;

	mdm_irq_rts = 0;
	mdm_irq_rdy = 0;
	printk("Sc8800 %s\n", __func__);

	printk("Sc8800 modem_alive_time: %lld :%lld\n",
		(long long)mdm_alive_time.tv_sec,
		(long long)mdm_alive_time.tv_usec);
	printk("Sc8800 modem_active_time: %lld :%lld\n",
		(long long)modem_active_time.tv_sec,
		(long long)modem_active_time.tv_usec);
	printk("Sc8800 active last mdm_rts_time %lld :%lld\n",
		(long long)mdm_rts_time.tv_sec, (long long)mdm_rts_time.tv_usec);
	printk("Sc8800 active last assert_aprdy_time: %lld :%lld\n",
		(long long)assert_aprdy_time.tv_sec,
		(long long)assert_aprdy_time.tv_usec);
	printk("Sc8800 active last rx_complete_time: %lld :%lld\n",
		(long long)rx_complete_time.tv_sec,
		(long long)rx_complete_time.tv_usec);
	printk("Sc8800 active last deassert_aprdy_time: %lld :%lld\n",
		(long long)deassert_aprdy_time.tv_sec,
		(long long)deassert_aprdy_time.tv_usec);
}

static void ipc_debug_result(void)
{
	printk("Sc8800 %s\n", __func__);
	printk
	    ("assert_ap_rts = %d,deassert_ap_rts = %d,assert_ap_rdy = %d,deassert_ap_rdy = %d\n",
		assert_ap_rts, deassert_ap_rts, assert_ap_rdy, deassert_ap_rdy);
	printk("mdm_irq_rts = %d,mdm_irq_rdy=%d\n", mdm_irq_rts, mdm_irq_rdy);

	printk("Sc8800 modem_alive_time: %lld :%lld\n",
		(long long)mdm_alive_time.tv_sec,
		(long long)mdm_alive_time.tv_usec);
	printk("Sc8800 modem_crash_time: %lld :%lld\n",
		(long long)modem_crash_time.tv_sec,
		(long long)modem_crash_time.tv_usec);
	printk("Sc8800 crash last mdm_rts_time %lld :%lld\n",
		(long long)mdm_rts_time.tv_sec, (long long)mdm_rts_time.tv_usec);
	printk("Sc8800 crash last assert_aprdy_time: %lld :%lld\n",
		(long long)assert_aprdy_time.tv_sec,
		(long long)assert_aprdy_time.tv_usec);
	printk("Sc8800 crash last rx_complete_time: %lld :%lld\n",
		(long long)rx_complete_time.tv_sec,
		(long long)rx_complete_time.tv_usec);
	printk("Sc8800 crash last deassert_aprdy_time: %lld :%lld\n",
		(long long)deassert_aprdy_time.tv_sec,
		(long long)deassert_aprdy_time.tv_usec);
}
#else
#define sc8800g_assert_ap_rts(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RTS].gpio, 1); \
} while (0)

#define sc8800g_deassert_ap_rts(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RTS].gpio, 0); \
} while (0)


#define sc8800g_assert_ap_rdy(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RDY].gpio, 0); \
} while (0)

#define sc8800g_deassert_ap_rdy(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RDY].gpio, 1); \
} while (0)
#endif

#define sc8800g_align(len, align)	\
	(((len) + (align) - 1) / (align)) * (align)

#define sc8800g_assert_mdm_reset(_pd) do { \
	gpio_set_value(_pd->gpios[MDM_EXTRSTN].gpio, 1); \
} while (0)

#define sc8800g_deassert_mdm_reset(_pd) do { \
	gpio_set_value(_pd->gpios[MDM_EXTRSTN].gpio, 0); \
} while (0)

#define sc8800g_assert_mdm_power(_pd) do { \
	gpio_set_value(_pd->gpios[BP_PWRON].gpio, 1); \
} while (0)

#define sc8800g_deassert_mdm_power(_pd) do { \
	gpio_set_value(_pd->gpios[BP_PWRON].gpio, 0); \
} while (0)

#define sc8800g_assert_usb_switch(_pd) do { \
	gpio_set_value(_pd->gpios[USB_SWITCH].gpio, 1); \
} while (0)

#define sc8800g_deassert_usb_switch(_pd) do { \
	gpio_set_value(_pd->gpios[USB_SWITCH].gpio, 0); \
} while (0)

#ifdef HAS_AP_RESEND
#define sc8800g_assert_ap_resend(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RESEND].gpio, 1); \
} while (0)

#define sc8800g_deassert_ap_resend(_pd) do { \
	gpio_set_value(_pd->gpios[AP_RESEND].gpio, 0); \
} while (0)
#endif

#define sc8800g_assert_ap2mdm2(_pd) do { \
	gpio_set_value(_pd->gpios[AP_TO_MDM2].gpio, 1); \
} while (0)

#define sc8800g_deassert_ap2mdm2(_pd) do { \
	gpio_set_value(_pd->gpios[AP_TO_MDM2].gpio, 0); \
} while (0)

#define sc8800g_ap_rts_asserted(_pd) \
	(gpio_get_value(_pd->gpios[AP_RTS].gpio) == 1)

#define sc8800g_ap_rdy_asserted(_pd) \
	(gpio_get_value(_pd->gpios[AP_RDY].gpio) == 0)

#define sc8800g_mdm_rdy_asserted(_pd) \
	(gpio_get_value(_pd->gpios[MDM_RDY].gpio) == 0)

#define sc8800g_mdm_rts_asserted(_pd) \
	(gpio_get_value(_pd->gpios[MDM_RTS].gpio) == 0)

#define sc8800g_mdm_reset_asserted(_pd)	\
	(gpio_get_value(_pd->gpios[MDM_EXTRSTN].gpio) == 1)

#define sc8800g_mdm_power_asserted(_pd) \
	(gpio_get_value(_pd->gpios[BP_PWRON].gpio) == 1)

#define sc8800g_mdm2ap1_asserted(_pd)	\
	(gpio_get_value(_pd->gpios[MDM_TO_AP1].gpio) == 1)

#define sc8800g_mdm2ap2_asserted(_pd)	\
	(gpio_get_value(_pd->gpios[MDM_TO_AP2].gpio) == 1)

#define sc8800g_mdm_alive_asserted(_pd) \
	(gpio_get_value(_pd->gpios[MDM_ALIVE].gpio) == 1)

#define sc8800g_usb_switch_asserted(_pd) \
	(gpio_get_value(_pd->gpios[USB_SWITCH].gpio) == 1)

#ifdef HAS_AP_RESEND
#define sc8800g_ap_resend_asserted(_pd) \
	(gpio_get_value(_pd->gpios[AP_RESEND].gpio) == 1)
#endif

#ifdef HAS_MDM_RESEND
#define sc8800g_mdm_resend_asserted(_pd) \
	(gpio_get_value(_pd->gpios[MDM_RESEND].gpio) == 1)
#endif

#define sc8800g_ap2mdm2_asserted(_pd) \
	(gpio_get_value(_pd->gpios[AP_TO_MDM2].gpio) == 1)

struct serial_sc8800g_pwr_state sc8800g_mdm_pwr_state[] = {
	{PWROFF, {"SC8800G_MDM_STATE=PWROFF", NULL} },
	{PWRING, {"SC8800G_MDM_STATE=PWRING", NULL} },
	{ACTIVE, {"SC8800G_MDM_STATE=ACTIVE", NULL} },
	{CRASH, {"SC8800G_MDM_STATE=CRASH", NULL} },
	{PWR_PRE, {"SC8800G_MDM_STATE=PWR_PRE", NULL} },
};

struct serial_sc8800g_offline_log_state sc8800g_mdm_offline_log_state[] = {
	{OFFLINE_LOG_OFF, {"SC8800G_MDM_LOG_STATE=LOGOFF", NULL} },
	{OFFLINE_LOG_ON, {"SC8800G_MDM_LOG_STATE=LOGON", NULL} },
};

struct serial_sc8800g_usb_switch_state sc8800g_usb_switch_state[] = {
	{{"SC8800G_USB_STATE=AP", NULL} },
	{{"SC8800G_USB_STATE=BP", NULL} },
};

static void sc8800g_show_info(struct sc8800g_mdm_dev *mdv)
{
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;

	ERROR("[%s] ap_rts %s, ap_rdy %s\n", __func__,
		sc8800g_ap_rts_asserted(pdata) ? "asserted" : "de-asserted",
		sc8800g_ap_rdy_asserted(pdata) ? "asserted" : "de-asserted");
	ERROR("[%s] mdm_rts %s, mdm_rdy %s\n", __func__,
		sc8800g_mdm_rts_asserted(pdata) ? "asserted" : "de-asserted",
		sc8800g_mdm_rdy_asserted(pdata) ? "asserted" : "de-asserted");
	ERROR("[%s] mdm_alive %s\n", __func__,
		sc8800g_mdm_alive_asserted(pdata) ? "asserted" : "de-asserted");
	ERROR("[%s] mdm2ap1 %s\n", __func__,
		sc8800g_mdm2ap1_asserted(pdata) ? "asserted" : "de-asserted");
	ERROR("[%s] mdm2ap2 %s\n", __func__,
		sc8800g_mdm2ap2_asserted(pdata) ? "asserted" : "de-asserted");
#ifdef HAS_AP_RESEND
	ERROR("[%s] ap_resend %s\n", __func__,
		sc8800g_ap_resend_asserted(pdata) ? "asserted" : "de-asserted");
#endif
#ifdef HAS_MDM_RESEND
	ERROR("[%s] mdm_resend %s\n", __func__,
		sc8800g_mdm_resend_asserted(pdata) ? "asserted" : "de-asserted");
#endif
	ERROR("[%s] flags = 0x%lX\n", __func__, mdv->flags);
}

static unsigned long sc8800g_checksum(const unsigned char *buf, int size)
{
	int i = 0;
	unsigned long checksum = 0;

	BUG_ON(!buf);
	BUG_ON(!size);

	for (i = 0; i < size; i++)
		checksum += buf[i];

	return checksum;
}

/* local shared */
static DEFINE_MUTEX(dev_mutex);
static struct tty_driver *sc8800g_tty_driver = NULL;

/* must be called with mdv->lock hold */
static inline int sc8800g_transfer_in_progress_locked(struct sc8800g_mdm_dev *mdv)
{
	BUG_ON(!mdv);
	return (test_bit(FLAG_BP_RTS, &mdv->flags) ||
		test_bit(FLAG_BP_RDY, &mdv->flags));
}

static inline int sc8800g_transfer_in_progress(struct sc8800g_mdm_dev *mdv)
{
	unsigned long flags;
	int in_progress;
	BUG_ON(!mdv);
	spin_lock_irqsave(&mdv->lock, flags);
	in_progress = (test_bit(FLAG_BP_RTS, &mdv->flags) ||
			test_bit(FLAG_BP_RDY, &mdv->flags));
	spin_unlock_irqrestore(&mdv->lock, flags);
	return in_progress;
}

static void sc8800g_spi_tx_complete(void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	struct packet_header *phead = NULL;
	unsigned long flags;
	unsigned char *c;
	int ret;

	spin_lock_irqsave(&mdv->lock, flags);

	if (!mdv->tx_m.status) {
		VDBG("[%s] clear FLAG_BP_RDY\n", __func__);
		clear_bit(FLAG_BP_RDY, &mdv->flags);

		hexdump(mdv->tx_t.tx_buf, mdv->tx_t.len);
		phead = (struct packet_header *)mdv->tx_t.tx_buf;
		c = (unsigned char *)mdv->tx_t.tx_buf;
		c += PACKET_HEADER_SIZE;
		mdv->tx_data_len -= phead->length;
		VDBG("[%s] sent %d bytes: 0x%02X 0x%02X ... 0x%02X 0x%02X, tx_data_len=%lu\n",
			__func__, phead->length,
			c[0], c[1], c[phead->length - 2], c[phead->length - 1],
			mdv->tx_data_len);
	} else {
		ERROR("[%s] spi transfer error, tx_m.status=%d\n",
			__func__, mdv->tx_m.status);
		if (sc8800g_mdm_reset_asserted(pdata)) {
			WARNING("[%s] modem reset has been asserted, ignore error %d\n",
					__func__, mdv->tx_m.status);
		} else {
			sc8800g_show_info(mdv);
			BUG();
			tasklet_hi_schedule(&mdv->tx_tasklet);
			/* TODO :error handling */
		}
	}
	if (mdv->suspending) {
		INFO("[%s] wake_up suspending handler\n", __func__);
		wake_up(&mdv->continue_suspend);
	}
	if (mdv->tty_closing) {
		INFO("[%s] wake_up tty_closing handler\n", __func__);
		wake_up(&mdv->continue_close);
	}
	mdv->last_ap_rts_jiffies = 0;
	VDBG("[%s] de-assert ap_rts\n", __func__);
	sc8800g_deassert_ap_rts(pdata);

	if (!SC8800G_CIRCBUF_EMPTY(&mdv->tx_cirbuf)) {
		if (!timer_pending(&mdv->ap_rts_request_timer)) {
			ret = mod_timer(&mdv->ap_rts_request_timer, jiffies + AP_RTS_RETRY_PERIOD);
			VDBG("[%s] mod_timer(&mdv->ap_rts_request_timer) = %d\n", __func__, ret);
		}
	}

	spin_unlock_irqrestore(&mdv->lock, flags);
}

static void sc8800g_tx_tasklet(unsigned long data)
{
	struct sc8800g_mdm_dev *mdv = (struct sc8800g_mdm_dev *)data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	struct sc8800g_mdm_packet *pkt;
	struct circ_buf	*cb;
	int size;
	int copied = 0;
	unsigned char *pktbuf;
	unsigned long flags;
	int pkt_len;
	int origin_cirbuf_tail;
#ifdef TEST_MDM_RESEND
	static unsigned long tx_packets = 0;
	static int make_header_error = 0;
	static int make_payload_error = 0;
#endif
	int ret;

	spin_lock_irqsave(&mdv->lock, flags);

	if (!test_bit(FLAG_BP_RTS, &mdv->flags)) {

		if (timer_pending(&mdv->bp_rdy_timer))
			del_timer_sync(&mdv->bp_rdy_timer);

		pkt = (struct sc8800g_mdm_packet *)mdv->sendbuf;

#ifdef HAS_MDM_RESEND
		if (sc8800g_mdm_resend_asserted(pdata)) {
			WARNING("[%s] mdm_resend asserted\n", __func__);
			/*
			 * modem asked for last packet due to a packet error.
			 * The packet is still valid in mdv->sendbuf.
			 */
			mdv->tx_data_len += pkt->head.length;

#ifdef TEST_MDM_RESEND
			if (make_header_error == 1) {
				pkt->head.type--;
				INFO("[%s] TEST_MDM_RESEND correct header: pkt->head.type = %2x\n",
					__func__, pkt->head.type);
				error_hexdump((unsigned char *)pkt, RX_FIRST_PACKET_LENGTH);
				make_header_error = 0;
			}

			if (make_payload_error == 1) {
				pkt->head.checksum--;
				INFO("[%s] TEST_MDM_RESEND correct checksum: pkt->head.checksum = %2x\n",
					__func__, pkt->head.checksum);
				error_hexdump((unsigned char *)pkt, RX_FIRST_PACKET_LENGTH);
				make_payload_error = 0;
			}
#endif

			goto packed;
		}
#endif

		pkt->head.tag = HEADER_TAG;
		pkt->head.type = HEADER_TYPE;
		pkt->head.index = 0;
		pktbuf = (unsigned char *)pkt->data;

		cb = &mdv->tx_cirbuf;
		origin_cirbuf_tail = cb->tail;
		while ((size = CIRC_CNT_TO_END
			(cb->head, cb->tail, SC8800G_TX_BUFSIZE)) > 0) {
			memcpy(pktbuf, cb->buf + cb->tail, size);
			cb->tail = (cb->tail + size) & (SC8800G_TX_BUFSIZE - 1);
			pktbuf += size;
			copied += size;
		}

		if (!copied) {
			clear_bit(FLAG_BP_RDY, &mdv->flags);
			VDBG("[%s] nothing to do, clear FLAG_BP_RDY\n", __func__);
			if (sc8800g_ap_rts_asserted(pdata)) {
				VDBG("[%s] de-assert ap_rts\n", __func__);
				sc8800g_deassert_ap_rts(pdata);
			}
			spin_unlock_irqrestore(&mdv->lock, flags);
			return; /* nothing to send */
		}

		pkt->head.length = copied;
		pkt->head.checksum = sc8800g_checksum((unsigned char *)pkt->data, pkt->head.length);
		VDBG("[%s] pkt->head.checksum = 0x%X, pkt->head.length = %u\n",
				__func__, pkt->head.checksum, pkt->head.length);

#ifdef TEST_MDM_RESEND
		if (((tx_packets + 1) % 17) == 0) {
			INFO("[%s] TEST_MDM_RESEND tx_packets = %lu, pkt->head.type = %2x\n",
				__func__, tx_packets, pkt->head.type);
			pkt->head.type++;
			INFO("[%s] TEST_MDM_RESEND tx_packets = %lu, pkt->head.type++ = %2x\n",
				__func__, tx_packets, pkt->head.type);
			error_hexdump((unsigned char *)pkt, RX_FIRST_PACKET_LENGTH);
			make_header_error = 1;
		}

		if (((tx_packets + 1) % 33) == 0) {
			INFO("[%s] TEST_MDM_RESEND tx_packets = %lu, pkt->head.checksum = %2x\n",
				__func__, tx_packets, pkt->head.checksum);
			pkt->head.checksum++;
			INFO("[%s] TEST_MDM_RESEND tx_packets = %lu, pkt->head.checksum++ = %2x\n",
				__func__, tx_packets, pkt->head.checksum);
			error_hexdump((unsigned char *)pkt, RX_FIRST_PACKET_LENGTH);
			make_payload_error = 1;
		}
#endif

		pkt_len = pkt->head.length + PACKET_HEADER_SIZE;
		pkt_len = sc8800g_align(pkt_len, PACKET_TX_ALIGNMENT);
		mdv->tx_t.tx_buf = mdv->sendbuf;
		mdv->tx_t.len = (size_t) pkt_len;
		mdv->tx_t.bits_per_word = SC8800G_TRANSFER_BITS_PER_WORD;
		mdv->tx_t.speed_hz = spi->max_speed_hz;

packed:
		VDBG("[%s] pkt->head.length = %d mdv->tx_t.len = %d\n", __func__,
				pkt->head.length, mdv->tx_t.len);

		spi_message_init(&mdv->tx_m);
		mdv->tx_m.context = mdv;
		mdv->tx_m.complete = sc8800g_spi_tx_complete;
		spi_message_add_tail(&mdv->tx_t, &mdv->tx_m);

		spin_unlock_irqrestore(&mdv->lock, flags);
		if ((ret = spi_async(spi, &mdv->tx_m)) != 0) {
			/* enqueue spi message failed, try later */
			ERROR("[%s] spi_async failed, ret = %d\n", __func__, ret);
			sc8800g_show_info(mdv);
			BUG();
			VDBG("[%s] clear FLAG_BP_RDY\n", __func__);
			clear_bit(FLAG_BP_RDY, &mdv->flags);
			tasklet_hi_schedule(&mdv->tx_tasklet);
			cb->tail = origin_cirbuf_tail;
		}
#ifdef TEST_MDM_RESEND
		else {
			tx_packets++;
			INFO("[%s] tx_packets = %lu\n", __func__, tx_packets);
		}
#endif

	} else {
		/* Actually, this shouldn't happen at all. */
		/* If we got here, it means modem pulls mdm_rts before tx complete */
		WARNING("[%s] modem assert mdm_rts before ap tx complete\n", __func__);
		/* RX currently in transfer, schedule */
		tasklet_schedule(&mdv->tx_tasklet);
		spin_unlock_irqrestore(&mdv->lock, flags);
	}
}

static int sc8800g_tty_rx(struct sc8800g_mdm_dev *mdv, unsigned char *buf, ssize_t len)
{
	struct tty_struct *tty = mdv->tty;

	VDBG("[%s] len = %d\n", __func__, len);
	hexdump(buf, len);

	if (!tty) {
		ERROR("[%s] tty is NULL\n", __func__);
		return -EPERM;
	}

	if (tty_buffer_request_room(tty, len) < len) {
		ERROR("[%s] tty_buffer_request_room less than needed\n", __func__);
		return -EPERM;
	} else {
		tty_insert_flip_string(tty, buf, len);
		tty_flip_buffer_push(tty);
	}

	return 0;
}

static inline int sc8800g_rx_check_header(struct packet_header *phead)
{
	if ((phead->tag != HEADER_TAG) || (phead->type != HEADER_TYPE)) {
		ERROR("[%s] header not found!\n", __func__);
		goto error_out;
	}
	if (phead->length > MAX_RX_PACKET_LENGTH) {
		ERROR("[%s] packet length invalid!\n", __func__);
		goto error_out;
	}
	return 0;

error_out:
	error_hexdump((unsigned char *)phead, RX_FIRST_PACKET_LENGTH);
	return -EPERM;
}

static inline int sc8800g_rx_check_payload(struct sc8800g_mdm_packet *packet)
{
	unsigned long checksum;
	unsigned char *payload = (unsigned char *)packet->data;

	checksum = sc8800g_checksum(payload, packet->head.length);
	if (checksum != packet->head.checksum) {
		ERROR("[%s] checksum invalid, length = %d, checksum = 0x%lX, head.checksum = 0x%X\n",
			__func__,
			packet->head.length, checksum, packet->head.checksum);
		goto error_out;
	}
	return 0;

error_out:
	error_hexdump(payload, packet->head.length);
	return -EPERM;
}

#ifdef HAS_AP_RESEND
static int sc8800g_rx_request_resend(struct sc8800g_mdm_dev *mdv)
{
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;

	wake_lock_timeout(&mdv->wakeup_wake_lock, (HZ));
	if (mdv->rx_retry++ < AP_RESEND_MAX_RETRY) {
		/* schedule re-send */
		if (!sc8800g_ap_resend_asserted(pdata)) {
			WARNING("[%s] assert ap_resend\n", __func__);
			sc8800g_assert_ap_resend(pdata);
			udelay(2000);
		}
		return 0;
	} else {
		/* TODO: error handling */
		ERROR("[%s] reached maximum retry %d times\n", __func__, AP_RESEND_MAX_RETRY);
		sc8800g_show_info(mdv);
		BUG();
		mdv->rx_retry = 0;
		VDBG("[%s] de-assert ap_resend\n", __func__);
		sc8800g_deassert_ap_resend(pdata);
		udelay(10);
		return -EPERM;
	}
}
#endif

#ifdef RX_HEADER_OFFSET_WORKAROUND
/*
 * modem sometimes has few bytes data left in its tx buffer and those bytes was
 * send before a valid packet header. Those bytes of data belongs to the
 * previous mux frame the modem has just sent.
 * [  108.289448] sc8800g: [sc8800g_rx_check_header] header not found!
 * [  108.289791] sc8800g:  03 03 03 03 7f 7e 55 aa 2b 02 00 00 ae 02 00 00
 * [  108.290385] sc8800g:  7c 05 00 00 f9 0b ef 48 04 01 01 01 01 01 01 01
 * [  108.290718] sc8800g:  01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01
 * [  108.291319] sc8800g:  01 01 01 01 01 01 01 01 01 01 01 01 01 01 01 01
 *
 */
static int sc8800g_rx_header_offset(unsigned char *buf, unsigned long size)
{
	struct packet_header *phead;
	int i;
	for (i = 0; i < size; i++) {
		phead = (struct packet_header *) &buf[i];
		if ((phead->tag == HEADER_TAG) && (phead->type == HEADER_TYPE)) {
			if (i != 0)
				WARNING("[%s] offset +%d bytes\n", __func__, i);
			return i;
		}
	}
	return -EPERM;
}
#endif

static void sc8800g_spi_rx_complete(void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	struct packet_header *phead;
	struct sc8800g_mdm_packet *packet;
	unsigned payload_size;
	unsigned long flags;
#ifdef TEST_AP_RESEND
	static int rx_packets = 0;
	static int resend_header = 0;
	static int resend_payload = 0;
#endif
	int ret;
#ifdef IPC_DEBUG
			do_gettimeofday(&rx_complete_time);
#endif
	/* serialized by spi master controller */
	spin_lock_irqsave(&mdv->lock, flags);

	if (mdv->rx_m.status) {
		ERROR("[%s] spi transfer error, rx_m.status = %d\n", __func__, mdv->rx_m.status);
		if (sc8800g_mdm_reset_asserted(pdata)) {
			WARNING("[%s] modem reset has been asserted, ignore error %d\n",
					__func__, mdv->rx_m.status);
			goto out;
		} else {
			sc8800g_show_info(mdv);
#ifdef IPC_DEBUG
			ipc_debug_result();
#endif
			BUG();
			spin_unlock_irqrestore(&mdv->lock, flags);
			return tasklet_hi_schedule(&mdv->rx_tasklet);
			/* TODO: error handling */
		}
	}

	if (mdv->rx_look_for_header) {
#ifdef RX_HEADER_OFFSET_WORKAROUND
		mdv->header_offset =
			sc8800g_rx_header_offset(mdv->recvbuf, RX_FIRST_PACKET_LENGTH);
		BUG_ON(mdv->header_offset < 0);
		phead = (struct packet_header *) &mdv->recvbuf[mdv->header_offset];
#else
		phead = (struct packet_header *) mdv->recvbuf;
#endif

#ifdef TEST_AP_RESEND
		if (((rx_packets + 1) % 28) == 0) {
			INFO("[%s] TEST_AP_RESEND rx_packets = %d\n", __func__, rx_packets);
			phead->type++;
			rx_packets++;
		}
#endif

		if (sc8800g_rx_check_header(phead) != 0) {
			if (sc8800g_mdm_reset_asserted(pdata)) {
				WARNING("[%s] modem reset has been asserted, ignore head invalid error\n",
						__func__);
				goto out;
			}
#ifdef HAS_AP_RESEND
			sc8800g_rx_request_resend(mdv);
#ifdef TEST_AP_RESEND
			resend_header = 1;
#endif
			goto out;
#else
			BUG();
			goto out;
#endif
		}
#ifdef TEST_AP_RESEND
		if (resend_header) {
			INFO("[%s] TEST_AP_RESEND corrected header\n", __func__);
			error_hexdump((unsigned char *)phead, RX_FIRST_PACKET_LENGTH);
			resend_header = 0;
		}
#endif

		/* header is valid */
		mdv->rx_look_for_header = 0;
#ifdef HAS_AP_RESEND
		mdv->rx_retry = 0;
#endif
		mdv->rx_packet_size = phead->length;
#ifdef	RX_HEADER_OFFSET_WORKAROUND
		payload_size = min((RX_FIRST_PAYLOAD_MAX - mdv->header_offset),
			phead->length);
#else
		payload_size = min((u32)RX_FIRST_PAYLOAD_MAX, phead->length);
#endif
		mdv->rx_bytes_left = phead->length - payload_size;
		VDBG("[%s] phead->length = %d payload_size = %d\n", __func__,
			phead->length, payload_size);
	} else {
		/* reading the rest of payload */
		payload_size = min(mdv->rx_t.len, mdv->rx_bytes_left);
		VDBG("[%s] rx_bytes_left = %d payload_size = %d\n", __func__,
			mdv->rx_bytes_left, payload_size);
		mdv->rx_bytes_left -= payload_size;
		BUG_ON(mdv->rx_bytes_left != 0);
	}

	if (mdv->rx_bytes_left) {
		/* schedule next rx */
		VDBG("[%s] rx_bytes_left = %d, schedule next rx\n",
			__func__, mdv->rx_bytes_left);
		tasklet_hi_schedule(&mdv->rx_tasklet);
		spin_unlock_irqrestore(&mdv->lock, flags);
		return;
	}
#ifdef	RX_HEADER_OFFSET_WORKAROUND
	/* mdv->rx_bytes_left = 0, got whole packet, push to tty core */
	packet = (struct sc8800g_mdm_packet *) &mdv->recvbuf[mdv->header_offset];
#else
	packet = (struct sc8800g_mdm_packet *) mdv->recvbuf;
#endif

#ifdef TEST_AP_RESEND
	if (((rx_packets + 1) % 12) == 0) {
		INFO("[%s] TEST_AP_RESEND rx_packets = %d\n", __func__, rx_packets);
		packet->data[0]++;
		rx_packets++;
	}
#endif

	if (sc8800g_rx_check_payload(packet)) {
#ifdef HAS_AP_RESEND
		sc8800g_rx_request_resend(mdv);
		mdv->rx_packet_size = 0;
		mdv->rx_look_for_header = 1;
#ifdef TEST_AP_RESEND
		resend_payload = 1;
#endif
		goto out;
#endif
	}
#ifdef TEST_AP_RESEND
	if (resend_payload) {
		INFO("[%s] corrected checksum 0x%lX\n", __func__);
		sc8800g_checksum((unsigned char *)packet->data, packet->head.length);
		resend_payload = 0;
	}
#endif

#ifdef HAS_AP_RESEND
	/* packet checksum valid */
	mdv->rx_retry = 0;
#endif
	sc8800g_tty_rx(mdv, (unsigned char *)packet->data, packet->head.length);

	mdv->rx_packet_size = 0;
	mdv->rx_look_for_header = 1;

#ifdef HAS_AP_RESEND
	if (sc8800g_ap_resend_asserted(pdata)) {
		WARNING("[%s] de-assert ap_resend\n", __func__);
		sc8800g_deassert_ap_resend(pdata);
		udelay(2000);
	}
#endif

#ifdef TEST_AP_RESEND
	rx_packets++;
#endif

out:
	VDBG("[%s] clear FLAG_BP_RTS\n", __func__);
	clear_bit(FLAG_BP_RTS, &mdv->flags);
	VDBG("[%s] de-assert ap_rdy\n", __func__);
	sc8800g_deassert_ap_rdy(pdata);
#ifdef IPC_DEBUG
	do_gettimeofday(&deassert_aprdy_time);
	if (((long long)deassert_aprdy_time.tv_sec  != (long long)mdm_rts_time.tv_sec) &&
		((long long)deassert_aprdy_time.tv_usec > (long long)mdm_rts_time.tv_usec)) {
		printk("Sc8800 Maybe timeout mdm_rts_time %lld :%lld\n",  (long long)mdm_rts_time.tv_sec, (long long)mdm_rts_time.tv_usec);
		printk("Sc8800 Maybe timeout deassert_aprdy_time: %lld :%lld\n",  (long long)deassert_aprdy_time.tv_sec, (long long)deassert_aprdy_time.tv_usec);
	}
#endif
	if (mdv->suspending) {
		INFO("[%s] wake_up suspending handler\n", __func__);
		wake_up(&mdv->continue_suspend);
	}

	if (mdv->tty_closing) {
		INFO("[%s] wake_up tty_closing handler\n", __func__);
		wake_up(&mdv->continue_close);
	}

	if (!SC8800G_CIRCBUF_EMPTY(&mdv->tx_cirbuf)) {
		if (!timer_pending(&mdv->ap_rts_request_timer)) {
			ret = mod_timer(&mdv->ap_rts_request_timer,
				jiffies + AP_RTS_RETRY_PERIOD);
			VDBG("[%s] mod_timer(&mdv->ap_rts_request_timer)=%d\n", __func__, ret);
		}
	}

	spin_unlock_irqrestore(&mdv->lock, flags);
}

/* only used for debugging */
static void sc8800g_init_recvbuf(struct sc8800g_mdm_dev *mdv)
{
	int total = (RECV_BUFFER_SIZE/sizeof(unsigned));
	unsigned pattern = 0xefbeadde;
	unsigned *ptr = (unsigned *)mdv->recvbuf;
	while (total) {
		*ptr = pattern;
		ptr++;
		total--;
	}
}

static void sc8800g_rx_tasklet(unsigned long data)
{
	struct sc8800g_mdm_dev *mdv = (struct sc8800g_mdm_dev *) data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mdv->lock, flags);

	if (!test_bit(FLAG_BP_RDY, &mdv->flags)) {

		if (!sc8800g_ap_rdy_asserted(pdata)) {
			VDBG("[%s] assert ap_rdy\n", __func__);
			sc8800g_assert_ap_rdy(pdata);
#ifdef IPC_DEBUG
			do_gettimeofday(&assert_aprdy_time);
#endif
		} else
			VDBG("[%s] start next rx\n", __func__);

		if (mdv->rx_look_for_header) {
			sc8800g_init_recvbuf(mdv); /* TODO: remove */
			/* receiving for packet header */
			mdv->rx_t.rx_buf = mdv->recvbuf;
			mdv->rx_t.len = RX_FIRST_PACKET_LENGTH;
		} else {
			mdv->rx_t.rx_buf =
				&mdv->recvbuf[RX_FIRST_PACKET_LENGTH];

			if (mdv->rx_bytes_left > RECV_BUFFER_SIZE) {
				ERROR("[%s] jumbo packet? mdv->rx_bytes_left = %d",
					__func__, mdv->rx_bytes_left);
				sc8800g_show_info(mdv);
				BUG();
			}

			mdv->rx_t.len = (size_t) mdv->rx_bytes_left;
		}

		mdv->rx_t.len = sc8800g_align(mdv->rx_t.len, PACKET_RX_ALIGNMENT);
		mdv->rx_t.bits_per_word = SC8800G_TRANSFER_BITS_PER_WORD;
		mdv->rx_t.speed_hz = spi->max_speed_hz;

		spi_message_init(&mdv->rx_m);

		mdv->rx_m.context = mdv;
		mdv->rx_m.complete = sc8800g_spi_rx_complete;
		spi_message_add_tail(&mdv->rx_t, &mdv->rx_m);

		spin_unlock_irqrestore(&mdv->lock, flags);
		if ((ret = spi_async(spi, &mdv->rx_m)) != 0) {
			ERROR("[%s] spi_async failed, ret = %d\n", __func__, ret);
			sc8800g_show_info(mdv);
			BUG();
			VDBG("[%s] clear FLAG_BP_RTS\n", __func__);
			clear_bit(FLAG_BP_RTS, &mdv->flags);
			tasklet_hi_schedule(&mdv->rx_tasklet);
			return;
		}

	} else {
		/* Actually, this shouldn't happen at all. */
		/* If we got here, it means modem pulls mdm_rdy before rx complete */
		WARNING("[%s] modem assert mdm_rdy before ap rx complete\n", __func__);
		spin_unlock_irqrestore(&mdv->lock, flags);
#ifdef IPC_DEBUG
		sc8800g_show_info(mdv);
		ipc_debug_result();
#endif
		sc8800g_force_assert(mdv);
	}
}

/* local functions */
static int sc8800g_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;
	unsigned long flags;
	int ret = 0;

	INFO("[%s]\n", __func__);

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	BUG_ON(tty_dev != tty->dev);

	mdv = dev_get_drvdata(tty_dev);
	BUG_ON(!mdv);

	spin_lock_irqsave(&mdv->lock, flags);
	if (!mdv->tty_closed) {
		/* has been opened, allow single open only */
		ret = -EBUSY;
		goto spin_unlock;
	}
	mdv->tty_closed = 0;

	set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);


	if (mdv->state->power_state != ACTIVE &&
	    mdv->state->power_state != PWRING) {
		WARNING("[%s] tty opened while modem han't been power on, state = %d\n",
			__func__, mdv->state->power_state);
		ret = -ENODEV;
		goto spin_unlock;
	}

	BUG_ON(sc8800g_transfer_in_progress_locked(mdv));
	mdv->tty = tty;
	SC8800G_CIRCBUF_RESET(&mdv->tx_cirbuf);
	mdv->tx_data_len = 0;

spin_unlock:
	spin_unlock_irqrestore(&mdv->lock, flags);
	mutex_unlock(&dev_mutex);

	return ret;
}

static void sc8800g_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;
	unsigned long flags;
	int ret;

	INFO("[%s]\n", __func__);

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	BUG_ON(tty_dev != tty->dev);

	mdv = dev_get_drvdata(tty_dev);
	BUG_ON(!mdv);

	spin_lock_irqsave(&mdv->lock, flags);
	mdv->tty = NULL;

	if (!sc8800g_transfer_in_progress_locked(mdv)) {
		spin_unlock_irqrestore(&mdv->lock, flags);
		goto out;
	}
	mdv->tty_closing = 1;
	spin_unlock_irqrestore(&mdv->lock, flags);

	/* either rx or tx is in place, allow the transfer to be completed */
	INFO("[%s] wait for current transfer...\n", __func__);
	ret = wait_event_timeout(mdv->continue_close,
		(!sc8800g_transfer_in_progress(mdv)), SC8800G_CLOSE_TIMEOUT);
	if (!ret) {
		ERROR("[%s] close timeout\n", __func__);
		BUG(); /* shouldn't happened */
	}
	INFO("[%s] transfer completed, continue close\n", __func__);

out:
	mdv->tty_closing = 0;
	mdv->tty_closed = 1;
	mutex_unlock(&dev_mutex);
}

static void sc8800g_ap_rts_request(unsigned long data)
{
	struct sc8800g_mdm_dev *mdv = (struct sc8800g_mdm_dev *)data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mdv->lock, flags);

	if (mdv->tty_closing) {
		WARNING("[%s] mdv->tty_closing, skip\n", __func__);
		goto out;
	}

	if (mdv->tty_closed) {
		WARNING("[%s] mdv->tty_closed, skip\n", __func__);
		goto out;
	}

	if (mdv->suspending) {
		WARNING("[%s] mdv->suspending, skip\n", __func__);
		goto out;
	}

	if (CRASH == mdv->state->power_state) {
		WARNING("[%s] power_state crash, skip\n", __func__);
		goto out;
	}

	/* ap_rts should be de-asserted before asserting ap_rts */
	if (sc8800g_ap_rts_asserted(pdata)) {
		INFO("[%s] ap_rts asserted, skip\n", __func__);
		/* we have a tx in place, try later */
		goto out;
	}

	if (SC8800G_CIRCBUF_EMPTY(&mdv->tx_cirbuf)
#ifdef HAS_MDM_RESEND
		 && !sc8800g_mdm_resend_asserted(pdata)
#endif
	) {
		/*
		 * neither has data in tx_cirbuf nor has to resend last packet
		 * nothing to do, skip assert ap_rts
		 */
		INFO("[%s] no need to do tx, skip\n", __func__);
		goto out;
	}

	/* last_ap_rts_jiffies initialization */
	if (!mdv->last_ap_rts_jiffies)
		mdv->last_ap_rts_jiffies = jiffies;

	/* mdm_rdy should be de-asserted before asserting ap_rts */
	if (sc8800g_mdm_rdy_asserted(pdata)) {
		INFO("[%s] mdm_rdy asserted, try later\n", __func__);
		/* we have a tx in place, try later */
		goto schedule_for_later_exec;
	}
#ifdef HAS_AP_RESEND
	/* ap_resend should be de-asserted before asserting ap_rts */
	if (sc8800g_ap_resend_asserted(pdata)) {
		WARNING("[%s] ap_resend asserted, try later\n", __func__);
		/* we have a rx resend in place, try later */
		goto schedule_for_later_exec;
	}
#endif

	/* mdm_rts should be de-asserted before asserting ap_rts */
	if (sc8800g_mdm_rts_asserted(pdata)) {
		INFO("[%s] mdm_rts asserted, try later\n", __func__);
		/* we have a rx in place, try later */
		goto schedule_for_later_exec;
	}

	/* neither tx nor rx is in place, can assert ap_rts now */
	sc8800g_assert_ap_rts(pdata);
	VDBG("[%s] assert ap_rts\n", __func__);
	mdv->last_ap_rts_jiffies = jiffies;

	/* delete fired timer if there is any */
	ret = del_timer(&mdv->ap_rts_request_timer);
	VDBG("[%s] del_timer(&mdv->ap_rts_request_timer) = %d\n", __func__, ret);
	ret = mod_timer(&mdv->bp_rdy_timer, jiffies + msecs_to_jiffies(BP_RDY_TIMEOUT));
	VDBG("[%s] mod_timer(&mdv->bp_rdy_timer) = %d\n", __func__, ret);

	/* ap_rts asserted successfully */
	goto out;

schedule_for_later_exec:
	if (time_before(jiffies, mdv->last_ap_rts_jiffies + AP_RTS_RETRY_TIMEOUT)) {
		ret = mod_timer(&mdv->ap_rts_request_timer, jiffies + AP_RTS_RETRY_PERIOD);
		VDBG("[%s] mod_timer(&mdv->ap_rts_request_timer) = %d\n",
			__func__, ret);
	} else {
		/* TODO: timeout error handling */
		mdv->last_ap_rts_jiffies = 0;
		ERROR("[%s] failed to assert ap_rts in the past %d second(s)\n",
			__func__, AP_RTS_RETRY_TIMEOUT/HZ);
		sc8800g_show_info(mdv);
#ifdef IPC_DEBUG
		sc8800g_show_info(mdv);
		ipc_debug_result();
#endif
		sc8800g_force_assert(mdv);
	}

out:
	spin_unlock_irqrestore(&mdv->lock, flags);
	return;
}

static int sc8800g_tty_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct sc8800g_mdm_dev *mdv;
	unsigned long flags;
	struct circ_buf *cb;
	int buffered = 0;
	struct device *tty_dev;
	int bs;	/* max available consecutive buffer size */

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	BUG_ON(tty_dev != tty->dev);
	mdv = dev_get_drvdata(tty_dev);
	BUG_ON(!mdv);

	if (mdv->state->power_state != ACTIVE &&
	    mdv->state->power_state != PWRING) {
		ERROR("[%s] tty write while modem han't been power on, state = %d\n",
			__func__, mdv->state->power_state);
		return -ENODEV;
	}

	if (sc8800g_mdm_reset_asserted(mdv->pdata)) {
		ERROR("[%s] tty write while modem was falling into reset state\n",
				__func__);
		return -ENODEV;
	}

	VDBG("[%s] count = %d\n", __func__, count);
	cb = &mdv->tx_cirbuf;

	spin_lock_irqsave(&mdv->lock, flags);

	BUG_ON(mdv->tty_closed);
	BUG_ON(mdv->tty_closing);

	if (mdv->suspending) {
		/* ignore write if suspending */
		WARNING("[%s] mdv->suspending\n", __func__);
		spin_unlock_irqrestore(&mdv->lock, flags);
		return 0;
	}

	if (mdv->suspended) {
		/* ignore write if suspended, shouldn't happen */
		WARNING("[%s] mdv->suspended\n", __func__);
		spin_unlock_irqrestore(&mdv->lock, flags);
		return 0;
	}

	while ((bs = CIRC_SPACE_TO_END(cb->head, cb->tail, SC8800G_TX_BUFSIZE)) > 0) {
		bs = (count < bs) ? count : bs;
		memcpy(cb->buf + cb->head, buf, bs);
		cb->head = ((cb->head + bs) & (SC8800G_TX_BUFSIZE - 1));
		buf += bs; /* advance the source */
		buffered += bs;
		count -= bs;
		/* all source buffered */
		if (count <= 0)
			break;
	}
	mdv->tx_data_len += buffered;
	VDBG("[%s] buffered = %d tx_data_len = %lu &mdv->flags = 0x%lX\n",
		__func__, buffered, mdv->tx_data_len, mdv->flags);

	spin_unlock_irqrestore(&mdv->lock, flags);

	sc8800g_ap_rts_request((unsigned long)mdv);

	return buffered;
}

static int sc8800g_tty_write_room(struct tty_struct *tty)
{
	struct sc8800g_mdm_dev *mdv;
	unsigned long flags;
	struct circ_buf *cb;
	int room;
	struct device *tty_dev;

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	BUG_ON(tty_dev != tty->dev);
	mdv = dev_get_drvdata(tty_dev);

	if (unlikely(!mdv || !mdv->tx_cirbuf.buf)) {
		ERROR("[%s] missing mdv or mdv->tx_buf.buf\n", __func__);
		room = -ENODEV;
		goto out;
	}

	cb = &mdv->tx_cirbuf;
	spin_lock_irqsave(&mdv->lock, flags);
	room = CIRC_SPACE(cb->head, cb->tail, SC8800G_TX_BUFSIZE);
	spin_unlock_irqrestore(&mdv->lock, flags);

out:
	VDBG("[%s] write_room %d\n", __func__, room);
	mutex_unlock(&dev_mutex);
	return room;
}

static void sc8800g_tty_flush_buffer(struct tty_struct *tty)
{
	struct sc8800g_mdm_dev *mdv;
	unsigned long flags;
	struct device *tty_dev;

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	BUG_ON(tty_dev != tty->dev);
	mdv = dev_get_drvdata(tty_dev);

	spin_lock_irqsave(&mdv->lock, flags);
	SC8800G_CIRCBUF_RESET(&mdv->tx_cirbuf);
	mdv->tx_data_len = 0;
	spin_unlock_irqrestore(&mdv->lock, flags);

	mutex_unlock(&dev_mutex);
}

static int sc8800g_tty_chars_in_buffer(struct tty_struct *tty)
{
	int pending_size;
	struct sc8800g_mdm_dev *mdv;
	struct circ_buf *cb;
	unsigned long flags;
	struct device *tty_dev;

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	BUG_ON(tty_dev != tty->dev);
	mdv = dev_get_drvdata(tty_dev);

	cb = &mdv->tx_cirbuf;
	spin_lock_irqsave(&mdv->lock, flags);
	pending_size = CIRC_CNT(cb->head, cb->tail, SC8800G_TX_BUFSIZE);
	spin_unlock_irqrestore(&mdv->lock, flags);

	VDBG("[%s] pending_size %d\n", __func__, pending_size);

	mutex_unlock(&dev_mutex);
	return pending_size;
}

static int
sc8800g_tty_ioctl(struct tty_struct *tty, unsigned int cmd, unsigned long arg)
{
	struct sc8800g_mdm_dev *mdv;
	int ret = 0;
	unsigned long flags;
	struct device *tty_dev;

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	BUG_ON(tty_dev != tty->dev);
	mdv = dev_get_drvdata(tty_dev);

	spin_lock_irqsave(&mdv->lock, flags);

	switch (cmd) {
	case SC8800G_IO_IGNORE_MDM_ALIVE:
		DBG("[%s] SC8800G_IO_IGNORE_MDM_ALIVE\n", __func__);
		mdv->ignore_mdm_alive = 1;
		break;
	case SC8800G_IO_IGNORE_ALL_IRQ:
		DBG("[%s] SC8800G_IO_IGNORE_ALL_IRQ\n", __func__);
		mdv->ignore_mdm_rts = 1;
		mdv->ignore_mdm_rdy = 1;
		mdv->ignore_mdm_alive = 1;
		mdv->ignore_mdm2ap1 = 1;
		mdv->ignore_mdm2ap2 = 1;
		break;

	case SC8800G_IO_IGNORE_ALL_IRQ_BUT_MDM2AP2:
		DBG("[%s] SC8800G_IO_IGNORE_ALL_IRQ_BUT_MDM2AP1\n", __func__);
		mdv->ignore_mdm_rts = 1;
		mdv->ignore_mdm_rdy = 1;
		mdv->ignore_mdm_alive = 1;
		mdv->ignore_mdm2ap1 = 1;
		mdv->ignore_mdm2ap2 = 0;
		break;

	case SC8800G_IO_ENABLE_ALL_IRQ:
		DBG("[%s] SC8800G_IO_ENABLE_ALL_IRQ\n", __func__);
		mdv->ignore_mdm_rts = 0;
		mdv->ignore_mdm_rdy = 0;
		mdv->ignore_mdm_alive = 0;
		mdv->ignore_mdm2ap1 = 0;
		mdv->ignore_mdm2ap2 = 0;
		break;
	}

	spin_unlock_irqrestore(&mdv->lock, flags);
	mutex_unlock(&dev_mutex);
	return ret;
}

const struct tty_operations sc8800g_tty_ops = {
	.open = sc8800g_tty_open,
	.close = sc8800g_tty_close,
	.write = sc8800g_tty_write,
	.write_room = sc8800g_tty_write_room,
	.flush_buffer = sc8800g_tty_flush_buffer,
	.chars_in_buffer = sc8800g_tty_chars_in_buffer,
	.ioctl = sc8800g_tty_ioctl,
};

/*
 * modem state prase function.
 *   mdm_alive: modem has been actived, data transmit & receive enabled.
 *   mdm2ap2: assist flag.
 *   mdm2ap1: modem power state.
 */
static void sc8800g_state_prase(struct work_struct *work)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;
	struct serial_sc8800g_pwr_state *state;
	unsigned int flags;

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	if (!mdv)
		BUG();
	flags = (mdm2ap2_state << 2) | (mdm_alive_state << 1) | mdm2ap1_state;
	DBG("[%s] mdm_alive_state = %d, mdm2ap1_state = %d, mdm2ap2_state = %d\n",
		__func__, mdm_alive_state, mdm2ap1_state, mdm2ap2_state);

	switch (flags) {
	case 0:
		state = &sc8800g_mdm_pwr_state[PWROFF];
		break;
	case 1:
		state = &sc8800g_mdm_pwr_state[PWRING];
		reset_download_send_buffer(mdv);
		break;
	case 5:
		if (mdv->state->power_state == ACTIVE) {
			state = &sc8800g_mdm_pwr_state[CRASH];
			/* Clear pending timer */
			if (timer_pending(&mdv->bp_rdy_timer))
				del_timer(&mdv->bp_rdy_timer);

			if (timer_pending(&mdv->ap_rts_request_timer))
				del_timer(&mdv->ap_rts_request_timer);

			if (timer_pending(&mdv->force_assert_timer))
				del_timer(&mdv->force_assert_timer);

#ifdef IPC_DEBUG
			do_gettimeofday(&modem_crash_time);
			sc8800g_show_info(mdv);
			ipc_debug_result();
#endif
		} else
			goto err;
		break;
	case 7:
		if (mdv->state->power_state == PWRING) {
			state = &sc8800g_mdm_pwr_state[ACTIVE];
			reset_avtive_send_buffer(mdv);
#ifdef IPC_DEBUG
			do_gettimeofday(&modem_active_time);
			ipc_debug_clear();
#endif
		} else if (mdv->state->power_state == CRASH)
			state = &sc8800g_mdm_pwr_state[PWR_PRE];
		else
			goto err;
		break;
	default:
		goto err;
	}

	if (mdv->state != state) {
		DBG("[%s] modem state transform, %d --> %d\n",
			__func__, mdv->state->power_state, state->power_state);
		mdv->state = state;
		kobject_uevent_env(&mdv->spi->dev.kobj,
				KOBJ_CHANGE, mdv->state->power_state_env);
	}
	return;

err:
	WARNING("[%s] unknown modem state transform, %d --> [0x%X]\n",
			__func__, mdv->state->power_state, flags);
}

static irqreturn_t sc8800g_irq_alive(int irq, void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	mdm_alive_state = sc8800g_mdm_alive_asserted(pdata);
#ifdef IPC_DEBUG
	do_gettimeofday(&mdm_alive_time);
#endif
	wake_lock_timeout(&mdv->wakeup_wake_lock, (HZ));

	printk("[%s] mdm_alive %s\n", __func__,
		sc8800g_mdm_alive_asserted(pdata) ? "asserted" : "de-asserted");

	if (mdv->ignore_mdm_alive) {
		DBG("[%s] ignore mdm_alive\n", __func__);
		goto out;
	}

	if (mdv->suspended) {
		mdv->suspended = 0;
		INFO("[%s] remote wake up\n", __func__);
	}

	schedule_work(&uevent_update_work);

out:
	return IRQ_HANDLED;
}

static irqreturn_t sc8800g_irq_mdm2ap1(int irq, void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	if ((0 == mdm2ap1_state) || enable_bp_interrupt) {
		mdm2ap1_state = sc8800g_mdm2ap1_asserted(pdata);
		printk("[%s] mdm2ap1 %s\n", __func__,
			sc8800g_mdm2ap1_asserted(pdata) ? "asserted" :
			"de-asserted");

		if (mdv->ignore_mdm2ap1) {
			DBG("[%s] ignore mdm2ap1\n", __func__);
			goto out;
		}
		schedule_work(&uevent_update_work);
	}

out:
	return IRQ_HANDLED;
}

static irqreturn_t sc8800g_irq_mdm2ap2(int irq, void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	mdm2ap2_state = sc8800g_mdm2ap2_asserted(pdata);

	printk("[%s] mdm2ap2 %s\n", __func__,
		sc8800g_mdm2ap2_asserted(pdata) ? "asserted" : "de-asserted");

	if (mdv->ignore_mdm2ap2) {
		DBG("[%s] ignore mdm2ap2\n", __func__);
		goto out;
	}
	schedule_work(&uevent_update_work);

out:
	return IRQ_HANDLED;
}

static void sc8800g_enable_interrupt(struct sc8800g_mdm_dev *mdv)
{
	struct serial_sc8800g_platform_data *pdata;
	void *ret = NULL;

	pdata = mdv->pdata;
	mdv->alive_irq = gpio_to_irq(pdata->gpios[MDM_ALIVE].gpio);
	if (request_irq(mdv->alive_irq, sc8800g_irq_alive,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		"MODEM ALIVE", mdv)) {
		ERROR("[%s] request alive_irq failed!\n", __func__);
		ret = ERR_PTR(-EBUSY);
		BUG();
	}

	mdv->mdm2ap2_irq = gpio_to_irq(pdata->gpios[MDM_TO_AP2].gpio);
	if (request_irq(mdv->mdm2ap2_irq, sc8800g_irq_mdm2ap2,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		"MODEM TO AP2", mdv)) {
		ERROR("[%s] request mdm2ap2_irq failed!\n", __func__);
		ret = ERR_PTR(-EBUSY);
		BUG();
	}
#ifdef HAS_MDM_RESEND
	mdv->resend_irq = gpio_to_irq(pdata->gpios[MDM_RESEND].gpio);
	if (request_irq(mdv->resend_irq, sc8800g_irq_resend,
		IRQF_TRIGGER_RISING,
		"MODEM RESEND", mdv)) {
		ERROR("[%s] request resend_irq failed!\n", __func__);
		ret = ERR_PTR(-EBUSY);
		BUG();
	}
#endif
	enable_bp_interrupt = 1;
}

/*
 * interrupt handler for bp_rdy
 */
static irqreturn_t sc8800g_irq_rdy(int irq, void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	unsigned long flags;
#ifdef IPC_DEBUG
	mdm_irq_rdy++;
#endif
	VDBG("[%s]\n", __func__);

	spin_lock_irqsave(&mdv->lock, flags);

	/* checks related to power on/off */
	if (mdv->ignore_mdm_rdy) {
		DBG("[%s] ignore mdm_rdy\n", __func__);
		goto out;
	}

	/* checks related to system power management */
	if (mdv->suspending) {
		WARNING("[%s] sc8800g is suspending!\n", __func__);
		goto out;
	}

	/* checks related to open/close */
	if (mdv->tty_closed) {
		/* ignore mdm_rts if closed */
		WARNING("[%s] mdv->closed\n", __func__);
		goto out;
	}

	if (mdv->state->power_state != ACTIVE &&
	    mdv->state->power_state != PWRING) {
		WARNING("[%s] mdm_rdy interrupt received while modem han't been power on, state = %d\n",
			__func__, mdv->state->power_state);
		goto out;
	}

	VDBG("[%s] set FLAG_BP_RDY\n", __func__);
	set_bit(FLAG_BP_RDY, &mdv->flags);
	tasklet_hi_schedule(&mdv->tx_tasklet);

out:
	spin_unlock_irqrestore(&mdv->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t sc8800g_irq_rts(int irq, void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	unsigned long flags;
#ifdef IPC_DEBUG
	mdm_irq_rts++;
	do_gettimeofday(&mdm_rts_time);
#endif
	VDBG("[%s]\n", __func__);

	spin_lock_irqsave(&mdv->lock, flags);

	wake_lock_timeout(&mdv->wakeup_wake_lock, (HZ * 3 / 2));
	/* checks related to power on/off */
	if (mdv->ignore_mdm_rts) {
		DBG("[%s] ignore mdm_rts\n", __func__);
		goto out;
	}

	/* checks related to system power management */
	if (mdv->suspending) {
		/* ignore mdm_rts if suspending */
		WARNING("[%s] mdv->suspending\n", __func__);
		goto out;
	}

	if (mdv->suspended) {
		mdv->suspended = 0;
		INFO("[%s] remote wake up\n", __func__);
	}

	/* checks related to open/close */
	if (mdv->tty_closed) {
		/* ignore mdm_rts if closed */
		WARNING("[%s] mdv->closed\n", __func__);
		goto out;
	}

	if (mdv->tty_closing) {
		/* ignore mdm_rts if tty_closing */
		WARNING("[%s] mdv->tty_closing\n", __func__);
		goto out;
	}

	if (mdv->state->power_state != ACTIVE &&
	    mdv->state->power_state != PWRING) {
		WARNING("[%s] mdm_rts interrupt received while modem han't been power on, state = %d\n",
			__func__, mdv->state->power_state);
		goto out;
	}

/*
 *  need to allow mdm_rts at closed/closing, otherwise, we will see on next open
 *  TODO: ask modem to clarify
[   98.554908] sc8800g: [sc8800g_rx_check_header] packet length invalid!
[   98.556496] sc8800g:  7f 7e 55 aa 7f 7e 55 aa 68 0a 00 00 00 00 00 00
[   98.557393] sc8800g:  00 ef cd ab f9 0f ef 04 06 69 c6 71 12 9f 3a 47
[   98.558962] sc8800g:  77 fd 50 04 48 08 69 fc 04 7b 9e a7 4b c1 51 bd
[   98.559869] sc8800g:  1f 8c f6 e4 06 95 fd d3 7e 15 b0 41 65 73 54 4d
 */
	VDBG("[%s] set FLAG_BP_RTS\n", __func__);
	set_bit(FLAG_BP_RTS, &mdv->flags);
	tasklet_hi_schedule(&mdv->rx_tasklet);

out:
	spin_unlock_irqrestore(&mdv->lock, flags);
	return IRQ_HANDLED;
}

#ifdef HAS_MDM_RESEND
static irqreturn_t sc8800g_irq_resend(int irq, void *data)
{
	struct sc8800g_mdm_dev *mdv = data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	int ret;

	WARNING("[%s]\n", __func__);

	wake_lock_timeout(&mdv->wakeup_wake_lock, (HZ));

	if (mdv->state->power_state != ACTIVE) {
		WARNING("[%s] mdm_resend interrupt received while modem is not ACTIVE, state = %d\n",
			__func__, mdv->state->power_state);
		goto out;
	}

	VDBG("[%s] mdm_resend %s\n", __func__,
		sc8800g_mdm_resend_asserted(pdata) ? "asserted" : "de-asserted");

	/* TODO: trigger timer */
	if (sc8800g_mdm_resend_asserted(pdata)) {
		if (!timer_pending(&mdv->ap_rts_request_timer)) {
			ret = mod_timer(&mdv->ap_rts_request_timer,
				jiffies + AP_RTS_RETRY_PERIOD);
			VDBG("[%s] mod_timer(&mdv->ap_rts_request_timer) = %d\n", __func__, ret);
		}
	}

out:
	return IRQ_HANDLED;
}
#endif

static void sc8800g_bp_rdy_timer_expired(unsigned long data)
{
	struct sc8800g_mdm_dev *mdv = (struct sc8800g_mdm_dev *)data;
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;

	/* modem has been reset, ignore mdm_rdy timeout error */
	if (sc8800g_mdm_reset_asserted(pdata)) {
		WARNING("[%s] modem has been reset, ignore mdm_rdy timeout error\n",
				__func__);
		return;
	}

	if (CRASH == mdv->state->power_state) {
		WARNING("[%s] modem has crash, ignore mdm_rdy timeout error\n",
				__func__);
		return;
	}

	/*
	 * do not obtain mdv->lock since we will call del_timer_sync() on this
	 * timer.
	 */
	if (!sc8800g_mdm_rdy_asserted(pdata)) {
		ERROR("[%s] modem didn't acknowledge ap_rts in %d ms\n",
			__func__, BP_RDY_TIMEOUT);
		sc8800g_show_info(mdv);
#ifdef IPC_DEBUG
		ipc_debug_result();
#endif
		sc8800g_force_assert(mdv);
	}
	/* TODO: report to user space and cleanup */
}

static struct sc8800g_mdm_dev *sc8800g_dev_alloc(struct spi_device *spi)
{
	struct serial_sc8800g_platform_data *pdata;
	struct sc8800g_mdm_dev *mdv;
	void *ret;

	pdata = (struct serial_sc8800g_platform_data *) spi->dev.platform_data;

	mdv = kzalloc(sizeof(struct sc8800g_mdm_dev), GFP_KERNEL);
	if (!mdv) {
		ERROR("[%s] kzalloc mdv failed!\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	mdv->spi = spi;
	mdv->pdata = pdata;

	download_sendbuf = kzalloc(DOWNLOAD_SEND_BUFFER_SIZE, GFP_KERNEL);
	active_sendbuf = kzalloc(AVTIVE_SEND_BUFFER_SIZE, GFP_KERNEL);

	mdv->sendbuf = download_sendbuf;
	if (!mdv->sendbuf) {
		ERROR("[%s] kzalloc mdv->sendbuf failed!\n", __func__);
		ret = ERR_PTR(-ENOMEM);
		goto alloc_sendbuf_failed;
	}

	mdv->recvbuf = (unsigned char *)kzalloc(RECV_BUFFER_SIZE, GFP_KERNEL);
	if (!mdv->recvbuf) {
		ERROR("[%s] kzalloc mdv->recvbuf failed!\n", __func__);
		ret = ERR_PTR(-ENOMEM);
		goto alloc_recvbuf_failed;
	}

	mdv->rts_irq = gpio_to_irq(pdata->gpios[MDM_RTS].gpio);
	if (request_irq(mdv->rts_irq, sc8800g_irq_rts, IRQF_TRIGGER_FALLING,
			"MODEM RTS", mdv)) {
		ERROR("[%s] request rts_irq failed!\n", __func__);
		ret = ERR_PTR(-EBUSY);
		goto request_rts_irq_failed;
	}

	mdv->rdy_irq = gpio_to_irq(pdata->gpios[MDM_RDY].gpio);
	if (request_irq(mdv->rdy_irq, sc8800g_irq_rdy, IRQF_TRIGGER_FALLING,
			"MODEM RDY", mdv)) {
		ERROR("[%s] request rdy_irq failed!\n", __func__);
		ret = ERR_PTR(-EBUSY);
		goto request_rdy_irq_failed;
	}

	mdv->mdm2ap1_irq = gpio_to_irq(pdata->gpios[MDM_TO_AP1].gpio);
	if (request_irq(mdv->mdm2ap1_irq, sc8800g_irq_mdm2ap1,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		"MODEM TO AP1", mdv)) {
		ERROR("[%s] request mdm2ap1_irq failed!\n", __func__);
		ret = ERR_PTR(-EBUSY);
		goto request_mdm2ap1_irq_failed;
	}

	download_cirbuf = (unsigned char *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 3);
	avtive_cirbuf = (unsigned char *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, 1);
	mdv->tx_cirbuf.buf = download_cirbuf;
	if (!mdv->tx_cirbuf.buf) {
		ERROR("[%s] get_zeroed_page failed!\n", __func__);
		ret = ERR_PTR(-ENOMEM);
		goto alloc_tx_cirbuf_failed;
	}
	SC8800G_CIRCBUF_RESET(&mdv->tx_cirbuf);
	mdv->tx_data_len = 0;

	tasklet_init(&mdv->rx_tasklet, sc8800g_rx_tasklet, (unsigned long)mdv);
	tasklet_init(&mdv->tx_tasklet, sc8800g_tx_tasklet, (unsigned long)mdv);
	init_timer(&mdv->ap_rts_request_timer);
	mdv->ap_rts_request_timer.data = (unsigned long)mdv;
	mdv->ap_rts_request_timer.function = sc8800g_ap_rts_request;

	mdv->flags = 0;

	mdv->suspending = 0;
	mdv->suspended = 0;
	mdv->tty_closed = 1;
	mdv->tty_closing = 0;

	mdv->rx_look_for_header = 1;
	spin_lock_init(&mdv->lock);

	init_waitqueue_head(&mdv->continue_close);
	init_waitqueue_head(&mdv->continue_suspend);

	init_timer(&mdv->bp_rdy_timer);
	mdv->bp_rdy_timer.data = (unsigned long)mdv;
	mdv->bp_rdy_timer.function = sc8800g_bp_rdy_timer_expired;

	init_timer(&mdv->force_assert_timer);
	mdv->force_assert_timer.data = (unsigned long)mdv;
	mdv->force_assert_timer.function = sc8800g_force_assert_timer_expired;

	wake_lock_init(&mdv->wakeup_wake_lock,
			WAKE_LOCK_SUSPEND, "sc8800g_wake_lock");
	mdv->last_ap_rts_jiffies = 0;
	return mdv;

alloc_tx_cirbuf_failed:
	free_irq(mdv->mdm2ap1_irq, mdv);
request_mdm2ap1_irq_failed:
	free_irq(mdv->rdy_irq, mdv);
request_rdy_irq_failed:
	free_irq(mdv->rts_irq, mdv);
request_rts_irq_failed:
	kfree(mdv->recvbuf);
alloc_recvbuf_failed:
	kfree(download_sendbuf);
	kfree(active_sendbuf);
alloc_sendbuf_failed:
	kfree(mdv);
	return ret;
}

static void sc8800g_dev_free(struct sc8800g_mdm_dev *mdv)
{
	INFO("[%s]\n", __func__);

	if (mdv) {
		wake_lock_destroy(&mdv->wakeup_wake_lock);

		if (timer_pending(&mdv->bp_rdy_timer))
			del_timer_sync(&mdv->bp_rdy_timer);

		tasklet_kill(&mdv->rx_tasklet);
		tasklet_kill(&mdv->tx_tasklet);
		if (timer_pending(&mdv->ap_rts_request_timer))
			del_timer_sync(&mdv->ap_rts_request_timer);
		if (timer_pending(&mdv->force_assert_timer))
			del_timer_sync(&mdv->force_assert_timer);

		free_irq(mdv->rts_irq, mdv);
		free_irq(mdv->rdy_irq, mdv);
		free_irq(mdv->alive_irq, mdv);
		free_irq(mdv->mdm2ap1_irq, mdv);
		free_irq(mdv->mdm2ap2_irq, mdv);
#ifdef HAS_MDM_RESEND
		free_irq(mdv->resend_irq, mdv);
#endif
		if (mdv->tx_cirbuf.buf) {
			free_page((unsigned long)download_cirbuf);
			free_page((unsigned long)avtive_cirbuf);
			mdv->tx_cirbuf.buf = NULL;
		}
		kfree(mdv->recvbuf);
		kfree(download_sendbuf);
		kfree(active_sendbuf);
		kfree(mdv);
	}
}

static int sc8800g_mdm_active(struct device *dev, int pwr_onoff)
{
	struct serial_sc8800g_platform_data *pdata = dev->platform_data;
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;
	int ret = 0;

	INFO("[%s]\n", __func__);

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	if (!mdv)
		BUG();

	mdv->ignore_mdm_alive = 0;
	mdv->ignore_mdm2ap2 = 0;
	mdv->ignore_mdm_rdy = 0;
	mdv->ignore_mdm_rts = 0;

	if (pwr_onoff) {
		if (sc8800g_mdm_power_asserted(pdata)) {
			WARNING("[%s] modem has been powered on already.\n", __func__);
			goto out;
		}
		INFO("[%s] assert mdm_power\n", __func__);
		sc8800g_assert_mdm_power(pdata);
		uarta_pg_init();
		spi1_pg_init();
		control_pg_init();
		if (sc8800g_gpio_init(pdata) < 0) {
			ERROR("[%s] sc8800g_gpio_init failed\n", __func__);
			ret = -EBUSY;
			goto out;
		}
		INFO("[%s] sc8800g_gpio_init done\n", __func__);
		bp_is_powered = 1;
		sc8800g_enable_interrupt(mdv);
	} else {
		if (!sc8800g_mdm_reset_asserted(pdata)) {
			WARNING("[%s] modem reset has been released already.\n", __func__);
			goto out;
		}
		INFO("[%s] deassert mdm_reset\n", __func__);
		sc8800g_deassert_mdm_reset(pdata);
	}

out:
	mutex_unlock(&dev_mutex);
	return ret;
}

/* unconditionally power off the modem, could be called during transfer */
static int sc8800g_mdm_deactive(struct device *dev, int pwr_onoff)
{
	struct serial_sc8800g_platform_data *pdata = dev->platform_data;
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;

	INFO("[%s]\n", __func__);

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	BUG_ON(!mdv);

	if (pwr_onoff) {
		if (!sc8800g_mdm_power_asserted(pdata)) {
			WARNING("[%s] modem has been powered off already.\n", __func__);
			goto out;
		}
	} else {
		if (sc8800g_mdm_reset_asserted(pdata)) {
			WARNING("[%s] modem has been set reset already.\n", __func__);
			goto out;
		}
	}

	/* stop outstanding asynchronous works */
	if (timer_pending(&mdv->bp_rdy_timer)) {
		del_timer(&mdv->bp_rdy_timer);
	}
	if (timer_pending(&mdv->ap_rts_request_timer)) {
		del_timer(&mdv->ap_rts_request_timer);
	}
	if (timer_pending(&mdv->force_assert_timer)) {
		del_timer(&mdv->force_assert_timer);
	}

	mdv->flags = 0;
	mdv->rx_look_for_header = 1;
#ifdef HAS_AP_RESEND
	mdv->rx_retry = 0;
#endif
	mdv->last_ap_rts_jiffies = 0;

	mdv->ignore_mdm_rts = 1;
	mdv->ignore_mdm_rdy = 1;

#ifdef HAS_AP_RESEND
	sc8800g_deassert_ap_resend(pdata);
#endif
	sc8800g_deassert_ap_rts(pdata);
	sc8800g_deassert_ap_rdy(pdata);
	sc8800g_deassert_ap2mdm2(pdata);

	if (mdv->suspending) {
		INFO("[%s] wake_up suspending handler\n", __func__);
		wake_up(&mdv->continue_suspend);
	}
	if (mdv->tty_closing) {
		INFO("[%s] wake_up tty_closing handler\n", __func__);
		wake_up(&mdv->continue_close);
	}

	if (pwr_onoff) {
		/* power off modem */
		sc8800g_deassert_mdm_power(pdata);
		bp_is_powered = 0;
		sc8800g_gpio_deinit(pdata);
		control_pg_deinit();
		spi1_pg_deinit();
		uarta_pg_deinit();
	} else {
		/* assert modem reset */
		sc8800g_assert_mdm_reset(pdata);
	}

out:
	mutex_unlock(&dev_mutex);
	return 0;
}

static int sc8800g_force_assert(struct sc8800g_mdm_dev *mdv)
{
	struct spi_device *spi = mdv->spi;
	struct serial_sc8800g_platform_data *pdata = spi->dev.platform_data;
	int ret;

	WARNING("[%s] pid = %d, comm = %s\n", __func__, current->pid, current->comm);

	if (sc8800g_ap2mdm2_asserted(pdata)) {
		WARNING("[%s] sc8800g ap2mdm2 already asserted\n", __func__);
		goto out;
	}

	sc8800g_assert_ap2mdm2(pdata);
	ret = mod_timer(&mdv->force_assert_timer, jiffies + msecs_to_jiffies(FORCE_ASSERT_TIMEOUT));
	VDBG("[%s] mod_timer(&mdv->force_assert_timer) = %d\n", __func__, ret);

out:
	return 1;
}

static void sc8800g_force_assert_timer_expired(unsigned long data)
{
	struct sc8800g_mdm_dev *mdv = (struct sc8800g_mdm_dev *)data;
	struct spi_device *spi = mdv->spi;
	int ret;

	INFO("[%s]\n", __func__);

	/* Timer expires means modem does not deal with ap2mdm2 gpio, modem should be reset */
	ERROR("[%s] modem didn't acknowledge ap2mdm2 in %d ms\n", __func__, FORCE_ASSERT_TIMEOUT);
	ret = sc8800g_mdm_deactive(&spi->dev, 0);
}

static ssize_t sc8800g_reset_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct serial_sc8800g_platform_data *pdata = dev->platform_data;
	int reset;

	INFO("[%s]\n", __func__);

	/* check for platform data */
	if (!pdata)
		return -EINVAL;

	if (sc8800g_mdm_reset_asserted(pdata))
		reset = 1;
	else
		reset = 0;

	return sprintf(buf, "%d\n", reset);
}

static ssize_t sc8800g_reset_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int reset;
	int ret;

	INFO("[%s]\n", __func__);

	if (sscanf(buf, "%d", &reset) != 1)
		return -EINVAL;

	if (reset == 1)
		ret = sc8800g_mdm_deactive(dev, 0);
	else if (reset == 0)
		ret = sc8800g_mdm_active(dev, 0);
	else
		return -EINVAL;

	if (ret)
		return ret;
	else
		return count;
}

static DEVICE_ATTR(reset, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
	sc8800g_reset_show, sc8800g_reset_store);

static ssize_t sc8800g_pwr_onoff_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct serial_sc8800g_platform_data *pdata = dev->platform_data;
	int power_onoff;

	INFO("[%s]\n", __func__);

	/* check for platform data */
	if (!pdata)
		return -EINVAL;

	if (sc8800g_mdm_power_asserted(pdata))
		power_onoff = 1;
	else
		power_onoff = 0;

	return sprintf(buf, "%d\n", power_onoff);
}

static ssize_t sc8800g_pwr_onoff_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int power_onoff;
	int ret;

	INFO("[%s]\n", __func__);

	if (sscanf(buf, "%d", &power_onoff) != 1)
		return -EINVAL;

	if (power_onoff == 0)
		ret = sc8800g_mdm_deactive(dev, 1);
	else if (power_onoff == 1)
		ret = sc8800g_mdm_active(dev, 1);
	else
		return -EINVAL;

	if (ret)
		return ret;
	else
		return count;
}

static DEVICE_ATTR(pwr_onoff, S_IRUSR | S_IWUSR | S_IRGRP,
	sc8800g_pwr_onoff_show, sc8800g_pwr_onoff_store);

static ssize_t sc8800g_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;

	INFO("[%s]\n", __func__);

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	if (!mdv)
		BUG();

	return sprintf(buf, "%d\n", mdv->state->power_state);
}

static ssize_t sc8800g_state_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;
	int state;

	INFO("[%s]\n", __func__);

	if (sscanf(buf, "%d", &state) != 1)
		return -EINVAL;

	if (state < 0 || state > PWR_PRE)
		return -EINVAL;

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	if (!mdv)
		BUG();

	if (state != mdv->state->power_state) {
		mdv->state = &sc8800g_mdm_pwr_state[state];
		kobject_uevent_env(&dev->kobj,
				KOBJ_CHANGE, mdv->state->power_state_env);
	}

	return count;
}

static DEVICE_ATTR(state, S_IRUSR | S_IWUSR | S_IRGRP,
	sc8800g_state_show, sc8800g_state_store);

static ssize_t sc8800g_usb_switch_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct serial_sc8800g_platform_data *pdata = dev->platform_data;
	int usb_switch;

	INFO("[%s]\n", __func__);

	/* check for platform data */
	if (!pdata)
		return -EINVAL;

	if (sc8800g_usb_switch_asserted(pdata))
		usb_switch = 1;
	else
		usb_switch = 0;

	return sprintf(buf, "%d\n", usb_switch);
}

static ssize_t sc8800g_usb_switch_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct serial_sc8800g_platform_data *pdata = dev->platform_data;
	int usb_switch, usb_switch_last_state;

	INFO("[%s]\n", __func__);

	if (sscanf(buf, "%d", &usb_switch) != 1)
		return -EINVAL;

	usb_switch_last_state = sc8800g_usb_switch_asserted(pdata);

	if (usb_switch == 1)
		sc8800g_assert_usb_switch(pdata);
	else {
		usb_switch = 0;
		sc8800g_deassert_usb_switch(pdata);
	}

	if (usb_switch_last_state != usb_switch) {
		kobject_uevent_env(&dev->kobj,
				KOBJ_CHANGE, sc8800g_usb_switch_state[usb_switch].usb_switch_state_env);
	}
	return count;
}

static DEVICE_ATTR(usb_switch, S_IRUSR | S_IWUSR | S_IRGRP,
	sc8800g_usb_switch_show, sc8800g_usb_switch_store);

static ssize_t sc8800g_offline_log_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;

	INFO("[%s]\n", __func__);

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	if (!mdv)
		BUG();

	return sprintf(buf, "%d\n", mdv->log_state->state);

}

static ssize_t sc8800g_offline_log_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;
	int state;

	INFO("[%s]\n", __func__);

	if (sscanf(buf, "%d", &state) != 1)
		return -EINVAL;

	if (state < OFFLINE_LOG_OFF || state > OFFLINE_LOG_ON)
		return -EINVAL;

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	if (!mdv)
		BUG();

	if (state != mdv->log_state->state) {
		mdv->log_state = &sc8800g_mdm_offline_log_state[state];
		kobject_uevent_env(&dev->kobj,
				KOBJ_CHANGE, mdv->log_state->offline_log_state_env);
	}

	return count;
}

static DEVICE_ATTR(offline_log, S_IRUSR | S_IWUSR | S_IRGRP,
	sc8800g_offline_log_show, sc8800g_offline_log_store);

static ssize_t sc8800g_assert_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;
	int force_assert;
	int ret = 0;

	INFO("[%s]\n", __func__);

	if (sscanf(buf, "%d", &force_assert) != 1)
		return -EINVAL;

	if (force_assert) {
		if (!sc8800g_tty_driver) {
			/* couldn't happen */
			ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
			BUG();
		}

		tty_dev = sc8800g_tty_driver->driver_state;
		mdv = dev_get_drvdata(tty_dev);
		BUG_ON(!mdv);

		ret = sc8800g_force_assert(mdv);
	}

	if (ret)
		return ret;
	else
		return count;
}

static DEVICE_ATTR(assert, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
	NULL, sc8800g_assert_store);

static int sc8800g_create_sysfs_attrs(struct device *dev)
{
	int err;

	INFO("[%s]", __func__);
	err = device_create_file(dev, &dev_attr_reset);
	if (err)
		goto create_reset_devfs_failed;
	err = device_create_file(dev, &dev_attr_pwr_onoff);
	if (err)
		goto create_pwr_onoff_devfs_failed;
	err = device_create_file(dev, &dev_attr_state);
	if (err)
		goto create_state_devfs_failed;
	err = device_create_file(dev, &dev_attr_usb_switch);
	if (err)
		goto create_usb_switch_devfs_failed;
	err = device_create_file(dev, &dev_attr_offline_log);
	if (err)
		goto create_offline_log_devfs_failed;
	err = device_create_file(dev, &dev_attr_assert);
	if (err)
		goto create_assert_devfs_failed;
	return err;

create_assert_devfs_failed:
	device_remove_file(dev, &dev_attr_offline_log);
create_offline_log_devfs_failed:
	device_remove_file(dev, &dev_attr_usb_switch);
create_usb_switch_devfs_failed:
	device_remove_file(dev, &dev_attr_state);
create_state_devfs_failed:
	device_remove_file(dev, &dev_attr_pwr_onoff);
create_pwr_onoff_devfs_failed:
	device_remove_file(dev, &dev_attr_reset);
create_reset_devfs_failed:
	return err;
}

static void sc8800g_remove_sysfs_attrs(struct device *dev)
{

	INFO("[%s]", __func__);
	device_remove_file(dev, &dev_attr_reset);
	device_remove_file(dev, &dev_attr_state);
	device_remove_file(dev, &dev_attr_pwr_onoff);
	device_remove_file(dev, &dev_attr_usb_switch);
	device_remove_file(dev, &dev_attr_offline_log);
	device_remove_file(dev, &dev_attr_assert);
}

static void bp_special_pg_init(struct serial_sc8800g_platform_data *pdata)
{
	struct gpio *gpios = pdata->gpios;
	/*bp_poweron*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_BP_PWRON, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_BP_PWRON, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_BP_PWRON, TEGRA_PIN_OUTPUT);
	/*mdm2ap1 */
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_TO_AP1,
				    TEGRA_PUPD_NORMAL);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_TO_AP1,
				  TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_TO_AP1, TEGRA_PIN_INPUT);
	/*request gpio */
	gpio_request_one(gpios[BP_PWRON].gpio, gpios[BP_PWRON].flags,
			 gpios[BP_PWRON].label);
	gpio_request_one(gpios[MDM_TO_AP1].gpio, gpios[MDM_TO_AP1].flags,
			 gpios[MDM_TO_AP1].label);

	/* request usb_switch gpio */
	gpio_request_one(gpios[USB_SWITCH].gpio, gpios[USB_SWITCH].flags, gpios[USB_SWITCH].label);
}

static void control_pg_init(void)
{
	/*ap_rts*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_AP_RTS, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_AP_RTS, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_AP_RTS, TEGRA_PIN_OUTPUT);
	/*ap_rdy*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_AP_RDY, TEGRA_PUPD_PULL_UP);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_AP_RDY, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_AP_RDY, TEGRA_PIN_OUTPUT);
	/*ap_resend*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_AP_RESEND, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_AP_RESEND, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_AP_RESEND, TEGRA_PIN_OUTPUT);
	/*ap2mdm1*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_AP_TO_MDM1, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_AP_TO_MDM1, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_AP_TO_MDM1, TEGRA_PIN_OUTPUT);
	/*ap2mdm2*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_AP_TO_MDM2, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_AP_TO_MDM2, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_AP_TO_MDM2, TEGRA_PIN_OUTPUT);
	/*mdm_extrstn*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_EXTRSTN, TEGRA_PUPD_PULL_UP);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_EXTRSTN, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_EXTRSTN, TEGRA_PIN_OUTPUT);
	/*mdm_power*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_PWRON, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_PWRON, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_PWRON, TEGRA_PIN_OUTPUT);
	/*mdm_rts*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_RTS, TEGRA_PUPD_PULL_UP);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_RTS, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_RTS, TEGRA_PIN_INPUT);
	/*mdm_rdy*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_RDY, TEGRA_PUPD_PULL_UP);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_RDY, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_RDY, TEGRA_PIN_INPUT);
	/*mdm_resend*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_RESEND, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_RESEND, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_RESEND, TEGRA_PIN_INPUT);
	/*mdm2ap2*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_TO_AP2, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_TO_AP2, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_TO_AP2, TEGRA_PIN_INPUT);
	/*mdm_alive*/
	tegra_pinmux_set_pullupdown(SC8800G_PINGROUP_MDM_ALIVE, TEGRA_PUPD_PULL_DOWN);
	tegra_pinmux_set_tristate(SC8800G_PINGROUP_MDM_ALIVE, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SC8800G_PINGROUP_MDM_ALIVE, TEGRA_PIN_INPUT);
}

static void control_pg_deinit(void)
{
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(control_pin_group); i++) {
		tegra_pinmux_set_pullupdown(control_pin_group[i],
					    TEGRA_PUPD_PULL_DOWN);
		tegra_pinmux_set_tristate(control_pin_group[i],
					  TEGRA_TRI_TRISTATE);
		tegra_pinmux_set_io(control_pin_group[i], TEGRA_PIN_OUTPUT);
	}
}

static void uarta_pg_init(void)
{
	/*uarta pu0*/
	tegra_pinmux_set_pullupdown(UARTA_GPIO_PU0, TEGRA_PUPD_NORMAL);
	tegra_pinmux_set_tristate(UARTA_GPIO_PU0, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(UARTA_GPIO_PU0, TEGRA_PIN_OUTPUT);
	/*uarta pu1*/
	tegra_pinmux_set_pullupdown(UARTA_GPIO_PU1, TEGRA_PUPD_NORMAL);
	tegra_pinmux_set_tristate(UARTA_GPIO_PU1, TEGRA_TRI_TRISTATE);
	tegra_pinmux_set_io(UARTA_GPIO_PU1, TEGRA_PIN_INPUT);
	/*uarta pu2*/
	tegra_pinmux_set_pullupdown(UARTA_GPIO_PU2, TEGRA_PUPD_NORMAL);
	tegra_pinmux_set_tristate(UARTA_GPIO_PU2, TEGRA_TRI_TRISTATE);
	tegra_pinmux_set_io(UARTA_GPIO_PU2, TEGRA_PIN_INPUT);
	/*uarta pu3*/
	tegra_pinmux_set_pullupdown(UARTA_GPIO_PU3, TEGRA_PUPD_NORMAL);
	tegra_pinmux_set_tristate(UARTA_GPIO_PU3, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(UARTA_GPIO_PU3, TEGRA_PIN_OUTPUT);
}

static void uarta_pg_deinit(void)
{
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(uarta_pin_group); i++) {
		tegra_pinmux_set_pullupdown(uarta_pin_group[i], TEGRA_PUPD_PULL_DOWN);
		tegra_pinmux_set_tristate(uarta_pin_group[i], TEGRA_TRI_TRISTATE);
		tegra_pinmux_set_io(uarta_pin_group[i], TEGRA_PIN_OUTPUT);
	}
}
static void spi1_pg_init(void)
{
	/*spi1 ulpi clock*/
	tegra_pinmux_set_pullupdown(SPI1_ULPI_CLK, TEGRA_PUPD_PULL_UP);
	tegra_pinmux_set_tristate(SPI1_ULPI_CLK, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SPI1_ULPI_CLK, TEGRA_PIN_INPUT);
	/*spi1 ulpi dir*/
	tegra_pinmux_set_pullupdown(SPI1_ULPI_DIR, TEGRA_PUPD_PULL_UP);
	tegra_pinmux_set_tristate(SPI1_ULPI_DIR, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SPI1_ULPI_DIR, TEGRA_PIN_INPUT);
	/*spi1 ulpi nxt*/
	tegra_pinmux_set_pullupdown(SPI1_ULPI_NXT, TEGRA_PUPD_NORMAL);
	tegra_pinmux_set_tristate(SPI1_ULPI_NXT, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SPI1_ULPI_NXT, TEGRA_PIN_INPUT);
	/*spi1 ulpi stp*/
	tegra_pinmux_set_pullupdown(SPI1_ULPI_STP, TEGRA_PUPD_NORMAL);
	tegra_pinmux_set_tristate(SPI1_ULPI_STP, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_io(SPI1_ULPI_STP, TEGRA_PIN_INPUT);
}
static void spi1_pg_deinit(void)
{
	int i = 0;
	for (i = 0; i < ARRAY_SIZE(spi1_pin_group); i++) {
		tegra_pinmux_set_pullupdown(spi1_pin_group[i], TEGRA_PUPD_PULL_DOWN);
		tegra_pinmux_set_tristate(spi1_pin_group[i], TEGRA_TRI_TRISTATE);
		tegra_pinmux_set_io(spi1_pin_group[i], TEGRA_PIN_OUTPUT);
	}
}

static int sc8800g_gpio_init(struct serial_sc8800g_platform_data *pdata)
{
	struct gpio *gpios = pdata->gpios;
	int ret;
	int i;

	/* gpio array request */

	for (i = 0; i < SC8800G_GPIO_MAX; i++) {
		if (i == USB_SWITCH) {
			/* usb_switch gpio is already requested */
			continue;
		}
		if ((BP_PWRON != i) && (MDM_TO_AP1 != i)) {
			ret = gpio_request_one(gpios[i].gpio, gpios[i].flags, gpios[i].label);
			if (ret) {
				ERROR("[%s] request gpio array failed, ret = %d\n", __func__, ret);
				return ret;
			}
		}
	}

	/* export gpios */
	for (i = 0; i < SC8800G_GPIO_MAX; i++) {
#ifndef CONFIG_ARCH_TEGRA_11x_SOC
		tegra_gpio_enable(gpios[i].gpio);
#endif
		/* export gpios */
		ret = gpio_export(gpios[i].gpio, false);
		if (ret) {
			ERROR("[%s] export gpio failed for %s\n", __func__, gpios[i].label);
			return ret;
		}
	}
	return 0;
}

static void sc8800g_gpio_deinit(struct serial_sc8800g_platform_data *pdata)
{
	struct gpio *gpios = pdata->gpios;
	int i;

	for (i = 0; i < SC8800G_GPIO_MAX; i++) {
		/* unexport gpios */
		if (i == USB_SWITCH) {
			/* usb_switch gpio should always exists */
			continue;
		}
		gpio_unexport(gpios[i].gpio);
		gpio_free(gpios[i].gpio);
	}
}

static void reset_avtive_send_buffer(struct sc8800g_mdm_dev *mdv)
{
	if (PAGE_SIZE * 2 == SC8800G_TX_BUFSIZE)
		return;
	SC8800G_TX_BUFSIZE = PAGE_SIZE * 2;
	if (mdv->sendbuf) {
		memset(active_sendbuf, 0, AVTIVE_SEND_BUFFER_SIZE);
		mdv->sendbuf = active_sendbuf;
		if (!mdv->sendbuf) {
			ERROR("[%s] kzalloc mdv->sendbuf failed!\n", __func__);
			BUG();
		}
	}
	if (mdv->tx_cirbuf.buf) {
		memset(avtive_cirbuf, 0, PAGE_SIZE * 2);
		mdv->tx_cirbuf.buf = avtive_cirbuf;
		if (!mdv->tx_cirbuf.buf) {
			ERROR("[%s] get_zeroed_page failed!\n", __func__);
			BUG();
		}
		SC8800G_CIRCBUF_RESET(&mdv->tx_cirbuf);
	}
}

static void reset_download_send_buffer(struct sc8800g_mdm_dev *mdv)
{
	if (PAGE_SIZE * 8 == SC8800G_TX_BUFSIZE)
		return;
	SC8800G_TX_BUFSIZE = PAGE_SIZE * 8;
	if (mdv->sendbuf) {
		memset(download_sendbuf, 0, DOWNLOAD_SEND_BUFFER_SIZE);
		mdv->sendbuf = download_sendbuf;
		if (!mdv->sendbuf) {
			ERROR("[%s] kzalloc mdv->sendbuf failed!\n", __func__);
			BUG();
		}
	}
	if (mdv->tx_cirbuf.buf) {
		memset(download_cirbuf, 0, PAGE_SIZE * 8);
		mdv->tx_cirbuf.buf = download_cirbuf;
		if (!mdv->tx_cirbuf.buf) {
			ERROR("[%s] get_zeroed_page failed!\n", __func__);
			BUG();
		}
		SC8800G_CIRCBUF_RESET(&mdv->tx_cirbuf);
	}
}

static int sc8800g_tty_register(void)
{
	int ret;
	struct device *tty_dev;

	INFO("[%s]", __func__);

	mutex_lock(&dev_mutex);

	if (sc8800g_tty_driver) {
		WARNING("[%s] sc8800g_tty_driver has been allocated\n", __func__);
		mutex_unlock(&dev_mutex);
		return 0;
	}

	/* support only one modem */
	sc8800g_tty_driver = alloc_tty_driver(1);
	if (!sc8800g_tty_driver) {
		ERROR("[%s] alloc_tty_driver failed\n", __func__);
		ret = -ENOMEM;
		goto alloc_tty_failed;
	}

	sc8800g_tty_driver->owner = THIS_MODULE;
	sc8800g_tty_driver->driver_name = "sc8800g_tty_driver";
	sc8800g_tty_driver->name = "ttySC";
	sc8800g_tty_driver->major = 0;
	sc8800g_tty_driver->minor_start = 0;
	sc8800g_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	sc8800g_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	sc8800g_tty_driver->init_termios = tty_std_termios;
	sc8800g_tty_driver->init_termios.c_iflag = 0;
	sc8800g_tty_driver->init_termios.c_oflag = 0;
	sc8800g_tty_driver->init_termios.c_lflag = 0;
	sc8800g_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS |
					TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;

	tty_set_operations(sc8800g_tty_driver, &sc8800g_tty_ops);
	INFO("[%s] tty_set_operations done\n", __func__);

	if ((ret = tty_register_driver(sc8800g_tty_driver)) != 0) {
		ERROR("[%s] tty_register_driver failed\n", __func__);
		goto tty_register_driver_failed;
	}
	INFO("[%s] tty_register_driver done\n", __func__);

	tty_dev = tty_register_device(sc8800g_tty_driver, 0, NULL);
	if (IS_ERR(tty_dev)) {
		ERROR("[%s] tty_register_device failed\n", __func__);
		ret = PTR_ERR(tty_dev);
		goto tty_register_device_failed;
	}
	sc8800g_tty_driver->driver_state = tty_dev;
	INFO("[%s] tty_register_device done\n", __func__);

	dev_set_drvdata(tty_dev, NULL);
	INFO("[%s] tty_registered\n", __func__);

	mutex_unlock(&dev_mutex);
	return 0;

tty_register_device_failed:
	tty_unregister_driver(sc8800g_tty_driver);
tty_register_driver_failed:
	put_tty_driver(sc8800g_tty_driver);
alloc_tty_failed:
	sc8800g_tty_driver = NULL;
	mutex_unlock(&dev_mutex);
	return ret;
}

static void sc8800g_tty_unregister(void)
{
	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		WARNING("[%s] sc8800g_tty_driver hasn't been allocated\n", __func__);
		goto out;
	}

	tty_unregister_device(sc8800g_tty_driver, 0);
	tty_unregister_driver(sc8800g_tty_driver);
	put_tty_driver(sc8800g_tty_driver);
	sc8800g_tty_driver = NULL;

out:
	mutex_unlock(&dev_mutex);
	return;
}

static int __devinit sc8800g_probe(struct spi_device *spi)
{
	struct serial_sc8800g_platform_data *pdata;
	struct sc8800g_mdm_dev *mdv;
	struct device *tty_dev;
	int ret;

	pdata = (struct serial_sc8800g_platform_data *)spi->dev.platform_data;
	if (!pdata) {
		ERROR("[%s] missing platform data\n", __func__);
		return -ENODEV;
	}
	INFO("[%s] got serial_sc8800g_platform_data\n", __func__);

	spi->bits_per_word = SC8800G_TRANSFER_BITS_PER_WORD;
	if ((ret = spi_setup(spi)) < 0) {
		ERROR("[%s] spi_setup failed\n", __func__);
		/* propagate error code */
		return ret;
	}
	INFO("[%s] spi_setup done\n", __func__);
	/*Special init for bp_poweron and mdm2ap1*/
	bp_special_pg_init(pdata);

	if ((ret = sc8800g_tty_register()) < 0) {
		ERROR("[%s] sc8800g_tty_register failed\n", __func__);
		return ret;
	}
	INFO("[%s] sc8800g_tty_register done\n", __func__);

	mdv = sc8800g_dev_alloc(spi);
	if (IS_ERR(mdv)) {
		ERROR("[%s] sc8800g_dev_alloc failed\n", __func__);
		ret = PTR_ERR(mdv);
		goto mdv_alloc_failed;
	}
	INFO("[%s] sc8800g_dev_alloc done\n", __func__);

	tty_dev = sc8800g_tty_driver->driver_state;
	/* install modem device context */
	dev_set_drvdata(tty_dev, mdv);

	if ((ret = sc8800g_create_sysfs_attrs(&spi->dev)) != 0) {
		ERROR("[%s] sc8800g_create_sysfs_attrs failed\n", __func__);
		goto creat_sysfs_failed;
	}
	INFO("[%s] sc8800g_create_sysfs_attrs done\n", __func__);

	/* no need to check transfer progress since mdm_power de-asserted */
#ifdef HAS_AP_RESEND
	sc8800g_deassert_ap_resend(pdata);
#endif
	sc8800g_deassert_ap_rts(pdata);
	sc8800g_deassert_ap_rdy(pdata);
	sc8800g_deassert_ap2mdm2(pdata);

	mdv->ignore_mdm_alive = 0;
	mdv->ignore_mdm2ap1 = 0;
	mdv->ignore_mdm2ap2 = 0;
	mdv->ignore_mdm_rdy = 0;
	mdv->ignore_mdm_rts = 0;
	mdv->state = &sc8800g_mdm_pwr_state[PWROFF];
	mdv->log_state = &sc8800g_mdm_offline_log_state[OFFLINE_LOG_OFF];

	return 0;

creat_sysfs_failed:
	sc8800g_dev_free(mdv);
mdv_alloc_failed:
	sc8800g_tty_unregister();
	return ret;
}

static int sc8800g_remove(struct spi_device *spi)
{
	struct serial_sc8800g_platform_data *pdata;
	struct device *tty_dev;
	struct sc8800g_mdm_dev *mdv;

	INFO("[%s]\n", __func__);

	pdata = (struct serial_sc8800g_platform_data *)spi->dev.platform_data;
	if (!pdata) {
		ERROR("[%s] missing platform data\n", __func__);
		return -ENODEV;
	}

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);

	sc8800g_mdm_deactive(&spi->dev, 1);
	sc8800g_gpio_deinit(pdata);
	sc8800g_dev_free(mdv);
	sc8800g_remove_sysfs_attrs(&spi->dev);
	sc8800g_tty_unregister();

	return 0;
}

#ifdef CONFIG_PM
static int sc8800g_suspend(struct spi_device *spi, pm_message_t state)
{
	struct sc8800g_mdm_dev *mdv;
	int ret;
	unsigned long flags;
	struct serial_sc8800g_platform_data *pdata;
	struct device *tty_dev;

	INFO("[%s]\n", __func__);

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);
	pdata = (struct serial_sc8800g_platform_data *)spi->dev.platform_data;

	/* disable the interrupt of mdm_rts */
	disable_irq(mdv->rts_irq);

	/* disable the interrupt of mdm_rdy */
	disable_irq(mdv->rdy_irq);

	spin_lock_irqsave(&mdv->lock, flags);
	mdv->suspending = 1;
	if (!sc8800g_transfer_in_progress_locked(mdv)) {
		spin_unlock_irqrestore(&mdv->lock, flags);
		goto out;
	}
	/* either rx or tx is in place */
	spin_unlock_irqrestore(&mdv->lock, flags);

	INFO("[%s] wait for current transfer...\n", __func__);
	ret = wait_event_timeout(mdv->continue_suspend,
		(!sc8800g_transfer_in_progress(mdv)), SUSPEND_TIMEOUT);
	if (!ret) {
		ERROR("[%s] suspend timeout\n", __func__);
		BUG(); /* shouldn't happened */
	}
	INFO("[%s] transfer completed, continue suspending\n", __func__);

out:
	if (unlikely(sc8800g_ap_rdy_asserted(pdata))) {
		ERROR("[%s] ap_rdy asserted?", __func__);
		BUG(); /* shouldn't happend */
		sc8800g_deassert_ap_rdy(pdata);
	}
#ifdef HAS_AP_RESEND
	if (unlikely(sc8800g_ap_resend_asserted(pdata))) {
		ERROR("[%s] ap_resend asserted?", __func__);
		BUG(); /* shouldn't happend */
		sc8800g_deassert_ap_resend(pdata);
	}
#endif
	mdv->suspending = 0;
	mdv->suspended = 1;
	if (bp_is_powered) {
		ret = enable_irq_wake(mdv->rts_irq);
		if (ret < 0)
			ERROR("[%s] enable_irq_wake rts_irq failed %d\n", __func__, ret);

		if (sc8800g_mdm_alive_asserted(pdata))
			irq_set_irq_type(mdv->alive_irq, IRQF_TRIGGER_FALLING);
		else
			irq_set_irq_type(mdv->alive_irq, IRQF_TRIGGER_RISING);
		ret = enable_irq_wake(mdv->alive_irq);
		if (ret < 0)
			ERROR("[%s] enable_irq_wake alive_irq failed %d\n", __func__, ret);

		ret = enable_irq_wake(mdv->mdm2ap1_irq);
		if (ret < 0)
			ERROR("[%s] enable_irq_wake mdm2ap1_irq failed %d\n", __func__, ret);

		ret = enable_irq_wake(mdv->rdy_irq);
		if (ret < 0)
			ERROR("[%s] enable_irq_wake rdy_irq failed %d\n", __func__, ret);

	}

	mutex_unlock(&dev_mutex);
	return 0;
}

static int sc8800g_resume(struct spi_device *spi)
{
	struct sc8800g_mdm_dev *mdv;
	int ret;
	unsigned long flags;
	struct device *tty_dev;

	INFO("[%s]\n", __func__);

	mutex_lock(&dev_mutex);

	if (!sc8800g_tty_driver) {
		/* couldn't happen */
		ERROR("[%s] sc8800g_tty_driver doesn't exist\n", __func__);
		BUG();
	}

	tty_dev = sc8800g_tty_driver->driver_state;
	mdv = dev_get_drvdata(tty_dev);

	spin_lock_irqsave(&mdv->lock, flags);
	mdv->suspended = 0;
	if (bp_is_powered) {
		ret = disable_irq_wake(mdv->rts_irq);
		if (ret < 0)
			ERROR("[%s] disable_irq_wake rts_irq failed %d\n", __func__, ret);

		ret = disable_irq_wake(mdv->alive_irq);
		if (ret < 0)
			ERROR("[%s] disable_irq_wake alive_irq failed %d\n", __func__, ret);

		irq_set_irq_type(mdv->alive_irq, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING);

		ret = disable_irq_wake(mdv->mdm2ap1_irq);
		if (ret < 0)
			ERROR("[%s] disable_irq_wake mdm2ap1_irq failed %d\n", __func__, ret);

		ret = disable_irq_wake(mdv->rdy_irq);
		if (ret < 0)
			ERROR("[%s] disable_irq_wake rdy_irq failed %d\n", __func__, ret);

	}
	spin_unlock_irqrestore(&mdv->lock, flags);

	/* Enable the interrupt of mdm_rts */
	enable_irq(mdv->rts_irq);

	/* Enable the interrupt of mdm_rdy */
	enable_irq(mdv->rdy_irq);

	mutex_unlock(&dev_mutex);
	return 0;
}
#endif /* CONFIG_PM */

static struct spi_driver sc8800g_driver = {
	.driver = {
		.name		= "sc8800g",
		.bus		= &spi_bus_type,
		.owner		= THIS_MODULE,
	},
	.probe		= sc8800g_probe,
	.remove		= __devexit_p(sc8800g_remove),
#ifdef CONFIG_PM_SLEEP
	.suspend	= sc8800g_suspend,
	.resume		= sc8800g_resume,
#endif
};

/*
 * Module initialization
 */

static int __init serial_sc8800g_init(void)
{
	INFO("[%s]\n", __func__);
	return spi_register_driver(&sc8800g_driver);
}

static void __exit serial_sc8800g_exit(void)
{
	INFO("[%s]\n", __func__);
	spi_unregister_driver(&sc8800g_driver);
}

module_init(serial_sc8800g_init);
module_exit(serial_sc8800g_exit);
MODULE_DESCRIPTION("Serial driver over SPI for Spreadtrum SC8800G modem");
MODULE_LICENSE("GPL");
