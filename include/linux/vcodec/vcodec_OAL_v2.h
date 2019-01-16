#ifndef VCODEC_OAL_V2_H
#define VCODEC_OAL_V2_H
#define IN
#define OUT

#ifndef NULL
#define NULL 0
#endif

#include "vcodec_if_v2.h"


#define VCODEC_OAL_VERSION 20140812
#define VCODEC_ASSERT(expr, a) do {if(!(expr)) rVCODEC_OAL_Function.VCodecAssertFail(__FILE__, __LINE__, a); } while (0)

void VCodecQueryMemType(IN void            *pBuffer_VA,
                        IN unsigned int    u4Size,
                        OUT VCODEC_MEMORY_TYPE_T *peMemType
                       );

void VCodecQueryPhysicalAddr(IN void       *pBuffer_VA,
                             OUT void     **pBufferOut_PA
                            );

// VCodecSwitchMemType - return 0 if success.
//                       return -1 if failed, but pBufferOut_VA will be assigned with pBuffer_VA
int VCodecSwitchMemType(IN void            *pBuffer_VA,
                        IN unsigned int    u4Size,
                        IN VCODEC_MEMORY_TYPE_T eMemType,
                        OUT void           **pBufferOut_VA
                       );

// VCodecFlushCachedBuffer - u4Size is in byte
void VCodecFlushCachedBuffer(IN void         *pBuffer_VA,
                             IN unsigned int u4Size
                            );

// VCodecInvalidateCachedBuffer - u4Size is in byte
void VCodecInvalidateCachedBuffer(IN void         *pBuffer_VA,
                                  IN unsigned int  u4Size
                                 );

void VCodecFlushCachedBufferAll(void);

void VCodecInvalidateCachedBufferAll(void);

void VCodecFlushInvalidateCacheBufferAll(void);

void  VCodecMemSet(IN void                *pBuffer_VA,
                   IN char                cValue,
                   IN unsigned int        u4Length
                  );

void  VCodecMemCopy(IN void             *pvDest ,
                    IN const void       *pvSrc ,
                    IN unsigned int      u4Length
                   );
typedef struct
{
    void *pBuffer_PA;                  ///< [IN]     The physical memory address
    unsigned int u4MemSize;      ///< [IN]     The memory size to be mapped
    void *pBuffer_VA;               ///< [OUT]     The mapped virtual memory address
} VCODEC_OAL_MMAP_T;

void VCodecMMAP(VCODEC_OAL_MMAP_T *prParam);
void VCodecUnMMAP(VCODEC_OAL_MMAP_T *prParam);


typedef enum
{
    VCODEC_OAL_VDEC,
    VCODEC_OAL_VENC
} VCODEC_OAL_CODEC;

typedef struct
{
    unsigned int u4ReadAddr;            /// [IN]    memory source address in VA
    unsigned int u4ReadData;            /// [OUT]   memory data
} VCODEC_OAL_MEM_STAUTS_T;

typedef struct
{
    unsigned int u4HWIsCompleted;                         ///< [IOUT]    HW is Completed or not, set by driver & clear by codec (0: not completed or still in lock status; 1: HW is completed or in unlock status)
    unsigned int u4HWIsTimeout;                            ///< [OUT]    HW is Timeout or not, set by driver & clear by codec (0: not in timeout status; 1: HW is in timeout status)
    unsigned int u4NumOfRegister;       ///< [IN]     Number of HW register need to store;
    VCODEC_OAL_MEM_STAUTS_T *pHWStatus;  ///< [OUT]    HW status based on input address.
} VCODEC_OAL_HW_REGISTER_T;

typedef struct
{
    void *pvHandle;                  ///< [IN]     The video codec handle
    void *pvIsrFunction;            ///< [IN]     The isr function
    unsigned int u4TimeoutMs;       ///< [IN]     The timeout in ms
    VCODEC_OAL_CODEC eCodec; ///< [IN]     VDEC or VENC interrupt
} VCODEC_OAL_ISR_T;

// return value: HW is completed (1) or not (0) when function return
int VCodecWaitISR(VCODEC_OAL_ISR_T *prParam);


