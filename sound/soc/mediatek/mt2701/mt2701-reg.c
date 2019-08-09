#include <linux/regmap.h>
#include "mt2701-afe-common.h"

void mt2701_regmap_update_bits(struct regmap *map, unsigned int reg,
			unsigned int mask, unsigned int val){
	int ret;

	ret = regmap_update_bits(map, reg, mask, val);
	if (ret != 0)
		dev_info(regmap_get_device(map),
			"regmap set error reg(0x%x) err(%d)", reg, ret);
}

void mt2701_regmap_write(struct regmap *map, unsigned int reg, unsigned int val)
{
	int ret;

	ret = regmap_write(map, reg, val);
	if (ret != 0)
		dev_info(regmap_get_device(map),
			"regmap set error reg(0x%x) err(%d)", reg, ret);
}

void mt2701_regmap_read(struct regmap *map, unsigned int reg, unsigned int *val)
{
	int ret;

	ret = regmap_read(map, reg, val);
	if (ret != 0)
		dev_info(regmap_get_device(map),
			"regmap read error reg(0x%x) err(%d)", reg, ret);
}
