#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/sizes.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_batterydata.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/err.h>


#undef KERNEL_VERSION
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))
#undef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(3, 1, 8)

#define SMB_VTG_MIN_UV		1800000
#define SMB_VTG_MAX_UV		1800000


#define CWFG_ENABLE_LOG 1
#define CWFG_I2C_BUSNUM 5
#define DOUBLE_SERIES_BATTERY 0

#define CW_PROPERTIES "bms"

#define REG_VERSION             0x00
#define REG_VCELL_H             0x02
#define REG_VCELL_L             0x03
#define REG_SOC_INT             0x04
#define REG_SOC_DECIMAL         0x05
#define REG_TEMP                0x06
#define REG_MODE_CONFIG         0x08
#define REG_GPIO_CONFIG         0x0A
#define REG_SOC_ALERT           0x0B
#define REG_TEMP_MAX            0x0C
#define REG_TEMP_MIN            0x0D
#define REG_VOLT_ID_H           0x0E
#define REG_VOLT_ID_L           0x0F
#define REG_CYCLE_H             0xA4
#define REG_CYCLE_L             0xA5
#define REG_SOH                 0xA6
#define REG_IBAT_H              0xA8
#define REG_IBAT_L              0xA9
#define REG_BATINFO             0x10

#define MODE_SLEEP              0x30
#define MODE_NORMAL             0x00
#define MODE_DEFAULT            0xF0
#define CONFIG_UPDATE_FLG       0x80


#ifdef CW2017_INTERRUPT
#define GPIO_CONFIG_MIN_TEMP             (0x01 << 4)
#define GPIO_CONFIG_MAX_TEMP             (0x01 << 5)
#define GPIO_CONFIG_SOC_CHANGE           (0x01 << 6)
#else
#define GPIO_CONFIG_MIN_TEMP             (0x00 << 4)
#define GPIO_CONFIG_MAX_TEMP             (0x00 << 5)
#define GPIO_CONFIG_SOC_CHANGE           (0x00 << 6)
#endif/*CW2017_INTERRUPT*/

#define GPIO_CONFIG_MIN_TEMP_MARK        (0x01 << 4)
#define GPIO_CONFIG_MAX_TEMP_MARK        (0x01 << 5)
#define GPIO_CONFIG_SOC_CHANGE_MARK      (0x01 << 6)
#define ATHD                              0x0
#define DEFINED_MAX_TEMP                          450
#define DEFINED_MIN_TEMP                          0

#define DESIGN_CAPACITY                   3000
#define CWFG_NAME "cw2017"
#define SIZE_BATINFO    80

#define FULL_CAPACITY   100
#define NO_START_VERSION 160


#define queue_delayed_work_time  8000