typedef struct
{
    void *pvHandle;                               ///< [IN]     The video codec handle
    unsigned int u4TimeoutMs;             ///< [IN]     The timeout ms
} VCODEC_OAL_HW_LOCK_T;

// return value: HW is completed (1) or not (0) when function return
int VCodecLockHW(VCODEC_OAL_HW_LOCK_T *prParam);

// return value: HW is completed (1) or not (0) when function return
int VCodecUnLockHW(VCODEC_OAL_HW_LOCK_T *prParam);


void VCodecInitHWLock(VCODEC_OAL_HW_REGISTER_T *prParam);

void VCodecDeInitHWLock(VCODEC_OAL_HW_REGISTER_T *prParam);


/****** Pthread define******/
#define VCODEC_PTHREAD_PROCESS_PRIVATE  0
#define VCODEC_PTHREAD_PROCESS_SHARED   1
#define VCODEC_PTHREAD_CREATE_JOINABLE  0
#define VCODEC_PTHREAD_CREATE_DETACHED  1
#define VCODEC_PTHREAD_SCOPE_PROCESS    0
#define VCODEC_PTHREAD_SCOPE_SYSTEM     1
#define VCODEC_PTHREAD_ONCE_INIT    0
typedef long VCODEC_PTHREAD_T;
typedef volatile int  VCODEC_PTHREAD_ONCE_T;
typedef long VCODEC_PTHREAD_MUTEXATTR_T;
typedef long VCODEC_PTHREAD_CONDATTR_T;
typedef struct
{
    unsigned int flags;
    void *stack_base;
    unsigned int stack_size;
    unsigned int guard_size;
    unsigned int sched_policy;
    unsigned int sched_priority;
} VCODEC_PTHREAD_ATTR_T;

typedef struct
{
    int volatile value;
} VCODEC_PTHREAD_MUTEX_T;

typedef struct
{
    int volatile value;
} VCODEC_PTHREAD_COND_T;

typedef struct
{
    int interlock;
    VCODEC_PTHREAD_MUTEX_T mutex;
} VCODEC_PTHREAD_SPINLOCK_T;
/****** End of Pthread define******/



typedef enum
{
    VCODEC_OAL_ERROR_NONE,
    VCODEC_OAL_ERROR_ERROR,
    VCODEC_OAL_ERROR_ASSERT_FAIL,
    VCODEC_OAL_ERROR_ATTR_NOT_SUPPORT,
    NUM_OF_VCODEC_OAL_ERROR_TYPE
} VCODEC_OAL_ERROR_T;

/* Semaphore */

typedef struct
{
    volatile unsigned int  count;
} VCODEC_OAL_SEM_T;


int VCodecPthread_attr_init(OUT VCODEC_PTHREAD_ATTR_T *attr);
int VCodecPthread_attr_destroy(IN VCODEC_PTHREAD_ATTR_T *attr);
int VCodecPthread_attr_getdetachstate(IN const VCODEC_PTHREAD_ATTR_T *attr,
                                      OUT int *detachstate);
int VCodecPthread_attr_setdetachstate(IN VCODEC_PTHREAD_ATTR_T *attr,
                                      IN  int detachstate);
int VCodecPthread_create(OUT VCODEC_PTHREAD_T *thread,
                         IN  const VCODEC_PTHREAD_ATTR_T *attr,
                         IN  void * (*start_routine)(void *),
                         IN  void *arg);
