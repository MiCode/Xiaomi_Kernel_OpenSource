/*
 * viatel_cbp_oem.c
 *
 * VIA CBP driver for Linux
 *
 * Copyright (C) 2011 VIA TELECOM Corporation, Inc.
 * Author: VIA TELECOM Corporation, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include "viatel.h"

#if defined(CONFIG_MACH_OMAP_KUNLUN)
int oem_gpio_request(int gpio, const char *label)
{
	return gpio_request(gpio, label);
}

void oem_gpio_free(int gpio)
{
    gpio_free(gpio);
}

/*config the gpio to be input for irq if the SOC need*/
int oem_gpio_direction_input_for_irq(int gpio)
{
	return gpio_direction_input(gpio);
}

int oem_gpio_direction_output(int gpio, int value)
{
	return gpio_direction_output(gpio, value);
}

/* 
 * Get the output level if the gpio is output type; 
 * Get the input level if the gpio is input type
 */
int oem_gpio_get_value(int gpio)
{
	return gpio_get_value(gpio);
}

int oem_gpio_to_irq(int gpio)
{
	return gpio_to_irq(gpio);
}

/* 
 * Set the irq type of the pin. 
 * Get the pin level and set the correct edge if the type is both edge and 
 * the SOC do not support both edge detection at one time
 */
int oem_gpio_set_irq_type(unsigned gpio, unsigned int type)
{
    return set_irq_type(oem_gpio_to_irq(gpio), type);
}


int oem_gpio_request_irq(int gpio, irq_handler_t handler, unsigned long flags,
	    const char *name, void *dev)
{
    return request_irq(oem_gpio_to_irq(gpio), handler, flags, name, dev);
}

void oem_gpio_irq_mask(int gpio)
{
    return ;
}

void oem_gpio_irq_unmask(int gpio)
{
    return ;
}
#endif

#if defined(CONFIG_SOC_JZ4770)
int oem_gpio_request(int gpio, const char *label)
{
	return gpio_request(gpio, label);
}

void oem_gpio_free(int gpio)
{
    gpio_free(gpio);
}

/*config the gpio to be input for irq if the SOC need*/
int oem_gpio_direction_input_for_irq(int gpio)
{
	return 0;
}

int oem_gpio_direction_output(int gpio, int value)
{
       return gpio_direction_output(gpio, value);
}

/* 
 * Get the output level if the gpio is output type; 
 * Get the input level if the gpio is input type
 */
int oem_gpio_get_value(int gpio)
{
	return gpio_get_value(gpio);
}

int oem_gpio_to_irq(int gpio)
{
	return gpio_to_irq(gpio);
}

#define GPIO_DEBOUNCE (3)
int read_gpio_pin(int pin)
{
	int t, v;
	int i;

	i = GPIO_DEBOUNCE;

	v = t = 0;

	while (i--) {
		t = __gpio_get_pin(pin);
		if (v != t)
			i = GPIO_DEBOUNCE;

		v = t;
		ndelay(100);
	}

	return v;
}

int oem_gpio_set_irq_type(int gpio, unsigned int type)
{
       if(type == IRQ_TYPE_EDGE_BOTH){
            if(read_gpio_pin(gpio)){
                type = IRQ_TYPE_EDGE_FALLING;
            }else{
                type = IRQ_TYPE_EDGE_RISING;
            }
       }

       if(type == IRQ_TYPE_LEVEL_MASK){
            if(read_gpio_pin(gpio)){
                type = IRQ_TYPE_LEVEL_LOW;
            }else{
                type = IRQ_TYPE_LEVEL_HIGH;
            }
       }

       switch(type){
            case IRQ_TYPE_EDGE_RISING:
                __gpio_as_irq_rise_edge(gpio);
                break;
            case IRQ_TYPE_EDGE_FALLING:
                __gpio_as_irq_fall_edge(gpio);
                break;
            case IRQ_TYPE_LEVEL_HIGH:
                __gpio_as_irq_high_level(gpio);
                break;
            case IRQ_TYPE_LEVEL_LOW:
                __gpio_as_irq_low_level(gpio);
                break;
            default:
                return -EINVAL;
       }
 
       return 0;
}

int oem_gpio_request_irq(int gpio, irq_handler_t handler, unsigned long flags,
	    const char *name, void *dev)
{
    int ret = request_irq(oem_gpio_to_irq(gpio), handler, flags, name, dev);
    enable_irq_wake(oem_gpio_to_irq(gpio));
    return ret;
}

void oem_gpio_irq_mask(int gpio)
{
    return ;
}

void oem_gpio_irq_unmask(int gpio)
{
    return ;
}
#endif

//#if defined(EVDO_DT_SUPPORT)
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <cust_eint.h>


int oem_gpio_request(int gpio, const char *label)
{
    return 0;
}

void oem_gpio_free(int gpio)
{
    return ;
}