#define cw_printk(fmt, arg...)        \
	({                                    \
		if(CWFG_ENABLE_LOG){              \
			printk("FG_CW2017 : %s : " fmt, __FUNCTION__ , ##arg);  \
		}else{}                           \
	})



static char config_info[SIZE_BATINFO] = {
	0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x9D, 0xAD, 0xCF, 0xC4, 0xC1, 0xE1, 0xAC, 0x58,
	0x2D, 0xFF, 0xE9, 0xCC, 0xAB, 0x94, 0x84, 0x6B,
	0x54, 0x43, 0x2E, 0x64, 0xBE, 0xDB, 0x7A, 0xD2,
	0xD1, 0xD2, 0xD1, 0xD0, 0xCD, 0xC5, 0xD0, 0xD3,
	0xAC, 0xC1, 0xC2, 0xA4, 0x93, 0x89, 0x79, 0x70,
	0x58, 0x56, 0x70, 0x90, 0xA8, 0x80, 0x7B, 0x86,
	0x00, 0x00, 0x90, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB1,
};




struct cw_battery {
	struct device		*dev;
    struct i2c_client *client;

    struct workqueue_struct *cwfg_workqueue;
	struct delayed_work battery_delay_work;
#ifdef CW2017_INTERRUPT
	struct delayed_work interrupt_work;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct power_supply cw_bat;
#else
	struct power_supply *cw_bat;
#endif
	struct power_supply *usb_psy;

	/*User set*/
	unsigned int design_capacity;
	/*IC value*/
	int version;
    int voltage;
    int capacity;
	int temp;
	int cycle_count;
	int SOH;
	int cw_current;

	/*IC config*/
	unsigned char int_config;
	unsigned char soc_alert;
	int temp_max;
	int temp_min;

	/*Get before profile write*/
	int volt_id;

	/*Get from charger power supply*/


	/*Mark for change cw_bat power_supply*/


	struct regulator *vcc_i2c;
	struct qpnp_vadc_chip *vadc_dev;
	const char *battery_type;
	int battery_id;

       int voltage_before_change;
};

/*Define CW2017 iic read function*/
static int cw_read(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;
	ret = i2c_smbus_read_i2c_block_data( client, reg, 1, buf);
	if(ret < 0){
		printk("cw2017: %s: IIC error %d\n", __func__, ret);
	}
	return ret;
}
/*Define CW2017 iic write function*/
static int cw_write(struct i2c_client *client, unsigned char reg, unsigned char const buf[])
{
	int ret = 0;
	ret = i2c_smbus_write_i2c_block_data( client, reg, 1, &buf[0] );
	if(ret < 0) {
		printk("cw2017: %s: IIC error %d\n", __func__, ret);
	}
	return ret;
}
/*Define CW2017 iic read word function*/
static int cw_read_word(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;
	ret = i2c_smbus_read_i2c_block_data( client, reg, 2, buf );
	if(ret < 0) {
		printk("cw2017: %s: IIC error %d\n", __func__, ret);
	}
	return ret;
}

static int cw2017_enable(struct cw_battery *cw_bat)
{
	int ret;
    unsigned char reg_val = MODE_SLEEP;

	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if(ret < 0)
		return ret;

	reg_val = MODE_NORMAL;
	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if(ret < 0)
		return ret;
	printk(KERN_ERR"cw2017: %s: end, ret = %d \n", __func__, ret);
	msleep(100);
	return 0;
}


static int cw_get_version(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val = 0;
	int version = 0;
	ret = cw_read(cw_bat->client, REG_VERSION, &reg_val);
	if(ret < 0)
		return INT_MAX;

	version = reg_val;
	return version;
}

static int cw_get_voltage(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val[2] = {0 , 0};
	unsigned int voltage = 0;

	ret = cw_read_word(cw_bat->client, REG_VCELL_H, reg_val);
	if(ret < 0)
		return INT_MAX;

	voltage = (reg_val[0] << 8) + reg_val[1];
	voltage = voltage  * 5 / 16;

	return(voltage);
}


#define DECIMAL_MAX 178
#define DECIMAL_MIN 76
static int cw_get_capacity(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val = 0;
	int soc = 0;
	int soc_decimal = 0;
	static int reset_loop = 0;
	ret = cw_read(cw_bat->client, REG_SOC_INT, &reg_val);
	if(ret < 0)
		return INT_MAX;
	soc = reg_val;

	ret = cw_read(cw_bat->client, REG_SOC_DECIMAL, &reg_val);
	if(ret < 0)
		return INT_MAX;
	soc_decimal = reg_val;

	if(soc > 100){
		reset_loop++;
		printk("cw2017: IC error read soc error %d times\n", reset_loop);
		if(reset_loop > 5){
			printk("cw2017: IC error. please reset IC");
			cw2017_enable(cw_bat);
		}
		return cw_bat->capacity;
	}

	/* case 1 : aviod swing */
	if((soc >= cw_bat->capacity - 1) && (soc <= cw_bat->capacity + 1)
		&& (soc_decimal > DECIMAL_MAX || soc_decimal < DECIMAL_MIN) && soc != FULL_CAPACITY){
		soc = cw_bat->capacity;
	}

	return soc;
}


#define DEFAULT_BATT_TEMP	200
static int cw_get_temp(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val = 0;
	int temp = 0;
	ret = cw_read(cw_bat->client, REG_TEMP, &reg_val);
	if(ret < 0)
		return INT_MAX;

	temp = reg_val * 10 / 2 - 400;
#ifdef CONFIG_DISABLE_TEMP_PROTECT
	printk("cw2017: DISABLE_TEMP_PROTECT: force change temp %d to %d\n", temp, DEFAULT_BATT_TEMP);
	return DEFAULT_BATT_TEMP;
#else
	return temp;
#endif
}

static int cw_get_cycle_count(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val[2] = {0 , 0};
	int ad_buff = 0;

	ret = cw_read_word(cw_bat->client, REG_CYCLE_H, reg_val);
	if(ret < 0)
		return INT_MAX;

	ad_buff = (reg_val[0] << 8) + reg_val[1];

	return(ad_buff);
}

static int cw_get_SOH(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val = 0;
	int SOH = 0;
	ret = cw_read(cw_bat->client, REG_SOH, &reg_val);
	if(ret < 0)
		return INT_MAX;

	SOH = reg_val;
	return SOH;
}

static int cw_get_current(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val[2] = {0 , 0};
	int ad_buff = 0;

	ret = cw_read_word(cw_bat->client, REG_IBAT_H, reg_val);
	if(ret < 0)
		return INT_MAX;



	ad_buff = (reg_val[0] << 8) + reg_val[1];

	if(ad_buff > 32768){
		ad_buff = ad_buff - 65536;
	}


	if (ad_buff < 0) {
		ad_buff = 0 - ad_buff;
		ad_buff = ad_buff * cw_bat->design_capacity / 4096;
		ad_buff = 0 - ad_buff;
	}else {
		ad_buff = ad_buff * cw_bat->design_capacity / 4096;
	}


	return -(ad_buff*1000);
}

static void cw_update_data(struct cw_battery *cw_bat)
{
	cw_bat->voltage = cw_get_voltage(cw_bat);
	cw_bat->capacity = cw_get_capacity(cw_bat);
	cw_bat->temp = cw_get_temp(cw_bat);
	cw_bat->cycle_count = cw_get_cycle_count(cw_bat);
	cw_bat->cw_current = cw_get_current(cw_bat);

	pr_err("cw2017:[update_data] current:%d, voltage:%d, capacity:%d, temp:%d\n", \
			cw_bat->cw_current, cw_bat->voltage, cw_bat->capacity, cw_bat->temp);

}

static int cw_init_data(struct cw_battery *cw_bat)
{
	cw_bat->version = cw_get_version(cw_bat);
	cw_bat->voltage = cw_get_voltage(cw_bat);
	cw_bat->capacity = cw_get_capacity(cw_bat);
	cw_bat->temp = cw_get_temp(cw_bat);
	cw_bat->cycle_count = cw_get_cycle_count(cw_bat);
	cw_bat->SOH = cw_get_SOH(cw_bat);
	cw_bat->cw_current = cw_get_current(cw_bat);
	if(cw_bat->version == INT_MAX){
		printk("cw2017: %s: version == INT_MAX ret -1\n", __func__);
		return -1;
	}

	pr_err("cw2017:[init_data] version:%d, voltage:%d, capacity:%d, temp:%d, SOH:%d, \n", \
			cw_bat->version, cw_bat->voltage, cw_bat->capacity, cw_bat->temp, cw_bat->SOH);
	return 0;
}

static int cw_init_config(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_gpio_config = 0;
	unsigned char athd = 0;
	unsigned char reg_val = 0;

	cw_bat->design_capacity = DESIGN_CAPACITY;
	/*IC config*/
	cw_bat->int_config = GPIO_CONFIG_MIN_TEMP | GPIO_CONFIG_MAX_TEMP | GPIO_CONFIG_SOC_CHANGE;
	cw_bat->soc_alert = ATHD;
	cw_bat->temp_max = DEFINED_MAX_TEMP;
	cw_bat->temp_min = DEFINED_MIN_TEMP;

	reg_gpio_config = cw_bat->int_config;

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: REG_SOC_ALERT ret = %d \n", __func__, ret);
		return ret;
	}

	athd = reg_val & CONFIG_UPDATE_FLG;
	athd = athd | cw_bat->soc_alert;

	if(reg_gpio_config | GPIO_CONFIG_MAX_TEMP_MARK)
	{
		reg_val = (cw_bat->temp_max + 400) * 2 /10;
		ret = cw_write(cw_bat->client, REG_TEMP_MAX, &reg_val);
		if(ret < 0) {
			printk(KERN_ERR"cw2017: %s: REG_TEMP_MAX ret = %d \n", __func__, ret);
			return ret;
		}
	}
	if(reg_gpio_config | GPIO_CONFIG_MIN_TEMP_MARK)
	{
		reg_val = (cw_bat->temp_min + 400) * 2 /10;
		ret = cw_write(cw_bat->client, REG_TEMP_MIN, &reg_val);
		if(ret < 0) {
			printk(KERN_ERR"cw2017: %s: REG_TEMP_MIN ret = %d \n", __func__, ret);
			return ret;
		}
	}

	ret = cw_write(cw_bat->client, REG_GPIO_CONFIG, &reg_gpio_config);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: REG_GPIO_CONFIG ret = %d \n", __func__, ret);
		return ret;
	}

	ret = cw_write(cw_bat->client, REG_SOC_ALERT, &athd);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: REG_SOC_ALERT ret = %d \n", __func__, ret);
		return ret;
	}
	printk(KERN_ERR"cw2017: %s: end, ret = %d \n", __func__, ret);
	return 0;
}

