

#ifndef __SYV690D_H
#define __SYV690D_H

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#ifdef CONFIG_WT_QGKI
#include <linux/hardware_info.h>
#endif
#include <linux/battmngr/battmngr_notifier.h>

#define PROBE_CNT_MAX	50

#define POWER_SUPPLY_TYPE_USB_FLOAT		QTI_POWER_SUPPLY_TYPE_USB_FLOAT
#define POWER_SUPPLY_TYPE_USB_HVDCP		QTI_POWER_SUPPLY_TYPE_USB_HVDCP

enum bq2589x_vbus_type {
	BQ2589X_VBUS_NONE,
	BQ2589X_VBUS_USB_SDP,
	BQ2589X_VBUS_USB_CDP, /*CDP for bq25890, Adapter for bq25892*/
	BQ2589X_VBUS_USB_DCP,
	BQ2589X_VBUS_MAXC,
	BQ2589X_VBUS_UNKNOWN,
	BQ2589X_VBUS_NONSTAND,
	BQ2589X_VBUS_OTG,
	BQ2589X_VBUS_TYPE_NUM,
};

enum bq2589x_part_no {
	SYV690 = 0x01,
	BQ25890 = 0x03,
	BQ25892 = 0x00,
	BQ25895 = 0x07,
	SC89890H = 0x04,
};

#define BQ2589X_STATUS_PLUGIN		0x0001
#define BQ2589X_STATUS_PG			0x0002
#define	BQ2589X_STATUS_CHARGE_ENABLE 0x0004
#define BQ2589X_STATUS_FAULT		0x0008

#define BQ2589X_STATUS_EXIST		0x0100

extern struct bq2589x *g_bq2589x;

struct bq2589x_config {
	bool	enable_auto_dpdm;
/*	bool	enable_12v;*/

	int		battery_voltage_term;
	int		charge_current;
	int		input_current;

	bool	enable_term;
	int		term_current;
	int		charge_voltage;

	bool 	enable_ico;
	bool	use_absolute_vindpm;
	bool	otg_status;
	int 	otg_vol;
	int 	otg_current;
};

struct bq2589x {
	struct device *dev;
	struct i2c_client *client;
	enum   bq2589x_part_no part_no;
	int    revision;

	unsigned int    status;
	int		vbus_type;
	int		charge_type;

	bool	enabled;

	int		vbus_volt;
	int		vbat_volt;

	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;

	int		rsoc;
	struct	bq2589x_config	cfg;
	struct work_struct irq_work;
	struct work_struct adapter_in_work;
	struct work_struct adapter_out_work;
	struct delayed_work monitor_work;
	struct delayed_work ico_work;
	struct delayed_work force_work;
	struct mutex bq2589x_i2c_lock;

	struct wakeup_source *xm_ws;
	int wakeup_flag;

	struct power_supply *batt_psy;
	struct power_supply *usb_psy;

	int old_type;
	int otg_gpio;
	int irq_gpio;
	//int usb_switch1;
	//int usb_switch2;
	//int usb_switch_flag;
	//int usb_swtich_status;
	int hz_flag;
	int curr_flag;
	bool force_exit_flag;
};

int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data);
int sc89890h_set_hv(struct bq2589x *bq, u8 hv);
int bq2589x_get_chg_type(struct bq2589x *bq);
int bq2589x_get_chg_usb_type(struct bq2589x *bq);
int bq2589x_enable_otg(struct bq2589x *bq);
int bq2589x_disable_otg(struct bq2589x *bq);
int bq2589x_set_otg_volt(struct bq2589x *bq, int volt);
int bq2589x_set_otg_current(struct bq2589x *bq, int curr);
int bq2589x_enable_charger(struct bq2589x *bq);
int bq2589x_disable_charger(struct bq2589x *bq);
int bq2589x_adc_start(struct bq2589x *bq, bool oneshot);
int bq2589x_adc_stop(struct bq2589x *bq);
int bq2589x_adc_read_battery_volt(struct bq2589x *bq);
int bq2589x_adc_read_vbus_volt(struct bq2589x *bq);
int bq2589x_read_vindpm_volt(struct bq2589x *bq);
int bq2589x_adc_read_charge_current(struct bq2589x *bq);
int bq2589x_set_charge_current(struct bq2589x *bq, int curr);
int bq2589x_set_term_current(struct bq2589x *bq, int curr);
int bq2589x_set_prechg_current(struct bq2589x *bq, int curr);
int bq2589x_set_chargevoltage(struct bq2589x *bq, int volt);
int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt);
int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr);
int bq2589x_set_vindpm_offset(struct bq2589x *bq, int offset);
int bq2589x_get_charging_status(struct bq2589x *bq);
int bq2589x_disable_watchdog_timer(struct bq2589x *bq);
int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout);
int bq2589x_reset_watchdog_timer(struct bq2589x *bq);
int bq2589x_is_dpdm_done(struct bq2589x *bq,int *done);
int bq2589x_force_dpdm(struct bq2589x *bq);
void bq2589x_force_dpdm_done(struct bq2589x *bq);
int bq2589x_reset_chip(struct bq2589x *bq);
int bq2589x_enter_hiz_mode(struct bq2589x *bq);
int bq2589x_exit_hiz_mode(struct bq2589x *bq);
int bq2589x_get_hiz_mode(struct bq2589x *bq, u8 *state);
int bq2589x_force_ico(struct bq2589x *bq);
int bq2589x_check_force_ico_done(struct bq2589x *bq);
int bq2589x_enable_term(struct bq2589x* bq, bool enable);
int bq2589x_enable_auto_dpdm(struct bq2589x* bq, bool enable);
int bq2589x_use_absolute_vindpm(struct bq2589x* bq, bool enable);
int bq2589x_enable_ico(struct bq2589x* bq, bool enable);
bool bq2589x_is_charge_present(struct bq2589x *bq);
bool bq2589x_is_charge_online(struct bq2589x *bq);
bool bq2589x_is_charge_done(struct bq2589x *bq);
void bq2589x_dump_regs(struct bq2589x *bq);
int bq2589x_init_device(struct bq2589x *bq);
int bq2589x_charge_status(struct bq2589x *bq);
//int bq2589x_usb_switch(struct bq2589x *bq, bool en);
int bq2589x_detect_device(struct bq2589x *bq);
void bq2589x_adjust_absolute_vindpm(struct bq2589x *bq);

int bq_init_iio_psy(struct bq2589x *chip);

#endif /* __SYV690D_H */


