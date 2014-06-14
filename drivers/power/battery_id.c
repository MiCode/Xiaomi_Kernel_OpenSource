#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/power/battery_id.h>

ATOMIC_NOTIFIER_HEAD(batt_id_notifier);

static struct ps_batt_chg_prof *batt_property;
static int batt_status;

int batt_id_reg_notifier(struct notifier_block *nb)
{
	return atomic_notifier_chain_register(&batt_id_notifier, nb);
}
EXPORT_SYMBOL_GPL(batt_id_reg_notifier);

void batt_id_unreg_notifier(struct notifier_block *nb)
{
	atomic_notifier_chain_unregister(&batt_id_notifier, nb);
}
EXPORT_SYMBOL_GPL(batt_id_unreg_notifier);


/**
 * battery_prop_changed - Update properties when  battery connection status
 *                        changes
 * @battery_conn_stat : The current connection status of battery
 * @batt_prop : Address of the ps_batt_chg_prof structure with the updated
 *              values passed from the calling function
 *
 * Whenever the battery connection status changes this function will be called
 * to indicate a change in the status and to update the status and value of
 * properties
 */
void battery_prop_changed(int battery_conn_stat,
			struct ps_batt_chg_prof *batt_prop)
{
	if (batt_status != battery_conn_stat) {
		if (battery_conn_stat == POWER_SUPPLY_BATTERY_INSERTED)
			batt_property = batt_prop;
		else
			batt_property = NULL;

		batt_status = battery_conn_stat;
	}

	atomic_notifier_call_chain(&batt_id_notifier,
			0, &(batt_property));

}
EXPORT_SYMBOL_GPL(battery_prop_changed);

/**
 * get_batt_prop - Get the battery connection status and updated properties
 * @batt_prop : battery properties structure copied to this address
 */
int get_batt_prop(struct ps_batt_chg_prof *batt_prop)
{
	if (batt_property)
		memcpy(batt_prop, batt_property,
			sizeof(struct ps_batt_chg_prof));
	else
		return -ENOMEM;
	return 0;
}
EXPORT_SYMBOL_GPL(get_batt_prop);
