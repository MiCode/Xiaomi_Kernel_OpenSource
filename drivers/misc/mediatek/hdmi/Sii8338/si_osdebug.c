#include "si_common.h"
#include "si_osdebug.h"
#include <linux/slab.h>
#include <linux/string.h>
#define ChannelIndex(channel) (channel >> 3)
#define ChannelMask(channel) (1 << (channel & 7))
extern unsigned char DebugChannelMasks[];
extern ushort DebugFormat;
void SiiOsDebugChannelEnable(SiiOsalDebugChannels_e channel)
{
#if defined(DEBUG)
	uint8_t index = ChannelIndex(channel);
	uint8_t mask = ChannelMask(channel);
	DebugChannelMasks[index] |= mask;
#endif
}

void SiiOsDebugChannelDisable(SiiOsalDebugChannels_e channel)
{
#if defined(DEBUG)
	uint8_t index = ChannelIndex(channel);
	uint8_t mask = ChannelMask(channel);
	DebugChannelMasks[index] &= ~mask;
#endif
}

bool_t SiiOsDebugChannelIsEnabled(SiiOsalDebugChannels_e channel)
{
#if defined(DEBUG)
	uint8_t index = ChannelIndex(channel);
	uint8_t mask = ChannelMask(channel);
	return (DebugChannelMasks[index] & mask) ? true : false;
#else
	return false;
#endif
}

void SiiOsDebugSetConfig(uint16_t flags)
{
#if defined(DEBUG)
	DebugFormat = flags;
#endif
}

uint16_t SiiOsDebugGetConfig(void)
{
#if defined(DEBUG)
	return DebugFormat;
#else
	return 0;
#endif
}

void SiiOsDebugPrintSimple(SiiOsalDebugChannels_e channel, char *pszFormat, ...)
{
#if 1
/* #if defined(DEBUG) */
	if (SiiOsDebugChannelIsEnabled(channel)) {
		va_list ap;
		va_start(ap, pszFormat);
		printk(pszFormat, ap);
		va_end(ap);
	}
#endif
}

void SiiOsDebugPrintShort(SiiOsalDebugChannels_e channel, char *pszFormat, ...)
{
/* #if defined(DEBUG) */
#if 1
	if (SiiOsDebugChannelIsEnabled(channel)) {
		va_list ap;
		va_start(ap, pszFormat);
		SiiOsDebugPrint(NULL, 0, channel, pszFormat, ap);
		va_end(ap);
	}
#endif
}

#define MAX_DEBUG_MSG_SIZE	512
void SiiOsDebugPrint(const char *pszFileName, uint32_t iLineNum, uint32_t channel,
		     const char *pszFormat, ...)
{
#if defined(DEBUG)
	uint8_t *pBuf = NULL;
	uint8_t *pBufOffset;
	int remainingBufLen = MAX_DEBUG_MSG_SIZE;
	int len;
	va_list ap;
	if (SiiOsDebugChannelIsEnabled(channel)) {
		pBuf = kmalloc(remainingBufLen, GFP_KERNEL);
		if (pBuf == NULL)
			return;
		pBufOffset = pBuf;
		if (pszFileName != NULL && (SII_OS_DEBUG_FORMAT_FILEINFO & DebugFormat)) {
			const char *pc;
			for (pc = &pszFileName[strlen(pszFileName)]; pc >= pszFileName; --pc) {
				if ('\\' == *pc) {
					++pc;
					break;
				}
				if ('/' == *pc) {
					++pc;
					break;
				}
			}
			len = scnprintf(pBufOffset, remainingBufLen, "%s:%d ", pc, (int)iLineNum);
			if (len < 0) {
				kfree(pBuf);
				return;
			}
			remainingBufLen -= len;
			pBufOffset += len;
		}
		if (SII_OS_DEBUG_FORMAT_CHANNEL & DebugFormat) {
			len = scnprintf(pBufOffset, remainingBufLen, "Chan:%d ", channel);
			if (len < 0) {
				kfree(pBuf);
				return;
			}
			remainingBufLen -= len;
			pBufOffset += len;
		}
		va_start(ap, pszFormat);
		vsnprintf(pBufOffset, remainingBufLen, pszFormat, ap);
		va_end(ap);
		printk(pBuf);
		kfree(pBuf);
	}
#endif
}
