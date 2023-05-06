#include <linux/battmngr/xm_battery_core.h>

static int xm_get_shutdown_delay(void)
{
	int val;

	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_CAPACITY, &val);
	xm_battmngr_read_iio_prop(g_battmngr_iio, BATT_FG, BATT_FG_SHUTDOWN_DELAY, &val);

	return val;
}

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uvent(struct work_struct *work)
{
	int count;
	struct batt_feature_info *chip = container_of(work,
			struct batt_feature_info, xm_prop_change_work.work);

	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n",	//length=28+8
	};
	static char *envp[] = {
		uevent_string[0],
		NULL,
	};
	char *prop_buf = NULL;

	count = chip->update_cont;
	if(chip->update_cont < 0)
		return;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return;

	scnprintf(prop_buf, PAGE_SIZE, "%u", xm_get_shutdown_delay());
	strncpy(uevent_string[0]+28, prop_buf,MAX_UEVENT_LENGTH-28);

	battery_err("uevent test : %s, count %d\n", envp[0], count);

	/*add our prop end*/

	kobject_uevent_env(&chip->dev->kobj, KOBJ_CHANGE, envp);

	free_page((unsigned long)prop_buf);
	chip->update_cont = count - 1;
	schedule_delayed_work(&chip->xm_prop_change_work, msecs_to_jiffies(2000));
	return;
}

int xm_batt_feature_init(struct xm_battery *battery)
{
	struct batt_feature_info *chip;

	battery_err("%s: Start\n", __func__);

	if (battery->batt_feature) {
		battery_err("%s: Already initialized\n", __func__);
		return -EINVAL;
	}

	chip = devm_kzalloc(battery->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->dev = battery->dev;
	chip->update_cont = 0;

	INIT_DELAYED_WORK( &chip->xm_prop_change_work, generate_xm_charge_uvent);

	schedule_delayed_work(&chip->xm_prop_change_work, msecs_to_jiffies(30000));

	battery->batt_feature = chip;
	battery_err("%s: End\n", __func__);

	return 0;
}

void xm_batt_feature_deinit(void)
{
	struct batt_feature_info *chip = g_xm_battery->batt_feature;

	if (!chip)
		return;

	cancel_delayed_work_sync(&chip->xm_prop_change_work);
	chip = NULL;

	return;
}

