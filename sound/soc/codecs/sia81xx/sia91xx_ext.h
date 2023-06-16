#ifndef __SIA91XX_EXT_INC__
#define __SIA91XX_EXT_INC__
#include <linux/i2c.h>

extern int sipa_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id);
extern int sipa_i2c_remove(struct i2c_client *i2c);

#endif /* __TFA98XX_EXT_INC__ */


