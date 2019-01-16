/******************************************************************************
 * mt_gpio_base.c - MTKLinux GPIO Device Driver
 * 
 * Copyright 2008-2009 MediaTek Co.,Ltd.
 * 
 * DESCRIPTION:
 *     This file provid the other drivers GPIO relative functions
 *
 ******************************************************************************/

#include <mach/sync_write.h>
#ifdef CONFIG_OF
#include <linux/of_address.h>
#else
#include <mach/mt_reg_base.h>
#endif
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_gpio_core.h>
#include <mach/mt_gpio_base.h>
#include <cust_gpio_usage.h>
#ifndef CONFIG_ARM64
#include <cust_gpio_suspend.h>
#endif
/******************************************************************************/
/*-------for special kpad pupd-----------*/
struct kpad_pupd {
	unsigned char 	pin;
	unsigned char   reg;
	unsigned char   bit;
};
static struct kpad_pupd kpad_pupd_spec[] = {
	{GPIO119,	0,	2},     //KROW0
	{GPIO120,	0,	6},     //KROW1
	{GPIO121,	0,	10},    //KROW2
	{GPIO122,	1,	2},     //KCOL0
	{GPIO123,	1,	6},     //KCOL1
	{GPIO124,	1,	10}     //KCOL2
};
/*-------for special sim pupd-----------*/
struct sim_pupd {
	unsigned char 	pin;
	unsigned char   reg;
	unsigned char   bit;
};
static struct sim_pupd sim_pupd_spec[] = {
	{GPIO17,	2,	2},       //sim1_clk
	{GPIO18,	2,	10},      //sim1_rst
	{GPIO19,	2,	6},       //sim1_io
	{GPIO20,	3,	2},       //sim2_clk
	{GPIO21,	3,	10},      //sim2_rst
	{GPIO22,	3,	6}        //sim2_io
};
/*---------------------------------------*/

struct mt_ies_smt_set{
	unsigned char	index_start;
	unsigned char	index_end;
	unsigned char	reg_index;
	unsigned char	bit;
};
static struct mt_ies_smt_set mt_ies_smt_map[] = {
	{GPIO0,		GPIO4,	 0,  1},
	{GPIO5,		GPIO9,	 0,  2},
	{GPIO16,	GPIO16,	 0,  2},
	{GPIO29,	GPIO32,	 0,  3},
	{GPIO33,	GPIO33,	 0,  4},
	{GPIO34,	GPIO36,	 0,  5},
	{GPIO37,	GPIO38,	 0,  6},
	{GPIO39,	GPIO39,  0,  7},
	{GPIO40,	GPIO40,	 0,  8},
	{GPIO41,	GPIO42,  0,  9},
	{GPIO10,	GPIO15,	 0, 10},
	{GPIO43,	GPIO46,	 0, 11},
	{GPIO92,	GPIO92,	 0, 13},
	{GPIO93,	GPIO95,  0, 14},
	{GPIO96,	GPIO99,  0, 15},
	{GPIO106,	GPIO107, 1,  0},
	{GPIO108,	GPIO112, 1,  1},
	{GPIO113,	GPIO116, 1,  2},
	{GPIO17,	GPIO19,  1,  3},
	{GPIO20,	GPIO22,  1,  4},
	{GPIO117,	GPIO118, 1,  5},   //bit21
	{GPIO119,	GPIO124, 1,  6},
	{GPIO125,	GPIO126, 1,  7},
	{GPIO136,	GPIO137, 1,  7},
	{GPIO129,	GPIO129, 1,  8},
	{GPIO132,	GPIO135, 1,  8},
	{GPIO130,	GPIO131, 1,  9},   //bit25
	{GPIO166,	GPIO169, 1, 14},   //bit30
	{GPIO176,	GPIO179, 1, 15},
	{GPIO180,	GPIO180, 2,  0},
	{GPIO181,	GPIO184, 2,  1},   //bit33
	{GPIO185,	GPIO191, 2,  2},
	{GPIO47,	GPIO61,  2,  3},
	{GPIO67,	GPIO67,  2,  3},
	{GPIO62,	GPIO66,  2,  4},
	{GPIO68,	GPIO72,  2,  5},   //bit37
	{GPIO73,	GPIO77,  2,  6},
	{GPIO78,	GPIO91,  2,  7},
	{GPIO192,	GPIO192, 2,  8},   //bit40
	{GPIO195,	GPIO196, 2,  8},   //bit40
	{GPIO193,	GPIO194, 2,  9}
};

