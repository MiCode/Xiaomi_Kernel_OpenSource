#ifndef __DC_TI_GPADC_H__
#define __DC_TI_GPADC_H__

#define GPADC_CH_NUM		4

#define GPADC_RSL(channel, res) (res->data[ffs(channel)-1])

struct iio_dev;

struct gpadc_result {
	int data[GPADC_CH_NUM];
};

int iio_dc_ti_gpadc_sample(struct iio_dev *indio_dev,
				int ch, struct gpadc_result *res);

int intel_dc_ti_gpadc_sample(int ch, struct gpadc_result *res);
#endif
