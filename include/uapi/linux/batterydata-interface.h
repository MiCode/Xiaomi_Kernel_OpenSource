#ifndef __BATTERYDATA_LIB_H__
#define __BATTERYDATA_LIB_H__

#include <linux/ioctl.h>

/**
 * struct battery_params - Battery profile data to be exchanged.
 * @soc:	SOC (state of charge) of the battery
 * @ocv_uv:	OCV (open circuit voltage) of the battery
 * @rbatt_sf:	RBATT scaling factor
 * @batt_temp:	Battery temperature in deci-degree.
 * @slope:	Slope of the OCV-SOC curve.
 * @fcc_mah:	FCC (full charge capacity) of the battery.
 */
struct battery_params {
	int soc;
	int ocv_uv;
	int rbatt_sf;
	int batt_temp;
	int slope;
	int fcc_mah;
};

/*  IOCTLs to query battery profile data */
#define BPIOCXSOC	_IOWR('B', 0x01, struct battery_params) /* SOC */
#define BPIOCXRBATT	_IOWR('B', 0x02, struct battery_params) /* RBATT SF */
#define BPIOCXSLOPE	_IOWR('B', 0x03, struct battery_params) /* SLOPE */
#define BPIOCXFCC	_IOWR('B', 0x04, struct battery_params) /* FCC */

#endif
