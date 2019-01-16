#include <linux/types.h>
#include <mach/mt_pm_ldo.h>
#include <cust_mag.h>


static struct mag_hw cust_mag_hw = {
    .i2c_num = 3,

#ifdef CONFIG_CM865_MAINBOARD
    .direction = 1,
#else
    .direction = 2,
#endif

    .power_id = MT65XX_POWER_NONE,  /*!< LDO is not used */
    .power_vol= VOL_DEFAULT,        /*!< LDO is not used */
    .is_batch_supported = false,
};
struct mag_hw* get_cust_mag_hw(void) 
{
    return &cust_mag_hw;
}
