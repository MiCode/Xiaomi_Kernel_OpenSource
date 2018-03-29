/***************************************************************************
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *    File  : lgtp_platform_api_power.c
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/
#define LGTP_MODULE "[POWER]"

/****************************************************************************
* Include Files
****************************************************************************/
#include <lgtp_common.h>

#include <lgtp_model_config_misc.h>
#include <lgtp_model_config_i2c.h>

#include <lgtp_platform_api_misc.h>
#include <lgtp_platform_api_i2c.h>


/****************************************************************************
* Manifest Constants / Defines
****************************************************************************/

/****************************************************************************
 * Macros
 ****************************************************************************/

/****************************************************************************
* Type Definitions
****************************************************************************/

/****************************************************************************
* Variables
****************************************************************************/


/****************************************************************************
* Extern Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Functions
****************************************************************************/


/****************************************************************************
* Global Functions
****************************************************************************/

#if defined(TOUCH_PLATFORM_QCT)
void TouchPowerPMIC(int isOn, char *id, int min_uV, int max_uV)
{
	static struct regulator *vdd_vio;
	struct i2c_client *client = Touch_Get_I2C_Handle();
	int error = 0;

	TOUCH_LOG("[Touch] %s : TouchPowerPMIC isOn:%d id:%s min_uv:%d max_uV:%d\n", __func__, isOn,
		  id, min_uV, max_uV);

	if (vdd_vio == NULL) {
		if (client != NULL)
			vdd_vio = regulator_get(&client->dev, id);
		else
			vdd_vio = regulator_get(NULL, id);
		if (IS_ERR(vdd_vio)) {
			error = PTR_ERR(vdd_vio);
			TOUCH_ERR("failed to get regulator ( error = %d )\n", error);
			return;
		}

		error = regulator_set_voltage(vdd_vio, min_uV, max_uV);
		if (error < 0) {
			TOUCH_ERR("failed to set regulator voltage ( error = %d )\n", error);
			return;
		}
	}

	if (vdd_vio != NULL) {
		if (isOn) {
			error = regulator_enable(vdd_vio);
			if (error < 0) {
				TOUCH_ERR("failed to enable regulator ( error = %d )\n", error);
				return;
			}
			usleep_range(10000, 15000);
		} else {
			error = regulator_disable(vdd_vio);
			if (error < 0) {
				TOUCH_ERR("failed to enable regulator ( error = %d )\n", error);
				return;
			}
		}
	}
}
#endif

#if defined(TOUCH_PLATFORM_MTK)
void TouchPowerPMIC(int isOn, int pin, int vol)
{
	TOUCH_LOG("[Touch] %s : TouchPowerPMIC isOn:%d pin:%d vol:%d\n", __func__, isOn, pin, vol);
	if (isOn)
		hwPowerOn(pin, vol, "TP");
	else
		hwPowerDown(pin, "TP");
}
#endif

void TouchPowerSetGpio(int pin, int value)
{
	TOUCH_LOG("[Touch] %s : TouchPowerGpio pin:%d value:%d\n", __func__, pin, value);
	gpio_set_value(pin, value);
}

void TouchSetGpioDirection(int pin, int value)
{
	TOUCH_LOG("[Touch] %s : TouchGpioDirection pin:%d value:%d\n", __func__, pin, value);
#if defined(TOUCH_PLATFORM_QCT)
	if (gpio_is_valid(pin))
		gpio_direction_output(pin, value);
#elif defined(TOUCH_PLATFORM_MTK)
	mt_set_gpio_dir(pin, GPIO_DIR_OUT);
	mt_set_gpio_out(pin, value);
#endif
}

/* End Of File */
