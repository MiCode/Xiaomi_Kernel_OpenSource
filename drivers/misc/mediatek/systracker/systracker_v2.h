/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __SYSTRAKCER_V2_H__
#define __SYSTRAKCER_V2_H__

#include <linux/platform_device.h>

#define BUS_DBG_CON			(BUS_DBG_BASE)
#define BUS_DBG_CON_INFRA		(BUS_DBG_INFRA_BASE)
#define BUS_DBG_TIMER_CON0		(BUS_DBG_BASE + 0x0004)
#define BUS_DBG_TIMER_CON1		(BUS_DBG_BASE + 0x0008)
#define BUS_DBG_TIMER_R0		(BUS_DBG_BASE + 0x000C)
#define BUS_DBG_TIMER_R1		(BUS_DBG_BASE + 0x0010)
#define BUS_DBG_WP			(BUS_DBG_BASE + 0x0014)
#define BUS_DBG_WP_MASK			(BUS_DBG_BASE + 0x0018)
#define BUS_DBG_MON			(BUS_DBG_BASE + 0x001C)
#define BUS_DBG_W_TRACK_DATA_VALID	(BUS_DBG_BASE + 0x0020)
#define BUS_DBG_AR_TRACK_L(__n)		(BUS_DBG_BASE + 0x0100 + 8 * (__n))
#define BUS_DBG_AR_TRACK_H(__n)		(BUS_DBG_BASE + 0x0104 + 8 * (__n))
#define BUS_DBG_AR_TRANS_TID(__n)	(BUS_DBG_BASE + 0x0180 + 4 * (__n))
#define BUS_DBG_AW_TRACK_L(__n)		(BUS_DBG_BASE + 0x0200 + 8 * (__n))
#define BUS_DBG_AW_TRACK_H(__n)		(BUS_DBG_BASE + 0x0204 + 8 * (__n))
#define BUS_DBG_AW_TRANS_TID(__n)	(BUS_DBG_BASE + 0x0280 + 4 * (__n))
#define BUS_DBG_W_TRACK_DATA6		(BUS_DBG_BASE + 0x02D8)
#define BUS_DBG_W_TRACK_DATA7		(BUS_DBG_BASE + 0x02DC)

#define BUS_DBG_BUS_MHZ             (266)
#define BUS_DBG_NUM_TRACKER         (8)

#define BUS_DBG_CON_BUS_DBG_EN      (0x00000001)
#define BUS_DBG_CON_TIMEOUT_EN      (0x00000002)
#define BUS_DBG_CON_SLV_ERR_EN      (0x00000004)
#define BUS_DBG_CON_WP_EN           (0x00000108)
#define BUS_DBG_CON_IRQ_AR_EN       (0x00000010)
#define BUS_DBG_CON_IRQ_AW_EN       (0x00000020)
#define BUS_DBG_CON_SW_RST_DN       (0x00000040)
/* more human-readable register name than above one */
#define BUS_DBG_CON_IRQ_WP_EN       (0x00000040)
#define BUS_DBG_CON_IRQ_CLR         (0x00000080)
#define BUS_DBG_CON_IRQ_AR_STA0     (0x00000100)
#define BUS_DBG_CON_IRQ_AW_STA0     (0x00000200)
#define BUS_DBG_CON_IRQ_WP_STA      (0x00000400)
#define BUS_DBG_CON_WDT_RST_EN      (0x00001000)
#define BUS_DBG_CON_HALT_ON_EN      (0x00002000)
#define BUS_DBG_CON_BUS_OT_EN       (0x00004000)
#define BUS_DBG_CON_SW_RST          (0x00010000)
#define BUS_DBG_CON_IRQ_AR_STA1     (0x00100000)
#define BUS_DBG_CON_IRQ_AW_STA1     (0x00200000)
#define BUS_DBG_CON_TIMEOUT_CLR     (0x00800000)
/* detect all stages of timeout */
#define BUS_DBG_CON_TIMEOUT	\
	(BUS_DBG_CON_IRQ_AR_STA0 | BUS_DBG_CON_IRQ_AW_STA0| \
	BUS_DBG_CON_IRQ_AR_STA1 | BUS_DBG_CON_IRQ_AW_STA1)

