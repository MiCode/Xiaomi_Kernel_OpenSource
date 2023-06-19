#define pr_fmt(fmt) "nopmi_chg %s: " fmt, __func__

#include "nopmi_chg.h"
#include "touch-charger.h"
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>

#define PROBE_CNT_MAX	100
#define MAIN_CHG_SUSPEND_VOTER "MAIN_CHG_SUSPEND_VOTER"
#define CHG_INPUT_SUSPEND_VOTER "CHG_INPUT_SUSPEND_VOTER"
#define THERMAL_DAEMON_VOTER            "THERMAL_DAEMON_VOTER"
#define MAIN_ICL_MIN 100
#define SLOW_CHARGING_CURRENT_STANDARD 400
#define UN_VERIFIED_PD_CHG_CURR  5400
//wangwei add module name start
#define SC_CHIP_ID   0x51
#define LN_CHIP_ID   0x42
#define CP_SIZE      32

#define RT1715_CHIP_ID 0x2173
#define SC2150A_CHIP_ID 0x0000
#define DS28E16_CHIP_ID 0x28e16

#define COSMX_BATT_ID     1
#define SUNWODA_BATT_ID   2
#define NVT_BATT_ID     3
typedef struct LCT_DEV_ID{
	u8 id;
	char *name;
	char *mfr;
}LCT_DEV_ID_NAME_MFR;
LCT_DEV_ID_NAME_MFR lct_dev_id_name_mfr[] = {
	{SC_CHIP_ID, "SC8551", "SOUTHCHIP"},
	{LN_CHIP_ID, "LN8000", "LION"},
};

typedef struct CC_DEV_ID{
	int id;
	char *name;
	char *mfr;
}CC_DEV_ID_NAME_MFR;
CC_DEV_ID_NAME_MFR cc_dev_id_name_mfr[] = {
	{RT1715_CHIP_ID, "RT1715", "RICHTEK"},
	{SC2150A_CHIP_ID, "SC2150A", "SOUTHCHIP"},
};

typedef struct DS_DEV_ID{
	int id;
	char *name;
	char *mfr;
}DS_DEV_ID_NAME_MFR;
DS_DEV_ID_NAME_MFR ds_dev_id_name_mfr[] = {
	{DS28E16_CHIP_ID, "DS28E16", "Maxim"},
};

typedef struct BATT_DEV_ID{
	int id;
	char *mfr;
}BATT_DEV_ID_MFR;
BATT_DEV_ID_MFR batt_dev_id_mfr[] = {
	{COSMX_BATT_ID, "COSMX"},
	{SUNWODA_BATT_ID, "SUNWODA"},
	{NVT_BATT_ID, "NVT"},
};
//wangwei add module name end

extern bool g_ffc_disable;
extern int adapter_dev_get_pd_verified(void);

static const int NOPMI_CHG_WORKFUNC_GAP = 10000;
static const int NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP = 2000;
static const int NOPMI_CHG_WORKFUNC_FIRST_GAP = 5000;

struct nopmi_chg *g_nopmi_chg = NULL;
EXPORT_SYMBOL_GPL(g_nopmi_chg);

struct step_config cc_cv_step_config[STEP_TABLE_MAX] = {
	{4240,    5400},
	{4440,    3920},
};
//longcheer nielianjie10 2022.12.05 Set CV according to circle count
struct step_config cc_cv_step1_config[STEP_TABLE_MAX] = {
        {4192,    5400},
        {4440,    3920},
};

static void start_nopmi_chg_workfunc(void);
static void stop_nopmi_chg_workfunc(void);

static const char * const power_supply_usbc_text[] = {
	"Nothing attached", "Sink attached", "Powered cable w/ sink",
	"Debug Accessory", "Audio Adapter", "Powered cable w/o sink",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
};

static const char *get_usbc_text_name(u32 usb_type)
{
	u32 i = 0;

	for (i = 0; i < ARRAY_SIZE(power_supply_usbc_text); i++) {
		if (i == usb_type)
			return power_supply_usbc_text[i];
	}
	return "Unknown";
}

static const char * const power_supply_usb_type_text[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB",
	"USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "USB_PD_DRP", "BrickID", "Wireless",
	"USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP_3P5", "USB_FLOAT",
	"BMS", "Parallel", "Main","Wipower","UFP","DFP","Charge_Pump",
};
//add ipc log start
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
	#if IS_ENABLED(CONFIG_DEBUG_OBJECTS)
		#define IPC_CHARGER_DEBUG_LOG
	#endif
#endif

#ifdef IPC_CHARGER_DEBUG_LOG
extern void *charger_ipc_log_context;

#define nopmi_err(fmt,...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#undef pr_err
#define pr_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "nopmi_chg: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#undef pr_info
#define pr_info(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_INFO pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "nopmi_chg: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#else
#define nopmi_err(fmt,...)
#endif
//add ipc log end

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i = 0;

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}
	return "Unknown";
}
static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0 ;

	if(!chg){
		pr_err("chg is null\n");
	}
	if (chg->pd_active) {
		val = POWER_SUPPLY_TYPE_USB_PD;
	} else {
		val = chg->real_type;
	}
	pr_info("real_type=%d\n",val);

	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(usb_real_type);
static ssize_t real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;

	if(!chg){
		pr_err("chg is null\n");
	}
	if (chg->pd_active) {
		val = POWER_SUPPLY_TYPE_USB_PD;
	} else {
		val = chg->real_type;
	}
	pr_info("real_type=%d\n",val);

	return scnprintf(buf, PAGE_SIZE, "%s", get_usb_type_name(val));
}
static CLASS_ATTR_RO(real_type);

static ssize_t shutdown_delay_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	int rc = 0;

	rc = nopmi_chg_get_iio_channel(chg, NOPMI_BMS,FG_SHUTDOWN_DELAY, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio shutdown_delay \n");
		return -ENODATA;
	}

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static CLASS_ATTR_RO(shutdown_delay);

static ssize_t mtbf_current_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->mtbf_cur);
}
static ssize_t mtbf_current_store(struct class *c,
				struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	int rc = 0;

	rc = kstrtoint(buf, 10, &val);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}
	chg->mtbf_cur = val;
	return len;
}
static CLASS_ATTR_RW(mtbf_current);

/*longcheer nielianjie10 2022.12.05 Set CV according to circle count start*/
static ssize_t cycle_count_select_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
			battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->cycle_count);
}
static ssize_t cycle_count_select_store(struct class *c,
		struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,battery_class);
        int val = 0;
        int rc = 0;

	rc = kstrtoint(buf, 10, &val);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}
	chg->cycle_count = val;

	return len;
}
static CLASS_ATTR_RW(cycle_count_select);
/*longcheer nielianjie10 2022.12.05 Set CV according to circle count end*/

static ssize_t cp_bus_current_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int rc = 0;
	int bus_current = 0;

	rc = nopmi_chg_get_iio_channel(chg,
			NOPMI_CP_MASTER, CHARGE_PUMP_BUS_CURRENT, &bus_current);
	if (rc < 0) {
		pr_err("[%s] Couldn't get iio [%s], ret = %d\n",
				__func__, cp_ext_iio_chan_name[CHARGE_PUMP_BUS_CURRENT], rc);
		return -ENODATA;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", bus_current);
}
static CLASS_ATTR_RO(cp_bus_current);

static ssize_t resistance_id_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int rc = 0;
	int resistance_id = 0;

	rc = nopmi_chg_get_iio_channel(chg,
			NOPMI_BMS, FG_RESISTANCE_ID, &resistance_id);
	if (rc < 0) {
		pr_err("[%s] Couldn't get iio [%s], ret = %d\n",
				__func__, fg_ext_iio_chan_name[FG_RESISTANCE_ID], rc);
		return -ENODATA;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", resistance_id);
}
static CLASS_ATTR_RO(resistance_id);

/* longcheer nielianjie10 2022.11.23 get battery id from gauge start */
static ssize_t fg_batt_id_show(struct class *c,
                                struct class_attribute *attr, char *buf)
{
        struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
                                                battery_class);
        int rc = 0;
        int batt_id = 0;

        rc = nopmi_chg_get_iio_channel(chg,
                        NOPMI_BMS, FG_BATT_ID, &batt_id);
        if (rc < 0) {
                pr_err("Couldn't get iio [%s], ret = %d\n",
                                fg_ext_iio_chan_name[FG_BATT_ID], rc);
                return -ENODATA;
        }

        return scnprintf(buf, PAGE_SIZE, "%d\n", batt_id);
}
static CLASS_ATTR_RO(fg_batt_id);
/* longcheer nielianjie10 2022.11.23 get battery id from gauge end */

