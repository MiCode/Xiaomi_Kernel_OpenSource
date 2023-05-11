/*
 * max77729-muic.h
 *
 * Copyrights (C) 2021 Maxim Integrated Products, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * This driver is based on max14577-muic.h
 *
 */

#ifndef __MAX77729_MUIC_H__
#define __MAX77729_MUIC_H__

#include <linux/workqueue.h>

#define MUIC_DEV_NAME			"muic-max77729"
/* muic chip specific internal data structure */
#define BC_STATUS_VBUSDET_SHIFT		7
#define BC_STATUS_VBUSDET_MASK		(0x1 << BC_STATUS_VBUSDET_SHIFT)

#define COMN1SW_SHIFT			0
#define COMP2SW_SHIFT			3
#define RCPS_SHIFT			6
#define NOBCCOMP_SHIFT			7
#define COMN1SW_MASK			(0x7 << COMN1SW_SHIFT)
#define COMP2SW_MASK			(0x7 << COMP2SW_SHIFT)
#define RCPS_MASK			(0x1 << RCPS_SHIFT)
#define NOBCCOMP_MASK			(0x1 << NOBCCOMP_SHIFT)

/* MAX77729 ID Monitor Config */
#define MODE_SHIFT			2
#define MODE_MASK			(0x3 << MODE_SHIFT)

enum {
	VB_LOW			= 0x00,
	VB_HIGH			= (0x1 << BC_STATUS_VBUSDET_SHIFT),
	VB_DONTCARE		= 0xff,
};

/* muic register value for COMN1, COMN2 in Switch command  */

/*
 * MAX77729 Switch values
 * NoBCComp [7] / RCPS [6] / D+ [5:3] / D- [2:0]
 * 0: Compare with BC1.2 / 1: Ignore BC1.2, Manual control
 * 0: Disable / 1: Enable
 * 000: Open / 001, 100: USB / 011, 101: UART
 */
enum max77729_switch_val {
	MAX77729_MUIC_NOBCCOMP_DIS	= 0x0,
	MAX77729_MUIC_NOBCCOMP_EN	= 0x1,

	MAX77729_MUIC_RCPS_DIS		= 0x0,
	MAX77729_MUIC_RCPS_EN		= 0x1,
	MAX77729_MUIC_RCPS_VAL		= MAX77729_MUIC_RCPS_DIS,

	MAX77729_MUIC_COM_USB		= 0x01,
	MAX77729_MUIC_COM_AUDIO		= 0x02,
	MAX77729_MUIC_COM_UART		= 0x03,
	MAX77729_MUIC_COM_USB_CP	= 0x04,
	MAX77729_MUIC_COM_UART_CP	= 0x05,
	MAX77729_MUIC_COM_OPEN		= 0x07,
};

enum {
	COM_OPEN	= (MAX77729_MUIC_NOBCCOMP_DIS << NOBCCOMP_SHIFT) |
			(MAX77729_MUIC_RCPS_VAL << RCPS_SHIFT) |
			(MAX77729_MUIC_COM_OPEN << COMP2SW_SHIFT) |
			(MAX77729_MUIC_COM_OPEN << COMN1SW_SHIFT),
	COM_USB		= (MAX77729_MUIC_NOBCCOMP_DIS << NOBCCOMP_SHIFT) |
			(MAX77729_MUIC_RCPS_VAL << RCPS_SHIFT) |
			(MAX77729_MUIC_COM_USB << COMP2SW_SHIFT) |
			(MAX77729_MUIC_COM_USB << COMN1SW_SHIFT),
	COM_UART	= (MAX77729_MUIC_NOBCCOMP_EN << NOBCCOMP_SHIFT) |
			(MAX77729_MUIC_RCPS_VAL << RCPS_SHIFT) |
			(MAX77729_MUIC_COM_UART << COMP2SW_SHIFT) |
			(MAX77729_MUIC_COM_UART << COMN1SW_SHIFT),
	COM_USB_CP	= (MAX77729_MUIC_NOBCCOMP_EN << NOBCCOMP_SHIFT) |
			(MAX77729_MUIC_RCPS_VAL << RCPS_SHIFT) |
			(MAX77729_MUIC_COM_USB_CP << COMP2SW_SHIFT) |
			(MAX77729_MUIC_COM_USB_CP << COMN1SW_SHIFT),
	COM_UART_CP	= (MAX77729_MUIC_NOBCCOMP_EN << NOBCCOMP_SHIFT) |
			(MAX77729_MUIC_RCPS_VAL << RCPS_SHIFT) |
			(MAX77729_MUIC_COM_UART_CP << COMP2SW_SHIFT) |
			(MAX77729_MUIC_COM_UART_CP << COMN1SW_SHIFT),
};

struct max77729_muic_data {
	struct device			*dev;
	struct i2c_client		*i2c; /* i2c addr: 0x4A; MUIC */
	struct mutex			muic_mutex;
	struct wakeup_source		*muic_ws;
	/* model dependent mfd platform data */
	struct max77729_platform_data		*mfd_pdata;
	struct max77729_usbc_platform_data	*usbc_pdata;

	int				irq_uiadc;
	int				irq_chgtyp;
	int				irq_spr;
	int				irq_dcdtmo;
	int				irq_vbadc;
	int				irq_vbusdet;

	u8 usbc_status1;
	u8 usbc_status2;
	u8 bc_status;
	u8 cc_status0;
	u8 cc_status1;
	u8 pd_status0;
	u8 pd_status1;


	/* Status of VBUS Dectection */
	u8 vbusdet;
	/* DCD detection timed out */
	u8 dcdtmo;
	/* Output of Charger Detection */
	enum max77729_chg_type chg_type;

	/* Output of Properietary Charger Detection */
	enum max77729_pr_chg_type pr_chg_type;
	struct delayed_work		debug_work;
	struct delayed_work		qc_work;

	/* CHGIN Voltage ADC interrupt */
	u8 vbadc;
	/* UID ADC Interrupt */
	u8 uidadc;

};

int max77729_bc12_probe(struct max77729_usbc_platform_data *usbc_data);
int max77729_muic_suspend(struct max77729_usbc_platform_data *usbc_data);
int max77729_muic_resume(struct max77729_usbc_platform_data *usbc_data);
#endif /* __MAX77729_MUIC_H__ */

