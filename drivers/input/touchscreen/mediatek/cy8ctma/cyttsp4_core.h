/* BEGIN PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
//add Touch driver for G610-T11
/* BEGIN PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* BEGIN PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* BEGIN PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/*
 * cyttsp4_core.h
 * Cypress TrueTouch(TM) Standard Product V4 Core driver module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 * Modifed by: Cypress Semiconductor to add touch settings
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_CORE_H
#define _LINUX_CYTTSP4_CORE_H

#define CYTTSP4_CORE_NAME "cyttsp4_core"

#define CYTTSP4_STR(x) #x
#define CYTTSP4_STRINGIFY(x) CYTTSP4_STR(x)

#define CY_DRIVER_NAME TTDA
#define CY_DRIVER_MAJOR 02
#define CY_DRIVER_MINOR 02

#define CY_DRIVER_REVCTRL 402597	/*00000A */

#define CY_DRIVER_VERSION			    \
CYTTSP4_STRINGIFY(CY_DRIVER_NAME)		    \
"." CYTTSP4_STRINGIFY(CY_DRIVER_MAJOR)		    \
"." CYTTSP4_STRINGIFY(CY_DRIVER_MINOR)		    \
"." CYTTSP4_STRINGIFY(CY_DRIVER_REVCTRL)

#define CY_DRIVER_DATE "20121109"	/* YYYYMMDD */

/* x-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_X_MASK 0x7F

/* y-axis resolution of panel in pixels */
#define CY_PCFG_RESOLUTION_Y_MASK 0x7F

/* x-axis, 0:origin is on left side of panel, 1: right */
#define CY_PCFG_ORIGIN_X_MASK 0x80

/* y-axis, 0:origin is on top side of panel, 1: bottom */
#define CY_PCFG_ORIGIN_Y_MASK 0x80

#define CY_TOUCH_SETTINGS_MAX 32
#define CY_TOUCH_SETTINGS_PARAM_REGS 6

struct touch_settings {
	const uint8_t   *data;
	uint32_t         size;
	const uint8_t *ver;
	uint32_t vsize;
	uint8_t         tag;
} __packed;

struct cyttsp4_touch_firmware {
	const uint8_t *img;
	uint32_t size;
	const uint8_t *ver;
	uint8_t vsize;
} __packed;

/* BEGIN PN:SPBB-1254  ,Added by F00184246, 2013/2/18*/
struct  cyttsp4_sett_param_map {
	u8 id;
	struct touch_settings *param;
};
/* END PN:SPBB-1254  ,Added by F00184246, 2013/2/18*/
/* BEGIN PN:SPBB-1254  ,Modified by F00184246, 2013/2/18*/
struct cyttsp4_loader_platform_data {
	struct cyttsp4_touch_firmware *fw;
	struct touch_settings *param_regs;
	struct touch_settings *param_size;
	struct cyttsp4_sett_param_map *param_map; 
	u32 flags;
} __packed;
/* END PN:SPBB-1254  ,Modified by F00184246, 2013/2/18*/

struct cyttsp4_core_platform_data {
	int irq_gpio;
	int rst_gpio;
	int level_irq_udelay;
	unsigned use_configure_sensitivity:1;
	int (*xres)(struct cyttsp4_core_platform_data *pdata,
		struct device *dev);
	int (*init)(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev);
	int (*power)(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq);
	int (*irq_stat)(struct cyttsp4_core_platform_data *pdata,
		struct device *dev);
	struct touch_settings *sett[CY_TOUCH_SETTINGS_MAX];
	struct cyttsp4_loader_platform_data *loader_pdata;
};

#ifdef VERBOSE_DEBUG
extern void cyttsp4_pr_buf(struct device *dev, u8 *pr_buf, u8 *dptr, int size,
			   const char *data_name);
#else
#define cyttsp4_pr_buf(a, b, c, d, e) do { } while (0)
#endif

#endif /* _LINUX_CYTTSP4_CORE_H */
/* END PN:SPBB-1218 ,Added by l00184147, 2012/12/20*/
/* END PN:DTS2013011401860  ,Modified by l00184147, 2013/1/14*/
/* END PN:DTS2013012601133 ,Modified by l00184147, 2013/1/26*/ 
/* END PN:DTS2013051703879 ,Added by l00184147, 2013/5/17*/
