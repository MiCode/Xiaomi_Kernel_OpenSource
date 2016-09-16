
#include "pixart_ots.h"

static void OTS_WriteRead(uint8_t address, uint8_t wdata);

bool OTS_Sensor_Init(void)
{
	unsigned char sensor_pid = 0, read_id_ok = 0;

	/* Read sensor_pid in address 0x00 to check if the
	 serial link is valid, read value should be 0x31. */
	sensor_pid = ReadData(0x00);

	if (sensor_pid == 0x31) {
		read_id_ok = 1;

		/* PAT9125 sensor recommended settings: */
		/* switch to bank0, not allowed to perform OTS_RegWriteRead */
		WriteData(0x7F, 0x00);
		/* software reset (i.e. set bit7 to 1).
		It will reset to 0 automatically */
		/* so perform OTS_RegWriteRead is not allowed. */
		WriteData(0x06, 0x97);

		delay_ms(1);				/* delay 1ms */

		/* disable write protect */
		OTS_WriteRead(0x09, 0x5A);
		/* set X-axis resolution (depends on application) */
		OTS_WriteRead(0x0D, 0x65);
		/* set Y-axis resolution (depends on application) */
		OTS_WriteRead(0x0E, 0xFF);
		/* set 12-bit X/Y data format (depends on application) */
		OTS_WriteRead(0x19, 0x04);
		/* ONLY for VDD=VDDA=1.7~1.9V: for power saving */
		OTS_WriteRead(0x4B, 0x04);

		if (ReadData(0x5E) == 0x04) {
			OTS_WriteRead(0x5E, 0x08);
			if (ReadData(0x5D) == 0x10)
				OTS_WriteRead(0x5D, 0x19);
		}
		OTS_WriteRead(0x09, 0x00);/* enable write protect */
	}
	return read_id_ok;
}

static void OTS_WriteRead(uint8_t address, uint8_t wdata)
{
	uint8_t read_value;
	do {
		/* Write data to specified address */
		WriteData(address, wdata);
		/* Read back previous written data */
		read_value = ReadData(address);
		/* Check if the data is correctly written */
	} while (read_value != wdata);
	return;
}

