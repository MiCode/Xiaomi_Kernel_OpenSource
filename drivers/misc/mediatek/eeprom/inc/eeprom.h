#ifndef _EEPROM_H
#define _EEPROM_H

#include <linux/ioctl.h>


#define EEPROMAGIC 'i'
/* IOCTRL(inode * ,file * ,cmd ,arg ) */
/* S means "set through a ptr" */
/* T means "tell by a arg value" */
/* G means "get by a ptr" */
/* Q means "get by return a value" */
/* X means "switch G and S atomically" */
/* H means "switch T and Q atomically" */

/*******************************************************************************
*
********************************************************************************/

/* EEPROM write */
#define EEPROMIOC_S_WRITE            _IOW(EEPROMAGIC, 0, stEEPROM_INFO_STRUCT)
/* EEPROM read */
#define EEPROMIOC_G_READ            _IOWR(EEPROMAGIC, 5, stPEEPROM_INFO_STRUCT)

#endif				/* _EEPROM_H */
