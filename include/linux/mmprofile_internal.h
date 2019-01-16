#ifndef __MMPROFILE_INTERNAL_H__
#define __MMPROFILE_INTERNAL_H__

#include <linux/mmprofile.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MMProfileMaxEventCount 4096

#define MMP_EVENT_STATE_ENABLED (1 << 0)
#define MMP_EVENT_STATE_FTRACE  (1 << 1)

typedef struct {
    unsigned int parentId;
    char name[MMProfileEventNameMaxLen + 1];
} MMProfile_EventInfo_t;

typedef struct {
    unsigned int lock;
    unsigned int id;
    unsigned int timeLow;
    unsigned int timeHigh;
    unsigned int flag;
#if 0 //#ifdef MMPROFILE_KERNEL_64
    unsigned long long data1;
    unsigned long long data2;
    unsigned long long meta_data_cookie;
#else
    unsigned int data1;
    unsigned int data2;
    unsigned int meta_data_cookie;
#endif
} MMProfile_Event_t;

typedef struct {
    unsigned int enable;
    unsigned int start;
    unsigned int write_pointer;
    unsigned int reg_event_index;
    unsigned int buffer_size_record;
    unsigned int buffer_size_bytes;
    unsigned int record_size;
    unsigned int meta_buffer_size;
    unsigned int new_buffer_size_record;
    unsigned int new_meta_buffer_size;
    unsigned int selected_buffer;
    unsigned int max_event_count;
    unsigned int event_state[MMProfileMaxEventCount];
} MMProfile_Global_t;

typedef struct {
    unsigned int cookie;
    MMP_MetaDataType data_type;
    unsigned int data_size;
    unsigned int data_offset;
} MMProfile_MetaData_t;

typedef struct {
    unsigned int id;
    MMP_LogType type;
    MMP_MetaData_t meta_data;
} MMProfile_MetaLog_t;


#define MMProfileGlobalsSize     ((sizeof(MMProfile_Global_t)+(PAGE_SIZE-1))&(~(PAGE_SIZE-1)))

#define CONFIG_MMPROFILE_PATH   "/data/MMProfileConfig.dat"

#define MMProfilePrimaryBuffer  1
#define MMProfileGlobalsBuffer  2
#define MMProfileMetaDataBuffer 3

#define MMP_IOC_MAGIC 'M'

#define MMP_IOC_ENABLE          _IOW(MMP_IOC_MAGIC, 1, int)
#define MMP_IOC_START           _IOW(MMP_IOC_MAGIC, 2, int)
#define MMP_IOC_TIME            _IOW(MMP_IOC_MAGIC, 3, int)
#define MMP_IOC_REGEVENT        _IOWR(MMP_IOC_MAGIC, 4, int)
#define MMP_IOC_FINDEVENT       _IOWR(MMP_IOC_MAGIC, 5, int)
#define MMP_IOC_ENABLEEVENT     _IOW(MMP_IOC_MAGIC, 6, int)
#define MMP_IOC_LOG             _IOW(MMP_IOC_MAGIC, 7, int)
#define MMP_IOC_DUMPEVENTINFO   _IOR(MMP_IOC_MAGIC, 8, int)
#define MMP_IOC_METADATALOG     _IOW(MMP_IOC_MAGIC, 9, int)
#define MMP_IOC_DUMPMETADATA    _IOR(MMP_IOC_MAGIC, 10, int)
#define MMP_IOC_SELECTBUFFER    _IOW(MMP_IOC_MAGIC, 11, int)
#define MMP_IOC_TRYLOG          _IOWR(MMP_IOC_MAGIC, 12, int)
#define MMP_IOC_ISENABLE        _IOR(MMP_IOC_MAGIC, 13, int)
#define MMP_IOC_REMOTESTART     _IOR(MMP_IOC_MAGIC, 14, int)
#define MMP_IOC_TEST            _IOWR(MMP_IOC_MAGIC, 100, int)

// fix build warning: unused
//static void MMProfileInitBuffer(void);
//static void MMProfileResetBuffer(void);

#ifdef __cplusplus
}
#endif
#endif
