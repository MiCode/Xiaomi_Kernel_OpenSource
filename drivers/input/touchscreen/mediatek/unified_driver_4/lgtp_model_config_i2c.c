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
 *    File  : lgtp_model_config_i2c.c
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/
#define LGTP_MODULE "[CONFIG]"

/****************************************************************************
* Include Files
****************************************************************************/
#include <lgtp_common.h>
#include <lgtp_model_config_i2c.h>



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
#if defined(TOUCH_PLATFORM_QCT) || defined(CONFIG_USE_OF)

#if defined(TOUCH_MODEL_C70)

static struct of_device_id match_table_s3320[] = {
	{.compatible = "unified_driver4,s3320",},
	{},
};

#elif defined(TOUCH_MODEL_Y30)
struct of_device_id Lu202x_MatchTable[2] = { {.compatible = "unified_driver_ver2,lu201x",}, };

#elif defined(TOUCH_MODEL_P1C)
static struct of_device_id Mit300_MatchTable[] = {
	{.compatible = "unified_driver,ver4,mit300",},
	{},
};

#elif defined(TOUCH_MODEL_M1V) || defined(TOUCH_MODEL_M4)
static struct of_device_id Mit300_MatchTable[] = {
	{.compatible = "mediatek,cap_touch",},
	{},
};

#elif defined(TOUCH_MODEL_K7)
static struct of_device_id FT8707_MatchTable[] = {
	{ .compatible = "unified_driver_ver4,ft8707", },
	{ },
};

#else

#error "Model should be defined"
#endif

#endif


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
#if defined(TOUCH_PLATFORM_MTK)
int TouchGetDeviceSlaveAddress(int index)
{
	int slave_addr = 0;

#if defined(TOUCH_MODEL_Y70)

	if (index == FIRST_MODULE)
		slave_addr = TOUCH_S3320_I2C_SLAVE_ADDR;

#elif defined(TOUCH_MODEL_LION_3G)

	if (index == FIRST_MODULE)
		slave_addr = TOUCH_TD4191_I2C_SLAVE_ADDR;

#elif defined(TOUCH_MODEL_M1V) || defined(TOUCH_MODEL_M4)
	if (index == FIRST_MODULE)
		slave_addr = TOUCH_MIT300_I2C_SLAVE_ADDR;
#elif defined(TOUCH_MODEL_K7)
	if (index == FIRST_MODULE)
		slave_addr = TOUCH_FT8707_I2C_SLAVE_ADDR;

#else
#error "Model should be defined"
#endif

	return slave_addr;
}
#endif

#if defined(TOUCH_PLATFORM_QCT) || defined(CONFIG_USE_OF)
struct of_device_id *TouchGetDeviceMatchTable(int index)
{
	struct of_device_id *match_table = NULL;

#if defined(TOUCH_MODEL_C70)

	if (index == FIRST_MODULE)
		match_table = match_table_s3320;
#elif defined(TOUCH_MODEL_Y30)
	if (index == FIRST_MODULE)
		match_table = Lu202x_MatchTable;
#elif defined(TOUCH_MODEL_M1V) || defined(TOUCH_MODEL_M4)
	if (index == FIRST_MODULE)
		match_table = Mit300_MatchTable;
#elif defined(TOUCH_MODEL_K7)
	if (index == FIRST_MODULE)
		match_table = FT8707_MatchTable;
#else
#error "Model should be defined"
#endif

	return match_table;

}
#endif


/* End Of File */
