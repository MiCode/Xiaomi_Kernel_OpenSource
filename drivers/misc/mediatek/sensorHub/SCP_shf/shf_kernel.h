#ifndef SHF_KERNEL_H
#define SHF_KERNEL_H

#include <linux/types.h>

//IPI received data cache
#define SHF_IPI_PROTOCOL_BYTES (48)
typedef struct ipi_data {
    uint8_t data[SHF_IPI_PROTOCOL_BYTES];
    size_t size;
} ipi_data_t;

typedef struct ipi_buffer {
    size_t head;
    size_t tail;
    size_t size;//data count
    ipi_data_t* data;
} ipi_buffer_t;

#define SHF_IOW(num, dtype)     _IOW('S', num, dtype)
#define SHF_IOR(num, dtype)     _IOR('S', num, dtype)
#define SHF_IOWR(num, dtype)    _IOWR('S', num, dtype)
#define SHF_IO(num)             _IO('S', num)

#define SHF_IPI_SEND            SHF_IOW(1, ipi_data_t)
#define SHF_IPI_POLL            SHF_IOR(2, ipi_data_t)
#define SHF_GESTURE_ENABLE      SHF_IOW(3, int)

#ifdef MTK_SENSOR_HUB_SUPPORT
extern void tpd_scp_wakeup_enable(bool enable);
#endif

#endif//SHF_KERNEL_H