int  VCodecPthread_kill(IN VCODEC_PTHREAD_T tid, IN  int sig);
void VCodecPthread_exit(OUT void *retval);
int  VCodecPthread_join(IN  VCODEC_PTHREAD_T thid, OUT void **ret_val);
int  VCodecPthread_once(IN VCODEC_PTHREAD_ONCE_T  *once_control,  IN void (*init_routine)(void));
VCODEC_PTHREAD_T VCodecPthread_self(void);
int VCodecPthread_mutexattr_init(OUT VCODEC_PTHREAD_MUTEXATTR_T *attr);
int VCodecPthread_mutexattr_destroy(IN VCODEC_PTHREAD_MUTEXATTR_T *attr);
int VCodecPthread_mutex_init(OUT VCODEC_PTHREAD_MUTEX_T *mutex, IN  const VCODEC_PTHREAD_MUTEXATTR_T *attr);
int VCodecPthread_mutex_destroy(IN VCODEC_PTHREAD_MUTEX_T *mutex);
int VCodecPthread_mutex_lock(IN VCODEC_PTHREAD_MUTEX_T *mutex);
int VCodecPthread_mutex_unlock(IN VCODEC_PTHREAD_MUTEX_T *mutex);
int VCodecPthread_mutex_trylock(IN VCODEC_PTHREAD_MUTEX_T *mutex);
int VCodecPthread_spin_init(OUT VCODEC_PTHREAD_SPINLOCK_T *lock, IN  int pshared);
int VCodecPthread_spin_destroy(IN VCODEC_PTHREAD_SPINLOCK_T *lock);
int VCodecPthread_spin_lock(IN VCODEC_PTHREAD_SPINLOCK_T *lock);
int VCodecPthread_spin_trylock(IN VCODEC_PTHREAD_SPINLOCK_T *lock);
int VCodecPthread_spin_unlock(IN VCODEC_PTHREAD_SPINLOCK_T *lock);
int VCodecPthread_condattr_init(OUT VCODEC_PTHREAD_CONDATTR_T *attr);
int VCodecPthread_condattr_destroy(IN VCODEC_PTHREAD_CONDATTR_T *attr);
int VCodecPthread_cond_init(OUT VCODEC_PTHREAD_COND_T *cond, IN const VCODEC_PTHREAD_CONDATTR_T *attr);
int VCodecPthread_cond_destroy(IN VCODEC_PTHREAD_COND_T *cond);
int VCodecPthread_cond_broadcast(IN VCODEC_PTHREAD_COND_T *cond);
int VCodecPthread_cond_signal(IN VCODEC_PTHREAD_COND_T *cond);
int VCodecPthread_cond_wait(IN VCODEC_PTHREAD_COND_T *cond, IN VCODEC_PTHREAD_MUTEX_T *mutex);

VCODEC_OAL_ERROR_T VCodecBindingCore(IN  VCODEC_PTHREAD_T tid,
                                     IN  unsigned int u4Mask);
VCODEC_OAL_ERROR_T VCodecDeBindingCore(IN  VCODEC_PTHREAD_T tid);
VCODEC_OAL_ERROR_T VCodecGetAffinity(IN  VCODEC_PTHREAD_T tid,
                                     OUT  unsigned int *pu4Mask,
                                     OUT  unsigned int *pu4SetMask);
VCODEC_OAL_ERROR_T VCodecCoreLoading(IN  int s4CPUid,
                                     OUT int *ps4Loading);
VCODEC_OAL_ERROR_T VCodecCoreNumber(OUT int *ps4CPUNums);
void VCodecSleep(IN unsigned int u4Tick);
int VCodec_sem_init(IN VCODEC_OAL_SEM_T *sem,
                    IN int pshared,
                    IN unsigned int value);
int VCodec_sem_destroy(IN VCODEC_OAL_SEM_T *sem);
int VCodec_sem_post(IN VCODEC_OAL_SEM_T *sem);
int VCodec_sem_wait(IN VCODEC_OAL_SEM_T *sem);

int VCodecCheck_Version(IN int version);

#define VCodecOALPrintf(...) rVCODEC_OAL_Function.VCodecPrintf(__VA_ARGS__);

