/*
 *  LEDs triggers for power supply class
 *
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *  Copyright (C) 2017 XiaoMi, Inc.
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>

#include "power_supply.h"

/* Battery specific LEDs triggers. */

static void power_supply_update_bat_leds(struct power_supply *psy)
{
	union power_supply_propval status, bat_percent;



	if (psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &status))
		return;
	if (psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &bat_percent))
		return;

	dev_dbg(psy->dev, "%s %d\n", __func__, status.intval);

	switch (status.intval) {
	case POWER_SUPPLY_STATUS_FULL:
		led_trigger_event(psy->charging_red_trig, LED_OFF);
		led_trigger_event(psy->charging_blue_trig, LED_OFF);
		led_trigger_event(psy->charging_green_trig, LED_FULL);
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if (bat_percent.intval < 15) {
			led_trigger_event(psy->charging_green_trig, LED_OFF);
			led_trigger_event(psy->charging_blue_trig, LED_OFF);
			led_trigger_event(psy->charging_red_trig, LED_FULL);
		} else if (bat_percent.intval < 90) {
			led_trigger_event(psy->charging_blue_trig, LED_OFF);
			led_trigger_event(psy->charging_green_trig, LED_FULL);
			led_trigger_event(psy->charging_red_trig, LED_FULL);
		} else {
			led_trigger_event(psy->charging_red_trig, LED_OFF);
			led_trigger_event(psy->charging_blue_trig, LED_OFF);
			led_trigger_event(psy->charging_green_trig, LED_FULL);
		}
		break;
	default:
		{
			led_trigger_event(psy->charging_red_trig, LED_OFF);
			led_trigger_event(psy->charging_green_trig, LED_OFF);
			led_trigger_event(psy->charging_blue_trig, LED_OFF);
		}
		break;
	}
}

static int power_supply_create_bat_triggers(struct power_supply *psy)
{
	int rc = 0;


	psy->charging_red_trig_name = kasprintf(GFP_KERNEL,
					"%s-red", psy->name);
	if (!psy->charging_red_trig_name)
		goto charging_red_failed;
	psy->charging_green_trig_name = kasprintf(GFP_KERNEL,
					"%s-green", psy->name);
	if (!psy->charging_green_trig_name)
		goto charging_green_failed;
	psy->charging_blue_trig_name = kasprintf(GFP_KERNEL,
					"%s-blue", psy->name);
	if (!psy->charging_blue_trig_name)
		goto charging_blue_failed;

	led_trigger_register_simple(psy->charging_red_trig_name,
				    &psy->charging_red_trig);
	led_trigger_register_simple(psy->charging_green_trig_name,
				    &psy->charging_green_trig);
	led_trigger_register_simple(psy->charging_blue_trig_name,
				    &psy->charging_blue_trig);
	goto success;

charging_blue_failed:
	kfree(psy->charging_green_trig_name);
charging_green_failed:
	kfree(psy->charging_red_trig_name);
charging_red_failed:
	rc = -ENOMEM;
success:
	return rc;
}

static void power_supply_remove_bat_triggers(struct power_supply *psy)
{
	led_trigger_unregister_simple(psy->charging_red_trig);
	led_trigger_unregister_simple(psy->charging_green_trig);
	led_trigger_unregister_simple(psy->charging_blue_trig);
	kfree(psy->charging_red_trig_name);
	kfree(psy->charging_green_trig_name);
	kfree(psy->charging_blue_trig_name);
}

/* Generated power specific LEDs triggers. */

static void power_supply_update_gen_leds(struct power_supply *psy)
{
	union power_supply_propval online;

	if (psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &online))
		return;

	dev_dbg(psy->dev, "%s %d\n", __func__, online.intval);

	if (online.intval)
		led_trigger_event(psy->online_trig, LED_FULL);
	else
		led_trigger_event(psy->online_trig, LED_OFF);
}

static int power_supply_create_gen_triggers(struct power_supply *psy)
{
	psy->online_trig_name = kasprintf(GFP_KERNEL, "%s-online", psy->name);
	if (!psy->online_trig_name)
		return -ENOMEM;

	led_trigger_register_simple(psy->online_trig_name, &psy->online_trig);

	return 0;
}

static void power_supply_remove_gen_triggers(struct power_supply *psy)
{
	led_trigger_unregister_simple(psy->online_trig);
	kfree(psy->online_trig_name);
}

/* Choice what triggers to create&update. */

void power_supply_update_leds(struct power_supply *psy)
{
	if (psy->type == POWER_SUPPLY_TYPE_BATTERY)
		power_supply_update_bat_leds(psy);
	else
		power_supply_update_gen_leds(psy);
}

int power_supply_create_triggers(struct power_supply *psy)
{
	if (strstr(saved_command_line, "androidboot.mode=charger") || strstr(saved_command_line, "androidboot.mode=recovery")) {
		if (psy->type == POWER_SUPPLY_TYPE_BATTERY)
			return power_supply_create_bat_triggers(psy);
		return power_supply_create_gen_triggers(psy);
	} else
		return 0;
}

void power_supply_remove_triggers(struct power_supply *psy)
{
	if (psy->type == POWER_SUPPLY_TYPE_BATTERY)
		power_supply_remove_bat_triggers(psy);
	else
		power_supply_remove_gen_triggers(psy);
}
