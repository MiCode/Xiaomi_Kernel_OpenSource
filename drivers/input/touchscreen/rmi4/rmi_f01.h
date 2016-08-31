/*
 * Copyright (c) 2012 Synaptics Incorporated
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef _RMI_F01_H
#define _RMI_F01_H

#define RMI_PRODUCT_ID_LENGTH    10
#define RMI_PRODUCT_INFO_LENGTH   2

#define RMI_DATE_CODE_LENGTH      3

#define PRODUCT_ID_OFFSET 0x10
#define PRODUCT_INFO_OFFSET 0x1E

#define F01_RESET_MASK 0x01

#define F01_SERIALIZATION_SIZE 7

/**
 * @manufacturer_id - reports the identity of the manufacturer of the RMI
 * device. Synaptics RMI devices report a Manufacturer ID of $01.
 * @custom_map - at least one custom, non
 * RMI-compatible register exists in the register address map for this device.
 * @non-compliant - the device implements a register map that is not compliant
 * with the RMI specification.
 * @has_lts - the device uses Synaptics' LTS hardware architecture.
 * @has_sensor_id - the SensorID query register (F01_RMI_Query22) exists.
 * @has_charger_input - the ChargerConnected bit (F01_RMI_Ctrl0, bit 5) is
 * meaningful.
 * @has_adjustable_doze - the doze (power management) control registers exist.
 * @has_adjustable_doze_holdoff - the doze holdoff register exists.
 * @has_product_properties - indicates the presence of F01_RMI_Query42,
 * ProductProperties2.
 * @productinfo_1 - meaning varies from product to product, consult your
 * product spec sheet.
 * @productinfo_2 - meaning varies from product to product, consult your
 * product spec sheet.
 */
struct f01_basic_queries {
	u8 manufacturer_id:8;

	u8 custom_map:1;
	u8 non_compliant:1;
	u8 has_lts:1;
	u8 has_sensor_id:1;
	u8 has_charger_input:1;
	u8 has_adjustable_doze:1;
	u8 has_adjustable_doze_holdoff:1;
	u8 has_product_properties_2:1;

	u8 productinfo_1:7;
	u8 q2_bit_7:1;
	u8 productinfo_2:7;
	u8 q3_bit_7:1;

} __attribute__((__packed__));

/** The status code field reports the most recent device status event.
 * @no_error - should be self explanatory.
 * @reset_occurred - no other event was seen since the last reset.
 * @invalid_config - general device configuration has a problem.
 * @device_failure - general device hardware failure.
 * @config_crc - configuration failed memory self check.
 * @firmware_crc - firmware failed memory self check.
 * @crc_in_progress - bootloader is currently testing config and fw areas.
 */
enum rmi_device_status {
	no_error = 0x00,
	reset_occurred = 0x01,
	invalid_config = 0x02,
	device_failure = 0x03,
	config_crc = 0x04,
	firmware_crc = 0x05,
	crc_in_progress = 0x06
};

/**
 * @status_code - reports the most recent device status event.
 * @flash_prog - if set, this indicates that flash programming is enabled and
 * normal operation is not possible.
 * @unconfigured - the device has lost its configuration for some reason.
 */
struct f01_device_status {
	enum rmi_device_status status_code:4;
	u8 reserved:2;
	u8 flash_prog:1;
	u8 unconfigured:1;
} __attribute__((__packed__));

/* control register bits */
#define RMI_SLEEP_MODE_NORMAL (0x00)
#define RMI_SLEEP_MODE_SENSOR_SLEEP (0x01)
#define RMI_SLEEP_MODE_RESERVED0 (0x02)
#define RMI_SLEEP_MODE_RESERVED1 (0x03)

#define RMI_IS_VALID_SLEEPMODE(mode) \
	(mode >= RMI_SLEEP_MODE_NORMAL && mode <= RMI_SLEEP_MODE_RESERVED1)

/**
 * @sleep_mode - This field controls power management on the device. This
 * field affects all functions of the device together.
 * @nosleep - When set to ‘1’, this bit disables whatever sleep mode may be
 * selected by the sleep_mode field,and forces the device to run at full power
 * without sleeping.
 * @charger_input - When this bit is set to ‘1’, the touch controller employs
 * a noise-filtering algorithm designed for use with a connected battery
 * charger.
 * @report_rate - sets the report rate for the device.  The effect of this
 * setting is highly product dependent.  Check the spec sheet for your
 * particular touch sensor.
 * @configured - written by the host as an indicator that the device has been
 * successfuly configured.
 */
struct f01_device_control_0 {
	u8 sleep_mode:2;
	u8 nosleep:1;
	u8 reserved:2;
	u8 charger_input:1;
	u8 report_rate:1;
	u8 configured:1;
} __attribute__((__packed__));

#endif