struct mt_gpio_vbase gpio_vbase;
static GPIO_REGS *gpio_reg = NULL;
/*---------------------------------------------------------------------------*/
int mt_set_gpio_dir_base(unsigned long pin, unsigned long dir)
{
    unsigned long pos;
    unsigned long bit;
    GPIO_REGS *reg = gpio_reg;

    pos = pin / MAX_GPIO_REG_BITS;
    bit = pin % MAX_GPIO_REG_BITS;
    
    if (dir == GPIO_DIR_IN)
        GPIO_SET_BITS((1L << bit), &reg->dir[pos].rst);
    else
        GPIO_SET_BITS((1L << bit), &reg->dir[pos].set);
    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_dir_base(unsigned long pin)
{    
    unsigned long pos;
    unsigned long bit;
    unsigned long data;
    GPIO_REGS *reg = gpio_reg;

    pos = pin / MAX_GPIO_REG_BITS;
    bit = pin % MAX_GPIO_REG_BITS;
    
    data = GPIO_RD32(&reg->dir[pos].val);
    return (((data & (1L << bit)) != 0)? 1: 0);        
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_enable_base(unsigned long pin, unsigned long enable)
{
    unsigned long pos;
    unsigned long bit;
	unsigned long i;
    GPIO_REGS *reg = gpio_reg;

	/*for special kpad pupd, NOTE DEFINITION REVERSE!!!*/
	/*****************for special kpad pupd, NOTE DEFINITION REVERSE!!!*****************/
	for(i = 0; i < sizeof(kpad_pupd_spec)/sizeof(kpad_pupd_spec[0]); i++){
		if (pin == kpad_pupd_spec[i].pin){
			if (enable == GPIO_PULL_DISABLE){
				GPIO_SET_BITS((3L << (kpad_pupd_spec[i].bit-2)), &reg->kpad_ctrl[kpad_pupd_spec[i].reg].rst);
			} else {
				GPIO_SET_BITS((1L << (kpad_pupd_spec[i].bit-2)), &reg->kpad_ctrl[kpad_pupd_spec[i].reg].set);    //single key: 75K 
			}
			return RSUCCESS;
		}
	}
	/*********************************sim gpio pupd, sim-IO: pullUp 5K*********************************/
	for(i = 0; i < sizeof(sim_pupd_spec)/sizeof(sim_pupd_spec[0]); i++){
		if (pin == sim_pupd_spec[i].pin){
			if (enable == GPIO_PULL_DISABLE){
				GPIO_SET_BITS((3L << (sim_pupd_spec[i].bit-2)), &reg->sim_ctrl[sim_pupd_spec[i].reg].rst);
			} else {
				GPIO_SET_BITS((2L << (sim_pupd_spec[i].bit-2)), &reg->sim_ctrl[sim_pupd_spec[i].reg].set);       //5K: 2'b10
				GPIO_SET_BITS((1L << (sim_pupd_spec[i].bit-2)), &reg->sim_ctrl[sim_pupd_spec[i].reg].rst);
			}
			return RSUCCESS;
		}
	}
	/********************************* DPI special *********************************/
	if (pin == GPIO138) {       //dpi ck
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->dpi_ctrl[0].rst);
		} else {
			GPIO_SET_BITS(2L, &reg->dpi_ctrl[0].set);    //defeature to 50K, jerick
			GPIO_SET_BITS(1L, &reg->dpi_ctrl[0].rst);
		}
		return RSUCCESS;
	} else if ( (pin >= GPIO139) && (pin <= GPIO153) ) {
		if (enable == GPIO_PULL_ENABLE) {
			if (pin == GPIO139) {         //dpi DE
				GPIO_SET_BITS(1L, &reg->dpi_ctrl[1].set);
			} else if (pin == GPIO140) {  //dpi D0
				GPIO_SET_BITS(1L, &reg->dpi_ctrl[2].set);
			} else if (pin == GPIO141) {  //dpi D1
				GPIO_SET_BITS((1L<<2), &reg->dpi_ctrl[2].set);
			} else if (pin == GPIO142) {  //dpi D2
				GPIO_SET_BITS((1L<<4), &reg->dpi_ctrl[2].set);
			} else if (pin == GPIO143) {  //dpi D3
				GPIO_SET_BITS((1L<<6), &reg->dpi_ctrl[2].set);
			} else if (pin == GPIO144) {  //dpi D4
				GPIO_SET_BITS((1L<<4), &reg->dpi_ctrl[0].set);
			} else if (pin == GPIO145) {  //dpi D5
				GPIO_SET_BITS((1L<<6), &reg->dpi_ctrl[0].set);
			} else if (pin == GPIO146) {  //dpi D6
				GPIO_SET_BITS((1L<<6), &reg->dpi_ctrl[1].set);
			} else if (pin == GPIO147) {  //dpi D7
				GPIO_SET_BITS((1L<<5), &reg->pullen[13].set);
			} else if (pin == GPIO148) {  //dpi D8
				GPIO_SET_BITS((1L<<7), &reg->pullen[13].set);
			} else if (pin == GPIO149) {  //dpi D9
				GPIO_SET_BITS((1L<<9), &reg->pullen[13].set);
			} else if (pin == GPIO150) {  //dpi D10
				GPIO_SET_BITS((1L<<11), &reg->pullen[13].set);
			} else if (pin == GPIO151) {  //dpi D11
				GPIO_SET_BITS((1L<<13), &reg->pullen[13].set);
			} else if (pin == GPIO152) {  //dpi HSYNC
				GPIO_SET_BITS((1L<<2), &reg->dpi_ctrl[1].set);
			} else if (pin == GPIO153) {  //dpi VSYNC
				GPIO_SET_BITS((1L<<4), &reg->dpi_ctrl[1].set);
			}
			return RSUCCESS;
		} else {      //disable
			if (pin == GPIO139) {         //dpi DE
				GPIO_SET_BITS(1L, &reg->dpi_ctrl[1].rst);
			} else if (pin == GPIO140) {  //dpi D0
				GPIO_SET_BITS(1L, &reg->dpi_ctrl[2].rst);
			} else if (pin == GPIO141) {  //dpi D1
				GPIO_SET_BITS((1L<<2), &reg->dpi_ctrl[2].rst);
			} else if (pin == GPIO142) {  //dpi D2
				GPIO_SET_BITS((1L<<4), &reg->dpi_ctrl[2].rst);
			} else if (pin == GPIO143) {  //dpi D3
				GPIO_SET_BITS((1L<<6), &reg->dpi_ctrl[2].rst);
			} else if (pin == GPIO144) {  //dpi D4
				GPIO_SET_BITS((1L<<4), &reg->dpi_ctrl[0].rst);
			} else if (pin == GPIO145) {  //dpi D5
				GPIO_SET_BITS((1L<<6), &reg->dpi_ctrl[0].rst);
			} else if (pin == GPIO146) {  //dpi D6
				GPIO_SET_BITS((1L<<6), &reg->dpi_ctrl[1].rst);
			} else if (pin == GPIO147) {  //dpi D7
				GPIO_SET_BITS((1L<<5), &reg->pullen[13].rst);
			} else if (pin == GPIO148) {  //dpi D8
				GPIO_SET_BITS((1L<<7), &reg->pullen[13].rst);
			} else if (pin == GPIO149) {  //dpi D9
				GPIO_SET_BITS((1L<<9), &reg->pullen[13].rst);
			} else if (pin == GPIO150) {  //dpi D10
				GPIO_SET_BITS((1L<<11), &reg->pullen[13].rst);
			} else if (pin == GPIO151) {  //dpi D11
				GPIO_SET_BITS((1L<<13), &reg->pullen[13].rst);
			} else if (pin == GPIO152) {  //dpi HSYNC
				GPIO_SET_BITS((1L<<2), &reg->dpi_ctrl[1].rst);
			} else if (pin == GPIO153) {  //dpi VSYNC
				GPIO_SET_BITS((1L<<4), &reg->dpi_ctrl[1].rst);
			}
			return RSUCCESS;
		}
	}
	/********************************* MSDC special *********************************/
	if (pin == GPIO164) {         //ms0 DS
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc0_ctrl4.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc0_ctrl4.set);	//1L:10K
		}
		return RSUCCESS;
	} else if (pin == GPIO165) {  //ms0 RST
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc0_ctrl3.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc0_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO162) {  //ms0 cmd
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc0_ctrl1.rst) ;
		} else {
			GPIO_SET_BITS(1L, &reg->msdc0_ctrl1.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO163) {  //ms0 clk
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc0_ctrl0.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc0_ctrl0.set);
		}
		return RSUCCESS;
	} else if ( (pin >= GPIO154) && (pin <= GPIO161) ) {  //ms0 data0~7
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc0_ctrl2.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc0_ctrl2.set);
		}
		return RSUCCESS;
    //////////////////////////////////////////////////
	} else if (pin == GPIO170) {  //ms1 cmd
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc1_ctrl1.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc1_ctrl1.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO171) {  //ms1 dat0
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc1_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO172) {  //ms1 dat1
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 4), &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 4), &reg->msdc1_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO173) {  //ms1 dat2
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 8), &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 8), &reg->msdc1_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO174) {  //ms1 dat3
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 12), &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 12), &reg->msdc1_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO175) {  //ms1 clk
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc1_ctrl0.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc1_ctrl0.set);
		}
		return RSUCCESS;
	//////////////////////////////////////////////////
	} else if (pin == GPIO100) {  //ms2 dat0
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc2_ctrl3.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc2_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO101) {  //ms2 dat1
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 4), &reg->msdc2_ctrl3.rst) ;
		} else {
			GPIO_SET_BITS((1L << 4), &reg->msdc2_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO102) {  //ms2 dat2
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 8), &reg->msdc2_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 8), &reg->msdc2_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO103) {  //ms2 dat3
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 12), &reg->msdc2_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 12), &reg->msdc2_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO104) {  //ms2 clk 
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc2_ctrl0.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc2_ctrl0.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO105) {  //ms2 cmd
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc2_ctrl1.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc2_ctrl1.set);
		}
		return RSUCCESS;
	//////////////////////////////////////////////////
	} else if (pin == GPIO23) {  //ms3 dat0
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc3_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO24) {  //ms3 dat1
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 4), &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 4), &reg->msdc3_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO25) {  //ms3 dat2
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 8), &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 8), &reg->msdc3_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO26) {  //ms3 dat3
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS((3L << 12), &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 12), &reg->msdc3_ctrl3.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO27) {  //ms3 clk 
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc3_ctrl0.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc3_ctrl0.set);
		}
		return RSUCCESS;
	} else if (pin == GPIO28) {  //ms3 cmd
		if (enable == GPIO_PULL_DISABLE) {
			GPIO_SET_BITS(3L, &reg->msdc3_ctrl1.rst);
		} else {
			GPIO_SET_BITS(1L, &reg->msdc3_ctrl1.set);
		}
		return RSUCCESS;
	}

	if (0){
		return GPIO_PULL_EN_UNSUPPORTED;
	}else{
		pos = pin / MAX_GPIO_REG_BITS;
		bit = pin % MAX_GPIO_REG_BITS;

		if (enable == GPIO_PULL_DISABLE)
			GPIO_SET_BITS((1L << bit), &reg->pullen[pos].rst);
		else
			GPIO_SET_BITS((1L << bit), &reg->pullen[pos].set);
	}
    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_enable_base(unsigned long pin)
{
    unsigned long pos;
    unsigned long bit;
    unsigned long data;
    GPIO_REGS *reg = gpio_reg;
	unsigned long i;
	/*****************for special kpad pupd, NOTE DEFINITION REVERSE!!!*****************/
	for(i = 0; i < sizeof(kpad_pupd_spec)/sizeof(kpad_pupd_spec[0]); i++){
        if (pin == kpad_pupd_spec[i].pin){
			return (((GPIO_RD32(&reg->kpad_ctrl[kpad_pupd_spec[i].reg].val) & (3L << (kpad_pupd_spec[i].bit-2))) != 0)? 1: 0);        
        }
	}
	/*********************************for special sim pupd*********************************/
	for(i = 0; i < sizeof(sim_pupd_spec)/sizeof(sim_pupd_spec[0]); i++){
		if (pin == sim_pupd_spec[i].pin){
			return (((GPIO_RD32(&reg->sim_ctrl[sim_pupd_spec[i].reg].val) & (3L << (sim_pupd_spec[i].bit-2))) != 0)? 1: 0);        
		}
	}
	/*********************************DPI special*********************************/
	if (pin == GPIO138) {	        //dpi ck
		return (((GPIO_RD32(&reg->dpi_ctrl[0].val) & (3L << 0)) != 0)? 1: 0); 
	} else if (pin == GPIO139) {    //dpi DE
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 0)) != 0)? 1: 0); 
	} else if (pin == GPIO140) {	//dpi D0
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 0)) != 0)? 1: 0); 
	} else if (pin == GPIO141) {    //dpi D1
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 2)) != 0)? 1: 0); 
	} else if (pin == GPIO142) {    //dpi D2
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 4)) != 0)? 1: 0); 
	} else if (pin == GPIO143) {    //dpi D3
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 6)) != 0)? 1: 0); 
	} else if (pin == GPIO144) {    //dpi D4
		return (((GPIO_RD32(&reg->dpi_ctrl[0].val) & (1L << 4)) != 0)? 1: 0); 
	} else if (pin == GPIO145) {    //dpi D5
		return (((GPIO_RD32(&reg->dpi_ctrl[0].val) & (1L << 6)) != 0)? 1: 0); 
	} else if (pin == GPIO146) {    //dpi D6
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 6)) != 0)? 1: 0); 
	} else if (pin == GPIO147) {    //dpi D7
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 5)) != 0)? 1: 0); 
	} else if (pin == GPIO148) {    //dpi D8
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 7)) != 0)? 1: 0); 
	} else if (pin == GPIO149) {    //dpi D9
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 9)) != 0)? 1: 0); 
	} else if (pin == GPIO150) {    //dpi D10
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 11)) != 0)? 1: 0); 
	} else if (pin == GPIO151) {    //dpi D11
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 13)) != 0)? 1: 0); 
	} else if (pin == GPIO152) {    //dpi HSYNC
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 2)) != 0)? 1: 0); 
	} else if (pin == GPIO153) {    //dpi VSYNC
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 4)) != 0)? 1: 0); 
	}
    /*********************************MSDC special pupd*********************************/
	if (pin == GPIO164) {         //ms0 DS
        return (((GPIO_RD32(&reg->msdc0_ctrl4.val) & (3L << 0)) != 0)? 1: 0); 
	} else if (pin == GPIO165) {  //ms0 RST
        return (((GPIO_RD32(&reg->msdc0_ctrl3.val) & (3L << 0)) != 0)? 1: 0);  
	} else if (pin == GPIO162) {  //ms0 cmd
        return (((GPIO_RD32(&reg->msdc0_ctrl1.val) & (3L << 0)) != 0)? 1: 0);  
	} else if (pin == GPIO163) {  //ms0 clk
        return (((GPIO_RD32(&reg->msdc0_ctrl0.val) & (3L << 0)) != 0)? 1: 0);
	} else if ((pin >= GPIO154) && (pin <= GPIO161)) {	  //ms0 data0~7
        return (((GPIO_RD32(&reg->msdc0_ctrl2.val) & (3L << 0)) != 0)? 1: 0);
	
	} else if (pin == GPIO170) {  //ms1 cmd
        return (((GPIO_RD32(&reg->msdc1_ctrl1.val) & (3L << 0)) != 0)? 1: 0);       
	} else if (pin == GPIO171) {  //ms1 dat0
        return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (3L << 0)) != 0)? 1: 0);    
	} else if (pin == GPIO172) {  //ms1 dat1
        return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (3L << 4)) != 0)? 1: 0);        
	} else if (pin == GPIO173) {  //ms1 dat2
        return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (3L << 8)) != 0)? 1: 0);        
	} else if (pin == GPIO174) {  //ms1 dat3
        return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (3L << 12)) != 0)? 1: 0);        
	} else if (pin == GPIO175) {  //ms1 clk
        return (((GPIO_RD32(&reg->msdc1_ctrl0.val) & (3L << 0)) != 0)? 1: 0);        

	} else if (pin == GPIO100) {  //ms2 dat0
        return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (3L << 0)) != 0)? 1: 0);        
	} else if (pin == GPIO101) {  //ms2 dat1
        return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (3L << 4)) != 0)? 1: 0);        
	} else if (pin == GPIO102) {  //ms2 dat2
        return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (3L << 8)) != 0)? 1: 0);        
	} else if (pin == GPIO103) {  //ms2 dat3
        return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (3L << 12)) != 0)? 1: 0);        
	} else if (pin == GPIO104) {  //ms2 clk 
        return (((GPIO_RD32(&reg->msdc2_ctrl0.val) & (3L << 0)) != 0)? 1: 0);        
	} else if (pin == GPIO105) {  //ms2 cmd
        return (((GPIO_RD32(&reg->msdc2_ctrl1.val) & (3L << 0)) != 0)? 1: 0);        

	} else if (pin == GPIO23) {  //ms3 dat0
        return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (3L << 0)) != 0)? 1: 0);        
	} else if (pin == GPIO24) {  //ms3 dat1
        return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (3L << 4)) != 0)? 1: 0);        
	} else if (pin == GPIO25) {  //ms3 dat2
        return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (3L << 8)) != 0)? 1: 0);        
	} else if (pin == GPIO26) {  //ms3 dat3
        return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (3L << 12)) != 0)? 1: 0);        
	} else if (pin == GPIO27) {  //ms3 clk 
        return (((GPIO_RD32(&reg->msdc3_ctrl0.val) & (3L << 0)) != 0)? 1: 0);        
	} else if (pin == GPIO28) {  //ms3 cmd
        return (((GPIO_RD32(&reg->msdc3_ctrl1.val) & (3L << 0)) != 0)? 1: 0);        
	}

	if (0){
		return GPIO_PULL_EN_UNSUPPORTED;
	}else{
		pos = pin / MAX_GPIO_REG_BITS;
		bit = pin % MAX_GPIO_REG_BITS;
		data = GPIO_RD32(&reg->pullen[pos].val);
	}
    return (((data & (1L << bit)) != 0)? 1: 0);        
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_smt_base(unsigned long pin, unsigned long enable)
{
	/* compatible with HAL */
    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_smt_base(unsigned long pin)
{
	/* compatible with HAL */
    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_ies_base(unsigned long pin, unsigned long enable)
{
	int i = 0;
	GPIO_REGS *reg = gpio_reg;
	
	for(; i < ARRAY_SIZE(mt_ies_smt_map); i++) {
		if(pin >= mt_ies_smt_map[i].index_start && pin <= mt_ies_smt_map[i].index_end) 
			break;
	}

	if (i >= ARRAY_SIZE(mt_ies_smt_map)) {
		return -ERINVAL;
	}

	if (enable == GPIO_IES_DISABLE)
		GPIO_SET_BITS((1L << mt_ies_smt_map[i].bit), &reg->ies[mt_ies_smt_map[i].reg_index].rst);
	else
		GPIO_SET_BITS((1L << mt_ies_smt_map[i].bit), &reg->ies[mt_ies_smt_map[i].reg_index].set);

    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_ies_base(unsigned long pin)
{
	int i = 0;
	unsigned long data;
	GPIO_REGS *reg = gpio_reg;
	
	for(; i < ARRAY_SIZE(mt_ies_smt_map); i++) {
		if(pin >= mt_ies_smt_map[i].index_start && pin <= mt_ies_smt_map[i].index_end) 
			break;
	}	

	if (i >= ARRAY_SIZE(mt_ies_smt_map)) {
		return -ERINVAL;
	}
	
	data = GPIO_RD32(&reg->ies[mt_ies_smt_map[i].reg_index].val);
    return (((data & (1L << mt_ies_smt_map[i].bit)) != 0)? 1: 0);
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_pull_select_base(unsigned long pin, unsigned long select)
{
    unsigned long pos;
    unsigned long bit;
	unsigned long i;
    GPIO_REGS *reg = gpio_reg;

	/***********************for special kpad pupd, NOTE DEFINITION REVERSE!!!**************************/
	for(i = 0; i < sizeof(kpad_pupd_spec)/sizeof(kpad_pupd_spec[0]); i++){
		if (pin == kpad_pupd_spec[i].pin){
			if (select == GPIO_PULL_DOWN)
				GPIO_SET_BITS((1L << kpad_pupd_spec[i].bit), &reg->kpad_ctrl[kpad_pupd_spec[i].reg].set);
			else
				GPIO_SET_BITS((1L << kpad_pupd_spec[i].bit), &reg->kpad_ctrl[kpad_pupd_spec[i].reg].rst);
			return RSUCCESS;
		}
	}
	/*************************for special sim pupd*************************/
	for(i = 0; i < sizeof(sim_pupd_spec)/sizeof(sim_pupd_spec[0]); i++){
		if (pin == sim_pupd_spec[i].pin){
			if (select == GPIO_PULL_DOWN)
				GPIO_SET_BITS((1L << sim_pupd_spec[i].bit), &reg->sim_ctrl[sim_pupd_spec[i].reg].set);
			else
				GPIO_SET_BITS((1L << sim_pupd_spec[i].bit), &reg->sim_ctrl[sim_pupd_spec[i].reg].rst);
			return RSUCCESS;
		}
	}
	/* ************************DPI special *************************/
	if (pin == GPIO138) {	        //dpi ck
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<2), &reg->dpi_ctrl[0].set);
		} else {
			GPIO_SET_BITS( (1L<<2), &reg->dpi_ctrl[0].rst);
		}
	} else if (pin == GPIO139) {    //dpi DE
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<1), &reg->dpi_ctrl[1].set);
		} else {
			GPIO_SET_BITS( (1L<<1), &reg->dpi_ctrl[1].rst);
		}
	} else if (pin == GPIO140) {	//dpi D0
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<1), &reg->dpi_ctrl[2].set);
		} else {
			GPIO_SET_BITS( (1L<<1), &reg->dpi_ctrl[2].rst);
		}
	} else if (pin == GPIO141) {    //dpi D1
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<3), &reg->dpi_ctrl[2].set);
		} else {
			GPIO_SET_BITS( (1L<<3), &reg->dpi_ctrl[2].rst);
		}
	} else if (pin == GPIO142) {    //dpi D2
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<5), &reg->dpi_ctrl[2].set);
		} else {
			GPIO_SET_BITS( (1L<<5), &reg->dpi_ctrl[2].rst);
		}
	} else if (pin == GPIO143) {    //dpi D3
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<7), &reg->dpi_ctrl[2].set);
		} else {
			GPIO_SET_BITS( (1L<<7), &reg->dpi_ctrl[2].rst);
		}
	} else if (pin == GPIO144) {    //dpi D4
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<5), &reg->dpi_ctrl[0].set);
		} else {
			GPIO_SET_BITS( (1L<<5), &reg->dpi_ctrl[0].rst);
		}
	} else if (pin == GPIO145) {    //dpi D5
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<7), &reg->dpi_ctrl[0].set);
		} else {
			GPIO_SET_BITS( (1L<<7), &reg->dpi_ctrl[0].rst);
		}
	} else if (pin == GPIO146) {    //dpi D6
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<7), &reg->dpi_ctrl[1].set);
		} else {
			GPIO_SET_BITS( (1L<<7), &reg->dpi_ctrl[1].rst);
		}
	} else if (pin == GPIO147) {    //dpi D7
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<6), &reg->pullen[13].set);
		} else {
			GPIO_SET_BITS( (1L<<6), &reg->pullen[13].rst);
		}
	} else if (pin == GPIO148) {    //dpi D8
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<8), &reg->pullen[13].set);
		} else {
			GPIO_SET_BITS( (1L<<8), &reg->pullen[13].rst);
		}
	} else if (pin == GPIO149) {    //dpi D9
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<10), &reg->pullen[13].set);
		} else {
			GPIO_SET_BITS( (1L<<10), &reg->pullen[13].rst);
		}
	} else if (pin == GPIO150) {    //dpi D10
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<12), &reg->pullen[13].set);
		} else {
			GPIO_SET_BITS( (1L<<12), &reg->pullen[13].rst);
		}
	} else if (pin == GPIO151) {    //dpi D11
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<14), &reg->pullen[13].set);
		} else {
			GPIO_SET_BITS( (1L<<14), &reg->pullen[13].rst);
		}
	} else if (pin == GPIO152) {    //dpi HSYNC
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<3), &reg->dpi_ctrl[1].set);
		} else {
			GPIO_SET_BITS( (1L<<3), &reg->dpi_ctrl[1].rst);
		}
	} else if (pin == GPIO153) {    //dpi VSYNC
		if (select == GPIO_PULL_DOWN) {
			GPIO_SET_BITS( (1L<<5), &reg->dpi_ctrl[1].set);
		} else {
			GPIO_SET_BITS( (1L<<5), &reg->dpi_ctrl[1].rst);
		}
	}
	if ((pin >= GPIO138) && (pin <= GPIO153)) return RSUCCESS;
	
	/*************************************MSDC special pupd*************************/
	if (pin == GPIO164) {         //ms0 DS
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl4.rst);
		} else {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl4.set);
		}
	} else if (pin == GPIO165) {  //ms0 RST
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl3.set);
		}
	} else if (pin == GPIO162) {  //ms0 cmd
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl1.rst);
		} else {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl1.set);
		}
	} else if (pin == GPIO163) {  //ms0 clk
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl0.rst);
		} else {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl0.set);
		}
	} else if ( (pin >= GPIO154) && (pin <= GPIO161) ) {  //ms0 data0~7
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl2.rst);
		} else {
			GPIO_SET_BITS((1L<<2), &reg->msdc0_ctrl2.set);
		}

	} else if (pin == GPIO170) {   //ms1 cmd
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc1_ctrl1.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc1_ctrl1.set);
		}
	} else if (pin == GPIO171) {   //ms1 dat0
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc1_ctrl3.set);
		}
	} else if (pin == GPIO172) {   //ms1 dat1
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 6), &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 6), &reg->msdc1_ctrl3.set);
		}
	} else if (pin == GPIO173) {   //ms1 dat2
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 10), &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 10), &reg->msdc1_ctrl3.set);
		}
	} else if (pin == GPIO174) {   //ms1 dat3
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 14), &reg->msdc1_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 14), &reg->msdc1_ctrl3.set);
		}
	} else if (pin == GPIO175) {   //ms1 clk
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc1_ctrl0.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc1_ctrl0.set);
		}

	} else if (pin == GPIO100) {   //ms2 dat0
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc2_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc2_ctrl3.set);
		}
	} else if (pin == GPIO101) {   //ms2 dat1
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 6), &reg->msdc2_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 6), &reg->msdc2_ctrl3.set);
		}
	} else if (pin == GPIO102) {   //ms2 dat2
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 10), &reg->msdc2_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 10), &reg->msdc2_ctrl3.set);
		}
	} else if (pin == GPIO103) {   //ms2 dat3
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 14), &reg->msdc2_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 14), &reg->msdc2_ctrl3.set);
		}
	} else if (pin == GPIO104) {   //ms2 clk 
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc2_ctrl0.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc2_ctrl0.set);
		}
	} else if (pin == GPIO105) {   //ms2 cmd
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc2_ctrl1.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc2_ctrl1.set);
		}

	} else if (pin == GPIO23) {   //ms3 dat0
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc3_ctrl3.set);
		}
	} else if (pin == GPIO24) {   //ms3 dat1
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 6), &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 6), &reg->msdc3_ctrl3.set);
		}
	} else if (pin == GPIO25) {   //ms3 dat2
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 10), &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 10), &reg->msdc3_ctrl3.set);
		}
	} else if (pin == GPIO26) {   //ms3 dat3
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 14), &reg->msdc3_ctrl3.rst);
		} else {
			GPIO_SET_BITS((1L << 14), &reg->msdc3_ctrl3.set);
		}
	} else if (pin == GPIO27) {   //ms3 clk 
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc3_ctrl0.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc3_ctrl0.set);
		}
	} else if (pin == GPIO28) {   //ms3 cmd
		if (select == GPIO_PULL_UP) {
			GPIO_SET_BITS((1L << 2), &reg->msdc3_ctrl1.rst);
		} else {
			GPIO_SET_BITS((1L << 2), &reg->msdc3_ctrl1.set);
		}
	}

	if (0){
		return GPIO_PULL_EN_UNSUPPORTED;
	}else{
	pos = pin / MAX_GPIO_REG_BITS;
	bit = pin % MAX_GPIO_REG_BITS;
	
	if (select == GPIO_PULL_DOWN)
		GPIO_SET_BITS((1L << bit), &reg->pullsel[pos].rst);
	else
		GPIO_SET_BITS((1L << bit), &reg->pullsel[pos].set);
	}
    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_pull_select_base(unsigned long pin)
{
    unsigned long pos;
    unsigned long bit;
    unsigned long data;
	unsigned long i;
    GPIO_REGS *reg = gpio_reg;


	/*********************************for special kpad pupd*********************************/
	for(i = 0; i < sizeof(kpad_pupd_spec)/sizeof(kpad_pupd_spec[0]); i++){
		if (pin == kpad_pupd_spec[i].pin){
			data = GPIO_RD32(&reg->kpad_ctrl[kpad_pupd_spec[i].reg].val);
			return (((data & (1L << kpad_pupd_spec[i].bit)) != 0)? 0: 1);
		}
	}
	/*********************************for special sim pupd*********************************/
	for(i = 0; i < sizeof(sim_pupd_spec)/sizeof(sim_pupd_spec[0]); i++){
		if (pin == sim_pupd_spec[i].pin){
			data = GPIO_RD32(&reg->sim_ctrl[sim_pupd_spec[i].reg].val);
			return (((data & (1L << sim_pupd_spec[i].bit)) != 0)? 0: 1);
		}
	}
	/* ************************DPI special *************************/
	if (pin == GPIO138) {	        //dpi ck
		return (((GPIO_RD32(&reg->dpi_ctrl[0].val) & (1L << 2)) != 0)? 0: 1); 
	} else if (pin == GPIO139) {    //dpi DE
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 1)) != 0)? 0: 1); 
	} else if (pin == GPIO140) {	//dpi D0
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 1)) != 0)? 0: 1); 
	} else if (pin == GPIO141) {    //dpi D1
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 3)) != 0)? 0: 1); 
	} else if (pin == GPIO142) {    //dpi D2
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 5)) != 0)? 0: 1); 
	} else if (pin == GPIO143) {    //dpi D3
		return (((GPIO_RD32(&reg->dpi_ctrl[2].val) & (1L << 7)) != 0)? 0: 1); 
	} else if (pin == GPIO144) {    //dpi D4
		return (((GPIO_RD32(&reg->dpi_ctrl[0].val) & (1L << 5)) != 0)? 0: 1); 
	} else if (pin == GPIO145) {    //dpi D5
		return (((GPIO_RD32(&reg->dpi_ctrl[0].val) & (1L << 7)) != 0)? 0: 1); 
	} else if (pin == GPIO146) {    //dpi D6
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 7)) != 0)? 0: 1); 
	} else if (pin == GPIO147) {    //dpi D7
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 6)) != 0)? 0: 1); 
	} else if (pin == GPIO148) {    //dpi D8
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 8)) != 0)? 0: 1); 
	} else if (pin == GPIO149) {    //dpi D9
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 10)) != 0)? 0: 1); 
	} else if (pin == GPIO150) {    //dpi D10
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 12)) != 0)? 0: 1); 
	} else if (pin == GPIO151) {    //dpi D11
		return (((GPIO_RD32(&reg->pullen[13].val) & (1L << 14)) != 0)? 0: 1); 
	} else if (pin == GPIO152) {    //dpi HSYNC
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 3)) != 0)? 0: 1); 
	} else if (pin == GPIO153) {    //dpi VSYNC
		return (((GPIO_RD32(&reg->dpi_ctrl[1].val) & (1L << 5)) != 0)? 0: 1); 
	}
    /********************************* MSDC special pupd *********************************/
	if (pin == GPIO164) {         //ms0 DS
        return (((GPIO_RD32(&reg->msdc0_ctrl4.val) & (1L << 2)) != 0)? 0: 1); 
	} else if (pin == GPIO165) {  //ms0 RST
        return (((GPIO_RD32(&reg->msdc0_ctrl3.val) & (1L << 2)) != 0)? 0: 1);  
	} else if (pin == GPIO162) {  //ms0 cmd
        return (((GPIO_RD32(&reg->msdc0_ctrl1.val) & (1L << 2)) != 0)? 0: 1);  
	} else if (pin == GPIO163) {  //ms0 clk
        return (((GPIO_RD32(&reg->msdc0_ctrl0.val) & (1L << 2)) != 0)? 0: 1);
	} else if ((pin >= GPIO154) && (pin <= GPIO161)) {	  //ms0 data0~7
        return (((GPIO_RD32(&reg->msdc0_ctrl2.val) & (1L << 2)) != 0)? 0: 1);

	} else if (pin == GPIO170) {  //ms1 cmd
		return (((GPIO_RD32(&reg->msdc1_ctrl1.val) & (1L << 2)) == 0)? 0: 1);        
	} else if (pin == GPIO171) {  //ms1 dat0
		return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (1L << 2)) == 0)? 0: 1);        
	} else if (pin == GPIO172) {  //ms1 dat1
		return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (1L << 6)) == 0)? 0: 1);        
	} else if (pin == GPIO173) {  //ms1 dat2
		return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (1L << 10)) == 0)? 0: 1);        
	} else if (pin == GPIO174) {  //ms1 dat3
		return (((GPIO_RD32(&reg->msdc1_ctrl3.val) & (1L << 14)) == 0)? 0: 1);        
	} else if (pin == GPIO175) {  //ms1 clk
		return (((GPIO_RD32(&reg->msdc1_ctrl0.val) & (1L << 2)) == 0)? 0: 1);        

	} else if (pin == GPIO100) {  //ms2 dat0
		return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (1L << 2)) == 0)? 0: 1);        
	} else if (pin == GPIO101) {  //ms2 dat1
		return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (1L << 6)) == 0)? 0: 1);        
	} else if (pin == GPIO102) {  //ms2 dat2
		return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (1L << 10)) == 0)? 0: 1);        
	} else if (pin == GPIO103) {  //ms2 dat3
		return (((GPIO_RD32(&reg->msdc2_ctrl3.val) & (1L << 14)) == 0)? 0: 1);        
	} else if (pin == GPIO104) {  //ms2 clk 
		return (((GPIO_RD32(&reg->msdc2_ctrl0.val) & (1L << 2)) == 0)? 0: 1);        
	} else if (pin == GPIO105) {  //ms2 cmd
		return (((GPIO_RD32(&reg->msdc2_ctrl1.val) & (1L << 2)) == 0)? 0: 1);        

	} else if (pin == GPIO23) {  //ms3 dat0
		return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (1L << 2)) == 0)? 0: 1);        
	} else if (pin == GPIO24) {  //ms3 dat1
		return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (1L << 6)) == 0)? 0: 1);        
	} else if (pin == GPIO25) {  //ms3 dat2
		return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (1L << 10)) == 0)? 0: 1);        
	} else if (pin == GPIO26) {  //ms3 dat3
		return (((GPIO_RD32(&reg->msdc3_ctrl3.val) & (1L << 14)) == 0)? 0: 1);        
	} else if (pin == GPIO27) {  //ms3 clk 
		return (((GPIO_RD32(&reg->msdc3_ctrl0.val) & (1L << 2)) == 0)? 0: 1);        
	} else if (pin == GPIO28) {  //ms3 cmd
		return (((GPIO_RD32(&reg->msdc3_ctrl1.val) & (1L << 2)) == 0)? 0: 1);        
	}

	if (0){
		return GPIO_PULL_EN_UNSUPPORTED;
	}else{
		pos = pin / MAX_GPIO_REG_BITS;
		bit = pin % MAX_GPIO_REG_BITS;

		data = GPIO_RD32(&reg->pullsel[pos].val);
	}
    return (((data & (1L << bit)) != 0)? 1: 0);        
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_inversion_base(unsigned long pin, unsigned long enable)
{/*FIX-ME
   */
    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_inversion_base(unsigned long pin)
{/*FIX-ME*/
   	return 0;//FIX-ME
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_out_base(unsigned long pin, unsigned long output)
{
    unsigned long pos;
    unsigned long bit;
    GPIO_REGS *reg = gpio_reg;

    pos = pin / MAX_GPIO_REG_BITS;
    bit = pin % MAX_GPIO_REG_BITS;
    
    if (output == GPIO_OUT_ZERO)
        GPIO_SET_BITS((1L << bit), &reg->dout[pos].rst);
    else
        GPIO_SET_BITS((1L << bit), &reg->dout[pos].set);
    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_out_base(unsigned long pin)
{
    unsigned long pos;
    unsigned long bit;
    unsigned long data;
    GPIO_REGS *reg = gpio_reg;

    pos = pin / MAX_GPIO_REG_BITS;
    bit = pin % MAX_GPIO_REG_BITS;

    data = GPIO_RD32(&reg->dout[pos].val);
    return (((data & (1L << bit)) != 0)? 1: 0);        
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_in_base(unsigned long pin)
{
    unsigned long pos;
    unsigned long bit;
    unsigned long data;
    GPIO_REGS *reg = gpio_reg;

    pos = pin / MAX_GPIO_REG_BITS;
    bit = pin % MAX_GPIO_REG_BITS;

    data = GPIO_RD32(&reg->din[pos].val);
    return (((data & (1L << bit)) != 0)? 1: 0);        
}
/*---------------------------------------------------------------------------*/
int mt_set_gpio_mode_base(unsigned long pin, unsigned long mode)
{
    unsigned long pos;
    unsigned long bit;
    unsigned long data;
    unsigned long mask = (1L << GPIO_MODE_BITS) - 1;    
    GPIO_REGS *reg = gpio_reg;

	pos = pin / MAX_GPIO_MODE_PER_REG;
	bit = pin % MAX_GPIO_MODE_PER_REG;
   
	data = GPIO_RD32(&reg->mode[pos].val);

	data &= ~(mask << (GPIO_MODE_BITS*bit));
	data |= (mode << (GPIO_MODE_BITS*bit));
	
	GPIO_WR32(&reg->mode[pos].val, data);

    return RSUCCESS;
}
/*---------------------------------------------------------------------------*/
int mt_get_gpio_mode_base(unsigned long pin)
{
    unsigned long pos;
    unsigned long bit;
    unsigned long data;
    unsigned long mask = (1L << GPIO_MODE_BITS) - 1;    
    GPIO_REGS *reg = gpio_reg;

	pos = pin / MAX_GPIO_MODE_PER_REG;
	bit = pin % MAX_GPIO_MODE_PER_REG;

	data = GPIO_RD32(&reg->mode[pos].val);
	
	return ((data >> (GPIO_MODE_BITS*bit)) & mask);
}
/*---------------------------------------------------------------------------*/
void get_gpio_vbase(struct device_node *node)
{
	/* compatible with HAL */
	if(!(gpio_vbase.gpio_regs)) {
    	gpio_vbase.gpio_regs = of_iomap(node, 0);
    	if(!gpio_vbase.gpio_regs) {
        	GPIOERR("GPIO base addr is NULL\n");
        	return;
   		}
	    gpio_reg = (GPIO_REGS*)(GPIO_BASE_1);
        GPIOERR("GPIO base add is 0x%p\n",gpio_reg);
	}
    GPIOERR("GPIO base add is 0x%p\n",gpio_reg);
}
/*-----------------------User need GPIO APIs before GPIO probe------------------*/
extern struct device_node *get_gpio_np(void);
void get_gpio_vbase_early(struct device_node *node)
{
    //void __iomem *gpio_base = NULL;
    gpio_vbase.gpio_regs = NULL;
	struct device_node *np_gpio;
	np_gpio = get_gpio_np();
    /* Setup IO addresses */
    gpio_vbase.gpio_regs = of_iomap(np_gpio, 0);
    if(!gpio_vbase.gpio_regs) {
        GPIOERR("GPIO base addr is NULL\n");
        return;
    }
    gpio_reg = (GPIO_REGS*)(GPIO_BASE_1);
    GPIOERR("GPIO base add is 0x%p\n",gpio_reg);
}
postcore_initcall(get_gpio_vbase_early);
/*---------------------------------------------------------------------------*/
void get_io_cfg_vbase(void)
{
	/* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_MD32_SUPPORT
/*---------------------------------------------------------------------------*/
void md32_gpio_handle_init(void)
{
	/* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_MD32_SUPPORT*/
/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM 
/*---------------------------------------------------------------------------*/
void mt_gpio_suspend(void)
{
#ifdef CONFIG_ARM64
/*de-feature vcore suspend feature*/
#else
#if (1 == MTK_DUAL_VCORE_SUSPEND)
    unsigned long i;
    int dir, pullen, pullsel, out;
    unsigned long GPIO_SUSPEND_STATE[197] = {
        GPIO0_SLEEP_STATE,	 GPIO1_SLEEP_STATE,   GPIO2_SLEEP_STATE,   GPIO3_SLEEP_STATE,	GPIO4_SLEEP_STATE,
        GPIO5_SLEEP_STATE,	 GPIO6_SLEEP_STATE,   GPIO7_SLEEP_STATE,   GPIO8_SLEEP_STATE,	GPIO9_SLEEP_STATE,
        GPIO10_SLEEP_STATE,  GPIO11_SLEEP_STATE,  GPIO12_SLEEP_STATE,  GPIO13_SLEEP_STATE,	GPIO14_SLEEP_STATE,
        GPIO15_SLEEP_STATE,  GPIO16_SLEEP_STATE,  GPIO17_SLEEP_STATE,  GPIO18_SLEEP_STATE,	GPIO19_SLEEP_STATE,
        GPIO20_SLEEP_STATE,  GPIO21_SLEEP_STATE,  GPIO22_SLEEP_STATE,  GPIO23_SLEEP_STATE,	GPIO24_SLEEP_STATE,
        GPIO25_SLEEP_STATE,  GPIO26_SLEEP_STATE,  GPIO27_SLEEP_STATE,  GPIO28_SLEEP_STATE,	GPIO29_SLEEP_STATE,
        GPIO30_SLEEP_STATE,  GPIO31_SLEEP_STATE,  GPIO32_SLEEP_STATE,  GPIO33_SLEEP_STATE,	GPIO34_SLEEP_STATE,
        GPIO35_SLEEP_STATE,  GPIO36_SLEEP_STATE,  GPIO37_SLEEP_STATE,  GPIO38_SLEEP_STATE,	GPIO39_SLEEP_STATE,
        GPIO40_SLEEP_STATE,  GPIO41_SLEEP_STATE,  GPIO42_SLEEP_STATE,  GPIO43_SLEEP_STATE,	GPIO44_SLEEP_STATE,
        GPIO45_SLEEP_STATE,  GPIO46_SLEEP_STATE,  GPIO47_SLEEP_STATE,  GPIO48_SLEEP_STATE,	GPIO49_SLEEP_STATE,
        GPIO50_SLEEP_STATE,  GPIO51_SLEEP_STATE,  GPIO52_SLEEP_STATE,  GPIO53_SLEEP_STATE,	GPIO54_SLEEP_STATE,
        GPIO55_SLEEP_STATE,  GPIO56_SLEEP_STATE,  GPIO57_SLEEP_STATE,  GPIO58_SLEEP_STATE,	GPIO59_SLEEP_STATE,
        GPIO60_SLEEP_STATE,  GPIO61_SLEEP_STATE,  GPIO62_SLEEP_STATE,  GPIO63_SLEEP_STATE,	GPIO64_SLEEP_STATE,
        GPIO65_SLEEP_STATE,  GPIO66_SLEEP_STATE,  GPIO67_SLEEP_STATE,  GPIO68_SLEEP_STATE,	GPIO69_SLEEP_STATE,
        GPIO70_SLEEP_STATE,  GPIO71_SLEEP_STATE,  GPIO72_SLEEP_STATE,  GPIO73_SLEEP_STATE,	GPIO74_SLEEP_STATE,
        GPIO75_SLEEP_STATE,  GPIO76_SLEEP_STATE,  GPIO77_SLEEP_STATE,  GPIO78_SLEEP_STATE,	GPIO79_SLEEP_STATE,
        GPIO80_SLEEP_STATE,  GPIO81_SLEEP_STATE,  GPIO82_SLEEP_STATE,  GPIO83_SLEEP_STATE,	GPIO84_SLEEP_STATE,
        GPIO85_SLEEP_STATE,  GPIO86_SLEEP_STATE,  GPIO87_SLEEP_STATE,  GPIO88_SLEEP_STATE,	GPIO89_SLEEP_STATE,
        GPIO90_SLEEP_STATE,  GPIO91_SLEEP_STATE,  GPIO92_SLEEP_STATE,  GPIO93_SLEEP_STATE,	GPIO94_SLEEP_STATE,
        GPIO95_SLEEP_STATE,  GPIO96_SLEEP_STATE,  GPIO97_SLEEP_STATE,  GPIO98_SLEEP_STATE,	GPIO99_SLEEP_STATE,
        GPIO100_SLEEP_STATE, GPIO101_SLEEP_STATE, GPIO102_SLEEP_STATE, GPIO103_SLEEP_STATE, GPIO104_SLEEP_STATE,
        GPIO110_SLEEP_STATE, GPIO111_SLEEP_STATE, GPIO112_SLEEP_STATE, GPIO113_SLEEP_STATE, GPIO114_SLEEP_STATE,
        GPIO115_SLEEP_STATE, GPIO116_SLEEP_STATE, GPIO117_SLEEP_STATE, GPIO118_SLEEP_STATE, GPIO119_SLEEP_STATE,
        GPIO120_SLEEP_STATE, GPIO121_SLEEP_STATE, GPIO122_SLEEP_STATE, GPIO123_SLEEP_STATE, GPIO124_SLEEP_STATE,
        GPIO125_SLEEP_STATE, GPIO126_SLEEP_STATE, GPIO127_SLEEP_STATE, GPIO128_SLEEP_STATE, GPIO129_SLEEP_STATE,
        GPIO130_SLEEP_STATE, GPIO131_SLEEP_STATE, GPIO132_SLEEP_STATE, GPIO133_SLEEP_STATE, GPIO134_SLEEP_STATE,
        GPIO135_SLEEP_STATE, GPIO136_SLEEP_STATE, GPIO137_SLEEP_STATE, GPIO138_SLEEP_STATE, GPIO139_SLEEP_STATE,
        GPIO140_SLEEP_STATE, GPIO141_SLEEP_STATE, GPIO142_SLEEP_STATE, GPIO143_SLEEP_STATE, GPIO144_SLEEP_STATE,
        GPIO145_SLEEP_STATE, GPIO146_SLEEP_STATE, GPIO147_SLEEP_STATE, GPIO148_SLEEP_STATE, GPIO149_SLEEP_STATE,
        GPIO150_SLEEP_STATE, GPIO151_SLEEP_STATE, GPIO152_SLEEP_STATE, GPIO153_SLEEP_STATE, GPIO154_SLEEP_STATE,
        GPIO155_SLEEP_STATE, GPIO156_SLEEP_STATE, GPIO157_SLEEP_STATE, GPIO158_SLEEP_STATE, GPIO159_SLEEP_STATE,
        GPIO160_SLEEP_STATE, GPIO161_SLEEP_STATE, GPIO162_SLEEP_STATE, GPIO163_SLEEP_STATE, GPIO164_SLEEP_STATE,
        GPIO165_SLEEP_STATE, GPIO166_SLEEP_STATE, GPIO167_SLEEP_STATE, GPIO168_SLEEP_STATE, GPIO169_SLEEP_STATE,
        GPIO170_SLEEP_STATE, GPIO171_SLEEP_STATE, GPIO172_SLEEP_STATE, GPIO173_SLEEP_STATE, GPIO174_SLEEP_STATE,
        GPIO175_SLEEP_STATE, GPIO176_SLEEP_STATE, GPIO177_SLEEP_STATE, GPIO178_SLEEP_STATE, GPIO179_SLEEP_STATE,
        GPIO180_SLEEP_STATE, GPIO181_SLEEP_STATE, GPIO182_SLEEP_STATE, GPIO183_SLEEP_STATE, GPIO184_SLEEP_STATE,
        GPIO185_SLEEP_STATE, GPIO186_SLEEP_STATE, GPIO187_SLEEP_STATE, GPIO188_SLEEP_STATE, GPIO189_SLEEP_STATE,
        GPIO190_SLEEP_STATE, GPIO191_SLEEP_STATE, GPIO192_SLEEP_STATE, GPIO193_SLEEP_STATE, GPIO194_SLEEP_STATE,
        GPIO195_SLEEP_STATE, GPIO196_SLEEP_STATE
    };

    for(i=0; i<197; i++) {
        dir = mt_get_gpio_dir_base(i);
        pullen = mt_get_gpio_pull_enable_base(i);
        pullsel = mt_get_gpio_pull_select_base(i);
        out = mt_get_gpio_out_base(i);
        switch (GPIO_SUSPEND_STATE[i]) {
            case INPUT_PU:
                if ( (dir != GPIO_DIR_IN) || (pullen != GPIO_PULL_ENABLE) || (pullsel != GPIO_PULL_UP) ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;
            case INPUT_PD:
                if ( (dir != GPIO_DIR_IN) || (pullen != GPIO_PULL_ENABLE) || (pullsel != GPIO_PULL_DOWN) ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;
            case INPUT_NOPULL:
                if ( (dir != GPIO_DIR_IN) || (pullen != GPIO_PULL_DISABLE) ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;
            case OUTPUT_HIGH:
                if ( (dir != GPIO_DIR_OUT) || (out != GPIO_OUT_ZERO) ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;
            case OUTPUT_LOW:
                if ( (dir != GPIO_DIR_OUT) || (out != GPIO_OUT_ONE) ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;
            case INPUT_DONT_CARE:
                if ( dir != GPIO_DIR_IN ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;
            case OUTPUT_DONT_CARE_and_INPUT_PD:
                if ( (dir == GPIO_DIR_IN) && ( (pullen != GPIO_PULL_ENABLE) || (pullsel != GPIO_PULL_DOWN) ) ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;	
            case OUTPUT_DONT_CARE_and_INPUT_NOPULL:
                if ( (dir == GPIO_DIR_IN) && (pullen != GPIO_PULL_DISABLE) ) {
                    GPIOERR(" GPIO%d, dir=%d, pullen=%d, pullsel=%d, out=%d.\n", i, dir, pullen, pullsel, out);
                }
                break;	
            case DONT_CARE:
            default:
                break;
        }
    }
#endif
#endif
}
/*---------------------------------------------------------------------------*/
void mt_gpio_resume(void)
{
	/* compatible with HAL */
}
/*---------------------------------------------------------------------------*/
#endif /*CONFIG_PM*/
/*---------------------------------------------------------------------------*/