/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  start */
static ssize_t soc_decimal_show(struct class *c,
				  struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *nopmi_chg = container_of(c, struct nopmi_chg,
                                                battery_class);
	union power_supply_propval pval={0, };
	int val = 0;

	val = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_BMS,FG_SOC_DECIMAL, &pval.intval);
	if (val) {
		pr_err("%s: Get fg_soc_decimal channel Error !\n", __func__);
		return -EINVAL;
	}
	val = pval.intval;

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *nopmi_chg = container_of(c, struct nopmi_chg,
						battery_class);
	union power_supply_propval pval={0, };
	int val= 0;

	val = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_BMS,FG_SOC_RATE_DECIMAL, &pval.intval);
	if (val) {
		pr_err("%s: Get fg_soc_decimal channel Error !\n", __func__);
		return -EINVAL;
	}
	val = pval.intval;
	if(val > 100 || val < 0)
		val = 0;

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static CLASS_ATTR_RO(soc_decimal_rate);
/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  end*/

static int nopmi_set_prop_input_suspend(struct nopmi_chg *nopmi_chg,const int value);
static ssize_t input_suspend_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
    val = chg->input_suspend;

	return scnprintf(buf, sizeof(int), "%d\n", val);
}

static ssize_t input_suspend_store(struct class *c,
				struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
    int rc = 0;

	rc = kstrtoint(buf, 10, &val);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	rc = nopmi_set_prop_input_suspend(chg, val);
	return len;
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t thermal_level_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);

	return scnprintf(buf, sizeof(int), "%d\n", chg->system_temp_level);
}

static int nopmi_set_prop_system_temp_level(struct nopmi_chg *nopmi_chg,
				const union power_supply_propval *val);
static ssize_t thermal_level_store(struct class *c,
				struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	union power_supply_propval val = {0, };
    int rc = 0;

	rc = kstrtoint(buf, 10, &val.intval);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	rc = nopmi_set_prop_system_temp_level(chg, &val);
	if (rc < 0) {
		pr_err("%s set thermal_level fail\n", __func__);
		return -EINVAL;
	}

	return len;
}
static CLASS_ATTR_RW(thermal_level);

static ssize_t typec_cc_orientation_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	if (chg->typec_mode != QTI_POWER_SUPPLY_TYPEC_NONE) {
		val = chg->cc_orientation +1;
	} else {
		val = 0;
	}
	return scnprintf(buf, sizeof(int), "%d\n", val);
}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t typec_mode_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	val = chg->typec_mode;

	return scnprintf(buf, PAGE_SIZE, "%s \n", get_usbc_text_name(val));
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t quick_charge_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct power_supply *psy = NULL;
	int val = 0;

	psy = power_supply_get_by_name("usb");
	if (!psy) {
		pr_err("%s get usb psy fail\n", __func__);
		return -EINVAL;
	} else {
		val = nopmi_get_quick_charge_type(psy);
		pr_err("%s nopmi_get_quick_charge_type[%d]\n", __func__, val);
		power_supply_put(psy);
	}

	return scnprintf(buf, sizeof(int), "%d", val);
}
static CLASS_ATTR_RO(quick_charge_type);

//HTH-261991 longcheer add chip_ok and authentic node start
static ssize_t authentic_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *nopmi_chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	int ret = 0;
	ret = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_DS,DS_AUTHEN_RESULT, &val);
	if (ret) {
		pr_err("%s: Get fg_soc_decimal channel Error !\n", __func__);
		return -EINVAL;
	}
	return scnprintf(buf, sizeof(int), "%d\n", val);
}
static CLASS_ATTR_RO(authentic);
static ssize_t chip_ok_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *nopmi_chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	int ret = 0;
	ret = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_DS,DS_CHIP_OK, &val);
	if (ret) {
		pr_err("%s: Get fg_soc_decimal channel Error !\n", __func__);
		return -EINVAL;
	}
	return scnprintf(buf, sizeof(int), "%d\n", val);
}
static CLASS_ATTR_RO(chip_ok);
//HTH-261991 longcheer add chip_ok and authentic node end

//HTH-262290 add pd_verifed and apdo_max node start
static ssize_t pd_verifed_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int pd_verifed = 0;

	if (NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type())
		pd_verifed = adapter_dev_get_pd_verified();
	else
		pd_verifed = 1;

	return scnprintf(buf, sizeof(int), "%d\n", pd_verifed);
}
static CLASS_ATTR_RO(pd_verifed);

static ssize_t apdo_max_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	int apdo_max = 0;

	apdo_max = (g_nopmi_chg->apdo_curr * g_nopmi_chg->apdo_volt)/1000000;
	return scnprintf(buf, sizeof(int), "%d\n", apdo_max);
}
static CLASS_ATTR_RO(apdo_max);
//HTH-262290 add pd_verifed and apdo_max node end

//wangwei add module name start
static ssize_t cp_modle_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = 0;
	int rc = 0;
	int i;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg, NOPMI_CP_MASTER,CHARGE_PUMP_CHIP_ID, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio chip id \n");
		return -ENODATA;
	}
	pr_info("longcheer get chip id is %x\n",val);
	for (i = 0; i < ARRAY_SIZE(lct_dev_id_name_mfr); i++)
	{
		if(val == lct_dev_id_name_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", lct_dev_id_name_mfr[i].name);
	}
	return -ENODATA;
}
static DEVICE_ATTR(cp_modle_name,0660,cp_modle_name_show,NULL);
static ssize_t cp_manufacturer_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = 0;
	int rc = 0;
	int i;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg, NOPMI_CP_MASTER,CHARGE_PUMP_CHIP_ID, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio chip id \n");
		return -ENODATA;
	}
	pr_info("longcheer get chip id is %x\n",val);
	for (i = 0; i < ARRAY_SIZE(lct_dev_id_name_mfr); i++)
	{
		if(val == lct_dev_id_name_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", lct_dev_id_name_mfr[i].mfr);
	}
		return -ENODATA;
}
static DEVICE_ATTR(cp_manufacturer,0664,cp_manufacturer_show,NULL);

static ssize_t cc_modle_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = 0;
	int rc = 0;
	int i;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg, NOPMI_CC,CC_CHIP_ID, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio chip id \n");
		return -ENODATA;
	}
	pr_info("longcheer get chip id is %x\n",val);
	for (i = 0; i < ARRAY_SIZE(cc_dev_id_name_mfr); i++)
	{
		if(val == cc_dev_id_name_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", cc_dev_id_name_mfr[i].name);
	}
		return -ENODATA;
}
static DEVICE_ATTR(cc_modle_name,0660,cc_modle_name_show,NULL);
static ssize_t cc_manufacturer_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = 0;
	int rc = 0;
	int i;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg, NOPMI_CC,CC_CHIP_ID, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio chip id \n");
		return -ENODATA;
	}
	pr_info("longcheer get chip id is %x\n",val);
	for (i = 0; i < ARRAY_SIZE(cc_dev_id_name_mfr); i++)
	{
		if(val == cc_dev_id_name_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", cc_dev_id_name_mfr[i].mfr);
	}
		return -ENODATA;
}
static DEVICE_ATTR(cc_manufacturer,0664,cc_manufacturer_show,NULL);

static ssize_t ds_modle_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = 0;
	int rc = 0;
	int i;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg, NOPMI_DS,DS_CHIP_ID, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio chip id \n");
		return -ENODATA;
	}
	pr_info("longcheer get chip id is %x\n",val);
	for (i = 0; i < ARRAY_SIZE(ds_dev_id_name_mfr); i++)
	{
		if(val == ds_dev_id_name_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", ds_dev_id_name_mfr[i].name);
	}
		return -ENODATA;
}
static DEVICE_ATTR(ds_modle_name,0660,ds_modle_name_show,NULL);
static ssize_t ds_manufacturer_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = 0;
	int rc = 0;
	int i;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg, NOPMI_DS,DS_CHIP_ID, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio chip id \n");
		return -ENODATA;
	}
	pr_info("longcheer get chip id is %x\n",val);
	for (i = 0; i < ARRAY_SIZE(ds_dev_id_name_mfr); i++)
	{
		if(val == ds_dev_id_name_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", ds_dev_id_name_mfr[i].mfr);
	}
		return -ENODATA;
}
static DEVICE_ATTR(ds_manufacturer,0664,ds_manufacturer_show,NULL);

static ssize_t batt_manufacturer_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int batt_id = 0;
	int i = 0;

	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_BMS, FG_RESISTANCE_ID, &batt_id);
	if (rc < 0) {
		pr_err("[%s] Couldn't get iio batt id, ret = %d\n",rc);
		return -ENODATA;
	}
	for (i = 0; i < ARRAY_SIZE(batt_dev_id_mfr); i++)
	{
		if(batt_id == batt_dev_id_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", batt_dev_id_mfr[i].mfr);
	}
		return -ENODATA;
}
static DEVICE_ATTR(batt_manufacturer,0664,batt_manufacturer_show,NULL);

static ssize_t fastcharge_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int fast_mode = 0;

	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_BMS, FG_FASTCHARGE_MODE, &fast_mode);
	if (rc < 0) {
		pr_err("Couldn't get iio fastcharge mode\n");
		return -ENODATA;
	}

	return scnprintf(buf, sizeof(int), "%d\n", fast_mode);
}
static DEVICE_ATTR(fastcharge_mode,0660,fastcharge_mode_show,NULL);
//wangwei add module name end

