#ifndef _MT_GPIO_AFFIX_H_
#define _MT_GPIO_AFFIX_H_

/******************************************************************************
* Enumeration for Clock output
******************************************************************************/
/*CLOCK OUT*/
/*
typedef enum {
    CLK_OUT_UNSUPPORTED = -1,
    CLK_OUT0,
    CLK_OUT1,
    CLK_OUT2,
    CLK_OUT3,
    CLK_OUT4,
    CLK_OUT5,
    CLK_OUT6,
    CLK_MAX	
}GPIO_CLKOUT;
typedef enum {
    CLKM_UNSUPPORTED = -1,
    CLKM0,
    CLKM1,
    CLKM2,
    CLKM3,
    CLKM4,
    CLKM5,
    CLKM6,
}GPIO_CLKM;
typedef enum CLK_SRC
{
    CLK_SRC_UNSUPPORTED = -1,	
    CLK_SRC_GATE 	= 0x0,
    CLK_SRC_SYS_26M,
    CLK_SRC_FRTC,
    CLK_SRC_WHPLL_250P25M,
    CLK_SRC_WPLL_245P76M,
    CLK_SRC_MDPLL2_416,
    CLK_SRC_MDPLL1_416,
    CLK_SRC_MCUPLL2_H481M,
    CLK_SRC_MCUPLL1_H481M,
    CLK_SRC_MSDC_H208M,
    CLK_SRC_ISP_208M,
    CLK_SRC_LVDS_H180M,
    CLK_SRC_TVHDMI_H,
    CLK_SRC_UPLL_178P3M,
    CLK_SRC_MAIN_H230P3M,
    CLK_SRC_MM_DIV7,

    CLK_SRC_MAX
}GPIO_CLKSRC;

//clock output setting
int mt_set_clock_output(unsigned long num, unsigned long src, unsigned long div);
int mt_get_clock_output(unsigned long num, unsigned long *src, unsigned long *div);
*/

/*For MD GPIO customization only, can be called by CCCI driver*/
int mt_get_md_gpio(char * gpio_name, int len);

#endif
