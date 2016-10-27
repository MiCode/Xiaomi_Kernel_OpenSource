/* drivers/input/misc/ots_pat9125/pixart_ots.c
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 */

#include "pixart_platform.h"
#include "pixart_ots.h"

static void ots_write_read(struct i2c_client *client, u8 address, u8 wdata)
{
	u8 read_value;

	do {
		write_data(client, address, wdata);
		read_value = read_data(client, address);
	} while (read_value != wdata);
}

bool ots_sensor_init(struct i2c_client *client)
{
	u8 sensor_pid = 0;
	bool read_id_ok = false;

	/*
	 * Read sensor_pid in address 0x00 to check if the
	 * serial link is valid, read value should be 0x31.
	 */
	sensor_pid = read_data(client, PIXART_PAT9125_PRODUCT_ID1_REG);

	if (sensor_pid == PIXART_PAT9125_SENSOR_ID) {
		read_id_ok = true;

		/*
		 * PAT9125 sensor recommended settings:
		 * switch to bank0, not allowed to perform ots_write_read
		 */
		write_data(client, PIXART_PAT9125_SELECT_BANK_REG,
				PIXART_PAT9125_BANK0);
		/*
		 * software reset (i.e. set bit7 to 1).
		 * It will reset to 0 automatically
		 * so perform OTS_RegWriteRead is not allowed.
		 */
		write_data(client, PIXART_PAT9125_CONFIG_REG,
				PIXART_PAT9125_RESET);

		/* delay 1ms */
		usleep_range(RESET_DELAY_US, RESET_DELAY_US + 1);

		/* disable write protect */
		ots_write_read(client, PIXART_PAT9125_WRITE_PROTECT_REG,
				PIXART_PAT9125_DISABLE_WRITE_PROTECT);
		/* set X-axis resolution (depends on application) */
		ots_write_read(client, PIXART_PAT9125_SET_CPI_RES_X_REG,
				PIXART_PAT9125_CPI_RESOLUTION_X);
		/* set Y-axis resolution (depends on application) */
		ots_write_read(client, PIXART_PAT9125_SET_CPI_RES_Y_REG,
				PIXART_PAT9125_CPI_RESOLUTION_Y);
		/* set 12-bit X/Y data format (depends on application) */
		ots_write_read(client, PIXART_PAT9125_ORIENTATION_REG,
				PIXART_PAT9125_MOTION_DATA_LENGTH);
		/* ONLY for VDD=VDDA=1.7~1.9V: for power saving */
		ots_write_read(client, PIXART_PAT9125_VOLTAGE_SEGMENT_SEL_REG,
				PIXART_PAT9125_LOW_VOLTAGE_SEGMENT);

		if (read_data(client, PIXART_PAT9125_MISC2_REG) == 0x04) {
			ots_write_read(client, PIXART_PAT9125_MISC2_REG, 0x08);
			if (read_data(client, PIXART_PAT9125_MISC1_REG) == 0x10)
				ots_write_read(client, PIXART_PAT9125_MISC1_REG,
						0x19);
		}
		/* enable write protect */
		ots_write_read(client, PIXART_PAT9125_WRITE_PROTECT_REG,
				PIXART_PAT9125_ENABLE_WRITE_PROTECT);
	}
	return read_id_ok;
}