static struct attribute *battery_class_attrs[] = {
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_real_type.attr,
	&class_attr_chip_ok.attr,
	&class_attr_typec_mode.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_resistance_id.attr,
	&class_attr_cp_bus_current.attr,
	&class_attr_input_suspend.attr,
	&class_attr_quick_charge_type.attr,
	&class_attr_mtbf_current.attr,
	//&class_attr_resistance.attr,
	&class_attr_authentic.attr,
	&class_attr_pd_verifed.attr,
	&class_attr_apdo_max.attr,
	&class_attr_thermal_level.attr,
	&class_attr_fg_batt_id.attr,
	&class_attr_cycle_count_select.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

static struct attribute *battery_attributes[] = {
	&dev_attr_cp_modle_name.attr,
	&dev_attr_cp_manufacturer.attr,
	&dev_attr_cc_modle_name.attr,
	&dev_attr_cc_manufacturer.attr,
	&dev_attr_ds_modle_name.attr,
	&dev_attr_ds_manufacturer.attr,
	&dev_attr_batt_manufacturer.attr,
	//&dev_attr_real_type.attr,
	//&dev_attr_input_suspend.attr,
	&dev_attr_fastcharge_mode.attr,
	NULL,
};

static const struct attribute_group battery_attr_group = {
	.attrs = battery_attributes,
};

static const struct attribute_group *battery_attr_groups[] = {
	&battery_attr_group,
	NULL,
};

static int nopmi_chg_init_dev_class(struct nopmi_chg *chg)
{
	int rc = -EINVAL;
	if(!chg)
		return rc;

	chg->battery_class.name = "qcom-battery";
	chg->battery_class.class_groups = battery_class_groups;
	rc = class_register(&chg->battery_class);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
	}

	chg->batt_device.class = &chg->battery_class;
	dev_set_name(&chg->batt_device, "odm_battery");
	chg->batt_device.parent = chg->dev;
	chg->batt_device.groups = battery_attr_groups;
	rc = device_register(&chg->batt_device);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
	}

	return rc;
}

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uvent(struct work_struct *work)
{
	int count;
	struct nopmi_chg *chg = container_of(work, struct nopmi_chg, xm_prop_change_work.work);
	union power_supply_propval pval = {0, };
	int val = 0;

	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SOC_DECIMAL=\n",	//length=31+8
		"POWER_SUPPLY_SOC_DECIMAL_RATE=\n",	//length=31+8
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n",//28+8
		"POWER_SUPPLY_REAL_TYPE=\n", //23
		"POWER_SUPPLY_QUICK_CHARGE_TYPE=\n", //31+8
	};
	static char *envp[] = {
		uevent_string[0],
		uevent_string[1],
		uevent_string[2],
		uevent_string[3],
		uevent_string[4],
		NULL,

	};
	char *prop_buf = NULL;

	count = chg->update_cont;
	if(chg->update_cont < 0)
		return;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return;
/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  start */
#if 1
	soc_decimal_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[0]+25, prop_buf,MAX_UEVENT_LENGTH-25);

	soc_decimal_rate_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[1]+30, prop_buf,MAX_UEVENT_LENGTH-30);

	shutdown_delay_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[2]+28, prop_buf,MAX_UEVENT_LENGTH-28);
#endif
/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  end */

	real_type_show(&(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[3]+23, prop_buf,MAX_UEVENT_LENGTH-23);


	quick_charge_type_show(&(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[4]+31, prop_buf,MAX_UEVENT_LENGTH-31);

	pr_info("uevent test : %s %s %s %s %s count=%d\n",
			envp[0], envp[1], envp[2], envp[3], envp[4], count);

	/*add our prop end*/

	kobject_uevent_env(&chg->dev->kobj, KOBJ_CHANGE, envp);

	free_page((unsigned long)prop_buf);
	chg->update_cont = count - 1;

/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  start */
#if 1
	if(chg->bms_psy == NULL){
		pr_err("uevent: chg->bms_psy addr is NULL !");
		return;
	}
	val = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if(val){
		pr_err("uevent: BMS PSY get CAPACITY Error !\n");
                return;
	}else if(pval.intval > 1){
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(500));
	}
	else{
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(2000));
	}
#endif
/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  end */

	return;
}

static bool is_bms_chan_valid(struct nopmi_chg *chip,
		enum fg_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->fg_ext_iio_chans[chan]))
		return false;

	if (!chip->fg_ext_iio_chans[chan]) {
		chip->fg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					fg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->fg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->fg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->fg_ext_iio_chans[chan] = NULL;

			pr_err("Failed to get IIO channel %s, rc=%d\n",
				fg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

bool is_cp_chan_valid(struct nopmi_chg *chip,
		enum cp_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_ext_iio_chans[chan]))
		return false;

	if (!chip->cp_ext_iio_chans[chan]) {
		chip->cp_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cp_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cp_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

bool is_cc_chan_valid(struct nopmi_chg *chip,
		enum cc_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cc_ext_iio_chans[chan]))
		return false;

	if (!chip->cc_ext_iio_chans[chan]) {
		chip->cc_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cc_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->cc_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cc_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cc_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cc_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

bool is_ds_chan_valid(struct nopmi_chg *chip,
		enum ds_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->ds_ext_iio_chans[chan]))
		return false;

	if (!chip->ds_ext_iio_chans[chan]) {
		chip->ds_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					ds_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->ds_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->ds_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->ds_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				ds_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_main_chg_chan_valid(struct nopmi_chg *chip,
		enum main_chg_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->main_chg_ext_iio_chans[chan]))
		return false;

	if (!chip->main_chg_ext_iio_chans[chan]) {
		chip->main_chg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					main_chg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->main_chg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->main_chg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->main_chg_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				main_chg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

int get_prop_battery_charging_enabled(struct votable *usb_icl_votable,
					int *value)
{
  	// reslove not enter pd charge issue
	int icl = MAIN_ICL_MIN;
	// end add.
	*value = !(get_client_vote(usb_icl_votable, MAIN_CHG_SUSPEND_VOTER) == icl);

	return 0;
}

int set_prop_battery_charging_enabled(struct votable *usb_icl_votable,
				const int value)
{
  	// reslove not enter pd charge issue
	int icl = MAIN_ICL_MIN;
  	// end add.
	int ret = 0;

	if (value == 0) {
		ret = vote(usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, true, icl);
	} else {
		ret = vote(usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, false, 0);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(get_prop_battery_charging_enabled);
EXPORT_SYMBOL_GPL(set_prop_battery_charging_enabled);

static int nopmi_set_prop_input_suspend(struct nopmi_chg *nopmi_chg,
				const int value)
{
	int rc;

	if (!nopmi_chg->usb_icl_votable) {
		nopmi_chg->usb_icl_votable  = find_votable("USB_ICL");
		printk(KERN_ERR "%s nopmi_chg usb_icl_votable is NULL, find FCC vote\n", __func__);

		if(!nopmi_chg->usb_icl_votable)
		{
			printk(KERN_ERR "%s nopmi_chg usb_icl_votable is NULL, ERR. \n", __func__);
			return -2;
		}
	}

	if (!nopmi_chg->fcc_votable) {
		nopmi_chg->fcc_votable  = find_votable("FCC");
		printk(KERN_ERR "%s nopmi_chg fcc_votable is NULL, find FCC vote\n", __func__);

		if(!nopmi_chg->fcc_votable)
		{
			printk(KERN_ERR "%s nopmi_chg fcc_votable is NULL, ERR. \n", __func__);
			return -2;
		}
	}

	if (!nopmi_chg->chg_dis_votable) {
		nopmi_chg->chg_dis_votable  = find_votable("CHG_DISABLE");
		printk(KERN_ERR "%s nopmi_chg chg_dis_votable is NULL, find FCC vote\n", __func__);

		if(!nopmi_chg->chg_dis_votable)
		{
			printk(KERN_ERR "%s nopmi_chg chg_dis_votable is NULL, ERR. \n", __func__);
			return -2;
		}
	}

	if (value) {
		pr_info("%s : enable usb_icl_votable to set current limit MIN\n", __func__);
		rc = vote(nopmi_chg->fcc_votable, CHG_INPUT_SUSPEND_VOTER, true, 0);
		rc |= vote(nopmi_chg->usb_icl_votable, CHG_INPUT_SUSPEND_VOTER, true, MAIN_ICL_MIN);
		rc |= vote(nopmi_chg->chg_dis_votable, CHG_INPUT_SUSPEND_VOTER, true, 0);
	} else {
		pr_info("%s : disenable usb_icl_votable\n", __func__);
		rc = vote(nopmi_chg->fcc_votable, CHG_INPUT_SUSPEND_VOTER, false, 0);
		rc |= vote(nopmi_chg->usb_icl_votable, CHG_INPUT_SUSPEND_VOTER, false, 0);
		rc |= vote(nopmi_chg->chg_dis_votable, CHG_INPUT_SUSPEND_VOTER, false, 0);
	}
	if (rc < 0) {
		pr_err("%s : Couldn't vote to %s USB rc = %d\n",
			__func__, (bool)value ? "suspend" : "resume", rc);
		return rc;
	}

	nopmi_chg->input_suspend = !!(value);

	return rc;
}

#define THERMAL_CALL_LEVEL
static int nopmi_set_prop_system_temp_level(struct nopmi_chg *nopmi_chg,
				const union power_supply_propval *val)
{
	int  rc = 0;

	if (val->intval < 0 ||
		nopmi_chg->thermal_levels <=0 ||
		val->intval > nopmi_chg->thermal_levels)
		return -EINVAL;

	if (val->intval == nopmi_chg->system_temp_level)
		return rc;

	nopmi_chg->system_temp_level = val->intval;
	/*if temp level at max and should be disable buck charger(vote icl as 0) & CP(vote ffc as 0) */
	if (nopmi_chg->system_temp_level == nopmi_chg->thermal_levels) { // thermal temp level
		rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true, 0);
		if(rc < 0){
			pr_err("%s : icl vote failed\n", __func__);
		}
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, true, 0);
		if(rc < 0){
			pr_err("%s : fcc vote failed \n", __func__);
		}
        	return rc;
	}

	/*if temp level exit max value and enable icl, and then continues fcc vote*/
	rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, false, 0);

	if (nopmi_chg->system_temp_level == 0) {
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
	} else {
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
			nopmi_chg->thermal_mitigation[nopmi_chg->system_temp_level] / 1000);//divide 1000 to match maxim driver fcc as mA
	}

#ifdef THERMAL_CALL_LEVEL // add level16 and level17 to limit IBUS to slove audio noise
	if (nopmi_chg->system_temp_level == 16 || nopmi_chg->system_temp_level == 17) {
		if (nopmi_chg->pd_active ||
				nopmi_chg->real_type == QTI_POWER_SUPPLY_TYPE_USB_HVDCP ) {
			rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true, 600);
		} else {
			rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true, 800);
		}
	}