/*CW2017 update profile function, Often called during initialization*/
static int cw_update_config_info(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	int version = NO_START_VERSION;

	/* update new battery info */
	for(i = 0; i < SIZE_BATINFO; i++)
	{
		reg_val = config_info[i];
		ret = cw_write(cw_bat->client, REG_BATINFO + i, &reg_val);
        if(ret < 0)
			return ret;
	}

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: REG_SOC_ALERT ret1 = %d \n", __func__, ret);
		return ret;
	}

	reg_val |= CONFIG_UPDATE_FLG;   /* set UPDATE_FLAG */
	ret = cw_write(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: REG_SOC_ALERT ret2 = %d \n", __func__, ret);
		return ret;
	}

	ret = cw2017_enable(cw_bat);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: cw2017_enable ret = %d \n", __func__, ret);
		return ret;
	}

    /*if get abnormal data, waitting (Max 3s, real 1.6s) for IC init complete*/
	for(i = 0; i < 30; i++)
	{
		ret = cw_read(cw_bat->client, REG_SOC_INT, &reg_val);
		if(ret < 0)
			return INT_MAX;
		version = cw_get_version(cw_bat);
		printk(KERN_ERR"cw2017: %s: get abnormal data: REG_SOC_INT = %d, version = %d \n", __func__, reg_val, version);
		if ((reg_val <= FULL_CAPACITY) && (NO_START_VERSION != version)) {
			break;
		}
		msleep(100);
	}

	printk(KERN_ERR"cw2017: %s: end, ret = %d \n", __func__, ret);
	return 0;
}

