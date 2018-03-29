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
 *    File	: lgtp_platform_api_api.c
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/
#define LGTP_MODULE "[SPI]"

/****************************************************************************
* Include Files
****************************************************************************/
#include <lgtp_common.h>
#include <lgtp_model_config_i2c.h>
#include <lgtp_platform_api_misc.h>


/****************************************************************************
* Manifest Constants / Defines
****************************************************************************/
#define LGE_TOUCH_NAME "touch_spi"


/****************************************************************************
 * Macros
 ****************************************************************************/


/****************************************************************************
* Type Definitions
****************************************************************************/

/****************************************************************************
* Variables
****************************************************************************/
#if defined(TOUCH_PLATFORM_MTK)
#endif

static struct spi_device *spi_dev;

/****************************************************************************
* Extern Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Function Prototypes
****************************************************************************/


/****************************************************************************
* Local Functions
****************************************************************************/

struct spi_device *Touch_Get_SPI_Handle(void)
{
	return spi_dev;
}


int Touch_SPI_Read(u16 addr, u8 *rxbuf, int len)
{
	int ret = 0;
	/* TBD */
	if (ret == TOUCH_FAIL) {
		TOUCH_ERR("failed to write spi ( reg = %d )\n", addr);
		return TOUCH_FAIL;
	}

	return TOUCH_SUCCESS;
}

int Touch_SPI_Write(u16 addr, u8 *txbuf, int len)
{
	int ret = 0;
	/* TBD */
	if (ret == TOUCH_FAIL) {
		TOUCH_ERR("failed to write spi ( reg = %d )\n", addr);
		return TOUCH_FAIL;
	}

	return TOUCH_SUCCESS;
}

static int __init touch_spi_init(void)
{
	int idx = 0;
	/* TBD */
#if defined(TOUCH_SPI_USE)
	TOUCH_FUNC();

#else
	TOUCH_LOG("TOUCH_SPI_USE is not defined in this model\n");
#endif
	return TOUCH_SUCCESS;

}

static void __exit touch_spi_exit(void)
{
	/* TBD */
	TOUCH_FUNC();
}
module_init(touch_spi_init);
module_exit(touch_spi_exit);

MODULE_AUTHOR("D3 BSP Touch Team");
MODULE_DESCRIPTION("LGE Touch Unified Driver");
MODULE_LICENSE("GPL");

/* End Of File */