#endif

	return rc;
}

static int nopmi_get_batt_health(struct nopmi_chg *nopmi_chg)
{
	union power_supply_propval pval = {0, };
	int ret;

	if (nopmi_chg == NULL) {
		pr_err("%s : nopmi_chg is null,can not use\n", __func__);
		return -EINVAL;
	}

	nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		pr_err("couldn't read batt temp property, ret=%d\n", ret);
		return -EINVAL;
	}

	pval.intval = pval.intval /10;

	if(pval.intval >= 60)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	}
	else if(pval.intval >= 58 && pval.intval < 60)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_HOT;
	}
	else if(pval.intval >= 45 && pval.intval < 58)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_WARM;
	}
	else if(pval.intval >= 15 && pval.intval < 45)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	}
	else if(pval.intval >= 0 && pval.intval < 15)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_COOL;
	}
	else if(pval.intval < 0)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_COLD;
	}

	return nopmi_chg->batt_health;
}

static enum power_supply_property nopmi_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int nopmi_batt_get_prop_internal(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;
	static int vbat_mv = 3800;
	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		pval->intval = nopmi_get_batt_health(nopmi_chg);
		//pval->intval = 1;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
			pval->intval = 1;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:

		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if(nopmi_chg->main_psy)
			rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_MAIN,
				MAIN_CHARGE_TYPE, &pval->intval);
		//pval->intval = 2;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:

		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        rc = power_supply_get_property(nopmi_chg->main_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		rc = power_supply_get_property(nopmi_chg->main_psy, psp, pval);
		if(pval->intval == POWER_SUPPLY_STATUS_FULL)
			pval->intval = POWER_SUPPLY_STATUS_FULL;
		else if (nopmi_chg->input_suspend)
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (((pval->intval == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(pval->intval == POWER_SUPPLY_STATUS_NOT_CHARGING)) &&
			(nopmi_chg->real_type > 0))
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (nopmi_chg->pd_active)
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;
		if (vbat_mv < 3300) {
			pval->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			if (!nopmi_chg->batt_psy)
				nopmi_chg->batt_psy = power_supply_get_by_name("battery");
			if (nopmi_chg->batt_psy)
				power_supply_changed(nopmi_chg->batt_psy);
			//pr_err("%s vbat_mv = %d\n", __func__, vbat_mv);
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, pval);
		vbat_mv = pval->intval / 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY, pval);
		if(!rc && nopmi_chg && pval->intval <= 1 )
		{
			rc = pval->intval;
			power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, pval);
			vbat_mv = pval->intval / 1000;
			nopmi_chg->update_cont = 15;
			if(vbat_mv > 3350){
				cancel_delayed_work_sync(&nopmi_chg->xm_prop_change_work);
				schedule_delayed_work(&nopmi_chg->xm_prop_change_work, msecs_to_jiffies(100));
			}else{
				generate_xm_charge_uvent(&nopmi_chg->xm_prop_change_work.work);
			}
			pval->intval = rc;
		}
		pr_info("rc = %d uisoc = %d \n", rc, pval->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_TEMP:
	//case POWER_SUPPLY_PROP_SOC_DECIMAL:
	//case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	//case POWER_SUPPLY_PROP_RESISTANCE_ID:
	//case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		rc = power_supply_get_property(nopmi_chg->bms_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pval->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		rc = power_supply_get_property(nopmi_chg->bms_psy,
			POWER_SUPPLY_PROP_CAPACITY_LEVEL, pval);
		break;
	default:
		break;
	}
	if (rc < 0) {
		pr_debug("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return 0;
}

static int nopmi_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int ret = 0;

	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
//		ret = max77729_batt_get_property(psy, psp, pval);
	}
	else
	{
		ret = nopmi_batt_get_prop_internal(psy, psp, pval);
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = nopmi_chg->system_temp_level;
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = nopmi_chg->thermal_levels;
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

static int nopmi_batt_set_prop_internal(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	//struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;
	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		break;
	default:
		rc = -EINVAL;
	}

	return rc;
}
static int nopmi_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int ret = 0;

	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
//		ret = max77729_batt_set_property(psy, prop, val);
	}
	else
	{
		ret = nopmi_batt_set_prop_internal(psy, prop, val);
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		ret = nopmi_set_prop_system_temp_level(nopmi_chg, val);
		break;
	default:
		break;
	}
	return ret;
}

static int nopmi_batt_prop_is_writeable_internal(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return 1;
	default:
		break;
	}
	return 0;
}

static int nopmi_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		return -1;
//0729		return batt_prop_is_writeable(psy, psp);
	}
	else
	{
		return nopmi_batt_prop_is_writeable_internal(psy, psp);
	}
}

static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = nopmi_batt_props,
	.num_properties = ARRAY_SIZE(nopmi_batt_props),
	.get_property = nopmi_batt_get_prop,
	.set_property = nopmi_batt_set_prop,
	.property_is_writeable = nopmi_batt_prop_is_writeable,
};

static int nopmi_init_batt_psy(struct nopmi_chg *chg)
{
	struct power_supply_config batt_cfg = {};
	int rc = 0;

	if(!chg) {
		pr_err("chg is NULL\n");
		return rc;
	}

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;
	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);

	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	return rc;
}

/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property nopmi_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	//POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_AUTHENTIC,
    POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	//POWER_SUPPLY_PROP_REAL_TYPE,
};

static int nopmi_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	int rc;
	union power_supply_propval value;
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	val->intval = 0;

	//pr_info(" 20220819 %s psp=%d,val->intval=%d",__func__, psp, val->intval);
	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			if(nopmi_chg->real_type > 0)
				val->intval = 1;
			else
				val->intval = 0;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			if(nopmi_chg->usb_online > 0)
				val->intval = 1;
			else
				val->intval = 0;
			ret = 0;

			if(!nopmi_chg->bms_psy)
				nopmi_chg->bms_psy = power_supply_get_by_name("bms");
			if (nopmi_chg->bms_psy) {
				rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
				if (rc < 0) {
					value.intval = 3800000;
					pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_AVG fail\n", __func__);
				}
			}
			if (value.intval < 3300000) {
				val->intval = 0;
				pr_err("%s : low power set POWER_SUPPLY_PROP_ONLINE=%d\n", __func__, val->intval);
			}
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		case POWER_SUPPLY_PROP_CURRENT_NOW:
		case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			if(!nopmi_chg->main_psy)
				nopmi_chg->main_psy = power_supply_get_by_name("bbc");
			if (nopmi_chg->main_psy) {
				rc = power_supply_get_property(nopmi_chg->main_psy, psp, val);
			}
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			break;
		case POWER_SUPPLY_PROP_TYPE:
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_SCOPE:
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			break;
		case POWER_SUPPLY_PROP_POWER_NOW:
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			val->intval = get_effective_result(nopmi_chg->fv_votable);
			ret =0;
			break;
		case POWER_SUPPLY_PROP_AUTHENTIC:
			val->intval = nopmi_chg->in_verified;
		default:
			break;
	}
	return ret;
}

#ifdef CONFIG_TOUCHSCREEN_COMMON
typedef struct touchscreen_usb_piugin_data{
		bool valid;
		bool usb_plugged_in;
		void (*event_callback)(void);
		} touchscreen_usb_piugin_data_t;
touchscreen_usb_piugin_data_t g_touchscreen_usb_pulgin = {0};
EXPORT_SYMBOL(g_touchscreen_usb_pulgin);
#endif

