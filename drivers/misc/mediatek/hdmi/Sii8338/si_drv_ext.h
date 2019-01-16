#ifndef _SI_DRV_EXT_H_
#define _SI_DRV_EXT_H_
#include "si_platform.h"
#ifndef __KERNEL__
#include <string.h>
#include "hal_local.h"
#include "si_common.h"
#else
#include <linux/types.h>
#endif
bool_t CheckExtVideo(void);
unsigned char GetExt_AudioType(void);
uint8_t GetExt_inputColorSpace(void);
uint8_t GetExt_inputVideoCode(void);
uint8_t GetExt_inputcolorimetryAspectRatio(void);
uint8_t GetExt_inputAR(void);
void InitExtVideo(void);
void TriggerExtInt(void);
#endif
