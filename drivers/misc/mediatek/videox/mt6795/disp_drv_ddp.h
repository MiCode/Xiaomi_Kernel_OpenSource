
// Only for DDP driver.
#ifndef __DISP_DRV_DDP_H__
#define __DISP_DRV_DDP_H__
#include <mach/mt_typedefs.h>

typedef int (*DISP_EXTRA_CHECKUPDATE_PTR)(int);
typedef int (*DISP_EXTRA_CONFIG_PTR)(int);
int DISP_RegisterExTriggerSource(DISP_EXTRA_CHECKUPDATE_PTR pCheckUpdateFunc , DISP_EXTRA_CONFIG_PTR pConfFunc);
void DISP_UnRegisterExTriggerSource(int u4ID);
void GetUpdateMutex(void);
void ReleaseUpdateMutex(void);
BOOL DISP_IsVideoMode(void);
unsigned long DISP_GetLCMIndex(void);

#endif