#define BUS_DBG_CON_IRQ_EN	\
	(BUS_DBG_CON_IRQ_AR_EN | BUS_DBG_CON_IRQ_AW_EN | \
	BUS_DBG_CON_IRQ_WP_EN)

#define BUS_DBG_MAX_TIMEOUT_VAL	    (0xffffffff)

static inline unsigned int extract_n2mbits(unsigned int input, int n, int m)
{
/*
 * 1. ~0 = 1111 1111 1111 1111 1111 1111 1111 1111
 * 2. ~0 << (m - n + 1) = 1111 1111 1111 1111 1100 0000 0000 0000
 * // assuming we are extracting 14 bits, the +1 is added
 * for inclusive selection
 * 3. ~(~0 << (m - n + 1)) = 0000 0000 0000 0000 0011 1111 1111 1111
 */
	int mask;

	if (n > m) {
		n = n + m;
		m = n - m;
		n = n - m;
	}
	mask = ~(~0 << (m - n + 1));
	return ((input >> n) & mask);
}

struct mt_systracker_driver {
	struct	platform_driver driver;
	struct	platform_device device;
	u32 support_2_stage_timeout;
	void	(*reset_systracker)(void);
	int	(*enable_watchpoint)(void);
	int	(*disable_watchpoint)(void);
	int	(*set_watchpoint_address)(unsigned int wp_phy_address);
	void	(*enable_systracker)(void);
	void	(*disable_systracker)(void);
	int	(*test_systracker)(void);
	int	(*systracker_probe)(struct platform_device *pdev);
	unsigned int (*systracker_timeout_value)(void);
	unsigned int (*systracker_timeout2_value)(void);
	int	(*systracker_remove)(struct platform_device *pdev);
	int	(*systracker_suspend)(struct platform_device *pdev,
			pm_message_t state);
	int	(*systracker_resume)(struct platform_device *pdev);
	int	(*systracker_hook_fault)(void);
	int	(*systracker_test_init)(void);
	void	(*systracker_test_cleanup)(void);
	void	(*systracker_wp_test)(void);
	void	(*systracker_read_timeout_test)(void);
	void	(*systracker_write_timeout_test)(void);
	void	(*systracker_withrecord_test)(void);
	void	(*systracker_notimeout_test)(void);
};

struct systracker_entry_t {
	unsigned int dbg_con;
	unsigned int dbg_con_infra;
	unsigned int ar_track_l[BUS_DBG_NUM_TRACKER];
	unsigned int ar_track_h[BUS_DBG_NUM_TRACKER];
	unsigned int ar_trans_tid[BUS_DBG_NUM_TRACKER];
	unsigned int aw_track_l[BUS_DBG_NUM_TRACKER];
	unsigned int aw_track_h[BUS_DBG_NUM_TRACKER];
	unsigned int aw_trans_tid[BUS_DBG_NUM_TRACKER];
	unsigned int w_track_data6;
	unsigned int w_track_data7;
	unsigned int w_track_data_valid;
};

struct systracker_config_t {
	int state;
	int enable_timeout;
	int enable_slave_err;
	int enable_wp;
	int enable_irq;
	int timeout_ms;
	int timeout2_ms;
	int wp_phy_address;
};

int tracker_dump(char *buf);
void dump_backtrace_entry_ramconsole_print
	(unsigned long where, unsigned long from, unsigned long frame);
void dump_regs
	(const char *fmt, const char v1, const unsigned int reg,
		const unsigned int reg_val);

static void save_entry(void);
static int systracker_probe(struct platform_device *pdev);
static int systracker_remove(struct platform_device *pdev);
static int systracker_suspend
	(struct platform_device *pdev, pm_message_t state);
static int systracker_resume(struct platform_device *pdev);
static void systracker_reset(void);
static void systracker_enable(void);
static void systracker_disable(void);

int systracker_set_watchpoint_addr(unsigned int phy_addr);
int systracker_watchpoint_disable(void);
int systracker_watchpoint_enable(void);

/* #define SYSTRACKER_TEST_SUIT */ /* enable for driver poring test suit */
/* #define TRACKER_DEBUG 0 */
#endif