/*CW2017 init function, Often called during initialization*/
static int cw_init(struct cw_battery *cw_bat)
{
    int ret;
    int i;
    unsigned char reg_val = MODE_NORMAL;
	unsigned char config_flg = 0;

	ret = cw_read(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: REG_MODE_CONFIG ret = %d \n", __func__, ret);
		return ret;
	}

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &config_flg);
	if(ret < 0) {
		printk(KERN_ERR"cw2017: %s: REG_SOC_ALERT ret = %d \n", __func__, ret);
		return ret;
	}


	if(reg_val != MODE_NORMAL || ((config_flg & CONFIG_UPDATE_FLG) == 0x00)){
		ret = cw_update_config_info(cw_bat);
		if(ret < 0) {
			printk(KERN_ERR"cw2017: %s: cw_update_config_info ret1 = %d \n", __func__, ret);
			return ret;
		}

	} else {
		for(i = 0; i < SIZE_BATINFO; i++)
		{
			ret = cw_read(cw_bat->client, REG_BATINFO +i, &reg_val);
			if(ret < 0)
				return ret;

			if(config_info[i] != reg_val)
			{
				break;
			}
		}
		if(i != SIZE_BATINFO)
		{

			ret = cw_update_config_info(cw_bat);
			if(ret < 0) {
				printk(KERN_ERR"cw2017: %s: cw_update_config_info ret2 = %d \n", __func__, ret);
				return ret;
			}
		}
	}
	printk(KERN_ERR"cw2017: %s: end, ret = %d \n", __func__, ret);
	return 0;
}


#define BAT_VOL_CHANGE_THRES 3450
#define BAT_VOL_REPORT_ABS_RANGE 50
static int abs_func(int a)
{
	if (a >= 0)
		return a;
	else
		return -a;
}


