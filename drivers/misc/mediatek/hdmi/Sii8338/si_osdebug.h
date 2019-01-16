#ifndef __SI_OSDEBUG_H__
#define __SI_OSDEBUG_H__
typedef enum {
	SII_OS_DEBUG_FORMAT_SIMPLE = 0x0000u, SII_OS_DEBUG_FORMAT_FILEINFO =
	    0x0001u, SII_OS_DEBUG_FORMAT_CHANNEL = 0x0002u, SII_OS_DEBUG_FORMAT_TIMESTAMP = 0x0004u
} SiiOsDebugFormat_e;
#define MODULE_SET(name) \
    SII_OSAL_DEBUG_##name, \
    SII_OSAL_DEBUG_##name##_DBG = SII_OSAL_DEBUG_##name, \
    SII_OSAL_DEBUG_##name##_ERR, \
    SII_OSAL_DEBUG_##name##_TRACE, \
    SII_OSAL_DEBUG_##name##_ALWAYS, \
    SII_OSAL_DEBUG_##name##_USER1, \
    SII_OSAL_DEBUG_##name##_USER2, \
    SII_OSAL_DEBUG_##name##_USER3, \
    SII_OSAL_DEBUG_##name##_USER4, \
    SII_OSAL_DEBUG_##name##_MASK = SII_OSAL_DEBUG_##name##_USER4,
typedef enum {
	MODULE_SET(APP)
	    MODULE_SET(TRACE)
	    MODULE_SET(POWER_MAN)
	    MODULE_SET(TX)
	    MODULE_SET(EDID)
	    MODULE_SET(HDCP)
	    MODULE_SET(AV_CONFIG)
	    MODULE_SET(ENTRY_EXIT)
	    MODULE_SET(CBUS)
	    MODULE_SET(SCRATCHPAD)
	    MODULE_SET(SCHEDULER)
	    MODULE_SET(CRA)
	SII_OSAL_DEBUG_NUM_CHANNELS
} SiiOsalDebugChannels_e;
#ifndef SII_DEBUG_CONFIG_RESOURCE_CONSTRAINED
typedef void SiiOsDebugChannel_t;
uint32_t SiiOsDebugChannelAdd(uint32_t numChannels, SiiOsDebugChannel_t *paChannelList);
#endif
void SiiOsDebugChannelEnable(SiiOsalDebugChannels_e channel);
void SiiOsDebugChannelDisable(SiiOsalDebugChannels_e channel);
#define SI_OS_DISABLE_DEBUG_CHANNEL(channel) SiiOsDebugChannelDisable(channel)
bool_t SiiOsDebugChannelIsEnabled(SiiOsalDebugChannels_e channel);
void SiiOsDebugSetConfig(uint16_t flags);
#define SiiOsDebugConfig(flags) SiiOsDebugSetConfig(flags)
uint16_t SiiOsDebugGetConfig(void);
void SiiOsDebugPrintSimple(SiiOsalDebugChannels_e channel, char *pszFormat, ...);
void SiiOsDebugPrintShort(SiiOsalDebugChannels_e channel, char *pszFormat, ...);
void SiiOsDebugPrint(const char *pFileName, uint32_t iLineNum, SiiOsalDebugChannels_e channel,
		     const char *pszFormat, ...);
#ifdef SII_DEBUG_CONFIG_NO_FILE_LINE
#define SII_DEBUG_PRINT(channel, ...) SiiOsDebugPrintShort(channel, __VA_ARGS__)
#else
#define SII_DEBUG_PRINT(channel, ...) SiiOsDebugPrint(__FILE__, __LINE__, channel, __VA_ARGS__)
#endif
#define SII_DEBUG(channel, x) if (SiiOsDebugChannelIsEnabled(channel) {x}
#define SII_PRINT_FULL(channel, ...) SiiOsDebugPrint(__FILE__, __LINE__, channel, __VA_ARGS__)
#define SII_PRINT(channel, ...) SiiOsDebugPrintShort(channel, __VA_ARGS__)
#define SII_PRINT_PLAIN(channel, ...) SiiOsDebugPrintSimple(channel, __VA_ARGS__)
#endif
