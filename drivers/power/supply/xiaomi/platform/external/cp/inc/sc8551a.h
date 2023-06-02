
#ifndef __SC8551A_H__
#define __SC8551A_H__

#define pr_fmt(fmt)	"[sc8551] %s: " fmt, __func__

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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/version.h>
#include <linux/battmngr/battmngr_notifier.h>

typedef enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TBUS,
	ADC_TBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
}ADC_CH;

#define SC8551_ROLE_STDALONE   	0
#define SC8551_ROLE_SLAVE		1
#define SC8551_ROLE_MASTER		2

enum {
	SC8551_STDALONE,
	SC8551_SLAVE,
	SC8551_MASTER,
};

static int sc8551_mode_data[] = {
	[SC8551_STDALONE] = SC8551_ROLE_STDALONE,
	[SC8551_MASTER] = SC8551_ROLE_MASTER,
	[SC8551_SLAVE] = SC8551_ROLE_SLAVE,
};

#define	BAT_OVP_ALARM		BIT(7)
#define BAT_OCP_ALARM		BIT(6)
#define	BUS_OVP_ALARM		BIT(5)
#define	BUS_OCP_ALARM		BIT(4)
#define	BAT_UCP_ALARM		BIT(3)
#define	VBUS_INSERT			BIT(2)
#define VBAT_INSERT			BIT(1)
#define	ADC_DONE			BIT(0)

#define BAT_OVP_FAULT		BIT(7)
#define BAT_OCP_FAULT		BIT(6)
#define BUS_OVP_FAULT		BIT(5)
#define BUS_OCP_FAULT		BIT(4)
#define TBUS_TBAT_ALARM		BIT(3)
#define TS_BAT_FAULT		BIT(2)
#define	TS_BUS_FAULT		BIT(1)
#define	TS_DIE_FAULT		BIT(0)

/*below used for comm with other module*/
#define	BAT_OVP_FAULT_SHIFT			0
#define	BAT_OCP_FAULT_SHIFT			1
#define	BUS_OVP_FAULT_SHIFT			2
#define	BUS_OCP_FAULT_SHIFT			3
#define	BAT_THERM_FAULT_SHIFT		4
#define	BUS_THERM_FAULT_SHIFT		5
#define	DIE_THERM_FAULT_SHIFT		6

#define	BAT_OVP_FAULT_MASK			(1 << BAT_OVP_FAULT_SHIFT)
#define	BAT_OCP_FAULT_MASK			(1 << BAT_OCP_FAULT_SHIFT)
#define	BUS_OVP_FAULT_MASK			(1 << BUS_OVP_FAULT_SHIFT)
#define	BUS_OCP_FAULT_MASK			(1 << BUS_OCP_FAULT_SHIFT)
#define	BAT_THERM_FAULT_MASK		(1 << BAT_THERM_FAULT_SHIFT)
#define	BUS_THERM_FAULT_MASK		(1 << BUS_THERM_FAULT_SHIFT)
#define	DIE_THERM_FAULT_MASK		(1 << DIE_THERM_FAULT_SHIFT)

#define	BAT_OVP_ALARM_SHIFT			0
#define	BAT_OCP_ALARM_SHIFT			1
#define	BUS_OVP_ALARM_SHIFT			2
#define	BUS_OCP_ALARM_SHIFT			3
#define	BAT_THERM_ALARM_SHIFT		4
#define	BUS_THERM_ALARM_SHIFT		5
#define	DIE_THERM_ALARM_SHIFT		6
#define BAT_UCP_ALARM_SHIFT			7

#define	BAT_OVP_ALARM_MASK			(1 << BAT_OVP_ALARM_SHIFT)
#define	BAT_OCP_ALARM_MASK			(1 << BAT_OCP_ALARM_SHIFT)
#define	BUS_OVP_ALARM_MASK			(1 << BUS_OVP_ALARM_SHIFT)
#define	BUS_OCP_ALARM_MASK			(1 << BUS_OCP_ALARM_SHIFT)
#define	BAT_THERM_ALARM_MASK		(1 << BAT_THERM_ALARM_SHIFT)
#define	BUS_THERM_ALARM_MASK		(1 << BUS_THERM_ALARM_SHIFT)
#define	DIE_THERM_ALARM_MASK		(1 << DIE_THERM_ALARM_SHIFT)
#define	BAT_UCP_ALARM_MASK			(1 << BAT_UCP_ALARM_SHIFT)

