#ifndef __IPI_H
#define __IPI_H

#define MD32_TO_HOST_ADDR       (md32reg.cfg + 0x001C)
#define MD32_TO_HOST_REG        (*(volatile unsigned int *)(MD32_TO_HOST_ADDR))
#define MD32_MD32_TO_SPM_REG    (*(volatile unsigned int *)(md32reg.cfg + 0x0020))
#define HOST_TO_MD32_REG        (*(volatile unsigned int *)(md32reg.cfg + 0x0024))
#define MD32_SPM_TO_MD32_REG    (*(volatile unsigned int *)(md32reg.cfg + 0x0028))
#define MD32_DEBUG_PC_REG       (*(volatile unsigned int *)(md32reg.cfg + 0x0060))
#define MD32_DEBUG_R14_REG      (*(volatile unsigned int *)(md32reg.cfg + 0x0064))
#define MD32_DEBUG_R15_REG      (*(volatile unsigned int *)(md32reg.cfg + 0x0068))
#define MD32_WDT_REG            (*(volatile unsigned int *)(md32reg.cfg + 0x0084))
#define MD32_TO_SPM_REG         (*(volatile unsigned int *)(md32reg.cfg + 0x0020))

#define DMEM_START_ADDR 0xF0028000

#define IPC_MD2HOST     (1 << 0)
//#define MD32_IPC_INT    (1 << 8)
//#define WDT_INT         (1 << 9)
//#define PMEM_DISP_INT   (1 << 10)
//#define DMEM_DISP_INT   (1 << 11)

struct reg_md32_to_host_ipc {
    unsigned int ipc_md2host   :1;
    unsigned int               :7;
    unsigned int md32_ipc_int  :1;
    unsigned int wdt_int       :1;
    unsigned int pmem_disp_int :1;
    unsigned int dmem_disp_int :1;
    unsigned int               :20;
};


typedef enum ipi_id
{
    IPI_WDT = 0,
    IPI_TEST1,
    IPI_TEST2,
    IPI_LOGGER,
    IPI_SWAP,
    IPI_ANC,
    IPI_SPK_PROTECT,
    IPI_THERMAL,
    IPI_SPM,
    IPI_DVT_TEST,
    IPI_BUF_FULL,
    IPI_VCORE_DVFS,
    MD32_NR_IPI,
}ipi_id;

typedef enum ipi_status
{
    ERROR =-1,
    DONE,
    BUSY,
}ipi_status;

typedef void(*ipi_handler_t)(int id, void * data, unsigned int  len);

struct ipi_desc{
    ipi_handler_t handler;
    const char  *name;
};

#define SHARE_BUF_SIZE 64
struct share_obj {
    enum ipi_id id;
    unsigned int len;
    unsigned char reserve[8];
    unsigned char share_buf[SHARE_BUF_SIZE - 16];
};

extern void md32_irq_init(void);
extern void md32_ipi_init(void);
extern void md32_ipi_handler(void);
ipi_status md32_ipi_registration(enum ipi_id id, ipi_handler_t handler, const char *name);
ipi_status md32_ipi_send(enum ipi_id id, void* buf, unsigned int  len, unsigned int wait);
ipi_status md32_ipi_status(enum ipi_id id);

#endif
