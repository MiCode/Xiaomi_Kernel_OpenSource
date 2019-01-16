#ifndef _MT_GPIO_BASE_H_
#define _MT_GPIO_BASE_H_

#include <mach/sync_write.h>
#include <mach/gpio_const.h>

#define GPIO_WR32(addr, data)   mt_reg_sync_writel(data, addr)
#define GPIO_RD32(addr)         __raw_readl(addr)
//#define GPIO_SET_BITS(BIT,REG)   ((*(volatile unsigned long*)(REG)) = (unsigned long)(BIT))
//#define GPIO_CLR_BITS(BIT,REG)   ((*(volatile unsigned long*)(REG)) &= ~((unsigned long)(BIT)))
#define GPIO_SW_SET_BITS(BIT,REG)   GPIO_WR32(REG,GPIO_RD32(REG) | ((unsigned long)(BIT)))
#define GPIO_SET_BITS(BIT,REG)   GPIO_WR32(REG, (unsigned long)(BIT))
#define GPIO_CLR_BITS(BIT,REG)   GPIO_WR32(REG,GPIO_RD32(REG) & ~((unsigned long)(BIT)))

/*----------------------------------------------------------------------------*/
typedef struct {        
    unsigned short val;        
    unsigned short _align1;
    unsigned short set;
    unsigned short _align2;
    unsigned short rst;
    unsigned short _align3[3];
} VAL_REGS;
/*----------------------------------------------------------------------------*/
typedef struct {
    VAL_REGS    dir[14];            /*0x0000 ~ 0x00DF: 224 bytes*/
    unsigned char       rsv00[32];  /*0x00E0 ~ 0x00FF: 	32 bytes*/
    VAL_REGS    pullen[14];         /*0x0100 ~ 0x01DF: 224 bytes*/
    unsigned char       rsv01[32];  /*0x01E0 ~ 0x01FF: 	32 bytes*/
    VAL_REGS    pullsel[14];        /*0x0200 ~ 0x02DF: 224 bytes*/
    unsigned char       rsv02[32];  /*0x02E0 ~ 0x02FF: 	32 bytes*/
    unsigned char       rsv03[256]; /*0x0300 ~ 0x03FF: 256 bytes*/
    VAL_REGS    dout[14];           /*0x0400 ~ 0x04DF: 224 bytes*/
    unsigned char       rsv04[32];  /*0x04B0 ~ 0x04FF: 	32 bytes*/
    VAL_REGS    din[14];            /*0x0500 ~ 0x05DF: 224 bytes*/
    unsigned char       rsv05[32];	/*0x05E0 ~ 0x05FF: 	32 bytes*/
    VAL_REGS    mode[43];           /*0x0600 ~ 0x08AF: 688 bytes*/  
	unsigned char		rsv06[80];	/*0x08B0 ~ 0x08FF:  80 bytes*/
	VAL_REGS    ies[3];            	/*0x0900 ~ 0x092F: 	48 bytes*/
    VAL_REGS    smt[3];        		/*0x0930 ~ 0x095F: 	48 bytes*/ 
	unsigned char		rsv07[160];	/*0x0960 ~ 0x09FF: 160 bytes*/
	VAL_REGS    tdsel[8];        	/*0x0A00 ~ 0x0A7F: 128 bytes*/ 
	VAL_REGS    rdsel[6];        	/*0x0A80 ~ 0x0ADF:  96 bytes*/ 
	unsigned char		rsv08[32];	/*0x0AE0 ~ 0x0AFF:  32 bytes*/
	VAL_REGS    drv_mode[10];       /*0x0B00 ~ 0x0B9F: 160 bytes*/ 
	unsigned char		rsv09[96];	/*0x0BA0 ~ 0x0BFF:  96 bytes*/
	VAL_REGS    msdc0_ctrl0;        /*0x0C00 ~ 0x0D4F: 336 bytes*/ 
	VAL_REGS    msdc0_ctrl1;        
	VAL_REGS    msdc0_ctrl2;        
	VAL_REGS    msdc0_ctrl5;        
	VAL_REGS    msdc1_ctrl0;        
	VAL_REGS    msdc1_ctrl1;        
	VAL_REGS    msdc1_ctrl2;        
	VAL_REGS    msdc1_ctrl4;        
	VAL_REGS    msdc2_ctrl0;        
	VAL_REGS    msdc2_ctrl1;        
	VAL_REGS    msdc2_ctrl2;        
	VAL_REGS    msdc2_ctrl4;        
	VAL_REGS    msdc3_ctrl0;        
	VAL_REGS    msdc3_ctrl1;        
	VAL_REGS    msdc3_ctrl2;        
	VAL_REGS    msdc3_ctrl4;        
	VAL_REGS    msdc0_ctrl3;        
	VAL_REGS    msdc0_ctrl4;        
	VAL_REGS    msdc1_ctrl3;        
	VAL_REGS    msdc2_ctrl3;        
	VAL_REGS    msdc3_ctrl3;        
	VAL_REGS    dpi_ctrl[4];        /*0x0D50 ~ 0x0D8F: 	64 bytes*/
	unsigned char		rsv10[48];	/*0x0D90 ~ 0x0DBF:  48 bytes*/
	VAL_REGS    exmd_ctrl[1];       /*0x0DC0 ~ 0x0DCF: 	16 bytes*/
	VAL_REGS    bpi_ctrl[1];       	/*0x0DD0 ~ 0x0DDF: 	16 bytes*/
	unsigned char		rsv11[32];	/*0x0D90 ~ 0x0DBF:  48 bytes*/
	VAL_REGS    kpad_ctrl[2];       /*0x0E00 ~ 0x0E1F: 	32 bytes*/
	VAL_REGS    sim_ctrl[4];        /*0x0E20 ~ 0x0E5F: 	64 bytes*/
} GPIO_REGS;

#endif //_MT_GPIO_BASE_H_