static void cw_bat_work(struct work_struct *work)
{
    struct delayed_work *delay_work;
    struct cw_battery *cw_bat;

    delay_work = container_of(work, struct delayed_work, work);
    cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);

	cw_update_data(cw_bat);
	if(cw_bat->capacity == 50){
		cw_bat->cycle_count = cw_get_cycle_count(cw_bat);
		cw_bat->SOH = cw_get_SOH(cw_bat);
	}


	if ((cw_bat->voltage <= BAT_VOL_CHANGE_THRES) ||
		(abs_func(cw_bat->voltage_before_change - cw_bat->voltage) >= BAT_VOL_REPORT_ABS_RANGE)){

		cw_bat->voltage_before_change = cw_bat->voltage;
		#ifdef CW_PROPERTIES
		#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
		power_supply_changed(&cw_bat->cw_bat);
		#else
		power_supply_changed(cw_bat->cw_bat);
		#endif
		#endif
		pr_err("cw2017: power_supply_changed  voltage\n");
	}


	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
}


static int g_batt_id_kohm;
int cw_get_battid_for_profile_check(void)
{
	return g_batt_id_kohm;

}
EXPORT_SYMBOL_GPL(cw_get_battid_for_profile_check);

#define DEFAULT_RESISTER_KOHM 330
static int cw_get_battid_resister(struct cw_battery *cw_bat)
{
	int rc = 0;
	u64 div1 = 0;
	int batt_id_kohm = 0;
	struct qpnp_vadc_result results;

	rc = qpnp_vadc_read(cw_bat->vadc_dev, P_MUX4_1_1, &results);
	if (rc) {
		pr_err("cw2017: Unable to read batt id resister rc=%d\n", rc);
		batt_id_kohm = DEFAULT_RESISTER_KOHM;
	} else {
		div1 =(u64)(results.physical*100);
		do_div(div1, (1800000 - results.physical));
		batt_id_kohm = (int)div1;
	}
	pr_err("cw2017: read batt_id_kohm =%d\n", batt_id_kohm);
	g_batt_id_kohm = batt_id_kohm;

	return batt_id_kohm;
}



static int cw_get_usb_present(struct cw_battery *cw_bat)
{
	union power_supply_propval prop = {0,};
	int ret;

	ret = cw_bat->usb_psy->get_property(cw_bat->usb_psy,
							POWER_SUPPLY_PROP_PRESENT, &prop);
	if (ret < 0)
		pr_err("could not read USB current_max property, ret=%d\n", ret);
	return prop.intval;
}

static int cw_get_batt_status(struct cw_battery *cw_bat)
{
#if (0)
	int usb_present = 0;

	usb_present = cw_get_usb_present(cw_bat);

	if ((FULL_CAPACITY == cw_bat->capacity) && usb_present)
		return POWER_SUPPLY_STATUS_FULL;/*this case will be checked in charger driver*/
	else
#endif
		return POWER_SUPPLY_STATUS_UNKNOWN;/*this case will not be checked*/
}

static int cw_get_batt_capacity_level(struct cw_battery *cw_bat)
{
	if (FULL_CAPACITY == cw_bat->capacity)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (0 == cw_bat->capacity)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
}


#ifdef CW_PROPERTIES
static int cw_battery_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
    struct cw_battery *cw_bat;
    cw_bat = container_of(psy, struct cw_battery, cw_bat);
#else
	struct cw_battery *cw_bat = power_supply_get_drvdata(psy);
#endif

    switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
            val->intval = cw_get_batt_status(cw_bat);
            break;
	case POWER_SUPPLY_PROP_ONLINE:
            val->intval = cw_get_usb_present(cw_bat);
            break;
    case POWER_SUPPLY_PROP_CAPACITY:
            val->intval = cw_get_capacity(cw_bat);
            break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
            val->intval = cw_get_batt_capacity_level(cw_bat);
            break;
    case POWER_SUPPLY_PROP_HEALTH:
            val->intval= POWER_SUPPLY_HEALTH_GOOD;
            break;

    case POWER_SUPPLY_PROP_PRESENT:
            val->intval = cw_bat->voltage <= 0 ? 0 : 1;
            break;

    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            val->intval = (cw_get_voltage(cw_bat) * 1000);
            break;

    case POWER_SUPPLY_PROP_TECHNOLOGY:
            val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
            break;

    case POWER_SUPPLY_PROP_CYCLE_COUNT:
            val->intval = cw_bat->cycle_count;
            break;

    case POWER_SUPPLY_PROP_TEMP:
            val->intval = cw_get_temp(cw_bat);
            break;

    case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
            val->intval = cw_bat->temp_min;
            break;

    case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
            val->intval = cw_bat->temp_max;
            break;

    case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
            val->intval = cw_bat->design_capacity;
            break;

    case POWER_SUPPLY_PROP_CHARGE_FULL:
            val->intval = cw_bat->design_capacity * cw_bat->SOH / 100;
            break;

    case POWER_SUPPLY_PROP_CURRENT_NOW:
            val->intval = cw_get_current(cw_bat);
            break;

	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		    val->intval = (cw_bat->battery_id)*1000;
            break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
            val->strval = cw_bat->battery_type;
            break;
	case POWER_SUPPLY_PROP_UPDATE_NOW:
            val->intval = 0;
            break;

    default:
			pr_err("get prop %d is not supported in bms\n", psp);
			ret = -EINVAL;
            break;
    }

	if (ret < 0) {
		pr_err("Couldn't get prop %d ret = %d\n", psp, ret);
		return -ENODATA;
	}

    return 0;
}


