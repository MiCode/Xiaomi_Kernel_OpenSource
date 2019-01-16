/* 
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Definitions for rpr410 als/ps sensor chip.
 */
#ifndef __RPR410_H__
#define __RPR410_H__

#include <linux/ioctl.h>

#define RPR410_SUCCESS						0
#define RPR410_ERR_I2C						-1
#define RPR410_ERR_STATUS					-3
#define RPR410_ERR_SETUP_FAILURE				-4
#define RPR410_ERR_GETGSENSORDATA			-5
#define RPR410_ERR_IDENTIFICATION			-6
#define RPR410_CHECKID_FAIL                             -7

/*******************************general definitions *******************************/
#define RPR410_DRIVER_VER ("1.0.0")
#define CALC_ERROR        (0x80000000)
#define SM_TIME_UNIT      (1000)
#define MN_TIME_UNIT      (1000000)
#define MASK_CHAR         (0xFF)
#define CLR_LOW2BIT       (0xFC)
#define CLR_LOW4BIT       (0xF0)
#define UNRELATEDNESS     (0xFF)
#define IRQ_NON_USE       (0)
#define IRQ_USE           (1)
#define LIGHT_SENSOR	(1)
#define PROX_SENSOR		(2)
#define MASK_LONG         (0xFFFFFFFF)


/************ definition parameter for als calculation ************/
#define COEFFICIENT               (4)
const unsigned long data0_coefficient[COEFFICIENT] = {192, 141, 127, 117};
const unsigned long data1_coefficient[COEFFICIENT] = {316, 108,  86,  74};
const unsigned long judge_coefficient[COEFFICIENT] = { 29,  65,  85, 158};


/************ define register for IC ************/
/* RPR410 REGSTER */
#define REG_SYSTEMCONTROL         (0x40)
#define REG_MODECONTROL           	(0x41)
#define REG_ALSPSCONTROL          	(0x42)
#define REG_PERSISTENCE          	 (0x43)
#define REG_PSDATA_LSB            	(0x44)
#define REG_PSDATA_MBS            	(0x45)
#define REG_ALSDATA0_LSB          	(0x46)
#define REG_ALSDATA0_MBS          	(0x47)
#define REG_ALSDATA1_LSB          	(0x48)
#define REG_ALSDATA1_MBS          (0x49)
#define REG_INTERRUPT             	(0x4A)
#define REG_PSTH_LSB             	 (0x4B)
#define REG_PSTH_MBS              	(0x4C)
#define REG_PSTL_LSB              	(0x4D)
#define REG_PSTL_MBS              	(0x4E)
#define REG_ALSDATA0TH_LSB       (0x4F)
#define REG_ALSDATA0TH_MBS       (0x50)
#define REG_ALSDATA0TL_LSB        (0x51)
#define REG_ALSDATA0TL_MBS        (0x52)
#define GEN_READ_MAX 			(19)

/************ define parameter for register ************/
/* REG_SYSTEMCONTROL(0x40) */
#define REG_SW_NOTRESET     (0 << 7)
#define REG_SW_RESET        (1 << 7)
#define REG_INT_NOTRESET    (0 << 6)
#define REG_INT_RESET       (1 << 6)

/* REG_MODECONTROL(0x41) */
#define NORMAL_MODE         (0 << 4)
#define LOW_NOISE_MODE      (1 << 4)
#define PWRON_STANDBY       (0)
#define PWRON_ONLY_PS       (3)
#define PWRON_ONLY_ALS      (5)
#define PWRON_PS_ALS        (6)
#define MEASUREMENT_MAX     (0xB)

/* REG_ALSPSCONTROL(0x42) */
#define LEDCURRENT_025MA    (0)
#define LEDCURRENT_050MA    (1)
#define LEDCURRENT_100MA    (2)
#define LEDCURRENT_200MA    (3)
#define ALSGAIN_X1X1        (0x0 << 2)
#define ALSGAIN_X1X2        (0x1 << 2)
#define ALSGAIN_X2X2        (0x5 << 2)
#define ALSGAIN_X64X64      (0xA << 2)
#define ALSGAIN_X128X64     (0xE << 2)
#define ALSGAIN_X128X128    (0xF << 2)
#define REG_ALSPSCTL_MAX    (0x3F)

/* REG_PERSIST(0x43) */
#define PERSISTENCE_MAX     (0x0F)