VCODEC_OAL_ERROR_T VCodecConfigMCIPort(IN unsigned int u4PortConfig, OUT unsigned int *pu4PortResult, IN VCODEC_CODEC_TYPE_T eCodecType);
typedef struct
{

    void (*VCodecQueryMemType)(IN void            *pBuffer_VA,
                               IN unsigned int    u4Size,
                               OUT VCODEC_MEMORY_TYPE_T *peMemType);

    void (*VCodecQueryPhysicalAddr)(IN void       *pBuffer_VA,
                                    OUT void     **pBufferOut_PA);

    // VCodecSwitchMemType - return 0 if success.
    //                       return -1 if failed, but pBufferOut_VA will be assigned with pBuffer_VA
    int (*VCodecSwitchMemType)(IN void            *pBuffer_VA,
                               IN unsigned int    u4Size,
                               IN VCODEC_MEMORY_TYPE_T eMemType,
                               OUT void           **pBufferOut_VA);

    // VCodecFlushCachedBuffer - u4Size is in byte
    void (*VCodecFlushCachedBuffer)(IN void         *pBuffer_VA,
                                    IN unsigned int u4Size);

    // VCodecInvalidateCachedBuffer - u4Size is in byte
    void (*VCodecInvalidateCachedBuffer)(IN void         *pBuffer_VA,
                                         IN unsigned int   u4Size);

    void (*VCodecFlushCachedBufferAll)(void);

    void (*VCodecInvalidateCachedBufferAll)(void);

    void (*VCodecFlushInvalidateCacheBufferAll)(void);

    void (*VCodecMemSet)(IN void                *pBuffer_VA,
                         IN char                cValue,
                         IN unsigned int        u4Length);

    void (*VCodecMemCopy)(IN void             *pvDest ,
                          IN const void      *pvSrc ,
                          IN unsigned int      u4Length);

    void (*VCodecAssertFail)(IN char *ptr,
                             IN int i4Line,
                             IN int i4Arg);

    void (*VCodecMMAP)(VCODEC_OAL_MMAP_T *prParam);
    void (*VCodecUnMMAP)(VCODEC_OAL_MMAP_T *prParam);
    int (*VCodecWaitISR)(VCODEC_OAL_ISR_T *prParam);
    int (*VCodecLockHW)(VCODEC_OAL_HW_LOCK_T *prParam);
    int (*VCodecUnLockHW)(VCODEC_OAL_HW_LOCK_T *prParam);

    void (*VCodecInitHWLock)(IN VCODEC_OAL_HW_REGISTER_T *prParam);

    void (*VCodecDeInitHWLock)(IN VCODEC_OAL_HW_REGISTER_T *prParam);
    int (*VCodecCheck_Version)(IN int version);
    /************  Multi-thread function ***********/

    /***** Thread Management Functions ******/
    int (*VCodecPthread_attr_init)(OUT VCODEC_PTHREAD_ATTR_T *attr);
    int (*VCodecPthread_attr_destroy)(IN VCODEC_PTHREAD_ATTR_T *attr);
    int (*VCodecPthread_attr_getdetachstate)(IN const VCODEC_PTHREAD_ATTR_T *attr,
                                             OUT int *detachstate);
    int (*VCodecPthread_attr_setdetachstate)(IN VCODEC_PTHREAD_ATTR_T *attr,
                                             IN  int detachstate);
    int (*VCodecPthread_create)(OUT VCODEC_PTHREAD_T *thread,
                                IN  const VCODEC_PTHREAD_ATTR_T *attr,
                                IN  void * (*start_routine)(void *),
                                IN  void *arg);
    int (*VCodecPthread_kill)(IN VCODEC_PTHREAD_T tid,
                              IN  int sig);
    void (*VCodecPthread_exit)(OUT void *retval);
    int (*VCodecPthread_join)(IN  VCODEC_PTHREAD_T thid,
                              OUT void **ret_val);
    int (*VCodecPthread_once)(IN VCODEC_PTHREAD_ONCE_T  *once_control,
                              IN void (*init_routine)(void));
    VCODEC_PTHREAD_T(*VCodecPthread_self)(void);

    /***** Mutex Functions ******/
    int (*VCodecPthread_mutexattr_init)(OUT VCODEC_PTHREAD_MUTEXATTR_T *attr);
    int (*VCodecPthread_mutexattr_destroy)(IN VCODEC_PTHREAD_MUTEXATTR_T *attr);
    int (*VCodecPthread_mutex_init)(OUT VCODEC_PTHREAD_MUTEX_T *mutex,
                                    IN  const VCODEC_PTHREAD_MUTEXATTR_T *attr);
    int (*VCodecPthread_mutex_destroy)(IN VCODEC_PTHREAD_MUTEX_T *mutex);
    int (*VCodecPthread_mutex_lock)(IN VCODEC_PTHREAD_MUTEX_T *mutex);
    int (*VCodecPthread_mutex_unlock)(IN VCODEC_PTHREAD_MUTEX_T *mutex);
    int (*VCodecPthread_mutex_trylock)(IN VCODEC_PTHREAD_MUTEX_T *mutex);

    /***** Spin Functions ******/
    int (*VCodecPthread_spin_init)(OUT VCODEC_PTHREAD_SPINLOCK_T *lock,
                                   IN  int pshared);
    int (*VCodecPthread_spin_destroy)(IN VCODEC_PTHREAD_SPINLOCK_T *lock);
    int (*VCodecPthread_spin_lock)(IN VCODEC_PTHREAD_SPINLOCK_T *lock);
    int (*VCodecPthread_spin_trylock)(IN VCODEC_PTHREAD_SPINLOCK_T *lock);
    int (*VCodecPthread_spin_unlock)(IN VCODEC_PTHREAD_SPINLOCK_T *lock);

    /***** Condition Variable Functions ******/
    int (*VCodecPthread_condattr_init)(OUT VCODEC_PTHREAD_CONDATTR_T *attr);
    int (*VCodecPthread_condattr_destroy)(IN VCODEC_PTHREAD_CONDATTR_T *attr);
    int (*VCodecPthread_cond_init)(OUT VCODEC_PTHREAD_COND_T *cond,
                                   IN  const VCODEC_PTHREAD_CONDATTR_T *attr);
    int (*VCodecPthread_cond_destroy)(IN VCODEC_PTHREAD_COND_T *cond);
    int (*VCodecPthread_cond_broadcast)(IN VCODEC_PTHREAD_COND_T *cond);
    int (*VCodecPthread_cond_signal)(IN VCODEC_PTHREAD_COND_T *cond);
    int (*VCodecPthread_cond_wait)(IN VCODEC_PTHREAD_COND_T *cond,
                                   IN  VCODEC_PTHREAD_MUTEX_T *mutex);

    /************  End of Multi-thread function ***********/

    /***** Semaphore Functions ******/

    int (*VCodec_sem_init)(IN VCODEC_OAL_SEM_T *sem,
                           IN int pshared,
                           IN unsigned int value);

    int (*VCodec_sem_destroy)(IN VCODEC_OAL_SEM_T *sem);

    int (*VCodec_sem_post)(IN VCODEC_OAL_SEM_T *sem);

    int (*VCodec_sem_wait)(IN VCODEC_OAL_SEM_T *sem);

    /***** Binding Functions ******/

    VCODEC_OAL_ERROR_T(*VCodecBindingCore)(IN  VCODEC_PTHREAD_T tid,
                                           IN  unsigned int u4SetMask);
    VCODEC_OAL_ERROR_T(*VCodecDeBindingCore)(IN  VCODEC_PTHREAD_T tid);

    VCODEC_OAL_ERROR_T(*VCodecGetAffinity)(IN  VCODEC_PTHREAD_T tid,
                                           OUT  unsigned int *pu4CPUMask,
                                           OUT  unsigned int *pu4SetMask);

    VCODEC_OAL_ERROR_T(*VCodecCoreLoading)(IN  int s4CPUid,
                                           OUT int *ps4Loading);
    VCODEC_OAL_ERROR_T(*VCodecCoreNumber)(OUT int *ps4CPUNums);
    /***** Others Functions ******/
    void (*VCodecSleep)(IN unsigned int u4Tick);

    VCODEC_OAL_ERROR_T(*VCodecConfigMCIPort)(IN unsigned int u4PortConfig, OUT unsigned int *pu4PortResult, IN VCODEC_CODEC_TYPE_T eCodecType);

    VCODEC_OAL_ERROR_T(*VCodecPrintf)(IN const char *_Format, ...);

} VCODEC_OAL_CALLBACK_T;

extern VCODEC_OAL_CALLBACK_T rVCODEC_OAL_Function;
int VCodecOALInit(IN VCODEC_OAL_CALLBACK_T *prVCODEC_OAL_Function);

#endif /* VCODEC_OAL_H */