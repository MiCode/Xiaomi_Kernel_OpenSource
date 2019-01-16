#ifndef __GED_BRIDGE_H__
#define __GED_BRIDGE_H__

#include "ged_base.h"
#include "ged_log.h"

typedef struct _GED_BRIDGE_PACKAGE
{
    unsigned int    ui32FunctionID;
    int             i32Size;
    void            *pvParamIn;
    int             i32InBufferSize;
    void            *pvParamOut;
    int             i32OutBufferSize;
} GED_BRIDGE_PACKAGE;

/*****************************************************************************
 *  IOCTL values.
 *****************************************************************************/

#define GED_MAGIC 'g'

#define GED_IO(INDEX)    _IO(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_IOW(INDEX)   _IOW(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_IOR(INDEX)   _IOR(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_IOWR(INDEX)  _IOWR(GED_MAGIC, INDEX, GED_BRIDGE_PACKAGE)
#define GED_GET_BRIDGE_ID(X)	_IOC_NR(X)

/*****************************************************************************
 *  IOCTL Commands
 *****************************************************************************/
typedef enum
{
    GED_BRIDGE_COMMAND_LOG_BUF_GET,
    GED_BRIDGE_COMMAND_LOG_BUF_WRITE,
    GED_BRIDGE_COMMAND_LOG_BUF_RESET,
    GED_BRIDGE_COMMAND_BOOST_GPU_FREQ,
    GED_BRIDGE_COMMAND_MONITOR_3D_FENCE
} GED_BRIDGE_COMMAND_ID;

#define GED_BRIDGE_IO_LOG_BUF_GET			GED_IOWR(GED_BRIDGE_COMMAND_LOG_BUF_GET)
#define GED_BRIDGE_IO_LOG_BUF_WRITE			GED_IOWR(GED_BRIDGE_COMMAND_LOG_BUF_WRITE)
#define GED_BRIDGE_IO_LOG_BUF_RESET			GED_IOWR(GED_BRIDGE_COMMAND_LOG_BUF_RESET)
#define GED_BRIDGE_IO_BOOST_GPU_FREQ		GED_IOWR(GED_BRIDGE_COMMAND_BOOST_GPU_FREQ)
#define GED_BRIDGE_IO_MONITOR_3D_FENCE      GED_IOWR(GED_BRIDGE_COMMAND_MONITOR_3D_FENCE)

/*****************************************************************************
 *  LOG_BUF_GET
 *****************************************************************************/
    
/* Bridge in structure for LOG_BUF_GET */
typedef struct GED_BRIDGE_IN_LOGBUFGET_TAG
{
    char acName[GED_LOG_BUF_NAME_LENGTH];
} GED_BRIDGE_IN_LOGBUFGET;


/* Bridge out structure for LOG_BUF_GETC */
typedef struct GED_BRIDGE_OUT_LOGBUFGET_TAG
{
    GED_ERROR eError;
    GED_LOG_BUF_HANDLE hLogBuf;
} GED_BRIDGE_OUT_LOGBUFGET;

/*****************************************************************************
 *  LOG_BUF_WRITE
 *****************************************************************************/

#define GED_BRIDGE_IN_LOGBUF_SIZE 320

/* Bridge in structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_IN_LOGBUFWRITE_TAG
{
    GED_LOG_BUF_HANDLE hLogBuf;
    int attrs;
    char acLogBuf[GED_BRIDGE_IN_LOGBUF_SIZE];
} GED_BRIDGE_IN_LOGBUFWRITE;

/* Bridge out structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_OUT_LOGBUFWRITE_TAG
{
    GED_ERROR eError;
} GED_BRIDGE_OUT_LOGBUFWRITE;

/******************************************************************************
 *  LOG_BUF_RESET
 ******************************************************************************/

/* Bridge in structure for LOG_BUF_RESET */
typedef struct GED_BRIDGE_IN_LOGBUFRESET_TAG
{
    GED_LOG_BUF_HANDLE hLogBuf;
} GED_BRIDGE_IN_LOGBUFRESET;

/* Bridge out structure for LOG_BUF_RESET */
typedef struct GED_BRIDGE_OUT_LOGBUFRESET_TAG
{
    GED_ERROR eError;
} GED_BRIDGE_OUT_LOGBUFRESET;

/*****************************************************************************
 *  BOOST GPU FREQ
 *****************************************************************************/

typedef enum
{
    GED_BOOST_GPU_FREQ_LEVEL_MAX = 100
} GED_BOOST_GPU_FREQ_LEVEL;

/* Bridge in structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_IN_BOOSTGPUFREQ_TAG
{
    GED_BOOST_GPU_FREQ_LEVEL eGPUFreqLevel;
} GED_BRIDGE_IN_BOOSTGPUFREQ;

/* Bridge out structure for LOG_BUF_WRITE */
typedef struct GED_BRIDGE_OUT_BOOSTGPUFREQ_TAG
{
    GED_ERROR eError;
} GED_BRIDGE_OUT_BOOSTGPUFREQ;

/*****************************************************************************
 *  MONITOR 3D FENCE
 *****************************************************************************/

/* Bridge in structure for MONITOR3DFENCE */
typedef struct GED_BRIDGE_IN_MONITOR3DFENCE_TAG
{
    int fd;
} GED_BRIDGE_IN_MONITOR3DFENCE;

/* Bridge out structure for RECORDSWAPBUFFERS */
typedef struct GED_BRIDGE_OUT_MONITOR3DFENCE_TAG
{
    GED_ERROR eError;
} GED_BRIDGE_OUT_MONITOR3DFENCE;

/*****************************************************************************
 *  BRIDGE FUNCTIONS
 *****************************************************************************/

int ged_bridge_log_buf_get(
    GED_BRIDGE_IN_LOGBUFGET *psLogBufGetIN,
    GED_BRIDGE_OUT_LOGBUFGET *psLogBufGetOUT);

int ged_bridge_log_buf_write(
    GED_BRIDGE_IN_LOGBUFWRITE *psLogBufWriteIN,
    GED_BRIDGE_OUT_LOGBUFWRITE *psLogBufWriteOUT);

int ged_bridge_log_buf_reset(
    GED_BRIDGE_IN_LOGBUFRESET *psLogBufResetIn,
    GED_BRIDGE_OUT_LOGBUFRESET *psLogBufResetOUT);

int ged_bridge_boost_gpu_freq(
    GED_BRIDGE_IN_BOOSTGPUFREQ *psBoostGpuFreqIN,
    GED_BRIDGE_OUT_BOOSTGPUFREQ *psBoostGpuFreqOUT);

int ged_bridge_monitor_3D_fence(
    GED_BRIDGE_IN_MONITOR3DFENCE *psMonitor3DFenceINT,
    GED_BRIDGE_OUT_MONITOR3DFENCE *psMonitor3DFenceOUT);

#endif
