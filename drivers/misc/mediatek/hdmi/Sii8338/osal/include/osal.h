#ifndef _OSAL_H
#define _OSAL_H
#if !defined(__KERNEL__)
#include <os_compiler.h>
#include <os_linux.h>
#include "os_data.h"
#include <os_socket.h>
#include <os_file.h>
#include <os_string.h>
#if !defined(DO_NOT_USE_DMLS)
#include <os_dmls.h>
#endif
#else
typedef bool bool_t;
#include "osal/include/os_types.h"
#include "osal/include/os_data.h"
#endif
#if (0)
#ifdef PLATFORM3
#if SHOW_PRINTF
#define TRC(a)                   printf(a", (%s, %d)\n",                                                       __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_I(a, b)               printf(a", (%d), (%s, %d)\n",                                              b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_P(a, b)               printf(a", (%p), (%s, %d)\n",                                       (void*)b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_S(a, b)               printf(a", (%s), (%s, %d)\n",                                              b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_IX(a, b)              printf(a", (0x%08x), (%s, %d)\n",                                          b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_II(a, b, c)            printf(a", (%d), (%d), (%s, %d)\n",                                     b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_PI(a, b, c)            printf(a", (%p), (%d), (%s, %d)\n",                              (void*)b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SI(a, b, c)            printf(a", (%s), (%d), (%s, %d)\n",                                     b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SS(a, b, c)            printf(a", (%s), (%s), (%s, %d)\n",                                     b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_IXI(a, b, c)           printf(a", (0x%08x), (%d), (%s, %d)\n",                                 b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SIX(a, b, c)           printf(a", (%s), (0x%08x), (%s, %d)\n",                                 b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SPI(a, b, c, d)         printf(a", (%s), (%p), (%d), (%s, %d)\n",                     b, (void*)c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_IXII(a, b, c, d)        printf(a", (0x%08x), (%d), (%d), (%s, %d)\n",                        b, c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SIXI(a, b, c, d)        printf(a", (%s), (0x%08x), (%d), (%s, %d)\n",                        b, c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SII(a, b, c, d)         printf(a", (%s), (%d), (%d), (%s, %d)\n",                            b, c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_IXIII(a, b, c, d, e)     printf(a", (0x%08x), (%d), (%d), (%d), (%s, %d)\n",               b, c, d, e, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SIXII(a, b, c, d, e)     printf(a", (%s), (0x%08x), (%d), (%d), (%s, %d)\n",               b, c, d, e, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_XXXX(a, b, c, d, e)      printf(a", (0x%02x), (0x%02x), (0x%02x), (0x%02x), (%s, %d)\n",   b, c, d, e, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRC_SIXIII(a, b, c, d, e, f)  printf(a", (%s), (0x%08x), (%d), (%d), (%d), (%s, %d)\n",      b, c, d, e, f, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout);
#define TRCS(a)                  printf(a", (%s, %d)\n",                                                       __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_I(a, b)              printf(a", (%d), (%s, %d)\n",                                              b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_P(a, b)              printf(a", (%p), (%s, %d)\n",                                       (void*)b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_S(a, b)              printf(a", (%s), (%s, %d)\n",                                              b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_IX(a, b)             printf(a", (0x%08x), (%s, %d)\n",                                          b, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_II(a, b, c)           printf(a", (%d), (%d), (%s, %d)\n",                                     b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_PI(a, b, c)           printf(a", (%p), (%d), (%s, %d)\n",                              (void*)b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SI(a, b, c)           printf(a", (%s), (%d), (%s, %d)\n",                                     b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SS(a, b, c)           printf(a", (%s), (%s), (%s, %d)\n",                                     b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_IXI(a, b, c)          printf(a", (0x%08x), (%d), (%s, %d)\n",                                 b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SIX(a, b, c)          printf(a", (%s), (0x%08x), (%s, %d)\n",                                 b, c, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SPI(a, b, c, d)        printf(a", (%s), (%p), (%d), (%s, %d)\n",                     b, (void*)c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_IXII(a, b, c, d)       printf(a", (0x%08x), (%d), (%d), (%s, %d)\n",                        b, c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SIXI(a, b, c, d)       printf(a", (%s), (0x%08x), (%d), (%s, %d)\n",                        b, c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SII(a, b, c, d)        printf(a", (%s), (%d), (%d), (%s, %d)\n",                            b, c, d, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_IXIII(a, b, c, d, e)    printf(a", (0x%08x), (%d), (%d), (%d), (%s, %d)\n",               b, c, d, e, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SIXII(a, b, c, d, e)    printf(a", (%s), (0x%08x), (%d), (%d), (%s, %d)\n",               b, c, d, e, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_XXXX(a, b, c, d, e)     printf(a", (0x%02x), (0x%02x), (0x%02x), (0x%02x), (%s, %d)\n",   b, c, d, e, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#define TRCS_SIXIII(a, b, c, d, e, f) printf(a", (%s), (0x%08x), (%d), (%d), (%d), (%s, %d)\n",      b, c, d, e, f, __BASE_FILE__, (uint32_t) __LINE__); fflush(stdout); getchar();
#else
#define TRC(a)
#define TRC_I(a, b)
#define TRC_P(a, b)
#define TRC_S(a, b)
#define TRC_IX(a, b)
#define TRC_II(a, b, c)
#define TRC_PI(a, b, c)
#define TRC_SI(a, b, c)
#define TRC_SS(a, b, c)
#define TRC_IXI(a, b, c)
#define TRC_SIX(a, b, c)
#define TRC_SPI(a, b, c, d)
#define TRC_IXII(a, b, c, d)
#define TRC_SIXI(a, b, c, d)
#define TRC_SII(a, b, c, d)
#define TRC_IXIII(a, b, c, d, e)
#define TRC_SIXII(a, b, c, d, e)
#define TRC_XXXX(a, b, c, d, e)
#define TRC_SIXIII(a, b, c, d, e, f)
#define TRCS(a)
#define TRCS_I(a, b)
#define TRCS_P(a, b)
#define TRCS_S(a, b)
#define TRCS_IX(a, b)
#define TRCS_II(a, b, c)
#define TRCS_PI(a, b, c)
#define TRCS_SI(a, b, c)
#define TRCS_SS(a, b, c)
#define TRCS_IXI(a, b, c)
#define TRCS_SIX(a, b, c)
#define TRCS_SPI(a, b, c, d)
#define TRCS_IXII(a, b, c, d)
#define TRCS_SIXI(a, b, c, d)
#define TRCS_SII(a, b, c, d)
#define TRCS_IXIII(a, b, c, d, e)
#define TRCS_SIXII(a, b, c, d, e)
#define TRCS_XXXX(a, b, c, d, e)
#define TRCS_SIXIII(a, b, c, d, e, f)
#endif
#endif
#endif
SiiOsStatus_t SiiOsInit(uint32_t maxChannels);
SiiOsStatus_t SiiOsTerm(void);
SiiOsStatus_t SiiOsSemaphoreCreate
    (const char *pName, uint32_t maxCount, uint32_t initialValue, SiiOsSemaphore_t *pRetSemId);