/*config the gpio to be input for irq if the SOC need*/
int oem_gpio_direction_input_for_irq(int gpio)
{
		switch(gpio)
		{
			case GPIO_VIATEL_MDM_RST_IND:
				mt_set_gpio_mode(gpio, GPIO_VIA_MDM_RST_IND_M_EINT);
				break;
			case GPIO_VIATEL_SDIO_DATA_ACK:
				mt_set_gpio_mode(gpio, GPIO_VIA_SDIO_ACK_M_EINT);
				break;
			case GPIO_VIATEL_SDIO_FLOW_CTRL:
				mt_set_gpio_mode(gpio, GPIO_VIA_FLOW_CTRL_M_EINT);
				break;		
			case GPIO_VIATEL_SDIO_MDM_WAKE_AP:
				mt_set_gpio_mode(gpio, GPIO_VIA_MDM_WAKE_AP_M_EINT);
				break;
			case GPIO_VIATEL_SDIO_MDM_RDY:
				mt_set_gpio_mode(gpio, GPIO_VIA_MDM_RDY_M_EINT);
				break;
		}
    mt_set_gpio_dir(gpio, GPIO_DIR_IN);
    return 0;
}

int oem_gpio_direction_output(int gpio, int value)
{
    mt_set_gpio_mode(gpio, GPIO_MODE_GPIO);
    mt_set_gpio_dir(gpio, GPIO_DIR_OUT);
    mt_set_gpio_out(gpio, !!value);
    return 0;
}

int oem_gpio_get_value(int gpio)
{
    if(GPIO_DIR_IN == mt_get_gpio_dir(gpio)){
        return mt_get_gpio_in(gpio);
    }else{
        return mt_get_gpio_out(gpio);
    }
}

typedef struct mtk_oem_gpio_des{
    int gpio;
    int irq;
	unsigned int deb_en;
	unsigned int pol;
    void (*redirect)(void);
    irq_handler_t handle;
    void *data;
}mtk_oem_gpio_des;

static void gpio_irq_handle_sdio_mdm_rdy(void);
static void gpio_irq_handle_sdio_mdm_wake_ap(void);
static void gpio_irq_handle_rst_ind(void);
static void gpio_irq_handle_data_ack(void);
static void gpio_irq_handle_flow_crtl(void);

mtk_oem_gpio_des oem_gpio_list[] = {
    {GPIO_VIATEL_SDIO_MDM_RDY, CUST_EINT_EVDO_DT_EXT_MDM_RDY_NUM, 
	CUST_EINT_EVDO_DT_EXT_MDM_RDY_DEBOUNCE_EN,2,
	gpio_irq_handle_sdio_mdm_rdy, NULL, NULL},
    {GPIO_VIATEL_SDIO_MDM_WAKE_AP, CUST_EINT_EVDO_DT_EXT_MDM_WAKE_AP_NUM, 
    CUST_EINT_EVDO_DT_EXT_MDM_WAKE_AP_DEBOUNCE_EN,2,
    gpio_irq_handle_sdio_mdm_wake_ap, NULL, NULL},
    {GPIO_VIATEL_MDM_RST_IND,  CUST_EINT_EVDO_DT_EXT_MDM_RST_IND_NUM, 
	CUST_EINT_EVDO_DT_EXT_MDM_RST_IND_DEBOUNCE_EN,2,
	gpio_irq_handle_rst_ind, NULL, NULL},
    {GPIO_VIATEL_SDIO_DATA_ACK, CUST_EINT_EVDO_DT_EXT_MDM_ACK_NUM, 
	CUST_EINT_EVDO_DT_EXT_MDM_ACK_DEBOUNCE_EN, 2,
	gpio_irq_handle_data_ack, NULL, NULL},
    {GPIO_VIATEL_SDIO_FLOW_CTRL, CUST_EINT_EVDO_DT_EXT_MDM_FLOW_CTRL_NUM, 
	CUST_EINT_EVDO_DT_EXT_MDM_FLOW_CTRL_DEBOUNCE_EN, 1,
	gpio_irq_handle_flow_crtl, NULL, NULL},
};

static mtk_oem_gpio_des* gpio_des_find_by_gpio(int gpio)
{
    int i = 0;
    mtk_oem_gpio_des *des = NULL;

    /*if(gpio < 0){
        return NULL;
    }*/
    
    for(i=0; i < sizeof(oem_gpio_list) / sizeof(mtk_oem_gpio_des); i++){
        des = oem_gpio_list + i;
        if(des->gpio == gpio){
            return des;
        }
    }

    return NULL;
}

