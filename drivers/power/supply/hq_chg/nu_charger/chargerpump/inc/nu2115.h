
#ifndef __NU2115_H__
#define __NU2115_H__

//#define pr_fmt(fmt)	"[Nu2115] %s: " fmt, __func__

enum {
	NU2115_STANDALONG = 0,
	NU2115_MASTER,
	NU2115_SLAVE,
};

static const char* nu2115_psy_name[] = {
	[NU2115_STANDALONG] = "nu2115-cp-standalone",
	[NU2115_MASTER] = "sc-cp-master",
	[NU2115_SLAVE] = "sc-cp-slave",
};

static const char* nu2115_irq_name[] = {
	[NU2115_STANDALONG] = "nu2115-standalone-irq",
	[NU2115_MASTER] = "nu2115-master-irq",
	[NU2115_SLAVE] = "nu2115-slave-irq",
};

static int nu2115_mode_data[] = {
	[NU2115_STANDALONG] = NU2115_STANDALONG,
	[NU2115_MASTER] = NU2115_MASTER,
	[NU2115_SLAVE] = NU2115_SLAVE,
};

typedef enum {
	ADC_IBUS,
	ADC_VBUS,
	ADC_VAC1,
	ADC_VAC2,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TSBUS,
	ADC_TSBAT,
	ADC_TDIE,
	ADC_MAX_NUM,
}ADC_CH;

#define NU2115_ROLE_STDALONE   	0
#define NU2115_ROLE_MASTER		1
#define NU2115_ROLE_SLAVE		2

#define	BAT_OVP_ALARM		BIT(7)
#define BAT_OCP_ALARM		BIT(6)
#define	BUS_OVP_ALARM		BIT(5)
#define	BUS_OCP_ALARM		BIT(4)
#define	BAT_UCP_ALARM		BIT(3)
#define	VBUS_INSERT			BIT(7)
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

#define	VBUS_ERROR_H		(0)
#define	VBUS_ERROR_L		(1)

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


#define VBUS_ERROR_H_SHIFT      3
#define VBUS_ERROR_L_SHIFT      2
#define VBUS_ERROR_H_MASK       (1 << VBUS_ERROR_H_SHIFT)
#define VBUS_ERROR_L_MASK       (1 << VBUS_ERROR_L_SHIFT)


#define ADC_REG_BASE NU2115_REG_17

#define nu_err(fmt, ...)								\
do {											\
	if (chip->mode == NU2115_ROLE_MASTER)						\
		printk(KERN_ERR "[nu2115-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (chip->mode == NU2115_ROLE_SLAVE)					\
		printk(KERN_ERR "[nu2115-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_ERR "[nu2115-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define nu_info(fmt, ...)								\
do {											\
	if (chip->mode == NU2115_ROLE_MASTER)						\
		printk(KERN_INFO "[nu2115-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (chip->mode == NU2115_ROLE_SLAVE)					\
		printk(KERN_INFO "[nu2115-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_INFO "[nu2115-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);

#define nu_dbg(fmt, ...)								\
do {											\
	if (chip->mode == NU2115_ROLE_MASTER)						\
		printk(KERN_DEBUG "[nu2115-MASTER]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else if (chip->mode == NU2115_ROLE_SLAVE)					\
		printk(KERN_DEBUG "[nu2115-SLAVE]:%s:" fmt, __func__, ##__VA_ARGS__);	\
	else										\
		printk(KERN_DEBUG "[nu2115-STANDALONE]:%s:" fmt, __func__, ##__VA_ARGS__);\
} while(0);


struct nu2115_cfg {
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

struct nu2115 {
	struct device *dev;
	struct chargerpump_dev *cp_dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	int mode;

	struct mutex data_lock;
	struct mutex i2c_rw_lock;

	bool batt_present;
	bool vbus_present;

	bool usb_present;
	bool charge_enabled;	/* Register bit status */

	bool acdrv1_enable;
	bool otg_enable;

	int  vbus_error_low;
	int  vbus_error_high;

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

	struct nu2115_cfg *cfg;
	int irq_gpio;
	int irq;

	int skip_writes;
	int skip_reads;

	struct delayed_work monitor_work;
	struct dentry *debug_root;

	#ifdef CONFIG_MTK_CLASS
	struct charger_device *chg_dev;
	#endif /*CONFIG_MTK_CLASS*/

	#ifdef CONFIG_HUAQIN_CP_POLICY_MODULE
	struct chargerpump_dev *master_cp_chg;
	struct chargerpump_dev *slave_cp_chg;
	#endif

	const char *chg_dev_name;

	struct power_supply_desc psy_desc;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
};

#endif /* __NU2115_H__ */
