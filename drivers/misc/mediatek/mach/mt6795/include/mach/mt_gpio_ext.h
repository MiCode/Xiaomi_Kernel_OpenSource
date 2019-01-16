#ifndef _MT_GPIO_EXT_H_
#define _MT_GPIO_EXT_H_

#include <mach/mt_pmic_wrap.h>

#define GPIOEXT_WR(addr, data)   pwrap_write((unsigned long)addr, data)
#define GPIOEXT_RD(addr)         ({ \
		unsigned long ext_data; \
		(pwrap_read((unsigned long)addr,&ext_data) != 0)?-1:ext_data;})
#define GPIOEXT_SET_BITS(BIT,REG)   (GPIOEXT_WR(REG, (unsigned long)(BIT)))
#define GPIOEXT_CLR_BITS(BIT,REG)    ({ \
		unsigned long ext_data; \
		int ret; \
		ret = GPIOEXT_RD(REG);\
		ext_data = ret;\
		(ret < 0)?-1:(GPIOEXT_WR(REG,ext_data & ~((unsigned long)(BIT))))})  

/*----------------------------------------------------------------------------*/
typedef struct {
    unsigned short val;        
    unsigned short set;
    unsigned short rst;
    unsigned short _align;
} EXT_VAL_REGS;
/*----------------------------------------------------------------------------*/
typedef struct {
    EXT_VAL_REGS    dir[4];            /*0x0000 ~ 0x001F: 32 bytes*/
    EXT_VAL_REGS    pullen[4];         /*0x0020 ~ 0x003F: 32 bytes*/
    EXT_VAL_REGS    pullsel[4];        /*0x0040 ~ 0x005F: 32 bytes*/
    EXT_VAL_REGS    dinv[4];           /*0x0060 ~ 0x007F: 32 bytes*/    
    EXT_VAL_REGS    dout[4];           /*0x0080 ~ 0x009F: 32 bytes*/
    EXT_VAL_REGS    din[4];            /*0x00A0 ~ 0x00BF: 32 bytes*/
    EXT_VAL_REGS    mode[10];          /*0x00C0 ~ 0x010F: 80 bytes*/  
} GPIOEXT_REGS;

#endif //_MT_GPIO_EXT_H_