static const uint8_t fg_dump_regs[] = {
	0x00, 0x02, 0x03, 0x04,
	0x05, 0x06, 0x08, 0x0A,
	0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0xA4, 0xA5, 0xA6,
	0xA8, 0xA9, 0x10, 0x11,
	0x5E, 0x5F
};

#define PRINT_LEN_ONCE 15
#define PRINT_LEN_MAX  (30*PRINT_LEN_ONCE)

static void cw_dump_registers(struct cw_battery *cw_bat)
{
	int i = 0;
	uint8_t reg_val = 0;
	uint8_t dump_regs[PRINT_LEN_MAX] = {0};
	uint32_t len = 0;
	for(i = 0; i < ARRAY_SIZE(fg_dump_regs); i++)
	{
		(void)cw_read(cw_bat->client, fg_dump_regs[i], &reg_val);
		len += snprintf((dump_regs + len), PRINT_LEN_ONCE, "Reg%02X:0x%02X ", fg_dump_regs[i], reg_val);
	}
	dump_regs[strlen(dump_regs)] = '\0';
	pr_err("cw2017:[dump_regs] %s\n", dump_regs);
}

static int cw_battery_set_property(struct power_supply *psy,
                enum power_supply_property psp,
                const union power_supply_propval *val)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct cw_battery *cw_bat;
	cw_bat = container_of(psy, struct cw_battery, cw_bat);
#else
	struct cw_battery *cw_bat = power_supply_get_drvdata(psy);
#endif

	switch (psp) {
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		cw_dump_registers(cw_bat);
		break;
	default:
		pr_err("set prop %d is not supported in bms\n", psp);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_err("Couldn't set prop %d ret = %d\n", psp, ret);
	}

	return 0;
}


static enum power_supply_property cw_battery_properties[] = {
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
	POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_UPDATE_NOW,
};

static int cw_battery_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_UPDATE_NOW:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

#endif


#ifdef CW2017_INTERRUPT
#define WAKE_LOCK_TIMEOUT       (10 * HZ)
static struct wake_lock cw2017_wakelock;
static void interrupt_work_do_wakeup(struct work_struct *work)
{
        struct delayed_work *delay_work;
        struct cw_battery *cw_bat;
		int ret = 0;
		unsigned char reg_val = 0;

        delay_work = container_of(work, struct delayed_work, work);
        cw_bat = container_of(delay_work, struct cw_battery, interrupt_work);

		ret = cw_read(cw_bat->client, REG_GPIO_CONFIG, &reg_val);
		if(ret < 0)
			return ret;
}

static irqreturn_t ops_cw2017_int_handler_int_handler(int irq, void *dev_id)
{
        struct cw_battery *cw_bat = dev_id;
        wake_lock_timeout(&cw2017_wakelock, WAKE_LOCK_TIMEOUT);
        queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->interrupt_work, msecs_to_jiffies(20));
        return IRQ_HANDLED;
}
#endif/*CW2017_INTERRUPT*/




