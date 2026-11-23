#ifndef POWERINSIGHT_IOCTL_H
#define POWERINSIGHT_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>


#define POWERINSIGHT_DIR_IOC(dir, type, nr, param) \
    _IOC(dir, POWERINSIGHT_IOC_##type, nr, sizeof(param))

#define POWERINSIGHT_GPU_DIR_IOC(dir, nr, param) \
    POWERINSIGHT_DIR_IOC(dir, GPU, nr, param)
#define POWERINSIGHT_UTILS_DIR_IOC(dir, nr, param) \
	POWERINSIGHT_DIR_IOC(dir, UTILS, nr, param)
#define POWERINSIGHT_MODEM_DIR_IOC(dir, nr, param) \
        POWERINSIGHT_DIR_IOC(dir, MODEM, nr, param)

enum powerinsight_ioctl_type_t {
	POWERINSIGHT_IOC_GPU = 0,
	POWERINSIGHT_IOC_UTILS = 2,
	POWERINSIGHT_IOC_MODEM = 3,
};

struct ioctl_memfd_request {
    void __user *user_ptr; // mmap 映射地址
};

#endif // POWERINSIGHT_IOCTL_H
