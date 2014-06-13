#ifndef __DC_XPWR_GPADC_H__
#define __DC_XPWR_GPADC_H__

#define GPADC_VBAT		(1 << 7)
#define GPADC_BAT_CUR		(1 << 6)
#define GPADC_PMICTEMP		(1 << 5)
#define GPADC_SYSTHERM		(1 << 4)
#define GPADC_BATTEMP0		(1 << 0)

#define GPADC_CH_NUM		6

#define GPADC_RSL(channel, res) (res->data[ffs(channel)-1])

struct iio_dev;

struct gpadc_result {
	int data[GPADC_CH_NUM];
};
#endif
