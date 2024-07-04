#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>

#include "battery_auth_class.h"
#include <linux/soc/qcom/smem.h>
#include "../../../../misc/hqsysfs/smem_type.h"

typedef struct
{
  u32  version;        /**< Version Info */
  int32_t   sbl_entry_mV;   /**< XBL loader entry Mv*/
  int32_t   sbl_entry_soc;  /**< XBL loader entry Soc; if in case the integrity bit is already set */
  int32_t   uefi_entry_mV;  /**< XBL Core Entry Mv*/
  int32_t   uefi_entry_soc; /**< XBL Core entry Soc */
  int32_t   uefi_exit_mV;   /**< XBL Core exit Mv*/
  int32_t   uefi_exit_soc;  /**< XBL Core Exit Soc */
  u32  uefi_charger_fw_mode; /**< uefi_charger_fw_mode */
  u8   uefi_battery_verify_result; /**< XBL battery verify */
}pm_chg_info_type;

enum {
	MAIN_SUPPLY = 0,
	MAX_SUPPLY,
};

/* N19A code for HQ-360184 by p-wumingzhu1 at 20240103 start */
static const char *auth_device_name[] = {
	"main_suppiler",
	"unknown",
};
/* N19A code for HQ-360184 by p-wumingzhu1 at 20240103 end */

struct auth_data {
	struct auth_device *auth_dev[MAX_SUPPLY];

	struct delayed_work dwork;
	struct delayed_work notify_dwork;

	bool auth_result;
	u8 batt_id;
	int batt_verfitied;
	int batt_verfitied_chip_ok;
};

static struct auth_data *g_info;
/* N19A code for HQ-360184 by p-wumingzhu1 at 20240103 */
static int auth_index = 0;

static int Get_battery_verify_result()
{
  static pm_chg_info_type *pSmemChargerInfo = NULL;
  size_t buf_size_ret = 0;

  if(!pSmemChargerInfo)
  {
    pSmemChargerInfo = (pm_chg_info_type *)qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_CHARGER_BATTERY_INFO, &buf_size_ret);
    if(!pSmemChargerInfo)
    {
       pr_err("%s get uefi charge data fail\n", __func__);
       return -1;
    }
  }
   pr_info("%s  Get_uefi_battery_verify_result , uefi_battery_verify_result = 0x%x", __func__, pSmemChargerInfo->uefi_battery_verify_result);

  return pSmemChargerInfo->uefi_battery_verify_result & 0x20;
}

static int set_battery_verify_result(struct auth_data *info)
{
  static pm_chg_info_type *pSmemChargerInfo = NULL;
  size_t buf_size_ret = 0;

  if(!pSmemChargerInfo)
  {
    pSmemChargerInfo = (pm_chg_info_type *)qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_CHARGER_BATTERY_INFO, &buf_size_ret);
    if(!pSmemChargerInfo)
    {
       pr_err("%s get uefi charge data fail\n", __func__);
       return -1;
    }
  }

    pSmemChargerInfo->uefi_battery_verify_result |=  (info->batt_verfitied << 5);
    pSmemChargerInfo->uefi_battery_verify_result |= (info->batt_verfitied_chip_ok << 4);
    pSmemChargerInfo->uefi_battery_verify_result &= 0xf0;
    pSmemChargerInfo->uefi_battery_verify_result |= info->batt_id;
    pr_info("%s  set_uefi_battery_verify_result , uefi_battery_verify_result = 0x%x", __func__, pSmemChargerInfo->uefi_battery_verify_result);

    return 0;
}

/* N19A code for HQ-360184 by p-wumingzhu1 at 20240103 */
#define AUTHENTIC_COUNT_MAX 3
static void auth_battery_dwork(struct work_struct *work)
{
	int i = 0, rc = 0;
	struct auth_data *info = container_of(to_delayed_work(work),
					      struct auth_data, dwork);
	/* N19 code for HQ-380271 by p-hankang1 at 20240402 */
	int authen_result = 1;
	static int retry_authentic = 0;

	for (i = 0; i < MAX_SUPPLY; i++) {
		if (!info->auth_dev[i]) {
			info->auth_dev[i] = get_batt_auth_by_name(auth_device_name[i]);
			if (!info->auth_dev[i])
				continue;
		}
		authen_result = auth_device_start_auth(info->auth_dev[i]);
		if (!authen_result) {
			auth_device_get_batt_id(info->auth_dev[i], &(info->batt_id));
			pr_info("auth1 get batt id = %d", info->batt_id);
			auth_index = i;
			break;
		}
	}

	if (i == MAX_SUPPLY) {
		retry_authentic++;
		if (retry_authentic < AUTHENTIC_COUNT_MAX) {
			pr_err
			    ("battery authentic work begin to restart %d\n",
			     retry_authentic);
			schedule_delayed_work(&(info->dwork),
					      msecs_to_jiffies(3000));
		}
		if (retry_authentic == AUTHENTIC_COUNT_MAX) {
			/* N19 code for HQ-380271 by p-hankang1 at 20240402 */
			if (authen_result != 0)
				pr_err("authentic result is fail\n");
			info->batt_id = 0x0f;
		}
	} else {
		/* N19 code for HQ-380271 by p-hankang1 at 20240402 */
		if (authen_result != 0)
			pr_err("authentic result is fail\n");
	}

	info->auth_result = ((authen_result == 0) ? true : false);
	if (info->auth_result) {
		info->batt_verfitied = 1;
		info->batt_verfitied_chip_ok = 1;
	}

	if (retry_authentic == AUTHENTIC_COUNT_MAX || info->auth_result) {
		rc = set_battery_verify_result(info);
		if (rc  == 0)
			Get_battery_verify_result();
	}
}

static int __init auth_battery_init(void)
{
	int ret = 0;
	int i = 0;
	struct auth_data *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	for (i = 0; i < MAX_SUPPLY; i++) {
		info->auth_dev[i] = get_batt_auth_by_name(auth_device_name[i]);
		if (!info->auth_dev[i]) {
			break;
		}
	}
	INIT_DELAYED_WORK(&info->dwork, auth_battery_dwork);
	info->batt_verfitied = 0;
	info->batt_verfitied_chip_ok = 0;
	info->batt_id = 0x0f;
	g_info = info;

	if (Get_battery_verify_result())
		return 0;

	for (i = 0; i < MAX_SUPPLY; i++) {
		if (!info->auth_dev[i])
			continue;
		ret = auth_device_start_auth(info->auth_dev[i]);
		if (!ret) {
			auth_device_get_batt_id(info->auth_dev[i], &(info->batt_id));
			pr_info("auth get batt id = %d", info->batt_id);
			auth_index = i;
			break;
		}
	}

	if (i >= MAX_SUPPLY)
		schedule_delayed_work(&info->dwork, msecs_to_jiffies(500));
	else
		info->auth_result = true;

	if (info->auth_result) {
		info->batt_verfitied = 1;
		info->batt_verfitied_chip_ok = 1;
	}

	if (info->auth_result) {
		ret = set_battery_verify_result(info);
		if (ret  == 0)
			Get_battery_verify_result();
	}

	return 0;
}

static void __exit auth_battery_exit(void)
{
	int i = 0;

	for (i = 0; i < MAX_SUPPLY; i++)
		auth_device_unregister(g_info->auth_dev[i]);

	kfree(g_info);
}

early_initcall(auth_battery_init);
module_exit(auth_battery_exit);
MODULE_LICENSE("GPL");