static mtk_oem_gpio_des* gpio_des_find_by_irq(int irq)
{
    int i = 0;
    mtk_oem_gpio_des *des = NULL;

    for(i=0; i < sizeof(oem_gpio_list) / sizeof(mtk_oem_gpio_des); i++){
        des = oem_gpio_list + i;
        if(des->irq == irq){
            return des;
        }
    }

    return NULL;
}
static void gpio_irq_handle_sdio_mdm_rdy(void)
{
    mtk_oem_gpio_des *des = NULL;

    des = gpio_des_find_by_gpio(GPIO_VIATEL_SDIO_MDM_RDY);
    if(des && des->handle){
        des->handle(des->irq, des->data);
    }
}
static void gpio_irq_handle_sdio_mdm_wake_ap(void)
{
    mtk_oem_gpio_des *des = NULL;

    des = gpio_des_find_by_gpio(GPIO_VIATEL_SDIO_MDM_WAKE_AP);
    if(des && des->handle){
        des->handle(des->irq, des->data);
    }
}

static void gpio_irq_handle_rst_ind(void)
{
    mtk_oem_gpio_des *des = NULL;

    des = gpio_des_find_by_gpio(GPIO_VIATEL_MDM_RST_IND);
    if(des && des->handle){
        des->handle(des->irq, des->data);
    }
}
static void gpio_irq_handle_data_ack(void)
{
    mtk_oem_gpio_des *des = NULL;
    
    des = gpio_des_find_by_gpio(GPIO_VIATEL_SDIO_DATA_ACK);
    if(des && des->handle){
        des->handle(des->irq, des->data);
    }
}

static void gpio_irq_handle_flow_crtl(void)
{
    mtk_oem_gpio_des *des = NULL;
	
    des = gpio_des_find_by_gpio(GPIO_VIATEL_SDIO_FLOW_CTRL);
    if(des && des->handle){
        des->handle(des->irq, des->data);
    }
}

int oem_gpio_to_irq(int gpio)
{
    mtk_oem_gpio_des *des = NULL;

    des = gpio_des_find_by_gpio(gpio);
    if(NULL == des){
        printk("%s: no irq for gpio %d\n", __FUNCTION__, gpio);
        return -1;
    }else{
        return des->irq;
    }
}

int oem_irq_to_gpio(int irq)
{
    mtk_oem_gpio_des *des = NULL;

    des = gpio_des_find_by_irq(irq);
    if(NULL == des){
        printk("%s: no gpio for irq %d\n", __FUNCTION__, irq);
        return -1;
    }else{
        return des->gpio;
    }
}

int oem_gpio_set_irq_type(int gpio, unsigned int type)
{
    int irq, level;

    irq = oem_gpio_to_irq(gpio);
    if(irq < 0){
        return irq;
    }
   
    level = oem_gpio_get_value(gpio);
 
    if(type == IRQ_TYPE_EDGE_BOTH){
        if(level){
            type = IRQ_TYPE_EDGE_FALLING;
        }else{
            type = IRQ_TYPE_EDGE_RISING;
        }
    }

    if(type == IRQ_TYPE_LEVEL_MASK){
        if(level){
            type = IRQ_TYPE_LEVEL_LOW;
        }else{
            type = IRQ_TYPE_LEVEL_HIGH;
        }
    }

    mt_eint_set_hw_debounce(irq, 0);
    switch(type){
        case IRQ_TYPE_EDGE_RISING:
            mt_eint_set_sens(irq, MT_EDGE_SENSITIVE);
            mt_eint_set_polarity(irq, MT_POLARITY_HIGH);
            break;
        case IRQ_TYPE_EDGE_FALLING:
            mt_eint_set_sens(irq, MT_EDGE_SENSITIVE);
            mt_eint_set_polarity(irq, MT_POLARITY_LOW);
            break;
        case IRQ_TYPE_LEVEL_HIGH:
            mt_eint_set_sens(irq, MT_LEVEL_SENSITIVE);
            mt_eint_set_polarity(irq, MT_POLARITY_HIGH);
            break;
        case IRQ_TYPE_LEVEL_LOW:
            mt_eint_set_sens(irq, MT_LEVEL_SENSITIVE);
            mt_eint_set_polarity(irq, MT_POLARITY_LOW);
            break;
        default:
            return -EINVAL;
   }
 
   return 0;
}

int oem_gpio_request_irq(int gpio, irq_handler_t handler, unsigned long flags,
	    const char *name, void *dev)
{
    mtk_oem_gpio_des *des = NULL;

    des = gpio_des_find_by_gpio(gpio);
    if(des == NULL){
        return -1;
    }
    des->data = dev;
    des->handle = handler;

    //mt_eint_registration(des->irq, des->deb_en, des->pol, des->redirect, 0);
    mt_eint_registration(des->irq, des->pol, des->redirect, 0);
	
    return 0;
}

void oem_gpio_irq_mask(int gpio)
{
    int irq;

    irq = oem_gpio_to_irq(gpio);
    if(irq < 0){
        return ;
    }

    mt_eint_mask(irq);
}

void oem_gpio_irq_unmask(int gpio)
{
    int irq;

    irq = oem_gpio_to_irq(gpio);
    if(irq < 0){
        return ;
    }

    mt_eint_unmask(irq);
}
//#endif