static int cw_get_battery_profile(struct cw_battery *cw_bat)
{
    int ret = 0;
	struct device_node *batt_node;
	struct device_node *batt_data_node;
	const char *data;
	int data_len = 0;
	int i = 0;

	batt_node = of_parse_phandle(cw_bat->dev->of_node, "qcom,battery-data", 0);
	if (!batt_node) {
		pr_err("cw2017: No Batterydata is available\n");
		return -ENODATA;
	}
	batt_data_node = of_batterydata_get_best_profile(batt_node, CW_PROPERTIES, NULL);
	if (!batt_data_node) {
		pr_err("cw2017: couldn't find battery profile handle\n");
		return -ENODATA;
	}
	ret = of_property_read_string(batt_data_node, "qcom,battery-type", &cw_bat->battery_type);
	if (ret < 0) {
		pr_err("cw2017: battery type unavailable, ret=%d\n", ret);
		return ret;
	} else {
		pr_err("cw2017: get battery type is <%s>\n", cw_bat->battery_type);
	}

	data = of_get_property(batt_data_node, "qcom,fg-profile-data", &data_len);
	if (!data) {
		pr_err("cw2017: No profile data available\n");
		return -ENODATA;
	}
	pr_err("cw2017: get battery profile data size is: %d\n", data_len);

	if (data_len != SIZE_BATINFO) {
		pr_err("cw2017: battery profile incorrect size: %d\n", data_len);
		return -EINVAL;
	}
	memcpy(config_info, data, data_len);

	/*dump profile data*/
	for(i = 0; i < SIZE_BATINFO; i++)
	{
		pr_err("[cw_batt_profile] 0x%02X ", config_info[i]);
	}
	return 0;
}


static int cw2017_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    int loop = 0;
	struct power_supply *usb_psy;
	struct cw_battery *cw_bat;

#ifdef CW2017_INTERRUPT
	int irq = 0;
#endif

#ifdef CW_PROPERTIES
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {0};
#endif
#endif

	cw_printk("\n");


	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		pr_err("cw2017: USB psy not found, defer probe\n");
		return -EPROBE_DEFER;
	}


    cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
    if (!cw_bat) {
		cw_printk("cw_bat create fail!\n");
        return -ENOMEM;
    }

    i2c_set_clientdata(client, cw_bat);

	cw_bat->dev = &client->dev;
	cw_bat->client = client;
	cw_bat->volt_id = 0;
	cw_bat->usb_psy = usb_psy;
	cw_bat->voltage_before_change = 0;


	cw_bat->vadc_dev = qpnp_get_vadc(cw_bat->dev, "battid");
	if (IS_ERR(cw_bat->vadc_dev)) {
		ret = PTR_ERR(cw_bat->vadc_dev);
		if (ret == -EPROBE_DEFER)
			pr_err("cw2017: vadc not found - defer ret=%d\n", ret);
		else
			pr_err("cw2017: vadc property missing, ret=%d\n", ret);

		return ret;
	}
	cw_bat->battery_id = cw_get_battid_resister(cw_bat);


	cw_bat->vcc_i2c = regulator_get(cw_bat->dev, "vcc_i2c");
	if (IS_ERR(cw_bat->vcc_i2c)) {
		pr_err("cw2017: Regulator get failed vdd ret=%d\n", ret);
	}
	if (regulator_count_voltages(cw_bat->vcc_i2c) > 0) {
		ret = regulator_set_voltage(cw_bat->vcc_i2c, SMB_VTG_MIN_UV,
					   SMB_VTG_MAX_UV);
		if (ret) {
			pr_err("cw2017: Regulator set_vtg failed vdd ret=%d\n", ret);
		}
	}
	ret = regulator_enable(cw_bat->vcc_i2c);
	if (ret) {
		pr_err("cw2017: Regulator vdd enable failed ret=%d\n", ret);
	}


	ret = cw_get_battery_profile(cw_bat);
	if (ret < 0) {
		pr_err("cw2017: cw_get_batt_profile failed ret=%d\n", ret);
	}


    ret = cw_init(cw_bat);
    while ((loop++ < 3) && (ret != 0)) {
		msleep(200);
        ret = cw_init(cw_bat);
    }
    if(ret < 0) {
		pr_err("%s : cw2017 init fail!\n", __func__);
        return ret;
    }

	ret = cw_init_config(cw_bat);
	if(ret < 0) {
		pr_err("%s : cw2017 init config fail!\n", __func__);
		return ret;
	}

	ret = cw_init_data(cw_bat);
    if(ret < 0) {
		pr_err("%s : cw2017 init data fail!\n", __func__);
        return ret;
    }

