/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux OTG Controller driver : hal
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* This is a hardware abstraction layer header file.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/

#ifndef	ISP1763_H
#define	ISP1763_H



/* For debugging option: ------------------- */
#define PTD_DUMP_SCHEDULE
#undef  PTD_DUMP_SCHEDULE

#define PTD_DUMP_COMPLETE
#undef  PTD_DUMP_COMPLETE
/* ------------------------------------*/
#define CONFIG_ISO_SUPPORT 

#ifdef CONFIG_ISO_SUPPORT

#define	ISO_DBG_ENTRY 1
#define	ISO_DBG_EXIT  1
#define	ISO_DBG_ADDR 1
#define	ISO_DBG_DATA 1
#define	ISO_DBG_ERR  1
#define	ISO_DBG_INFO 1

#if 0				/* Set to 1 to enable isochronous debugging */
#define	iso_dbg(category, format, arg...) \
do \
{ \
	if(category) \
	{ \
		printk(format, ## arg);	\
	} \
} while(0)
#else
#define	iso_dbg(category, format, arg...) while(0)
#endif

#endif /* CONFIG_ISO_SUPPORT */

/*Debug	For Entry/Exit of the functions	*/
//#define HCD_DEBUG_LEVEL1 
#ifdef HCD_DEBUG_LEVEL1
#define	pehci_entry(format, args... ) printk(format, ##args)
#else
#define	pehci_entry(format, args...) do	{ } while(0)
#endif

/*Debug	for Port Info and Errors */
//#define HCD_DEBUG_LEVEL2 
#ifdef HCD_DEBUG_LEVEL2
#define	pehci_print(format, args... ) printk(format, ##args)
#else
#define	pehci_print(format, args...) do	{ } while(0)
#endif

/*Debug	For the	Port changes and Enumeration */
//#define HCD_DEBUG_LEVEL3 
#ifdef HCD_DEBUG_LEVEL3
#define	pehci_info(format,arg...) printk(format, ##arg)
#else
#define	pehci_info(format,arg...) do {}	while (0)
#endif

/*Debug	For Transfer flow  */
// #define HCD_DEBUG_LEVEL4 
#ifdef HCD_DEBUG_LEVEL4
#define	pehci_check(format,args...) printk(format, ##args)
#else
#define	pehci_check(format,args...)
#endif
/*******************END	HOST CONTROLLER**********************************/



/*******************START DEVICE CONTROLLER******************************/

/* For MTP support */
#undef MTP_ENABLE		/* Enable to add MTP support; But requires MTP class driver to be present to work */
/*For CHAPTER8 TEST */
#undef	CHAPTER8_TEST		/* Enable to Pass Chapter 8 Test */

/* Debug Entery/Exit of	Function as well as some other Info */
//#define DEV_DEBUG_LEVEL2
#ifdef DEV_DEBUG_LEVEL2
#define	dev_print(format,arg...) printk(format,	##arg)
#else
#define	dev_print(format,arg...) do {} while (0)
#endif

/*Debug	for Interrupt ,	Registers , device Enable/Disable and some other info */
//#define DEV_DEBUG_LEVEL3
#undef dev_info
#ifdef DEV_DEBUG_LEVEL3
#define	dev_info(format,arg...)	printk(format, ##arg)
#else
#define	dev_info(format,arg...)	do {} while (0)
#endif

/*Debug	for Tranffer flow , Enumeration	and Packet info	*/
//#define DEV_DEBUG_LEVEL4
#ifdef DEV_DEBUG_LEVEL4
#define	dev_check(format,args...) printk(format, ##args)
#else
#define	dev_check(format,args...) do{}while(0)
#endif
/*******************END	DEVICE CONTROLLER********************************/


/*******************START MSCD*******************************************/
/*Debug	Entery/Exit of Function	as well	as some	other Information*/
//#define MSCD_DEBUG_LEVEL2
#ifdef MSCD_DEBUG_LEVEL2
#define	mscd_print(format,arg...) printk(format, ##arg)
#else
#define	mscd_print(format,arg...) do {}	while (0)
#endif

/*Debug	for Info */
//#define MSCD_DEBUG_LEVEL3
#ifdef MSCD_DEBUG_LEVEL3
#define	mscd_info(format,arg...) printk(format,	##arg)
#else
#define	mscd_info(format,arg...) do {} while (0)
#endif
/*******************END	MSCD*********************************************/


/*******************START OTG CONTROLLER*********************************/
/*#define	OTG */			/*undef	for Device only	and Host only */
#define	ALL_FSM_FLAGS
/*Debug	for Entry/Exit and Info	*/
/* #define OTG_DEBUG_LEVEL1 */
#ifdef OTG_DEBUG_LEVEL1
#define	otg_entry(format, args... ) printk(format, ##args)
#else
#define	otg_entry(format, args...) do {	} while(0)
#endif

/*Debug	for State Machine Flow */
/* #define OTG_DEBUG_LEVEL2 */
#ifdef OTG_DEBUG_LEVEL2
#define	otg_print(format,arg...) printk(format,	##arg)
#else
#define	otg_print(format,arg...) do {} while (0)
#endif
/*Debug	for Info */
/* #define OTG_DEBUG_LEVEL3 */
#ifdef OTG_DEBUG_LEVEL3
#define	otg_info(format,arg...)	printk(format, ##arg)
#else
#define	otg_info(format,arg...)	do {} while (0)
#endif

/* #define OTG_DEBUG_LEVEL4 */
#ifdef OTG_DEBUG_LEVEL4
#define	otg_printB(format,arg...) printk(format, ##arg)
#else
#define	otg_printB(format,arg...) do {}	while (0)
#endif
/*******************END	OTG CONTROLLER***********************************/



/*******************START FOR HAL ***************************************/
#define info pr_debug
#define warn pr_warn
/*Debug For Entry and Exit of the functions */
#undef HAL_DEBUG_LEVEL1
#ifdef HAL_DEBUG_LEVEL1
#define	hal_entry(format, args... ) printk(format, ##args)
#else
#define	hal_entry(format, args...) do {	} while(0)
#endif

/*Debug	For Interrupt information */
#undef HAL_DEBUG_LEVEL2
#ifdef HAL_DEBUG_LEVEL2
#define	hal_int(format,	args...	) printk(format, ##args)
#else
#define	hal_int(format,	args...) do { }	while(0)
#endif

/*Debug	For HAL	Initialisation and Mem Initialisation */
#undef HAL_DEBUG_LEVEL3
#ifdef HAL_DEBUG_LEVEL3
#define	hal_init(format, args... ) printk(format, ##args)
#else
#define	hal_init(format, args...) do { } while(0)
#endif
/*******************END	FOR HAL*******************************************/



/*******************START FOR ALL CONTROLLERS*****************************/
/*#define	CONFIG_USB_OTG */	/*undef	for Device only	and Host only */
/*#define	ISP1763_DEVICE */

#ifdef CONFIG_USB_DEBUG
#define	DEBUG
#else
#undef DEBUG
#endif
/*******************END	FOR ALL	CONTROLLERS*******************************/
#endif
