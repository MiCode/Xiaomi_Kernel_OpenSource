#ifndef _EEPROM_DATA_H
#define _EEPROM_DATA_H

/* #define MT6516ISP_MaxTableSize_CNT 4096 */
/* #define EEPROM_DBG_MESSAGE */

typedef struct {
	u32 u4Offset;
	u32 u4Length;
	u8 *pu1Params;
} stEEPROM_INFO_STRUCT, *stPEEPROM_INFO_STRUCT;
#endif				/* _EEPROM_DATA_H */
