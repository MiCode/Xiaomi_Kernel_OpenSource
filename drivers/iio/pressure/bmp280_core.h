#ifndef _BMP280_CORE_H
#define _BMP280_CORE_H

#include "BMP280.h"
#include "bs_log.h"

#define BMP_NAME "bmp280"
#define BMP_INPUT_NAME "bmpX80"

#define BMP_REG_NAME(name) BMP280_##name
#define BMP_VAL_NAME(name) BMP280_##name
#define BMP_CALL_API(name) bmp280_##name

struct bmp_bus_ops {
	BMP280_WR_FUNC_PTR;
	BMP280_RD_FUNC_PTR;
};

struct bmp_data_bus {
	const struct bmp_bus_ops	*bops;
	void	*client;
};

int bmp_probe(struct device *dev, struct bmp_data_bus *data_bus);
int bmp_remove(struct device *dev);
#ifdef CONFIG_PM
int bmp_enable(struct device *dev);
int bmp_disable(struct device *dev);
#endif

#endif/*_BMP280_CORE_H*/