static int nopmi_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int ret = 0;
	int rc;
	//longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic
	union power_supply_propval pval = {0, };
	union power_supply_propval pval2 = {0, };

	pr_info("psp=%d,val->intval=%d", psp, val->intval);
	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			nopmi_chg->usb_online = val->intval;
			/* longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic start */
			if(NOPMI_CHARGER_IC_NONE != nopmi_get_charger_ic_type() && NOPMI_CHARGER_IC_MAX != nopmi_get_charger_ic_type())
			{
				if(nopmi_chg->usb_online)
				{
					pm_stay_awake(nopmi_chg->dev);
					if(!nopmi_chg->fcc_votable){
						nopmi_chg->fcc_votable  = find_votable("FCC");
					}
					rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_DS, DS_AUTHEN_RESULT, &pval.intval);
					if (rc < 0) {
						pr_err("%s : get battery authen_result fail\n",__func__);
					}
					rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_DS, DS_CHIP_OK, &pval2.intval);
					if (rc < 0) {
						pr_err("%s : get battery chip_ok fail\n",__func__);
					}
					if(nopmi_chg->fcc_votable){
						if(pval.intval || pval2.intval){
							vote(nopmi_chg->fcc_votable, BAT_VERIFY_VOTER, true, VERIFY_BAT);
							pr_info("VERIFY_BAT successful !\n");
						}else{
							vote(nopmi_chg->fcc_votable, BAT_VERIFY_VOTER, true, UNVEIRFY_BAT);
							pr_err("VERIFY_BAT fail, current limit !\n");
						}
					}
					start_nopmi_chg_workfunc();
				}else{
					pm_relax(nopmi_chg->dev);
					stop_nopmi_chg_workfunc();
				}
			}
			/* longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic end */
			ret = 0;

			#ifdef CONFIG_TOUCHSCREEN_COMMON
				g_touchscreen_usb_pulgin.usb_plugged_in = nopmi_chg->usb_online;
				if(g_touchscreen_usb_pulgin.valid){
					g_touchscreen_usb_pulgin.event_callback();
				}
			#endif

			break;
		case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			rc = power_supply_set_property(nopmi_chg->main_psy, psp, val);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			break;
		case POWER_SUPPLY_PROP_POWER_NOW:
			break;
		case POWER_SUPPLY_PROP_AUTHENTIC:
			nopmi_chg->in_verified = val->intval;
		/*case POWER_SUPPLY_PROP_REAL_TYPE:
			nopmi_chg->real_type = val->intval;
			break;*/
		default:
			break;
	}

	return ret;
}

static int nopmi_usb_prop_is_writeable_internal(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_POWER_NOW:
	case POWER_SUPPLY_PROP_AUTHENTIC:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return 1;
	default:
		break;
	}

	return 0;

}

static int nopmi_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())
	{
		return -1;
	}
	else
	{
		return nopmi_usb_prop_is_writeable_internal(psy, psp);
	}
}

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB_PD,
	.properties = nopmi_usb_props,
	.num_properties = ARRAY_SIZE(nopmi_usb_props),
	.get_property = nopmi_usb_get_prop,
	.set_property = nopmi_usb_set_prop,
	.property_is_writeable = nopmi_usb_prop_is_writeable,
};

static int nopmi_init_usb_psy(struct nopmi_chg *chg)
{
	struct power_supply_config usb_cfg = {};

	usb_cfg.drv_data = chg;
	usb_cfg.of_node = chg->dev->of_node;
	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}

	return 0;
}

static int nopmi_parse_dt_jeita(struct nopmi_chg *chg, struct device_node *np)
{
	u32 val;

	if (of_property_read_bool(np, "enable_sw_jeita"))
		chg->jeita_ctl.dt.enable_sw_jeita = true;
	else
		chg->jeita_ctl.dt.enable_sw_jeita = false;

	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_above_t4_cv = val;
	else {
		pr_err("use default JEITA_TEMP_ABOVE_T4_CV:%d\n",
		   JEITA_TEMP_ABOVE_T4_CV);
		chg->jeita_ctl.dt.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t3_to_t4_cv	 = val;
	else {
		pr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
		   JEITA_TEMP_T3_TO_T4_CV);
		chg->jeita_ctl.dt.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t2_to_t3_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
		   JEITA_TEMP_T2_TO_T3_CV);
		chg->jeita_ctl.dt.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t1p5_to_t2_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t1p5_to_t2_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T1P5_TO_T2_CV:%d\n",
		   JEITA_TEMP_T1P5_TO_T2_CV);
		chg->jeita_ctl.dt.jeita_temp_t1p5_to_t2_cv = JEITA_TEMP_T1P5_TO_T2_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t1_to_t1p5_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t1_to_t1p5_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T1_TO_T1P5_CV:%d\n",
		   JEITA_TEMP_T1_TO_T1P5_CV);
		chg->jeita_ctl.dt.jeita_temp_t1_to_t1p5_cv = JEITA_TEMP_T1_TO_T1P5_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t0_to_t1_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
		   JEITA_TEMP_T0_TO_T1_CV);
		chg->jeita_ctl.dt.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_tn1_to_t0_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_tn1_to_t0_cv = val;
	else {
		pr_err("use default JEITA_TEMP_TN1_TO_T0_CV:%d\n",
		   JEITA_TEMP_TN1_TO_T0_CV);
		chg->jeita_ctl.dt.jeita_temp_tn1_to_t0_cv = JEITA_TEMP_TN1_TO_T0_CV;
	}
	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_below_t0_cv = val;
	else {
		pr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
		   JEITA_TEMP_BELOW_T0_CV);
		chg->jeita_ctl.dt.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "normal-charge-voltage", &val) >= 0)
		chg->jeita_ctl.dt.normal_charge_voltage = val;
	else {
		pr_err("use default JEITA_TEMP_NORMAL_VOLTAGE:%d\n",
		   JEITA_TEMP_NORMAL_VOLTAGE);
		chg->jeita_ctl.dt.normal_charge_voltage = JEITA_TEMP_NORMAL_VOLTAGE;
	}
#if 0
	if(!of_property_read_u32(np, "normal-charge-voltage",&chg->jeita_ctl.dt.normal_charge_voltage))
	{
		chg->jeita_ctl.dt.normal_charge_voltage = JEITA_TEMP_NORMAL_VOLTAGE;
	}
