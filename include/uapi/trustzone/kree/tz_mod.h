#ifndef TZ_MOD_H
#define TZ_MOD_H


 /*****************************************************************************
 * MODULE DEFINITION
 *****************************************************************************/
#define MODULE_NAME	    "[MTEE_MOD]"
#define TZ_DEV_NAME        "trustzone"
#define MAJOR_DEV_NUM   197

 /*****************************************************************************
 * IOCTL DEFINITION
 *****************************************************************************/
#define MTEE_IOC_MAGIC       'T'
#define MTEE_CMD_OPEN_SESSION   _IOWR(MTEE_IOC_MAGIC,  1, struct kree_session_cmd_param)
#define MTEE_CMD_CLOSE_SESSION  _IOWR(MTEE_IOC_MAGIC,  2, struct kree_session_cmd_param)
#define MTEE_CMD_TEE_SERVICE    _IOWR(MTEE_IOC_MAGIC,  3, struct kree_tee_service_cmd_param)
#define MTEE_CMD_SHM_REG        _IOWR(MTEE_IOC_MAGIC,  4, struct kree_tee_service_cmd_param)
#define MTEE_CMD_SHM_UNREG      _IOWR(MTEE_IOC_MAGIC,  5, struct kree_tee_service_cmd_param)


#define DEV_IOC_MAXNR       (10)

// param for open/close session
struct kree_session_cmd_param
{
    int ret;
    int handle;
    void *data;
};

// param for tee service call
struct kree_tee_service_cmd_param
{
    int ret;
    int handle;
    unsigned int command;
    unsigned int paramTypes;
    void *param;
};

// param for shared memory
struct kree_sharedmemory_cmd_param
{
    int ret;
    uint32_t session;
    uint32_t mem_handle;
    uint32_t command;
    void *buffer;
    uint32_t size;
    uint32_t control; // 0 = write, 1 = read only
};



#endif /* end of DEVFINO_H */