#define VBAT_REG_STATUS_SHIFT		0
#define IBAT_REG_STATUS_SHIFT		1

#define VBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)
#define IBAT_REG_STATUS_MASK		(1 << VBAT_REG_STATUS_SHIFT)


#define ADC_REG_BASE SC8551_REG_16

#define sc_err(fmt, ...)								\
do {											\
	if (sc->mode == SC8551_ROLE_MASTER)						\
		printk(KERN_ERR "[sc8551-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SC8551_ROLE_SLAVE)					\
		printk(KERN_ERR "[sc8551-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_ERR "[sc8551-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define sc_info(fmt, ...)								\
do {											\
	if (sc->mode == SC8551_ROLE_MASTER)						\
		printk(KERN_INFO "[sc8551-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SC8551_ROLE_SLAVE)					\
		printk(KERN_INFO "[sc8551-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_INFO "[sc8551-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define sc_dbg(fmt, ...)								\
do {											\
	if (sc->mode == SC8551_ROLE_MASTER)						\
		printk(KERN_DEBUG "[sc8551-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (sc->mode == SC8551_ROLE_SLAVE)					\
		printk(KERN_DEBUG "[sc8551-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_DEBUG "[sc8551-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);


struct sc8551_cfg {
	bool bat_ovp_disable;
	bool bat_ocp_disable;
	bool bat_ovp_alm_disable;
	bool bat_ocp_alm_disable;

	int bat_ovp_th;
	int bat_ovp_alm_th;
	int bat_ocp_th;
	int bat_ocp_alm_th;

	bool bus_ovp_alm_disable;
	bool bus_ocp_disable;
	bool bus_ocp_alm_disable;

	int bus_ovp_th;
	int bus_ovp_alm_th;
	int bus_ocp_th;
	int bus_ocp_alm_th;

	bool bat_ucp_alm_disable;

	int bat_ucp_alm_th;
	int ac_ovp_th;

	bool bat_therm_disable;
	bool bus_therm_disable;
	bool die_therm_disable;

	int bat_therm_th; /*in %*/
	int bus_therm_th; /*in %*/
	int die_therm_th; /*in degC*/

	int sense_r_mohm;
};

struct sc8551 {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;
	struct mutex charging_disable_lock;
	struct mutex irq_complete;

	bool irq_waiting;
	bool irq_disabled;
	bool resume_completed;

	bool batt_present;
	bool vbus_present;

	bool usb_present;
	bool charge_enabled;	/* Register bit status */

	bool is_sc8551;
	int  vbus_error;

	/* ADC reading */
	int vbat_volt;
	int vbus_volt;
	int vout_volt;
	int vac_volt;

	int ibat_curr;
	int ibus_curr;

	int bat_temp;
	int bus_temp;
	int die_temp;

	/* alarm/fault status */
	bool bat_ovp_fault;
	bool bat_ocp_fault;
	bool bus_ovp_fault;
	bool bus_ocp_fault;

	bool bat_ovp_alarm;
	bool bat_ocp_alarm;
	bool bus_ovp_alarm;
	bool bus_ocp_alarm;

	bool bat_ucp_alarm;

	bool bat_therm_alarm;
	bool bus_therm_alarm;
	bool die_therm_alarm;

	bool bat_therm_fault;
	bool bus_therm_fault;
	bool die_therm_fault;

	bool therm_shutdown_flag;
	bool therm_shutdown_stat;

	bool vbat_reg;
	bool ibat_reg;

	int  prev_alarm;
	int  prev_fault;

	int chg_ma;
	int chg_mv;
	int adc_status;
	int charge_state;

	struct sc8551_cfg *cfg;

	int skip_writes;
	int skip_reads;

	struct delayed_work monitor_work;
	struct dentry *debug_root;
	struct iio_dev          *indio_dev;
	struct iio_chan_spec    *iio_chan;
	struct iio_channel	*int_iio_chans;
};

int sc8551_read_byte(struct sc8551 *sc, u8 reg, u8 *data);
int sc8551_enable_charge(struct sc8551 *sc, bool enable);
int sc8551_check_charge_enabled(struct sc8551 *sc, bool *enabled);
int sc8551_enable_wdt(struct sc8551 *sc, bool enable);
int sc8551_set_reg_reset(struct sc8551 *sc);
int sc8551_enable_batovp(struct sc8551 *sc, bool enable);
int sc8551_set_batovp_th(struct sc8551 *sc, int threshold);
int sc8551_enable_batovp_alarm(struct sc8551 *sc, bool enable);
int sc8551_set_batovp_alarm_th(struct sc8551 *sc, int threshold);
int sc8551_enable_batocp(struct sc8551 *sc, bool enable);
int sc8551_set_batocp_th(struct sc8551 *sc, int threshold);
int sc8551_enable_batocp_alarm(struct sc8551 *sc, bool enable);
int sc8551_set_batocp_alarm_th(struct sc8551 *sc, int threshold);
int sc8551_set_busovp_th(struct sc8551 *sc, int threshold);
int sc8551_enable_busovp_alarm(struct sc8551 *sc, bool enable);
int sc8551_set_busovp_alarm_th(struct sc8551 *sc, int threshold);
int sc8551_enable_busocp(struct sc8551 *sc, bool enable);
int sc8551_set_busocp_th(struct sc8551 *sc, int threshold);
int sc8551_enable_busocp_alarm(struct sc8551 *sc, bool enable);
int sc8551_set_busocp_alarm_th(struct sc8551 *sc, int threshold);
int sc8551_enable_batucp_alarm(struct sc8551 *sc, bool enable);
int sc8551_set_batucp_alarm_th(struct sc8551 *sc, int threshold);
int sc8551_set_acovp_th(struct sc8551 *sc, int threshold);
int sc8551_set_vdrop_th(struct sc8551 *sc, int threshold);
int sc8551_set_vdrop_deglitch(struct sc8551 *sc, int us);
int sc8551_enable_bat_therm(struct sc8551 *sc, bool enable);
int sc8551_set_bat_therm_th(struct sc8551 *sc, u8 threshold);
int sc8551_enable_bus_therm(struct sc8551 *sc, bool enable);
int sc8551_set_bus_therm_th(struct sc8551 *sc, u8 threshold);
int sc8551_set_die_therm_th(struct sc8551 *sc, u8 threshold);
int sc8551_enable_adc(struct sc8551 *sc, bool enable);
int sc8551_set_adc_scanrate(struct sc8551 *sc, bool oneshot);
int sc8551_get_adc_data(struct sc8551 *sc, int channel,  int *result);
int sc8551_set_adc_scan(struct sc8551 *sc, int channel, bool enable);
int sc8551_set_alarm_int_mask(struct sc8551 *sc, u8 mask);
int sc8551_set_sense_resistor(struct sc8551 *sc, int r_mohm);
int sc8551_enable_regulation(struct sc8551 *sc, bool enable);
int sc8551_set_ss_timeout(struct sc8551 *sc, int timeout);
int sc8551_set_ibat_reg_th(struct sc8551 *sc, int th_ma);
int sc8551_set_vbat_reg_th(struct sc8551 *sc, int th_mv);
int sc8551_check_vbus_error_status(struct sc8551 *sc);
int sc8551_detect_device(struct sc8551 *sc);
void sc8551_check_alarm_status(struct sc8551 *sc);
void sc8551_check_fault_status(struct sc8551 *sc);
int sc8551_set_present(struct sc8551 *sc, bool present);

#endif /* __SC8551A_H__ */

