/******************************************************************************
 * mt_gpio_debug.c - MTKLinux GPIO Device Driver
 * 
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 * 
 * DESCRIPTION:
 *     This file provid the other drivers GPIO debug functions
 *
 ******************************************************************************/

#include <linux/slab.h>
#ifndef CONFIG_OF
#include <mach/mt_reg_base.h>
#endif
#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>

/*----------------------------------------------------------------------------*/
typedef struct {        /*FIXME: check GPIO spec*/
    unsigned int no     : 16;
    unsigned int mode   : 3;    
    unsigned int pullsel: 1;
    unsigned int din    : 1;
    unsigned int dout   : 1;
    unsigned int pullen : 1;
    unsigned int dir    : 1;
/*    unsigned int dinv   : 1;*/
    unsigned int ies    : 1;
    unsigned int _align : 7; 
} GPIO_CFG; 

//#define MAX_GPIO_REG_BITS      16
//#define MAX_GPIO_MODE_PER_REG  5
//#define GPIO_MODE_BITS         3 
/******************************************************************************
*clock out module
*******************************************************************************/
int mt_set_clock_output(unsigned long num, unsigned long src, unsigned long div)
{
	GPIOERR("GPIO CLKM module not be implement any more!\n");
    return RSUCCESS;
}

int mt_get_clock_output(unsigned long num, unsigned long * src, unsigned long *div)
{
	GPIOERR("GPIO CLKM module not be implement any more!\n");
	return RSUCCESS;
}
/*****************************************************************************/
/* sysfs operation                                                           */
/*****************************************************************************/
void mt_gpio_self_test(void)
{
    int i, val;
    for (i = 0; i < MT_GPIO_EXT_MAX; i++)
    {
        s32 res,old;
        GPIOMSG("GPIO-%3d test\n", i);
        /*direction test*/
        old = mt_get_gpio_dir_base(i);
        if (old == 0 || old == 1) {
            GPIOLOG(" dir old = %d\n", old);
        } else {
            GPIOERR(" test dir fail: %d\n", old);
            break;
        }
        if ((res = mt_set_gpio_dir_base(i, GPIO_DIR_OUT)) != RSUCCESS) {
            GPIOERR(" set dir out fail: %d\n", res);
            break;
        } else if ((res = mt_get_gpio_dir_base(i)) != GPIO_DIR_OUT) {
            GPIOERR(" get dir out fail: %d\n", res);
            break;
        } else {
            /*output test*/
            s32 out = mt_get_gpio_out_base(i);
            if (out != 0 && out != 1) {
                GPIOERR(" get out fail = %d\n", old);
                break;
            } 
            for (val = 0; val < GPIO_OUT_MAX; val++) {
                if ((res = mt_set_gpio_out_base(i,0)) != RSUCCESS) {
                    GPIOERR(" set out[%d] fail: %d\n", val, res);
                    break;
                } else if ((res = mt_get_gpio_out_base(i)) != 0) {
                    GPIOERR(" get out[%d] fail: %d\n", val, res);
                    break;
                }
            }
            if ((res = mt_set_gpio_out_base(i,out)) != RSUCCESS)
            {
                GPIOERR(" restore out fail: %d\n", res);
                break;
            }
        }
            
        if ((res = mt_set_gpio_dir_base(i, GPIO_DIR_IN)) != RSUCCESS) {
            GPIOERR(" set dir in fail: %d\n", res);
            break;
        } else if ((res = mt_get_gpio_dir_base(i)) != GPIO_DIR_IN) {
            GPIOERR(" get dir in fail: %d\n", res);
            break;
        } else {
            GPIOLOG(" input data = %d\n", res);
        }
        
        if ((res = mt_set_gpio_dir_base(i, old)) != RSUCCESS) {
            GPIOERR(" restore dir fail: %d\n", res);
            break;
        }
        for (val = 0; val < GPIO_PULL_EN_MAX; val++) {
            if ((res = mt_set_gpio_pull_enable_base(i,val)) != RSUCCESS) {
                GPIOERR(" set pullen[%d] fail: %d\n", val, res);
                break;
            } else if ((res = mt_get_gpio_pull_enable_base(i)) != val) {
                GPIOERR(" get pullen[%d] fail: %d\n", val, res);
                break;
            }
        }        
        if ((res = mt_set_gpio_pull_enable_base(i, old)) != RSUCCESS) {
            GPIOERR(" restore pullen fail: %d\n", res);
            break;
        }

        /*pull select test*/
        old = mt_get_gpio_pull_select_base(i);
        if (old == 0 || old == 1)
            GPIOLOG(" pullsel old = %d\n", old);
        else {
            GPIOERR(" pullsel fail: %d\n", old);
            break;
        }
        for (val = 0; val < GPIO_PULL_MAX; val++) {
            if ((res = mt_set_gpio_pull_select_base(i,val)) != RSUCCESS) {
                GPIOERR(" set pullsel[%d] fail: %d\n", val, res);
                break;
            } else if ((res = mt_get_gpio_pull_select_base(i)) != val) {
                GPIOERR(" get pullsel[%d] fail: %d\n", val, res);
                break;
            }
        } 
        if ((res = mt_set_gpio_pull_select_base(i, old)) != RSUCCESS)
        {
            GPIOERR(" restore pullsel fail: %d\n", res);
            break;
        }     

        /*data inversion*/
        old = mt_get_gpio_inversion_base(i);
        if (old == 0 || old == 1)
            GPIOLOG(" inv old = %d\n", old);
        else {
            GPIOERR(" inv fail: %d\n", old);
            break;
        }
        for (val = 0; val < GPIO_DATA_INV_MAX; val++) {
            if ((res = mt_set_gpio_inversion_base(i,val)) != RSUCCESS) {
                GPIOERR(" set inv[%d] fail: %d\n", val, res);
                break;
            } else if ((res = mt_get_gpio_inversion_base(i)) != val) {
                GPIOERR(" get inv[%d] fail: %d\n", val, res);
                break;
            }
        } 
        if ((res = mt_set_gpio_inversion_base(i, old)) != RSUCCESS) {
            GPIOERR(" restore inv fail: %d\n", res);
            break;
        }     

        /*mode control*/
//		if((i<=GPIOEXT6) || (i >= GPIOEXT9)){
        old = mt_get_gpio_mode_base(i);
        if ((old >= GPIO_MODE_00) && (val < GPIO_MODE_MAX))
        {
            GPIOLOG(" mode old = %d\n", old);
        }
        else
        {
            GPIOERR(" get mode fail: %d\n", old);
            break;
        }
        for (val = 0; val < GPIO_MODE_MAX; val++) {
            if ((res = mt_set_gpio_mode_base(i, val)) != RSUCCESS) {
                GPIOERR("set mode[%d] fail: %d\n", val, res);
                break;
            } else if ((res = mt_get_gpio_mode_base(i)) != val) {
                GPIOERR("get mode[%d] fail: %d\n", val, res);
                break;
            }            
        }        
        if ((res = mt_set_gpio_mode_base(i,old)) != RSUCCESS) {
            GPIOERR(" restore mode fail: %d\n", res);
            break;
        }   
//		}    
    }
    GPIOLOG("GPIO test done\n");
}
/*----------------------------------------------------------------------------
void mt_gpio_load_ext(GPIOEXT_REGS *regs) 
{
    GPIOEXT_REGS *pReg = (GPIOEXT_REGS*)(GPIOEXT_BASE);
    int idx;
    
    if (!regs)
        GPIOERR("%s: null pointer\n", __func__);
    memset(regs, 0x00, sizeof(*regs));
    for (idx = 0; idx < sizeof(pReg->dir)/sizeof(pReg->dir[0]); idx++)
        regs->dir[idx].val = GPIOEXT_RD(&pReg->dir[idx]);
    for (idx = 0; idx < sizeof(pReg->pullen)/sizeof(pReg->pullen[0]); idx++)
        regs->pullen[idx].val = GPIOEXT_RD(&pReg->pullen[idx]);
    for (idx = 0; idx < sizeof(pReg->pullsel)/sizeof(pReg->pullsel[0]); idx++)
        regs->pullsel[idx].val =GPIOEXT_RD(&pReg->pullsel[idx]);
    for (idx = 0; idx < sizeof(pReg->dinv)/sizeof(pReg->dinv[0]); idx++)
        regs->dinv[idx].val =GPIOEXT_RD(&pReg->dinv[idx]);
    for (idx = 0; idx < sizeof(pReg->dout)/sizeof(pReg->dout[0]); idx++)
        regs->dout[idx].val = GPIOEXT_RD(&pReg->dout[idx]);
    for (idx = 0; idx < sizeof(pReg->mode)/sizeof(pReg->mode[0]); idx++)
        regs->mode[idx].val = GPIOEXT_RD(&pReg->mode[idx]);
    for (idx = 0; idx < sizeof(pReg->din)/sizeof(pReg->din[0]); idx++)
        regs->din[idx].val = GPIOEXT_RD(&pReg->din[idx]);
}
EXPORT_SYMBOL(mt_gpio_load_ext);
----------------------------------------------------------------------------*/
void mt_gpio_load_base(GPIO_REGS *regs) 
{
    GPIO_REGS *pReg = (GPIO_REGS*)(GPIO_BASE_1);
    int idx;
    
    if (!regs)
        GPIOERR("%s: null pointer\n", __func__);
    memset(regs, 0x00, sizeof(*regs));
    for (idx = 0; idx < sizeof(pReg->dir)/sizeof(pReg->dir[0]); idx++)
        regs->dir[idx].val = __raw_readl(&pReg->dir[idx]);
    for (idx = 0; idx < sizeof(pReg->ies)/sizeof(pReg->ies[0]); idx++)
        regs->ies[idx].val = __raw_readl(&pReg->ies[idx]);
    for (idx = 0; idx < sizeof(pReg->pullen)/sizeof(pReg->pullen[0]); idx++)
        regs->pullen[idx].val = __raw_readl(&pReg->pullen[idx]);
    for (idx = 0; idx < sizeof(pReg->pullsel)/sizeof(pReg->pullsel[0]); idx++)
        regs->pullsel[idx].val =__raw_readl(&pReg->pullsel[idx]);
/*    for (idx = 0; idx < sizeof(pReg->dinv)/sizeof(pReg->dinv[0]); idx++)
        regs->dinv[idx].val =__raw_readl(&pReg->dinv[idx]);*/
    for (idx = 0; idx < sizeof(pReg->dout)/sizeof(pReg->dout[0]); idx++)
        regs->dout[idx].val = __raw_readl(&pReg->dout[idx]);
    for (idx = 0; idx < sizeof(pReg->mode)/sizeof(pReg->mode[0]); idx++)
        regs->mode[idx].val = __raw_readl(&pReg->mode[idx]);
    for (idx = 0; idx < sizeof(pReg->din)/sizeof(pReg->din[0]); idx++)
        regs->din[idx].val = __raw_readl(&pReg->din[idx]);
}
//EXPORT_SYMBOL(mt_gpio_load_base);
/*----------------------------------------------------------------------------
void mt_gpio_dump_ext( GPIOEXT_REGS *regs) 
{
    GPIOEXT_REGS *cur = NULL ;    
    int idx;
    GPIOMSG("%s\n", __func__);
	//if arg is null, load & dump; otherwise, dump only
	if (regs == NULL) { 
		cur = kzalloc(sizeof(*cur), GFP_KERNEL);    
		if (cur == NULL) {
			GPIOERR("GPIO extend null pointer\n");
			return;
		}
        regs = cur;
		mt_gpio_load_ext(cur);
        GPIOMSG("dump current: %p\n", regs);
    } else {
        GPIOMSG("dump %p ...\n", regs);    
    }
    GPIOMSG("\nGPIO extend-------------------------------------------------------------------\n");    
    GPIOMSG("---# dir #-----------------------------------------------------------------\n");
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->dir[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->dir)/sizeof(regs->dir[0]); idx++) {
        GPIOMSG("0x%04X ", regs->dir[idx].val);
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    //GPIOMSG("\n---# ies #-----------------------------------------------------------------\n");
    //GPIOMSG("Offset 0x%04X\n",(void *)(&regs->ies[0]);
    //for (idx = 0; idx < sizeof(regs->ies)/sizeof(regs->ies[0]); idx++) {
    //    GPIOMSG("0x%04X ", regs->ies[idx].val);
    //    if (7 == (idx % 8)) GPIOMSG("\n");
    //}
    GPIOMSG("\n---# pullen #--------------------------------------------------------------\n");        
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->pullen[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->pullen)/sizeof(regs->pullen[0]); idx++) {
        GPIOMSG("0x%04X ", regs->pullen[idx].val);    
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# pullsel #-------------------------------------------------------------\n");   
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->pullsel[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->pullsel)/sizeof(regs->pullsel[0]); idx++) {
        GPIOMSG("0x%04X ", regs->pullsel[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# dinv #----------------------------------------------------------------\n");   
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->dinv[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->dinv)/sizeof(regs->dinv[0]); idx++) {
        GPIOMSG("0x%04X ", regs->dinv[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# dout #----------------------------------------------------------------\n");   
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->dout[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->dout)/sizeof(regs->dout[0]); idx++) {
        GPIOMSG("0x%04X ", regs->dout[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# din  #----------------------------------------------------------------\n");   
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->din[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->din)/sizeof(regs->din[0]); idx++) {
        GPIOMSG("0x%04X ", regs->din[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# mode #----------------------------------------------------------------\n");   
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->mode[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->mode)/sizeof(regs->mode[0]); idx++) {
        GPIOMSG("0x%04X ", regs->mode[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }    
    GPIOMSG("\n---------------------------------------------------------------------------\n");    


	if (cur != NULL) {
		kfree(cur);
	}
}
----------------------------------------------------------------------------*/
void mt_gpio_dump_base(GPIO_REGS *regs) 
{
    GPIO_REGS *cur = NULL ;    
    int idx;
    GPIOMSG("%s\n", __func__);
	if (regs == NULL) { /*if arg is null, load & dump; otherwise, dump only*/
		cur = kzalloc(sizeof(*cur), GFP_KERNEL);    
		if (cur == NULL) {
			GPIOERR("null pointer\n");
			return;
		}
        regs = cur;
        mt_gpio_load_base(regs);
        GPIOMSG("dump current: %p\n", regs);
    } else {
        GPIOMSG("dump %p ...\n", regs);    
    }

    GPIOMSG("---# dir #-----------------------------------------------------------------\n");
#ifdef CONFIG_64BIT
    GPIOMSG("Offset 0x%lx\n",(void *)(&regs->dir[0])-(void *)regs);
#else
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->dir[0])-(void *)regs);
#endif
    for (idx = 0; idx < sizeof(regs->dir)/sizeof(regs->dir[0]); idx++) {
        GPIOMSG("0x%04X ", regs->dir[idx].val);
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# ies #-----------------------------------------------------------------\n");
#ifdef CONFIG_64BIT
    GPIOMSG("Offset 0x%04lx\n",(void *)(&regs->ies[0])-(void *)regs);
#else
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->pullen[0])-(void *)regs);
#endif	
    for (idx = 0; idx < sizeof(regs->ies)/sizeof(regs->ies[0]); idx++) {
        GPIOMSG("0x%04X ", regs->ies[idx].val);
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# pullen #--------------------------------------------------------------\n");        
#ifdef CONFIG_64BIT
    GPIOMSG("Offset 0x%04lx\n",(void *)(&regs->pullen[0])-(void *)regs);
#else
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->pullen[0])-(void *)regs);
#endif
    for (idx = 0; idx < sizeof(regs->pullen)/sizeof(regs->pullen[0]); idx++) {
        GPIOMSG("0x%04X ", regs->pullen[idx].val);    
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# pullsel #-------------------------------------------------------------\n");   
#ifdef CONFIG_64BIT
    GPIOMSG("Offset 0x%04lx\n",(void *)(&regs->pullsel[0])-(void *)regs);
#else
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->pullsel[0])-(void *)regs);
#endif
    for (idx = 0; idx < sizeof(regs->pullsel)/sizeof(regs->pullsel[0]); idx++) {
        GPIOMSG("0x%04X ", regs->pullsel[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
/*    GPIOMSG("\n---# dinv #----------------------------------------------------------------\n");   
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->dinv[0])-(void *)regs);
    for (idx = 0; idx < sizeof(regs->dinv)/sizeof(regs->dinv[0]); idx++) {
        GPIOMSG("0x%04X ", regs->dinv[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }*/
    GPIOMSG("\n---# dout #----------------------------------------------------------------\n");   
#ifdef CONFIG_64BIT
    GPIOMSG("Offset 0x%lx\n",(void *)(&regs->dout[0])-(void *)regs);
#else
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->dout[0])-(void *)regs);
#endif
    for (idx = 0; idx < sizeof(regs->dout)/sizeof(regs->dout[0]); idx++) {
        GPIOMSG("0x%04X ", regs->dout[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# din  #----------------------------------------------------------------\n");   
#ifdef CONFIG_64BIT
    GPIOMSG("Offset 0x%lx\n",(void *)(&regs->din[0])-(void *)regs);
#else
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->din[0])-(void *)regs);
#endif
    for (idx = 0; idx < sizeof(regs->din)/sizeof(regs->din[0]); idx++) {
        GPIOMSG("0x%04X ", regs->din[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }
    GPIOMSG("\n---# mode #----------------------------------------------------------------\n");   
#ifdef CONFIG_64BIT
    GPIOMSG("Offset 0x%lx\n",(void *)(&regs->mode[0])-(void *)regs);
#else
    GPIOMSG("Offset 0x%04X\n",(void *)(&regs->mode[0])-(void *)regs);
#endif
    for (idx = 0; idx < sizeof(regs->mode)/sizeof(regs->mode[0]); idx++) {
        GPIOMSG("0x%04X ", regs->mode[idx].val);     
        if (7 == (idx % 8)) GPIOMSG("\n");
    }    
    GPIOMSG("\n---------------------------------------------------------------------------\n");    


	if (cur != NULL) {
		kfree(cur);
	}
}
/*----------------------------------------------------------------------------*/
typedef struct {        
    unsigned short val;        
    unsigned short _align1;
    unsigned short set;
    unsigned short _align2;
    unsigned short rst;
    unsigned short _align3[3];
} VAL_REGS_test;
/*----------------------------------------------------------------------------*/
typedef struct {
    VAL_REGS_test    dir[14];            /*0x0000 ~ 0x00DF: 224 bytes*/
    unsigned char    rsv00[32];          /*0x00E0 ~ 0x00FF: 	32 bytes*/
    VAL_REGS_test    pullen[14];         /*0x0100 ~ 0x01DF: 224 bytes*/
    unsigned char    rsv01[32];          /*0x01E0 ~ 0x01FF: 	32 bytes*/
    VAL_REGS_test    pullsel[14];        /*0x0200 ~ 0x02DF: 224 bytes*/
    unsigned char    rsv02[32];          /*0x02E0 ~ 0x02FF: 	32 bytes*/
    unsigned char    rsv03[256];         /*0x0300 ~ 0x03FF: 256 bytes*/
    VAL_REGS_test    dout[14];           /*0x0400 ~ 0x04DF: 224 bytes*/
    unsigned char    rsv04[32];          /*0x04B0 ~ 0x04FF: 	32 bytes*/
    VAL_REGS_test    din[14];            /*0x0500 ~ 0x05DF: 224 bytes*/
    unsigned char    rsv05[32];	         /*0x05E0 ~ 0x05FF: 	32 bytes*/
    VAL_REGS_test    mode[43];           /*0x0600 ~ 0x08AF: 688 bytes*/  
	unsigned char	 rsv06[80];       	 /*0x08B0 ~ 0x08FF:  80 bytes*/
	VAL_REGS_test    ies[3];             /*0x0900 ~ 0x092F: 	48 bytes*/
    VAL_REGS_test    smt[3];        	 /*0x0930 ~ 0x095F: 	48 bytes*/ 
	unsigned char    rsv07[160];	     /*0x0960 ~ 0x09FF: 160 bytes*/
	VAL_REGS_test    tdsel[8];         	 /*0x0A00 ~ 0x0A7F: 128 bytes*/ 
	VAL_REGS_test    rdsel[6];        	 /*0x0A80 ~ 0x0ADF:  96 bytes*/ 
	unsigned char	 rsv08[32];	         /*0x0AE0 ~ 0x0AFF:  32 bytes*/
	VAL_REGS_test    drv_mode[10];       /*0x0B00 ~ 0x0B9F: 160 bytes*/ 
	unsigned char	 rsv09[96];	         /*0x0BA0 ~ 0x0BFF:  96 bytes*/
	VAL_REGS_test    msdc0_ctrl0;        /*0x0C00 ~ 0x0D4F: 336 bytes*/ 
	VAL_REGS_test    msdc0_ctrl1;        
	VAL_REGS_test    msdc0_ctrl2;        
	VAL_REGS_test    msdc0_ctrl5;        
	VAL_REGS_test    msdc1_ctrl0;        
	VAL_REGS_test    msdc1_ctrl1;        
	VAL_REGS_test    msdc1_ctrl2;        
	VAL_REGS_test    msdc1_ctrl4;        
	VAL_REGS_test    msdc2_ctrl0;        
	VAL_REGS_test    msdc2_ctrl1;        
	VAL_REGS_test    msdc2_ctrl2;        
	VAL_REGS_test    msdc2_ctrl4;        
	VAL_REGS_test    msdc3_ctrl0;        
	VAL_REGS_test    msdc3_ctrl1;        
	VAL_REGS_test    msdc3_ctrl2;        
	VAL_REGS_test    msdc3_ctrl4;        
	VAL_REGS_test    msdc0_ctrl3;        
	VAL_REGS_test    msdc0_ctrl4;        
	VAL_REGS_test    msdc1_ctrl3;        
	VAL_REGS_test    msdc2_ctrl3;        
	VAL_REGS_test    msdc3_ctrl3;        
	VAL_REGS_test    dpi_ctrl[4];        /*0x0D50 ~ 0x0D8F: 	64 bytes*/
	unsigned char	 rsv10[48];	         /*0x0D90 ~ 0x0DBF:  48 bytes*/
	VAL_REGS_test    exmd_ctrl[1];       /*0x0DC0 ~ 0x0DCF: 	16 bytes*/
	VAL_REGS_test    bpi_ctrl[1];        /*0x0DD0 ~ 0x0DDF: 	16 bytes*/
	unsigned char	 rsv11[32];	         /*0x0D90 ~ 0x0DBF:  48 bytes*/
	VAL_REGS_test    kpad_ctrl[2];       /*0x0E00 ~ 0x0E1F: 	32 bytes*/
	VAL_REGS_test    sim_ctrl[4];        /*0x0E20 ~ 0x0E5F: 	64 bytes*/
} GPIO_REGS_test;

typedef union {
	GPIO_REGS_test cur_mem;
    u32 data[1024];
} TEST_NUION;

void mt_reg_test(void) 
{
	TEST_NUION *test_data = NULL;
	GPIO_REGS_test *cur_mem_data = NULL ;    
    int idx, i;

	test_data = kzalloc(sizeof(*test_data), GFP_KERNEL);  
	if (test_data == NULL) {
		GPIOERR("cur_mem null pointer.\n");
		return;
	}
#ifdef CONFIG_64BIT
	GPIOERR("test_data size=%ld.\n", sizeof(*test_data));
#else
	GPIOERR("test_data size=%d.\n", sizeof(*test_data));
#endif
	memset(test_data, 0x00, sizeof(*test_data));
	cur_mem_data = (GPIO_REGS_test *)&test_data->cur_mem;

	for (i=0; i<14; i++) {
	    cur_mem_data->dir[i].val = i;
	    cur_mem_data->pullen[i].val = i+3;
	    cur_mem_data->pullsel[i].val = i+5;
	    cur_mem_data->dout[i].val = i+16;
	    cur_mem_data->din[i].val = i+40;
	}
	for (i=0; i<43; i++) {
	    cur_mem_data->mode[i].val = i+1;
	}
	for (i=0; i<3; i++) {
	    cur_mem_data->ies[i].val = i+6;
	    cur_mem_data->smt[i].val = i+9;
	}
	for (i=0; i<6; i++) {
	    cur_mem_data->tdsel[i].val = i+1;
	    cur_mem_data->rdsel[i].val = i;
	}
	for (i=0; i<10; i++) {
	    cur_mem_data->drv_mode[i].val = i+15;
	}
	for (i=0; i<4; i++) {
	    cur_mem_data->dpi_ctrl[i].val = i+115;
	}
	for (i=0; i<4; i++) {
	    cur_mem_data->sim_ctrl[i].val = i+515;
	}

	cur_mem_data->exmd_ctrl[0].val = 0x115;
	cur_mem_data->bpi_ctrl[0].val = 0xaaa;
	cur_mem_data->kpad_ctrl[0].val = 0xfac;
	cur_mem_data->kpad_ctrl[1].val = 0x12c;

	cur_mem_data->msdc0_ctrl5.val = 0x12c;
	cur_mem_data->msdc1_ctrl2.val = 0x22c;
	cur_mem_data->msdc2_ctrl1.val = 0x32c;
	cur_mem_data->msdc3_ctrl1.val = 0x42c;
	cur_mem_data->msdc2_ctrl3.val = 0x52c;

	//GPIOMSG("0x%04X\n",(void *)(&regs->mode[0])-(void *)regs);
	for (i=0; i<1023; i+=4) {
		if (i%16==0) GPIOMSG("////i=0x%X /////\n", i);
	    GPIOMSG("0x%04X, 0x%04X, 0x%04X, 0x%04X\n", test_data->data[i], test_data->data[i+1], test_data->data[i+2], test_data->data[i+3]);
	}

	kfree(test_data);
}


void mt_gpio_dump(void) 
{
	mt_gpio_dump_base(NULL);
	//mt_gpio_dump_ext(NULL);
}

/*----------------------------------------------------------------------------*/
void gpio_dump_regs(void)
{
    int idx = 0;	
	/*GPIOMSG("PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [INV] [IES]\n");*/
	GPIOMSG("PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [IES]\n");
    //for (idx = 0; idx < MT_GPIO_MAX_PIN; idx++) {
	//	printk("idx = %3d: %d %d %d %d %d %d %d %d\n",
	//	   idx,mt_get_gpio_mode(idx), mt_get_gpio_pull_select(idx), mt_get_gpio_in(idx),mt_get_gpio_out(idx),
	//	   mt_get_gpio_pull_enable(idx),mt_get_gpio_dir(idx),mt_get_gpio_inversion(idx),mt_get_gpio_ies(idx)); 
    //}
    for (idx = MT_GPIO_BASE_START; idx < MT_GPIO_BASE_MAX; idx++) {
		printk("idx = %3d: %d %d %d %d %d %d %d\n",
		   idx,mt_get_gpio_mode_base(idx), mt_get_gpio_pull_select_base(idx), mt_get_gpio_in_base(idx),mt_get_gpio_out_base(idx),
		   mt_get_gpio_pull_enable_base(idx),mt_get_gpio_dir_base(idx),mt_get_gpio_ies_base(idx)); 
    }
    /*for (idx = MT_GPIO_EXT_START; idx < MT_GPIO_EXT_MAX; idx++) {
		printk("idx = %3d: %d %d %d %d %d %d %d %d\n",
		   idx,mt_get_gpio_mode_ext(idx), mt_get_gpio_pull_select_ext(idx), mt_get_gpio_in_ext(idx),mt_get_gpio_out_ext(idx),
		   mt_get_gpio_pull_enable_ext(idx),mt_get_gpio_dir_ext(idx),mt_get_gpio_inversion_ext(idx),mt_get_gpio_ies_ext(idx)); 
    }*/
}
//EXPORT_SYMBOL(gpio_dump_regs);
/*----------------------------------------------------------------------------*/
/*
static ssize_t mt_gpio_dump_regs(char *buf, ssize_t bufLen)
{
    int idx = 0, len = 0;
	char tmp[]="PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [INV] [IES]\n";
	len += snprintf(buf+len, bufLen-len, "%s",tmp);
    for (idx = 0; idx < MT_GPIO_MAX_PIN; idx++) {
		len += snprintf(buf+len, bufLen-len, "%3d:%d%d%d%d%d%d%d%d\n",
		   idx,mt_get_gpio_mode(idx), mt_get_gpio_pull_select(idx), mt_get_gpio_in(idx),mt_get_gpio_out(idx),
		   mt_get_gpio_pull_enable(idx),mt_get_gpio_dir(idx),mt_get_gpio_inversion(idx),mt_get_gpio_ies(idx)); 
    }
    return len;
}
*/
/*---------------------------------------------------------------------------*/
static void mt_gpio_read_pin_base(GPIO_CFG* cfg, int method)
{
    if (method == 0) {
        GPIO_REGS *cur = (GPIO_REGS*)GPIO_BASE_1;    
        u32 mask = (1L << GPIO_MODE_BITS) - 1;        
        int num, bit; 
		num = cfg->no / MAX_GPIO_REG_BITS;
		bit = cfg->no % MAX_GPIO_REG_BITS;
		if(cfg->no < MT_GPIO_BASE_MAX){
			cfg->pullsel= (cur->pullsel[num].val & (1L << bit)) ? (1) : (0);
			cfg->din    = (cur->din[num].val & (1L << bit)) ? (1) : (0);
			cfg->dout   = (cur->dout[num].val & (1L << bit)) ? (1) : (0);
			cfg->pullen = (cur->pullen[num].val & (1L << bit)) ? (1) : (0);
			cfg->dir    = (cur->dir[num].val & (1L << bit)) ? (1) : (0);
/*			cfg->dinv   = (cur->dinv[num].val & (1L << bit)) ? (1) : (0);*/
			num = cfg->no / MAX_GPIO_MODE_PER_REG;        
			bit = cfg->no % MAX_GPIO_MODE_PER_REG;
			cfg->mode   = (cur->mode[num].val >> (GPIO_MODE_BITS*bit)) & mask;
		}
    } else if (method == 1) {
        cfg->pullsel= mt_get_gpio_pull_select_base(cfg->no);
        cfg->din    = mt_get_gpio_in_base(cfg->no);
        cfg->dout   = mt_get_gpio_out_base(cfg->no);
        cfg->pullen = mt_get_gpio_pull_enable_base(cfg->no);
        cfg->dir    = mt_get_gpio_dir_base(cfg->no);
/*        cfg->dinv   = mt_get_gpio_inversion(cfg->no);*/
        cfg->ies    = mt_get_gpio_ies_base(cfg->no);
        cfg->mode   = mt_get_gpio_mode_base(cfg->no);
    }
}
/*
static void mt_gpio_read_pin_ext(GPIO_CFG* cfg, int method)
{
    if (method == 0) {
        GPIOEXT_REGS *cur = (GPIOEXT_REGS*)GPIOEXT_BASE;    
        u32 mask = (1L << GPIO_MODE_BITS) - 1;        
        int num, bit; 
		num = cfg->no / MAX_GPIO_REG_BITS;
		bit = cfg->no % MAX_GPIO_REG_BITS;
		if(cfg->no >= MT_GPIO_EXT_START){
			//
			cfg->pullsel= (cur->pullsel[num].val & (1L << bit)) ? (1) : (0);
			cfg->din    = (cur->din[num].val & (1L << bit)) ? (1) : (0);
			cfg->dout   = (cur->dout[num].val & (1L << bit)) ? (1) : (0);
			cfg->pullen = (cur->pullen[num].val & (1L << bit)) ? (1) : (0);
			cfg->dir    = (cur->dir[num].val & (1L << bit)) ? (1) : (0);
			cfg->dinv   = (cur->dinv[num].val & (1L << bit)) ? (1) : (0);
			num = cfg->no / MAX_GPIO_MODE_PER_REG;        
			bit = cfg->no % MAX_GPIO_MODE_PER_REG;
			cfg->mode   = (cur->mode[num].val >> (GPIO_MODE_BITS*bit)) & mask;
			
		}
    } else if (method == 1) {
        cfg->pullsel= mt_get_gpio_pull_select(cfg->no);
        cfg->din    = mt_get_gpio_in(cfg->no);
        cfg->dout   = mt_get_gpio_out(cfg->no);
        cfg->pullen = mt_get_gpio_pull_enable(cfg->no);
        cfg->dir    = mt_get_gpio_dir(cfg->no);
        cfg->dinv   = mt_get_gpio_inversion(cfg->no);
        cfg->mode   = mt_get_gpio_mode(cfg->no);
    }
}*/
/*---------------------------------------------------------------------------
static void mt_gpio_dump_addr_ext(void)
{
    int idx;
    GPIOEXT_REGS *reg = (GPIOEXT_REGS*)GPIOEXT_BASE;

    GPIOMSG("# GPIOEXT dump\n");
    GPIOMSG("# direction\n");
    for (idx = 0; idx < sizeof(reg->dir)/sizeof(reg->dir[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->dir[idx].val, idx, &reg->dir[idx].set, idx, &reg->dir[idx].rst);
    GPIOMSG("# pull enable\n");
    for (idx = 0; idx < sizeof(reg->pullen)/sizeof(reg->pullen[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->pullen[idx].val, idx, &reg->pullen[idx].set, idx, &reg->pullen[idx].rst);
    GPIOMSG("# pull select\n");
    for (idx = 0; idx < sizeof(reg->pullsel)/sizeof(reg->pullsel[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->pullsel[idx].val, idx, &reg->pullsel[idx].set, idx, &reg->pullsel[idx].rst);
    GPIOMSG("# data inversion\n");
    for (idx = 0; idx < sizeof(reg->dinv)/sizeof(reg->dinv[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->dinv[idx].val, idx, &reg->dinv[idx].set, idx, &reg->dinv[idx].rst);
    GPIOMSG("# data output\n");
    for (idx = 0; idx < sizeof(reg->dout)/sizeof(reg->dout[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->dout[idx].val, idx, &reg->dout[idx].set, idx, &reg->dout[idx].rst);
    GPIOMSG("# data input\n");
    for (idx = 0; idx < sizeof(reg->din)/sizeof(reg->din[0]); idx++)
        GPIOMSG("val[%2d] %p\n", idx, &reg->din[idx].val);
    GPIOMSG("# mode\n");
    for (idx = 0; idx < sizeof(reg->mode)/sizeof(reg->mode[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->mode[idx].val, idx, &reg->mode[idx].set, idx, &reg->mode[idx].rst);    
}*/
static ssize_t mt_gpio_dump_addr_base(void)
{
    int idx;
    GPIO_REGS *reg = (GPIO_REGS*)GPIO_BASE_1;

    GPIOMSG("# direction\n");
    for (idx = 0; idx < sizeof(reg->dir)/sizeof(reg->dir[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->dir[idx].val, idx, &reg->dir[idx].set, idx, &reg->dir[idx].rst);
    GPIOMSG("# ies\n");
    for (idx = 0; idx < sizeof(reg->ies)/sizeof(reg->ies[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->ies[idx].val, idx, &reg->ies[idx].set, idx, &reg->ies[idx].rst);
    GPIOMSG("# pull enable\n");
    for (idx = 0; idx < sizeof(reg->pullen)/sizeof(reg->pullen[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->pullen[idx].val, idx, &reg->pullen[idx].set, idx, &reg->pullen[idx].rst);
    GPIOMSG("# pull select\n");
    for (idx = 0; idx < sizeof(reg->pullsel)/sizeof(reg->pullsel[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->pullsel[idx].val, idx, &reg->pullsel[idx].set, idx, &reg->pullsel[idx].rst);
/*    GPIOMSG("# data inversion\n");
    for (idx = 0; idx < sizeof(reg->dinv)/sizeof(reg->dinv[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->dinv[idx].val, idx, &reg->dinv[idx].set, idx, &reg->dinv[idx].rst);*/
    GPIOMSG("# data output\n");
    for (idx = 0; idx < sizeof(reg->dout)/sizeof(reg->dout[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->dout[idx].val, idx, &reg->dout[idx].set, idx, &reg->dout[idx].rst);
    GPIOMSG("# data input\n");
    for (idx = 0; idx < sizeof(reg->din)/sizeof(reg->din[0]); idx++)
        GPIOMSG("val[%2d] %p\n", idx, &reg->din[idx].val);
    GPIOMSG("# mode\n");
    for (idx = 0; idx < sizeof(reg->mode)/sizeof(reg->mode[0]); idx++)
        GPIOMSG("val[%2d] %p\nset[%2d] %p\nrst[%2d] %p\n", idx, &reg->mode[idx].val, idx, &reg->mode[idx].set, idx, &reg->mode[idx].rst);
    return 0;    
}
/*---------------------------------------------------------------------------
static void mt_gpio_compare_ext(void)
{
    int idx;
    GPIOEXT_REGS *reg = (GPIOEXT_REGS*)GPIOEXT_BASE;
    GPIOEXT_REGS *cur = kzalloc(sizeof(*cur), GFP_KERNEL);    

    if (!cur)
        return;
    
    mt_gpio_load_ext(cur);
    for (idx = 0; idx < sizeof(reg->dir)/sizeof(reg->dir[0]); idx++)
        if (reg->dir[idx].val != cur->dir[idx].val)
            GPIOERR("GPIOEXT mismatch dir[%2d]: %x <> %x\n", idx, reg->dir[idx].val, cur->dir[idx].val);
    for (idx = 0; idx < sizeof(reg->pullen)/sizeof(reg->pullen[0]); idx++)
        if (reg->pullen[idx].val != cur->pullen[idx].val)
            GPIOERR("GPIOEXT mismatch pullen[%2d]: %x <> %x\n", idx, reg->pullen[idx].val, cur->pullen[idx].val);
    for (idx = 0; idx < sizeof(reg->pullsel)/sizeof(reg->pullsel[0]); idx++)
        if (reg->pullsel[idx].val != cur->pullsel[idx].val)
            GPIOERR("GPIOEXT mismatch pullsel[%2d]: %x <> %x\n", idx, reg->pullsel[idx].val, cur->pullsel[idx].val);
    for (idx = 0; idx < sizeof(reg->dinv)/sizeof(reg->dinv[0]); idx++)
        if (reg->dinv[idx].val != cur->dinv[idx].val)
            GPIOERR("GPIOEXT mismatch dinv[%2d]: %x <> %x\n", idx, reg->dinv[idx].val, cur->dinv[idx].val);
    for (idx = 0; idx < sizeof(reg->dout)/sizeof(reg->dout[0]); idx++)
        if (reg->dout[idx].val != cur->dout[idx].val)
            GPIOERR("GPIOEXT mismatch dout[%2d]: %x <> %x\n", idx, reg->dout[idx].val, cur->dout[idx].val);
    for (idx = 0; idx < sizeof(reg->din)/sizeof(reg->din[0]); idx++)
        if (reg->din[idx].val != cur->din[idx].val)
            GPIOERR("GPIOEXT mismatch din[%2d]: %x <> %x\n", idx, reg->din[idx].val, cur->din[idx].val);
    for (idx = 0; idx < sizeof(reg->mode)/sizeof(reg->mode[0]); idx++)
        if (reg->mode[idx].val != cur->mode[idx].val)
            GPIOERR("GPIOEXT mismatch mode[%2d]: %x <> %x\n", idx, reg->mode[idx].val, cur->mode[idx].val); 

    kfree(cur);
    return;
}*/
static ssize_t mt_gpio_compare_base(void)
{
    int idx;
    GPIO_REGS *reg = (GPIO_REGS*)GPIO_BASE_1;
    GPIO_REGS *cur = kzalloc(sizeof(*cur), GFP_KERNEL);    

    if (!cur)
        return 0;
    
    mt_gpio_load_base(cur);
    for (idx = 0; idx < sizeof(reg->dir)/sizeof(reg->dir[0]); idx++)
        if (reg->dir[idx].val != cur->dir[idx].val)
            GPIOERR("mismatch dir[%2d]: %x <> %x\n", idx, reg->dir[idx].val, cur->dir[idx].val);
    for (idx = 0; idx < sizeof(reg->pullen)/sizeof(reg->pullen[0]); idx++)
        if (reg->pullen[idx].val != cur->pullen[idx].val)
            GPIOERR("mismatch pullen[%2d]: %x <> %x\n", idx, reg->pullen[idx].val, cur->pullen[idx].val);
    for (idx = 0; idx < sizeof(reg->pullsel)/sizeof(reg->pullsel[0]); idx++)
        if (reg->pullsel[idx].val != cur->pullsel[idx].val)
            GPIOERR("mismatch pullsel[%2d]: %x <> %x\n", idx, reg->pullsel[idx].val, cur->pullsel[idx].val);
/*    for (idx = 0; idx < sizeof(reg->dinv)/sizeof(reg->dinv[0]); idx++)
        if (reg->dinv[idx].val != cur->dinv[idx].val)
            GPIOERR("mismatch dinv[%2d]: %x <> %x\n", idx, reg->dinv[idx].val, cur->dinv[idx].val);*/
    for (idx = 0; idx < sizeof(reg->dout)/sizeof(reg->dout[0]); idx++)
        if (reg->dout[idx].val != cur->dout[idx].val)
            GPIOERR("mismatch dout[%2d]: %x <> %x\n", idx, reg->dout[idx].val, cur->dout[idx].val);
    for (idx = 0; idx < sizeof(reg->din)/sizeof(reg->din[0]); idx++)
        if (reg->din[idx].val != cur->din[idx].val)
            GPIOERR("mismatch din[%2d]: %x <> %x\n", idx, reg->din[idx].val, cur->din[idx].val);
    for (idx = 0; idx < sizeof(reg->mode)/sizeof(reg->mode[0]); idx++)
        if (reg->mode[idx].val != cur->mode[idx].val)
            GPIOERR("mismatch mode[%2d]: %x <> %x\n", idx, reg->mode[idx].val, cur->mode[idx].val); 

    kfree(cur);
    return 0;
}
static ssize_t mt_gpio_dump_regs(char *buf, ssize_t bufLen)
{
    int idx = 0, len = 0;
#ifdef CONFIG_MTK_FPGA
	char tmp[]="PIN: [DIN] [DOUT] [DIR]\n";
	len += snprintf(buf+len, bufLen-len, "%s",tmp);
    for (idx = MT_GPIO_BASE_START; idx < MT_GPIO_BASE_MAX; idx++) {
		len += snprintf(buf+len, bufLen-len, "%3d:%d%d%d\n",
		   idx, mt_get_gpio_in_base(idx),mt_get_gpio_out_base(idx),
		   mt_get_gpio_dir_base(idx)); 
	}
#else
	/*char tmp[]="PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [INV] [IES]\n";*/
	char tmp[]="PIN: [MODE] [PULL_SEL] [DIN] [DOUT] [PULL EN] [DIR] [IES]\n";
	len += snprintf(buf+len, bufLen-len, "%s",tmp);
    for (idx = MT_GPIO_BASE_START; idx < MT_GPIO_BASE_MAX; idx++) {
		len += snprintf(buf+len, bufLen-len, "%3d:%d%d%d%d%d%d%d\n",
		   idx,mt_get_gpio_mode_base(idx), mt_get_gpio_pull_select_base(idx), mt_get_gpio_in_base(idx),mt_get_gpio_out_base(idx),
		   mt_get_gpio_pull_enable_base(idx),mt_get_gpio_dir_base(idx),mt_get_gpio_ies_base(idx)); 
		//printk("%3d:%d%d%d%d%d%d%d%d\n",idx,
		//		mt_get_gpio_mode_base(idx), mt_get_gpio_pull_select_base(idx), mt_get_gpio_in_base(idx),mt_get_gpio_out_base(idx),
		//   mt_get_gpio_pull_enable_base(idx),mt_get_gpio_dir_base(idx),mt_get_gpio_inversion_base(idx),mt_get_gpio_ies_base(idx)); 

    }
#endif
/*	len += snprintf(buf+len, bufLen-len, "%s","EXT GPIO\n");
    for (idx = MT_GPIO_EXT_START; idx < MT_GPIO_EXT_MAX; idx++) {
		len += snprintf(buf+len, bufLen-len, "%3d:%d%d%d%d%d%d%d%d\n",
		   idx,mt_get_gpio_mode_ext(idx), mt_get_gpio_pull_select_ext(idx), mt_get_gpio_in_ext(idx),mt_get_gpio_out_ext(idx),
		   mt_get_gpio_pull_enable_ext(idx),mt_get_gpio_dir_ext(idx),mt_get_gpio_inversion_ext(idx),mt_get_gpio_ies_ext(idx)); 
    }*/
    return len;
}
/*---------------------------------------------------------------------------*/
ssize_t mt_gpio_show_pin(struct device* dev, 
                                struct device_attribute *attr, char *buf)
{
    return mt_gpio_dump_regs(buf, PAGE_SIZE);
}
void mt_get_md_gpio_debug(char *str);
/*---------------------------------------------------------------------------*/
ssize_t mt_gpio_store_pin(struct device* dev, struct device_attribute *attr,  
                                 const char *buf, size_t count)
{
    int pin;
	int mode, pullsel, dout, pullen, dir, ies;
    u32 num,src,div;
	char md_str[128]="GPIO_MD_TEST";
    //struct mt_gpio_obj *obj = (struct mt_gpio_obj*)dev_get_drvdata(dev);    
    if (!strncmp(buf, "-h", 2)) {
        GPIOMSG("cat pin  #show all pin setting\n");
		GPIOMSG("echo -wmode num x > pin #num:pin,x:the mode 0~7\n");
		GPIOMSG("echo -wpsel num x > pin #x: 1,pull-up; 0,pull-down\n");
		GPIOMSG("echo -wdout num x > pin #x: 1,high; 0, low\n");
		GPIOMSG("echo -wpen num x > pin  #x: 1,pull enable; 0 pull disable\n");
		GPIOMSG("echo -wies num x > pin  #x: 1,ies enable; 0 ies disable\n");
		GPIOMSG("echo -wdir num x > pin  #x: 1, output; 0, input\n");
		/*GPIOMSG("echo -wdinv num x > pin #x: 1, inversion enable; 0, disable\n");*/
		GPIOMSG("echo -w=num x x x x x x > pin #set all property one time\n");
		GPIOMSG("PIN: [MODE] [PSEL] [DIN] [DOUT] [PEN] [DIR] [IES]\n");
    } else if (!strncmp(buf, "-r0", 3) && (1 == sscanf(buf+3, "%d", &pin))) {
        if (pin >= 0) {
            GPIO_CFG cfg = {.no = pin};
            /*if pmic*/
            mt_gpio_read_pin_base(&cfg, 0);
            GPIOMSG("%3d: %d %d %d %d %d %d\n", cfg.no, cfg.mode, cfg.pullsel, 
                cfg.din, cfg.dout, cfg.pullen, cfg.dir);
        }
    } else if (!strncmp(buf, "-r1", 3) && (1 == sscanf(buf+3, "%d", &pin))) {
        if (pin >= 0) {
            GPIO_CFG cfg = {.no = pin};
            mt_gpio_read_pin_base(&cfg, 1);
            GPIOMSG("%3d: %d %d %d %d %d %d %d\n", cfg.no, cfg.mode, cfg.pullsel, 
                cfg.din, cfg.dout, cfg.pullen, cfg.dir, cfg.ies);
        }
    } else if (!strncmp(buf, "-w", 2)) {
        buf += 2;
        if (!strncmp(buf, "mode", 4) && (2 == sscanf(buf+4, "%d %d", &pin, &mode)))
            GPIOMSG("set mode(%3d, %d)=%d\n", pin, mode, mt_set_gpio_mode(pin, mode));
        else if (!strncmp(buf, "psel", 4) && (2 == sscanf(buf+4, "%d %d", &pin, &pullsel)))
            GPIOMSG("set psel(%3d, %d)=%d\n", pin, pullsel, mt_set_gpio_pull_select(pin, pullsel));
        else if (!strncmp(buf, "dout", 4) && (2 == sscanf(buf+4, "%d %d", &pin, &dout)))
            GPIOMSG("set dout(%3d, %d)=%d\n", pin, dout, mt_set_gpio_out(pin, dout));
        else if (!strncmp(buf, "pen", 3) &&  (2 == sscanf(buf+3, "%d %d", &pin, &pullen)))
            GPIOMSG("set pen (%3d, %d)=%d\n", pin, pullen, mt_set_gpio_pull_enable(pin, pullen));
        else if (!strncmp(buf, "ies", 3) &&  (2 == sscanf(buf+3, "%d %d", &pin, &ies)))
            GPIOMSG("set ies (%3d, %d)=%d\n", pin, ies, mt_set_gpio_ies(pin, ies));
        else if (!strncmp(buf, "dir", 3) &&  (2 == sscanf(buf+3, "%d %d", &pin, &dir)))
            GPIOMSG("set dir (%3d, %d)=%d\n", pin, dir, mt_set_gpio_dir(pin, dir));
        /*else if (!strncmp(buf, "dinv", 4) && (2 == sscanf(buf+4, "%d %d", &pin, &dinv)))
            GPIOMSG("set dinv(%3d, %d)=%d\n", pin, dinv, mt_set_gpio_inversion(pin, dinv));     
        else if (8 == sscanf(buf, "=%d:%d %d %d %d %d %d %d", &pin, &mode, &pullsel, &din, &dout, &pullen, &dir, &dinv)) {*/ 
#ifdef CONFIG_MTK_FPGA
		else if (3 == sscanf(buf, "=%d:%d %d", &pin, &dout, &dir)) {
            GPIOMSG("set dout(%3d, %d)=%d\n", pin, dout, mt_set_gpio_out(pin, dout));
            GPIOMSG("set dir (%3d, %d)=%d\n", pin, dir, mt_set_gpio_dir(pin, dir));
#else
		else if (7 == sscanf(buf, "=%d:%d %d %d %d %d %d", &pin, &mode, &pullsel, &dout, &pullen, &dir, &ies)) {
            GPIOMSG("set mode(%3d, %d)=%d\n", pin, mode, mt_set_gpio_mode(pin, mode));
            GPIOMSG("set psel(%3d, %d)=%d\n", pin, pullsel, mt_set_gpio_pull_select(pin, pullsel));
            GPIOMSG("set dout(%3d, %d)=%d\n", pin, dout, mt_set_gpio_out(pin, dout));
            GPIOMSG("set pen (%3d, %d)=%d\n", pin, pullen, mt_set_gpio_pull_enable(pin, pullen));
            GPIOMSG("set dir (%3d, %d)=%d\n", pin, dir, mt_set_gpio_dir(pin, dir));
			GPIOMSG("set ies (%3d, %d)=%d\n", pin, ies, mt_set_gpio_ies(pin, dir));
            /*GPIOMSG("set dinv(%3d, %d)=%d\n", pin, dinv, mt_set_gpio_inversion(pin, dinv));    */  
#endif
        } else 
            GPIOMSG("invalid format: '%s'", buf);
    } else if (!strncmp(buf, "-t", 2)) {
        mt_gpio_self_test();
    } else if (!strncmp(buf, "-c", 2)) {
        mt_gpio_compare_base();
        //mt_gpio_compare_ext();
    } else if (!strncmp(buf, "-da", 3)) {
        mt_gpio_dump_addr_base();
        //mt_gpio_dump_addr_ext();
    } else if (!strncmp(buf, "-dp", 3)) {
		gpio_dump_regs();
    }else if (!strncmp(buf, "-d", 2)) {
        mt_gpio_dump();
    }else if (!strncmp(buf, "tt", 2)) {
		GPIOMSG("gpio reg test for next chip!\n");
		mt_reg_test();
    } else if (!strncmp(buf, "-md", 3)) {
	//	buf +=3;
	//	sscanf(buf,"%s",md_str);
	//	mt_get_md_gpio_debug(md_str);
    } else if (!strncmp(buf, "-k", 2)) {
        buf += 2;
        if (!strncmp(buf, "s", 1) && (3 == sscanf(buf+1, "%d %d %d", &num, &src, &div)))
            GPIOMSG("set num(%d, %d, %d)=%d\n", num, src, div, mt_set_clock_output(num, src,div));
	}else if(!strncmp(buf, "g", 1) && (1 == sscanf(buf+1, "%d", &num))){
	    //ret = mt_get_clock_output(num, &src,&div);
        //GPIOMSG("get num(%d, %d, %d)=%d\n", num, src, div,ret);
	}else {
            GPIOMSG("invalid format: '%s'", buf);
    }
    return count;    
}


/******************************************************************************
*MD convert gpio-name to gpio-number
*******************************************************************************/
struct mt_gpio_modem_info {
	char name[40];
	int num;
};

static struct mt_gpio_modem_info mt_gpio_info[]={
	{"GPIO_MD_TEST",800},
#ifdef GPIO_AST_CS_PIN
	{"GPIO_AST_HIF_CS",GPIO_AST_CS_PIN},
#endif
#ifdef GPIO_AST_CS_PIN_NCE
	{"GPIO_AST_HIF_CS_ID",GPIO_AST_CS_PIN_NCE},
#endif
#ifdef GPIO_AST_RST_PIN
	{"GPIO_AST_Reset",GPIO_AST_RST_PIN},
#endif
#ifdef GPIO_AST_CLK32K_PIN
	{"GPIO_AST_CLK_32K",GPIO_AST_CLK32K_PIN},
#endif
#ifdef GPIO_AST_CLK32K_PIN_CLK
	{"GPIO_AST_CLK_32K_CLKM",GPIO_AST_CLK32K_PIN_CLK},
#endif
#ifdef GPIO_AST_WAKEUP_PIN
	{"GPIO_AST_Wakeup",GPIO_AST_WAKEUP_PIN},
#endif
#ifdef GPIO_AST_INTR_PIN
	{"GPIO_AST_INT",GPIO_AST_INTR_PIN},
#endif
#ifdef GPIO_AST_WAKEUP_INTR_PIN
	{"GPIO_AST_WAKEUP_INT",GPIO_AST_WAKEUP_INTR_PIN},
#endif
#ifdef GPIO_AST_AFC_SWITCH_PIN
	{"GPIO_AST_AFC_Switch",GPIO_AST_AFC_SWITCH_PIN},
#endif
#ifdef GPIO_FDD_BAND_SUPPORT_DETECT_1ST_PIN
	{"GPIO_FDD_Band_Support_Detection_1",GPIO_FDD_BAND_SUPPORT_DETECT_1ST_PIN},
#endif
#ifdef GPIO_FDD_BAND_SUPPORT_DETECT_2ND_PIN
	{"GPIO_FDD_Band_Support_Detection_2",GPIO_FDD_BAND_SUPPORT_DETECT_2ND_PIN},
#endif
#ifdef GPIO_FDD_BAND_SUPPORT_DETECT_3RD_PIN
	{"GPIO_FDD_Band_Support_Detection_3",GPIO_FDD_BAND_SUPPORT_DETECT_3RD_PIN},
#endif
#ifdef GPIO_SIM_SWITCH_CLK_PIN
	{"GPIO_SIM_SWITCH_CLK",GPIO_SIM_SWITCH_CLK_PIN},
#endif
#ifdef GPIO_SIM_SWITCH_DAT_PIN
	{"GPIO_SIM_SWITCH_DAT",GPIO_SIM_SWITCH_DAT_PIN},
#endif
/*if you have new GPIO pin add bellow*/

};
int mt_get_md_gpio(char * gpio_name, int len)
{
	unsigned int i;
	unsigned long number;

	for (i = 0; i < ARRAY_SIZE(mt_gpio_info); i++)
	{
		if (!strncmp (gpio_name, mt_gpio_info[i].name, len))
		{
			number = mt_gpio_info[i].num;
			GPIOMSG("Modern get number=%d, name:%s\n", mt_gpio_info[i].num, gpio_name);
			mt_gpio_pin_decrypt(&number);
			return (number);
		}
	}
	GPIOERR("Modem gpio name can't match!!!\n");
	return -1;
}

void mt_get_md_gpio_debug(char * str)
{
	if(strcmp(str,"ALL")==0){
		int i;
		for(i=0;i<ARRAY_SIZE(mt_gpio_info);i++){
			GPIOMSG("GPIO number=%d,%s\n", mt_gpio_info[i].num, mt_gpio_info[i].name);
		}
	}else{
		GPIOMSG("GPIO number=%d,%s\n",mt_get_md_gpio(str,strlen(str)),str);
	}
	return;
}

