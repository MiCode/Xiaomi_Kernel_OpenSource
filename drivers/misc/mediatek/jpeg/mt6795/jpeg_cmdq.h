#ifndef __JPEG_CMDQ_H__
#define __JPEG_CMDQ_H__

#include <linux/types.h>





#ifdef __cplusplus
extern "C" {
#endif

int32_t cmdqJpegClockOn(uint64_t engineFlag);

int32_t cmdqJpegDumpInfo(uint64_t engineFlag,
                        int level);

int32_t cmdqJpegResetEng(uint64_t engineFlag);

int32_t cmdqJpegClockOff(uint64_t engineFlag);

#ifdef __cplusplus
}
#endif

#endif  // __CMDQ_MDP_H__