#endif

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t4_thres = val;
	else {
		pr_err("use default TEMP_T4_THRES:%d\n",
		   TEMP_T4_THRES);
		chg->jeita_ctl.dt.temp_t4_thres = TEMP_T4_THRES;
	}
	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t4_thres_minus_x_degree = val;
	else {
		pr_err("use default TEMP_T4_THRES_MINUS_X_DEGREE:%d\n",
		   TEMP_T4_THRES_MINUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t4_thres_minus_x_degree =
				   TEMP_T4_THRES_MINUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_thres = val;
	else {
		pr_err("use default TEMP_T3_THRES:%d\n",
		   TEMP_T3_THRES);
		chg->jeita_ctl.dt.temp_t3_thres = TEMP_T3_THRES;
	}
	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_thres_minus_x_degree = val;
	else {
		pr_err("use default TEMP_T3_THRES_MINUS_X_DEGREE:%d\n",
		   TEMP_T3_THRES_MINUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t3_thres_minus_x_degree =
				   TEMP_T3_THRES_MINUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_thres = val;
	else {
		pr_err("use default TEMP_T2_THRES:%d\n",
		   TEMP_T2_THRES);
		chg->jeita_ctl.dt.temp_t2_thres = TEMP_T2_THRES;
	}
	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T2_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T2_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t2_thres_plus_x_degree =
				   TEMP_T2_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t1p5_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_thres = val;
	else {
		pr_err("use default TEMP_T1P5_THRES:%d\n",
		   TEMP_T1P5_THRES);
		chg->jeita_ctl.dt.temp_t1p5_thres = TEMP_T1P5_THRES;
	}
	if (of_property_read_u32(np, "temp_t1p5_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T1P5_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T1P5_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t1p5_thres_plus_x_degree =
				   TEMP_T1P5_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_thres = val;
	else {
		pr_err("use default TEMP_T1_THRES:%d\n",
		   TEMP_T1_THRES);
		chg->jeita_ctl.dt.temp_t1_thres = TEMP_T1_THRES;
	}
	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T1_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T1_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t1_thres_plus_x_degree =
				   TEMP_T1_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_thres = val;
	else {
		pr_err("use default TEMP_T0_THRES:%d\n",
		   TEMP_T0_THRES);
		chg->jeita_ctl.dt.temp_t0_thres = TEMP_T0_THRES;
	}
	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T0_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T0_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t0_thres_plus_x_degree =
				   TEMP_T0_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_tn1_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_thres = val;
	else {
		pr_err("use default TEMP_TN1_THRES:%d\n",
		   TEMP_TN1_THRES);
		chg->jeita_ctl.dt.temp_tn1_thres = TEMP_TN1_THRES;
	}
	if (of_property_read_u32(np, "temp_tn1_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_TN1_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_TN1_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_tn1_thres_plus_x_degree =
				   TEMP_TN1_THRES_PLUS_X_DEGREE;
	}
	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_neg_10_thres = val;
	else {
		pr_err("use default TEMP_NEG_10_THRES:%d\n",
		   TEMP_NEG_10_THRES);
		chg->jeita_ctl.dt.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}
	if (of_property_read_u32(np, "temp_t3_to_t4_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_to_t4_fcc = val;
	else {
		pr_err("use default TEMP_T3_TO_T4_FCC:%d\n",
		   TEMP_T3_TO_T4_FCC);
		chg->jeita_ctl.dt.temp_t3_to_t4_fcc = TEMP_T3_TO_T4_FCC;
	}
	if (of_property_read_u32(np, "temp_t2_to_t3_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_to_t3_fcc = val;
	else {
		pr_err("use default TEMP_T2_TO_T3_FCC:%d\n",
		   TEMP_T2_TO_T3_FCC);
		chg->jeita_ctl.dt.temp_t2_to_t3_fcc = TEMP_T2_TO_T3_FCC;
	}
	if (of_property_read_u32(np, "temp_t1p5_to_t2_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_to_t2_fcc = val;
	else {
		pr_err("use default TEMP_T1P5_TO_T2_FCC:%d\n",
		   TEMP_T1P5_TO_T2_FCC);
		chg->jeita_ctl.dt.temp_t1p5_to_t2_fcc = TEMP_T1P5_TO_T2_FCC;
	}
	if (of_property_read_u32(np, "temp_t1_to_t1p5_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_to_t1p5_fcc = val;
	else {
		pr_err("use default TEMP_T1_TO_T1P5_FCC:%d\n",
		   TEMP_T1_TO_T1P5_FCC);
		chg->jeita_ctl.dt.temp_t1_to_t1p5_fcc = TEMP_T1_TO_T1P5_FCC;
	}
	if (of_property_read_u32(np, "temp_t0_to_t1_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_to_t1_fcc = val;
	else {
		pr_err("use default TEMP_T0_TO_T1_FCC:%d\n",
		   TEMP_T0_TO_T1_FCC);
		chg->jeita_ctl.dt.temp_t0_to_t1_fcc = TEMP_T0_TO_T1_FCC;
	}
	if (of_property_read_u32(np, "temp_tn1_to_t0_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_to_t0_fcc = val;
	else {
		pr_err("use default TEMP_TN1_TO_T0_FCC:%d\n",
		   TEMP_TN1_TO_T0_FCC);
		chg->jeita_ctl.dt.temp_tn1_to_t0_fcc = TEMP_TN1_TO_T0_FCC;
	}

	return 0;
}

static int nopmi_parse_dt_thermal(struct nopmi_chg *chg, struct device_node *np)
{
	int byte_len;
	int rc = 0;

	if (of_find_property(np, "nopmi,thermal-mitigation", &byte_len)) {
		chg->thermal_mitigation = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->thermal_mitigation == NULL)
			return -ENOMEM;

		chg->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(np,
				"nopmi,thermal-mitigation",
				chg->thermal_mitigation,
				chg->thermal_levels);
		if (rc < 0) {
			pr_err("Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int nopmi_parse_dt(struct nopmi_chg *chg)
{
	struct device_node *np = chg->dev->of_node;
	int rc = 0;

	if (!np) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(np,
				"qcom,fv-max-uv", &chg->dt.batt_profile_fv_uv);
	if (rc < 0)
		chg->dt.batt_profile_fv_uv = -EINVAL;
	else
		pr_err("nopmi_parse_dt %d\n",chg->dt.batt_profile_fv_uv);

	rc = nopmi_parse_dt_jeita(chg, np);
	if (rc < 0)
		return rc;

	rc = nopmi_parse_dt_thermal(chg, np);
	if (rc < 0)
		return rc;

	return 0;
};

static void nopmi_chg_workfunc(struct work_struct *work)
{
	struct nopmi_chg *chg = container_of(work,
		struct nopmi_chg, nopmi_chg_work.work);
	struct power_supply *psy = NULL;
	int val = 0;
	static int last_quick_charge_type = 0;

	if (nopmi_chg_is_usb_present(chg->main_psy)) {
		psy = power_supply_get_by_name("usb");
		if (!psy){
			pr_err("%s get usb psy fail\n", __func__);
                }
		else{
			val = nopmi_get_quick_charge_type(psy);
                }

		if (last_quick_charge_type != val) {
			chg->update_cont = 15;
			cancel_delayed_work_sync(&chg->xm_prop_change_work);
			schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(100));
			pr_err("%s nopmi_get_quick_charge_type[%d]:last_quick_charge_type[%d]\n", __func__, val, last_quick_charge_type);
			last_quick_charge_type = val;
		}

		start_nopmi_chg_jeita_workfunc();
		schedule_delayed_work(&chg->nopmi_chg_work, msecs_to_jiffies(NOPMI_CHG_WORKFUNC_GAP));
	}
}

static void start_nopmi_chg_workfunc(void)
{
	pr_info("g_nopmi_chg:0x%x\n", g_nopmi_chg);
	if(g_nopmi_chg)
	{
		schedule_delayed_work(&g_nopmi_chg->nopmi_chg_work, 0);
		schedule_delayed_work(&g_nopmi_chg->cvstep_monitor_work,
					msecs_to_jiffies(NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP));
	}
}

static void stop_nopmi_chg_workfunc(void)
{
	pr_info("g_nopmi_chg:0x%x\n", g_nopmi_chg);

	if(g_nopmi_chg)
	{
		cancel_delayed_work_sync(&g_nopmi_chg->cvstep_monitor_work);
		cancel_delayed_work_sync(&g_nopmi_chg->nopmi_chg_work);
		stop_nopmi_chg_jeita_workfunc();
	}
}

/*longcheer nielianjie10 2022.12.05 Set CV according to circle count start*/
static int nopmi_select_cycle(struct nopmi_chg *nopmi_chg)
{
	union power_supply_propval pval = {0, };
	int cycle_count_now = 0;
	int ret = 0;

	if (!nopmi_chg->bms_psy) {
		pr_err("nopmi_chg->bms_psy: is NULL !\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if(ret < 0){
		pr_err("get POWER_SUPPLY_PROP_CYCLE_COUNT, fail !\n");
		return -EINVAL;
	}

	cycle_count_now = pval.intval;
	pr_info("cycle count now: %d, nopmi_chg->cycle_count: %d, CYCLE_COUNT: %d.\n",
			cycle_count_now, nopmi_chg->cycle_count, CYCLE_COUNT);
	if(nopmi_chg->cycle_count > 0){
		if(cycle_count_now > nopmi_chg->cycle_count){
			nopmi_chg->select_cc_cv_step_config = cc_cv_step1_config;
		}else{
			nopmi_chg->select_cc_cv_step_config = cc_cv_step_config;
		}
	}else{
		if(cycle_count_now > CYCLE_COUNT){
			nopmi_chg->select_cc_cv_step_config = cc_cv_step1_config;
		}else{
			nopmi_chg->select_cc_cv_step_config = cc_cv_step_config;
		}
	}
	return 0;
}

/*longcheer nielianjie10 2022.12.05 Set CV according to circle count end*/

static void  nopmi_cv_step_monitor_work(struct work_struct *work)
{
#if 1
	struct nopmi_chg *nopmi_chg = container_of(work,
		struct nopmi_chg, cvstep_monitor_work.work);
#endif
	//struct nopmi_chg *nopmi_chg = g_nopmi_chg;
	union power_supply_propval pval={0, };
	int rc = 0;
	int batt_curr = 0, batt_volt = 0;
	u32 i = 0, stepdown = 0, finalFCC = 0, votFCC = 0;
	static u32 count = 0;
	struct step_config *pcc_cv_step_config;
	u32 step_table_max;
	int pd_verified = 0;

	if (nopmi_chg->bms_psy) {
		pval.intval = 0;
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_NOW fail\n", __func__);
			goto out;
		}
		batt_curr =  pval.intval / 1000;

		pval.intval = 0;
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_NOW fail\n", __func__);
			goto out;
		}
		batt_volt = pval.intval / 1000;
	}
	else
	{
		goto out;
	}
	pr_info("fg_cc_cv_step_check: batt_volt:%d batt_curr:%d", batt_volt, batt_curr);
	/*discharging*/
  	if(!nopmi_chg->fcc_votable)
        {
          	nopmi_chg->fcc_votable  = find_votable("FCC");
          	if(!nopmi_chg->fcc_votable)
        		goto out;
        }
	if(batt_curr < 0){
		vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, false, votFCC);
		goto out;
	}

	//longcheer nielianjie10 2022.12.05 Set CV according to circle count
	rc = nopmi_select_cycle(nopmi_chg);
	if(rc < 0){
		pr_err("select CV Fail,set cv_step_config to 0-100.\n");
		pcc_cv_step_config = cc_cv_step_config;
	}else{
		pcc_cv_step_config = nopmi_chg->select_cc_cv_step_config;
	}
	step_table_max = STEP_TABLE_MAX;
	for(i = 0; i < step_table_max; i++){
		if((batt_volt >= (pcc_cv_step_config[i].volt_lim - CV_BATT_VOLT_HYSTERESIS))
						&& (batt_curr > pcc_cv_step_config[i].curr_lim)){
			count++;
			if(count >= 2){
				stepdown = true;
				count = 0;
				pr_info("fg_cc_cv_step_check:stepdown");
			}
			break;
		}
	}
	finalFCC = get_effective_result(nopmi_chg->fcc_votable);
	pd_verified = adapter_dev_get_pd_verified();
	if(!pd_verified){
		if(finalFCC > pcc_cv_step_config[step_table_max-1].curr_lim){
			votFCC = UN_VERIFIED_PD_CHG_CURR;
			vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, true, votFCC);
			goto out;
		}
	}

	if(!stepdown || finalFCC <= pcc_cv_step_config[step_table_max-1].curr_lim)
		goto out;

	if(finalFCC - pcc_cv_step_config[i].curr_lim < STEP_DOWN_CURR_MA)
		votFCC = pcc_cv_step_config[i].curr_lim;
	else
		votFCC = finalFCC - STEP_DOWN_CURR_MA;
	vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, true, votFCC);
	pr_info("fg_cc_cv_step_check: i:%d cccv_step vote:%d stepdown:%d finalFCC:%d",
					i, votFCC, stepdown, finalFCC);
out:
	schedule_delayed_work(&nopmi_chg->cvstep_monitor_work,
				msecs_to_jiffies(NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP));
	pr_info("nopmi_cv_step_monitor_work: end");
}

int nopmi_chg_get_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
	case NOPMI_CP_MASTER:
		if (is_cp_chan_valid(chg, channel)) {
			iio_chan_list = chg->cp_ext_iio_chans[channel];
		} else if (is_cp_chan_valid(chg, channel+LN8000_IIO_CHANNEL_OFFSET)) {
			iio_chan_list = chg->cp_ext_iio_chans[channel+LN8000_IIO_CHANNEL_OFFSET];
		} else {
			pr_err("There is no vaild cp channel!\n");
			return -ENODEV;
		}
		break;
	case NOPMI_BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->fg_ext_iio_chans[channel];
		break;
	case NOPMI_MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	case NOPMI_CC:
		if (!is_cc_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->cc_ext_iio_chans[channel];
		break;
	case NOPMI_DS:
		if (!is_ds_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->ds_ext_iio_chans[channel];
		break;

	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(nopmi_chg_get_iio_channel);

int nopmi_chg_set_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
	case NOPMI_CP_MASTER:
		if (is_cp_chan_valid(chg, channel)) {
			iio_chan_list = chg->cp_ext_iio_chans[channel];
		} else if (is_cp_chan_valid(chg, channel+LN8000_IIO_CHANNEL_OFFSET)) {
			iio_chan_list = chg->cp_ext_iio_chans[channel+LN8000_IIO_CHANNEL_OFFSET];
		} else {
			pr_err("There is no vaild cp channel!\n");
			return -ENODEV;
		}
		break;
	case NOPMI_BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->fg_ext_iio_chans[channel];
		break;
	case NOPMI_MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(nopmi_chg_set_iio_channel);

static int nopmi_chg_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct nopmi_chg *chip = iio_priv(indio_dev);
	int rc = 0;
	static int old_real_type = POWER_SUPPLY_STATUS_UNKNOWN;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		chip->pd_active = val1;
		if (val1)
			chip->usb_online = 1;
		else
			chip->usb_online = 0;

		if(chip->usb_psy) {
			pr_info("chip->pd_active: %d\n", chip->pd_active);
			power_supply_changed(chip->usb_psy);
		}

		if (chip) {
			chip->update_cont = 15;
			cancel_delayed_work_sync(&chip->xm_prop_change_work);
			schedule_delayed_work(&chip->xm_prop_change_work, msecs_to_jiffies(100));
		} else {
			pr_err("uevent: chip or chip->xm_prop_change_work is NULL !\n");
		}
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		chip->pd_usb_suspend = val1;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		chip->pd_in_hard_reset = val1;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		chip->pd_cur_max = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		chip->pd_min_vol = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		chip->pd_max_vol = val1;
		break;
	case PSY_IIO_USB_REAL_TYPE:
		chip->real_type = val1;
		pr_info("uevent: old_real_type:%d real_type_now:%d \n", old_real_type, chip->real_type );
		if (old_real_type!=chip->real_type) {
			if (chip) {
				chip->update_cont = 15;
				cancel_delayed_work_sync(&chip->xm_prop_change_work);
				schedule_delayed_work(&chip->xm_prop_change_work, msecs_to_jiffies(100));
			} else {
				pr_err("uevent: chip or chip->xm_prop_change_work is NULL !\n");
			}
			old_real_type = chip->real_type;
		}
		break;
	case PSY_IIO_TYPEC_MODE:
		chip->typec_mode = val1;
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		chip->cc_orientation = val1;
		break;
	case PSY_IIO_CHARGING_ENABLED:
		set_prop_battery_charging_enabled(chip->jeita_ctl.usb_icl_votable, val1);
		break;
	case PSY_IIO_INPUT_SUSPEND:
		pr_info("Set input suspend prop, value:%d\n", val1);
		rc = nopmi_set_prop_input_suspend(chip, val1);
		break;
	case PSY_IIO_MTBF_CUR:
		chip->mtbf_cur = val1;
		break;
	case PSY_IIO_APDO_VOLT:
		chip->apdo_volt = val1;
		break;
	case PSY_IIO_APDO_CURR:
		chip->apdo_curr = val1;
		break;
	case PSY_IIO_FFC_DISABLE:
		g_ffc_disable = val1;
		break;
	default:
		pr_info("Unsupported battery IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0) {
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}
	return IIO_VAL_INT;
}

static int nopmi_chg_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct nopmi_chg *chip = iio_priv(indio_dev);
	int rc = 0;

	*val1 = 0;

	switch (chan->channel) {
		case PSY_IIO_PD_ACTIVE:
		*val1 = chip->pd_active;;
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		*val1 = chip->pd_usb_suspend;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		*val1 = chip->pd_in_hard_reset;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		*val1 = chip->pd_cur_max;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		*val1 = chip->pd_min_vol;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		*val1 = chip->pd_max_vol;;
		break;
	case PSY_IIO_USB_REAL_TYPE:
		if(!chip){
  			pr_err("chip is null\n");
			break;
  		}
 		if (chip->pd_active) {
  			*val1 = POWER_SUPPLY_TYPE_USB_PD;
  		} else {
  			*val1 = chip->real_type;
			chip->apdo_curr = 0;
			chip->apdo_volt = 0;
  		}
  		pr_info("real_type=%d\n", *val1);
		break;
	case PSY_IIO_TYPEC_MODE:
		*val1 = chip->typec_mode;
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
//HTH-260166 longcheer wangwei add typec status start
		if((chip->cc_orientation == 1) && (chip->usb_online > 0))
			*val1 = 2;
		else if((chip->cc_orientation == 0) && (chip->usb_online > 0))
			*val1 = 1;
		else
			*val1 = 0;
//HTH-260166 longcheer wangwei add typec status end
		break;
	case PSY_IIO_CHARGING_ENABLED:
		get_prop_battery_charging_enabled(chip->jeita_ctl.usb_icl_votable, val1);
		break;
	case PSY_IIO_INPUT_SUSPEND:
		*val1 = chip->input_suspend;
		break;
	case PSY_IIO_MTBF_CUR:
		*val1 = chip->mtbf_cur;
		break;
	case PSY_IIO_CHARGE_IC_TYPE:
		*val1 = chip->charge_ic_type;
		break;
	case PSY_IIO_FFC_DISABLE:
		*val1 = g_ffc_disable;
		break;
	default:
		pr_debug("Unsupported battery IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}
	return IIO_VAL_INT;
}

static int nopmi_chg_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct nopmi_chg *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(nopmi_chg_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info nopmi_chg_iio_info = {
	.read_raw	= nopmi_chg_iio_read_raw,
	.write_raw	= nopmi_chg_iio_write_raw,
	.of_xlate	= nopmi_chg_iio_of_xlate,
};
static int nopmi_init_iio_psy(struct nopmi_chg *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(nopmi_chg_iio_psy_channels);
	int rc = 0, i = 0;

	pr_info("nopmi_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &nopmi_chg_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "nopmi_chg";
	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = nopmi_chg_iio_psy_channels[i].channel_num;
		chan->type = nopmi_chg_iio_psy_channels[i].type;
		chan->datasheet_name =
			nopmi_chg_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			nopmi_chg_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			nopmi_chg_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register nopmi chg IIO device, rc=%d\n", rc);

	pr_info("nopmi chg IIO device, rc=%d\n", rc);
	return rc;
}

static int nopmi_chg_ext_init_iio_psy(struct nopmi_chg *chip)
{
	if (!chip)
		return -ENOMEM;

	chip->fg_ext_iio_chans = devm_kcalloc(chip->dev,
				ARRAY_SIZE(fg_ext_iio_chan_name), sizeof(*chip->fg_ext_iio_chans), GFP_KERNEL);
	if (!chip->fg_ext_iio_chans)
		return -ENOMEM;

	chip->cp_ext_iio_chans = devm_kcalloc(chip->dev,
		        ARRAY_SIZE(cp_ext_iio_chan_name), sizeof(*chip->cp_ext_iio_chans), GFP_KERNEL);
	if (!chip->cp_ext_iio_chans)
		return -ENOMEM;

	chip->main_chg_ext_iio_chans = devm_kcalloc(chip->dev,
		        ARRAY_SIZE(main_chg_ext_iio_chan_name), sizeof(*chip->main_chg_ext_iio_chans), GFP_KERNEL);
	if (!chip->main_chg_ext_iio_chans)
		return -ENOMEM;

	chip->cc_ext_iio_chans = devm_kcalloc(chip->dev,
		        ARRAY_SIZE(cc_ext_iio_chan_name), sizeof(*chip->cc_ext_iio_chans), GFP_KERNEL);
	if (!chip->cc_ext_iio_chans)
		return -ENOMEM;

	chip->ds_ext_iio_chans = devm_kcalloc(chip->dev,
		        ARRAY_SIZE(ds_ext_iio_chan_name), sizeof(*chip->ds_ext_iio_chans), GFP_KERNEL);
	if (!chip->ds_ext_iio_chans)
		return -ENOMEM;

	return 0;
}

static void nopmi_init_config( struct nopmi_chg *chip)
{
	if(chip==NULL)
		return;
	chip->update_cont = 5;
}

static void nopmi_init_config_ext(struct nopmi_chg *nopmi_chg)
{
	int rc;
	union power_supply_propval pval={0, };
	/* init usbonline pd_active and realtype from charge ic */
	rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
	if(!rc) {
		nopmi_chg->usb_online = pval.intval;
		pr_err("get usb_online from charge ic: %d\n", nopmi_chg->usb_online);
	} else {
		pr_err("get usb_online from charge ic fail\n");
	}

	rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_MAIN, MAIN_CHARGE_PD_ACTIVE, &pval.intval);
	if (!rc) {
		nopmi_chg->pd_active = pval.intval;
		pr_err("get pd active from charge ic:%d \n", nopmi_chg->pd_active);
		if(nopmi_chg->pd_active)
		{
			nopmi_chg->usb_online = 1;
		}
	} else {
		pr_err("get pd active from charge ic fail\n");
	}

	rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (!rc) {
		nopmi_chg->real_type = pval.intval;
		if (nopmi_chg->real_type != POWER_SUPPLY_TYPE_UNKNOWN)
		{
			nopmi_chg->usb_online = 1;
		}
		pr_err("get charge type from charge ic:%d ,usb_online = %d\n", nopmi_chg->real_type, nopmi_chg->usb_online);
	} else {
		pr_err("get charger type from charge ic fail\n");
	}
	/*  init usbonline pd_active and realtype from charge ic */
}

static int nopmi_chg_probe(struct platform_device *pdev)
{
	struct nopmi_chg *nopmi_chg = NULL;
	struct power_supply *bms_psy = NULL;
	struct power_supply *main_psy = NULL;
	struct iio_dev *indio_dev = NULL;
 	int rc = 0;
	static int probe_cnt = 0;
	union power_supply_propval pval={0, };

	if (probe_cnt == 0)
		pr_err("start \n");

	probe_cnt ++;
	bms_psy = power_supply_get_by_name("bms");
	if (IS_ERR_OR_NULL(bms_psy)) {
		return -EPROBE_DEFER;
	}

	main_psy = power_supply_get_by_name("bbc");
	if (IS_ERR_OR_NULL(main_psy)) {
		if (bms_psy)
			power_supply_put(bms_psy);
		return -EPROBE_DEFER;
	}

	if (!pdev->dev.of_node)
		return -ENODEV;

	if (pdev->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct nopmi_chg));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			nopmi_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
    }

	nopmi_chg = iio_priv(indio_dev);
	nopmi_chg->indio_dev = indio_dev;
	nopmi_chg->dev = &pdev->dev;
	nopmi_chg->pdev = pdev;
	platform_set_drvdata(pdev, nopmi_chg);

	nopmi_init_config(nopmi_chg);
	nopmi_chg->cycle_count = 0;
	rc = nopmi_chg_ext_init_iio_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Failed to initialize nopmi chg ext IIO PSY, rc=%d\n", rc);
		nopmi_err("Failed to initialize nopmi chg ext IIO PSY, rc=%d\n", rc);
        goto err_free;
	}

	/* longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic start */
	rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_DS, DS_AUTHEN_RESULT, &pval.intval);
	if (rc) {
		if (probe_cnt <= PROBE_CNT_MAX) {
			return -EPROBE_DEFER;
		} else {
			pr_err("check ds chip fail, skip \n");
		}
	}
	/* longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic end */

	pr_err("really start, probe_cnt = %d \n", probe_cnt);

	rc = nopmi_parse_dt(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto err_free;
	}

	nopmi_chg->bms_psy = bms_psy;
	nopmi_chg->main_psy = main_psy;
	rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_MAIN, MAIN_CHARGE_IC_TYPE, &pval.intval);
	if (rc) {
		nopmi_chg->charge_ic_type = NOPMI_CHARGER_IC_NONE;
	} else {
		nopmi_chg->charge_ic_type = pval.intval;
	}
	pr_info("nopmi_chg->charge_ic_type = %d\n", nopmi_chg->charge_ic_type);

	nopmi_chg_jeita_init(&nopmi_chg->jeita_ctl);

	INIT_DELAYED_WORK(&nopmi_chg->nopmi_chg_work, nopmi_chg_workfunc);
	INIT_DELAYED_WORK(&nopmi_chg->cvstep_monitor_work, nopmi_cv_step_monitor_work);
	INIT_DELAYED_WORK( &nopmi_chg->xm_prop_change_work, generate_xm_charge_uvent);
//2021.09.21 wsy edit reomve vote to jeita
#if 1
	nopmi_chg->fcc_votable = find_votable("FCC");
	nopmi_chg->fv_votable = find_votable("FV");
	nopmi_chg->usb_icl_votable = find_votable("USB_ICL");
	nopmi_chg->chg_dis_votable = find_votable("CHG_DISABLE");
#endif
	schedule_delayed_work(&nopmi_chg->nopmi_chg_work, msecs_to_jiffies(NOPMI_CHG_WORKFUNC_FIRST_GAP));
	schedule_delayed_work(&nopmi_chg->xm_prop_change_work, msecs_to_jiffies(30000));
	if((NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type()) || (NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type())||(NOPMI_CHARGER_IC_SC == nopmi_get_charger_ic_type()))
		device_init_wakeup(nopmi_chg->dev, true);

    rc = nopmi_init_iio_psy(nopmi_chg);
 	if (rc < 0) {
		pr_err("Failed to initialize nopmi IIO PSY, rc=%d\n", rc);
		nopmi_err("Failed to initialize nopmi IIO PSY, rc=%d\n", rc);
		goto err_free;
	}

	rc = nopmi_init_batt_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		nopmi_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = nopmi_init_usb_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", rc);
		nopmi_err("Couldn't initialize usb psy rc=%d\n", rc);
		goto cleanup;
	}

	g_nopmi_chg = nopmi_chg;
	nopmi_init_config_ext(nopmi_chg);

    rc = nopmi_chg_init_dev_class(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		nopmi_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
    }
	pr_err("nopmi_chg probe successfully!\n");
	nopmi_err("nopmi_chg probe successfully!\n");
	return 0;

cleanup:
err_free:
	pr_err("nopmi_chg probe fail\n");
	nopmi_err("nopmi_chg probe fail\n");
	//devm_kfree(&pdev->dev,nopmi_chg);
	return rc;
}

static int nopmi_chg_remove(struct platform_device *pdev)
{
	struct nopmi_chg *nopmi_chg = platform_get_drvdata(pdev);

	nopmi_chg_jeita_deinit(&nopmi_chg->jeita_ctl);
	//devm_kfree(&pdev->dev,nopmi_chg);
	return 0;
}

static const struct of_device_id nopmi_chg_dt_match[] = {
	{.compatible = "qcom,nopmi-chg"},
	{},
};

static struct platform_driver nopmi_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "qcom,nopmi-chg",
		.of_match_table = nopmi_chg_dt_match,
	},
	.probe = nopmi_chg_probe,
	.remove = nopmi_chg_remove,
};

static int __init nopmi_chg_init(void)
{
    platform_driver_register(&nopmi_chg_driver);
	pr_err("nopmi_chg init end\n");
    return 0;
}

static void __exit nopmi_chg_exit(void)
{
	pr_err("nopmi_chg exit\n");
	platform_driver_unregister(&nopmi_chg_driver);
}

late_initcall_sync(nopmi_chg_init);
module_exit(nopmi_chg_exit);

MODULE_SOFTDEP("pre: bq2589x_charger sm5602_fg ");
MODULE_AUTHOR("WingTech Inc.");
MODULE_DESCRIPTION("battery driver");
MODULE_LICENSE("GPL");