#ifdef CW_PROPERTIES
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	cw_bat->cw_bat.name = CW_PROPERTIES;
	cw_bat->cw_bat.type = POWER_SUPPLY_TYPE_BMS;
	cw_bat->cw_bat.properties = cw_battery_properties;
	cw_bat->cw_bat.num_properties = ARRAY_SIZE(cw_battery_properties);
	cw_bat->cw_bat.get_property = cw_battery_get_property;
	cw_bat->cw_bat.set_property = cw_battery_set_property;
	cw_bat->cw_bat.property_is_writeable = cw_battery_prop_is_writeable;

	ret = power_supply_register(&client->dev, &cw_bat->cw_bat);
	if(ret < 0) {
	    power_supply_unregister(&cw_bat->cw_bat);
		pr_err(KERN_ERR"cw2017: failed to register battery: %d \n", ret);
	    return ret;
	}
#else
	psy_desc = devm_kzalloc(&client->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;

	psy_cfg.drv_data = cw_bat;
	psy_desc->name = CW_PROPERTIES;
	psy_desc->type = POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = cw_battery_properties;
	psy_desc->num_properties = ARRAY_SIZE(cw_battery_properties);
	psy_desc->get_property = cw_battery_get_property;
	psy_desc->set_property = cw_battery_set_property;
	psy_desc->property_is_writeable = cw_battery_prop_is_writeable;
	cw_bat->cw_bat = power_supply_register(&client->dev, psy_desc, &psy_cfg);
	if(IS_ERR(cw_bat->cw_bat)) {
		ret = PTR_ERR(cw_bat->cw_bat);
	    pr_err(KERN_ERR"[cw2017] failed to register battery: %d\n", ret);
	    return ret;
	}
#endif
#endif

	cw_bat->cwfg_workqueue = create_singlethread_workqueue("cwfg_gauge");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work , msecs_to_jiffies(50));


#ifdef CW2017_INTERRUPT
	INIT_DELAYED_WORK(&cw_bat->interrupt_work, interrupt_work_do_wakeup);
	wake_lock_init(&cw2017_wakelock, WAKE_LOCK_SUSPEND, "cw2017_detect");
	if (client->irq > 0) {
			irq = client->irq;
			ret = request_irq(irq, ops_cw2017_int_handler_int_handler, IRQF_TRIGGER_FALLING, "cw2017_detect", cw_bat);
			if (ret < 0) {
					printk(KERN_ERR"fault interrupt registration failed err = %d\n", ret);
			}
			enable_irq_wake(irq);
	}
#endif/*CW2017_INTERRUPT*/


	pr_err("cw2017: driver probe success\n");
    return 0;
}

static int cw2017_remove(struct i2c_client *client)
{
	cw_printk("\n");
	return 0;
}

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
        cancel_delayed_work(&cw_bat->battery_delay_work);
        return 0;
}

static int cw_bat_resume(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
        queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(2));
        return 0;
}

static const struct dev_pm_ops cw_bat_pm_ops = {
        .suspend  = cw_bat_suspend,
        .resume   = cw_bat_resume,
};
#endif

static const struct i2c_device_id cw2017_id_table[] = {
	{CWFG_NAME, 0},
	{}
};

static struct of_device_id cw2017_match_table[] = {
	{ .compatible = "cellwise,cw2017", },
	{ },
};

static struct i2c_driver cw2017_driver = {
	.driver 	  = {
		.name = CWFG_NAME,
#ifdef CONFIG_PM
        .pm     = &cw_bat_pm_ops,
#endif
		.owner	= THIS_MODULE,
		.of_match_table = cw2017_match_table,
	},
	.probe		  = cw2017_probe,
	.remove 	  = cw2017_remove,
	.id_table = cw2017_id_table,
};

/*
static struct i2c_board_info __initdata fgadc_dev = {
	I2C_BOARD_INFO(CWFG_NAME, 0x63)
};
*/

static int __init cw2017_init(void)
{







    i2c_add_driver(&cw2017_driver);
    return 0;
}

static void __exit cw2017_exit(void)
{
    i2c_del_driver(&cw2017_driver);
}

module_init(cw2017_init);
module_exit(cw2017_exit);

MODULE_AUTHOR("Chaman Qi");
MODULE_DESCRIPTION("CW2017 FGADC Device Driver V0.5");
MODULE_LICENSE("GPL");