/* REG_INTERRUPT(0x4A) */
#define PS_INT_MASK		(1 << 7)
#define ALS_INT_MASK	(1 << 6)
#define PS_THH_ONLY         (0 << 4)
#define PS_THH_BOTH_HYS     (1 << 4)
#define PS_THH_BOTH_OUTSIDE (2 << 4)
#define POLA_ACTIVEL        (0 << 3)
#define POLA_INACTIVEL      (1 << 3)
#define OUTPUT_ANYTIME      (0 << 2)
#define OUTPUT_LATCH        (1 << 2)
#define MODE_NONUSE         (0)
#define MODE_PROXIMITY      (1)
#define MODE_ILLUMINANCE    (2)
#define MODE_BOTH           (3)
#define REG_INTERRUPT_MAX   (0x2F)

/* moved mode of ALS or PS  */
#define CTL_STANDBY         (0)
#define CTL_STANDALONE      (1)

/************ define parameter for init ************/
#define PS_ALS_SET_MODE_CONTROL   (NORMAL_MODE) //(LOW_NOISE_MODE) //(NORMAL_MODE)
#define PS_ALS_SET_ALSPS_CONTROL  (LEDCURRENT_050MA | ALSGAIN_X2X2) //(LEDCURRENT_200MA | ALSGAIN_X128X128)  //LEDCURRENT_200MA ALSGAIN_X1X1
#define PS_ALS_SET_INTR_PERSIST   (2) //(3)
#define PS_ALS_SET_INTR           (PS_THH_BOTH_OUTSIDE | POLA_ACTIVEL | OUTPUT_LATCH | MODE_PROXIMITY)
#define PS_ALS_SET_PS_TH          (90)	//(50)
#define PS_ALS_SET_PS_TL          (35)
#define PS_ALS_SET_ALS_TH         (2000) 	//Compare with ALS_DATA0. ALS_Data equals 0.192*ALS_DATA0 roughly. 
#define PS_ALS_SET_ALS_TL         (0x0000)
#define PS_ALS_SET_MIN_DELAY_TIME (125)

#define MODE_CTL_FACTOR (16)

static const struct MCTL_TABLE {
    short ALS;
    short PS;
}MCTL_TABLE[MODE_CTL_FACTOR] = {
    {  0,   0},   /*  0 */
    {  0,  10},   /*  1 */
    {  0,  40},   /*  2 */
    {  0, 100},   /*  3 */
    {  0, 400},   /*  4 */
    {100,   0},   /*  5 */
    {100, 100},   /*  6 */
    {100, 400},   /*  7 */
    {400,   0},   /*  8 */
    {400, 100},   /*  9 */
    {400,   0},   /* 10 */
    {400, 400},   /* 11 */
    {  0,   0},   /* 12 */
    {  0,   0},   /* 13 */
    {  0,   0},   /* 14 */
    {  0,   0}    /* 15 */
};

/* gain table */
#define GAIN_FACTOR (16)
static const struct GAIN_TABLE {
    char DATA0;
    char DATA1;
} GAIN_TABLE[GAIN_FACTOR] = {
    {  1,   1},   /*  0 */
    {  2,   1},   /*  1 */
    {  0,   0},   /*  2 */
    {  0,   0},   /*  3 */
    {  0,   0},   /*  4 */
    {  2,   2},   /*  5 */
    {  0,   0},   /*  6 */
    {  0,   0},   /*  7 */
    {  0,   0},   /*  8 */
    {  0,   0},   /*  9 */
    { 64,  64},   /* 10 */
    {  0,   0},   /* 11 */
    {  0,   0},   /* 12 */
    {  0,   0},   /* 13 */
    {128,  64},   /* 14 */
    {128, 128}    /* 15 */
};

/* structure to read data value from sensor */
typedef struct {
    unsigned short als_data0;        /* data value of ALS data0 from sensor      */
    unsigned short als_data1;        /* data value of ALS data1 from sensor      */
} READ_DATA_BUF;

typedef struct {
    unsigned char time;
    unsigned char gain;
    unsigned char led_current;
} DEVICE_VAL;

typedef struct {
    unsigned char als_state;         /* state value of ALS from sensor           */
    unsigned char ps_state;          /* state value of PS from sensor            */
} PWR_ST;


#endif
