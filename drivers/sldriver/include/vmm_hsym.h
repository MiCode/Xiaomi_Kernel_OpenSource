#ifndef VMM_HSYM_H__
#define VMM_HSYM_H__

//#include "hip.h"
#include "vmm_hsym_common.h"


#define ALIGN_DATA_SECT(x) __attribute__ ((aligned (4096), section (x)))
#define ALIGN_CODE_SECT(x) __attribute__ ((aligned (4096), section (x), noinline))

extern int verbose;
//extern bool binary;

enum {
    CONFIG_MAX_CPUS = 64,
    EXPECTED_API_VERSION  = 3,
    EXPECTED_API_REVISION = 0,
};

typedef enum perf_type_t {
    SL_CMD_HSEC_GET_INFO        =    0x56583264, // hypersec interface
    SL_CMD_HSEC_CONFIG          =    0x56583265, // hypersec interface
    SL_CMD_HSEC_START           =    0x56583266, // hypersec interface
    SL_CMD_HSEC_STOP            =    0x56583267, // hypersec interface
    SL_CMD_HSEC_REG_SECT        =    0x56583268, // hypersec interface
    SL_CMD_HSEC_CREATE_VIEW     =    0x56583269, // hypersec interface
    SL_CMD_HSEC_REMOVE_VIEW     =    0x5658326A, // hypersec interface
    SL_CMD_HSEC_ADD_PAGE        =    0x5658326B, // hypersec interface
    SL_CMD_HSEC_INIT_VIEW       =    0x5658326C, // hypersec interface
    SL_CMD_HSEC_CHANGE_MEM_PERM =    0x5658326D, // hypersec interface
    SL_CMD_HSEC_CHK_ACCESS_RIGHT=    0x5658326E, // hypersec interface
    SL_CMD_HSEC_REG_VIDT        =    0x5658326F, // hypersec interface
    SL_CMD_HSEC_REG_SL_INFO     =    0x56583270, // hypersec interface
    SL_CMD_HSEC_GET_CURR_VIEW   =    0x56583271, // hypersec interface
    SL_CMD_HSEC_UPDATE_PERM     =    0x56583272, // hypersec interface
    SL_CMD_HSEC_UUID            =    0x56583273, // hypersec interface
    SL_CMD_HSEC_MAP_SHM         =    0x56583274, // hypersec interface
    SL_CMD_HSEC_VIDT_VERIFY_STATUS = 0x56583275,
    SL_CMD_HSEC_MAP_CHAABI_TO_PSTA = 0x56583276,
    SL_CMD_HSEC_VERIFY_VIDT     =    0x56583277,
    SL_CMD_HSEC_UNMAP_SHM       =    0x56583278,
    SL_CMD_HSEC_GET_UUID_INSTANCE_COUNT = 0x56583279,
    SL_CMD_HSEC_GET_TA_PROPERTIES =  0x56583280,
    SL_CMD_HSEC_PSTA_GET_BOOT_INFO=  0x56583281,
    SL_CMD_HSEC_MAX             =    0x56583282, // hypersec interface
}perf_type_t;

typedef enum {
    CMD_INFO   = 0,
    CMD_CONFIG = 1,
    CMD_CONFIG_ID = 10,
    CMD_START  = 2,
    CMD_STOP   = 3,
    CMD_CLEAR  = 4,
    CMD_GET    = 5,
    CMD_REPLAY = 6,
    CMD_NEXT   = 7,
    CMD_ANNOTATE = 8,
} perf_cmd_t;

//extern struct hip_t hip;
extern bool hip_valid;

typedef struct{
    uint64_t size;
    uint64_t val[6];
}intlist_t;

//void time_get();
//void cpuid2(const intlist_t *);

//unsigned instance;


// void set_instance(unsigned new_instance) { instance = new_instance; }

/* void do_get_data( perf_cmd_t cmd, void *buf, uint32 n,
                     uint32 cpu, uint32 offset);*/

//static void do_get_data(const char *msg, perf_type_t type, uint32_t cmd,
//                        void *buf, uint32_t n, uint32_t cpu, uint32_t offset);
//static void do_put_data(const char *msg, perf_type_t type, uint32_t cmd,
//                        const void *buf, uint32_t n);
/*
 void do_put_data( perf_cmd_t cmd, const void *buf, uint32 n);
*/
// void cpuid_ta(uint32_t cmd, uint32_t c, uint32_t d, uint32_t S, uint32_t D);

// void start();
// void stop();
// void clear();


 //const char *object_name() { return "hypersec"; }
 perf_type_t cpuid_code(void);
 void info(void);
 void get(void);
 void config(const intlist_t *);
 void config2(const char *id, const intlist_t *);
 perf_type_t cpuid_code_ex(uint32_t);
 void reg_sections(hsec_register_t*);
 bool get_hip(void);
 void reg_vIDT(void *data);
 void reg_sl_global_info(hsec_sl_param_t *sl_info);

#endif

