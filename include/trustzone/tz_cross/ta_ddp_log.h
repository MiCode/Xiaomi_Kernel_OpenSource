#ifndef __TRUSTZONE_TA_DDP_LOG__
#define __TRUSTZONE_TA_DDP_LOG__

// for self-defined log output marco
#ifndef __MTEE_LOG_H__
#include <tz_private/log.h>
#endif

// to control the DEBUG level output. define it some where else.
extern unsigned int g_tee_dbg_log;

// for temporary debugging purpose
#define MTEE_LOG_CUSTOM_LEVEL MTEE_LOG_LVL_INFO

#define MTEE_LOG_I(args...) \
do { if ((MTEE_LOG_LVL_INFO) >= MTEE_LOG_BUILD_LEVEL && g_tee_dbg_log > 0) { _MTEE_LOG(MTEE_LOG_LVL_INFO, args); } } while (0)

#define MTEE_LOG_D(args...) \
do { if ((MTEE_LOG_LVL_DEBUG) >= MTEE_LOG_BUILD_LEVEL && g_tee_dbg_log > 0) { _MTEE_LOG(MTEE_LOG_LVL_DEBUG, args); } } while (0)

#define MTEE_LOG_P(args...) \
do { if ((MTEE_LOG_LVL_PRINTF) >= MTEE_LOG_BUILD_LEVEL) { _MTEE_LOG(MTEE_LOG_LVL_PRINTF, args); } } while (0)

#define MTEE_LOG_W(args...) \
do { if ((MTEE_LOG_LVL_WARN) >= MTEE_LOG_BUILD_LEVEL) { _MTEE_LOG(MTEE_LOG_LVL_WARN, args); } } while (0)

#define MTEE_LOG_B(args...) \
do { if ((MTEE_LOG_LVL_BUG) >= MTEE_LOG_BUILD_LEVEL) { _MTEE_LOG(MTEE_LOG_LVL_BUG, args); } } while (0)

#define MTEE_LOG_A(args...) \
do { if ((MTEE_LOG_LVL_ASSERT) >= MTEE_LOG_BUILD_LEVEL) { _MTEE_LOG(MTEE_LOG_LVL_ASSERT, args); } } while (0)

#endif /* __TRUSTZONE_TA_DDP_LOG__ */