SiiOsStatus_t SiiOsSemaphoreDelete(SiiOsSemaphore_t semId);
SiiOsStatus_t SiiOsSemaphoreGive(SiiOsSemaphore_t semId);
SiiOsStatus_t SiiOsSemaphoreTake(SiiOsSemaphore_t semId, int32_t timeMsec);
#ifndef PLATFORM3
#if defined(OS_CONFIG_DEBUG)
void SiiOsSemaphoreDumpList(uint32_t channel);
#define SII_OS_SEMAPHORE_DUMP_LIST(channel) SiiOsSemaphoreDumpList(channel)
#else
#define SII_OS_SEMAPHORE_DUMP_LIST(channel)
#endif
#endif
SiiOsStatus_t SiiOsQueueCreate
    (const char *pName, size_t elementSize, uint32_t maxElements, SiiOsQueue_t *pRetQueueId);
SiiOsStatus_t SiiOsQueueDelete(SiiOsQueue_t queueId);
SiiOsStatus_t SiiOsQueueSend(SiiOsQueue_t queueId, const void *pBuffer, size_t size);
SiiOsStatus_t SiiOsQueueReceive
    (SiiOsQueue_t queueId, void *pBuffer, int32_t timeMsec, size_t *pSize);
#ifndef PLATFORM3
#if defined(OS_CONFIG_DEBUG)
void SiiOsQueueDumpList(uint32_t channel);
#define SII_OS_QUEUE_DUMP_LIST(channel) SiiOsQueueDumpList(channel)
#else
#define SII_OS_QUEUE_DUMP_LIST(channel)
#endif
#endif
SiiOsStatus_t SiiOsTaskCreate
    (const char *pName,
     void (*pTaskFunction) (void *pArg),
     void *pTaskArg, uint32_t priority, size_t stackSize, SiiOsTask_t *pRetTaskId);
void SiiOsTaskSelfDelete(void);
SiiOsStatus_t SiiOsTaskSleepUsec(uint64_t timeUsec);
#ifndef PLATFORM3
#if defined(OS_CONFIG_DEBUG)
void SiiOsTaskDumpList(uint32_t channel);
#define SII_OS_TASK_DUMP_LIST(channel) SiiOsTaskDumpList(channel)
#else
#define SII_OS_TASK_DUMP_LIST(channel)
#endif
#endif
SiiOsStatus_t SiiOsTimerCreate
    (const char *pName,
     void (*pTimerFunction) (void *pArg),
     void *pTimerArg,
     bool_t timerStartFlag, uint32_t timeMsec, bool_t periodicFlag, SiiOsTimer_t *pRetTimerId);
SiiOsStatus_t SiiOsTimerDelete(SiiOsTimer_t timerId);
SiiOsStatus_t SiiOsTimerSchedule(SiiOsTimer_t timerId, uint32_t timeMsec);
uint32_t SiiOsGetTimeResolution(void);
void SiiOsGetTimeCurrent(SiiOsTime_t *pRetTime);
int64_t SiiOsGetTimeDifferenceMs(const SiiOsTime_t *pTime1, const SiiOsTime_t *pTime2);
#ifndef PLATFORM3
#if defined(OS_CONFIG_DEBUG)
void SiiOsTimerDumpList(uint32_t channel);
#define SII_OS_TIMER_DUMP_LIST(channel) SiiOsTimerDumpList(channel)
#else
#define SII_OS_TIMER_DUMP_LIST(channel)
#endif
#endif
void *SiiOsAlloc(const char *pName, size_t size, uint32_t flags);
void *SiiOsCalloc(const char *pName, size_t size, uint32_t flags);
void SiiOsFree(void *pAddr);
#endif
