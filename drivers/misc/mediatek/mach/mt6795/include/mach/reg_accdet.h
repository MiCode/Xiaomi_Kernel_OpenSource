#include <mach/mt_reg_base.h>
//Register address define
//#define PMIC_RESERVE_CON2 0xF0007500

#define ACCDET_BASE                 0x00000000
#define TOP_RST_ACCDET		     ACCDET_BASE + 0x0160
#define TOP_RST_ACCDET_SET 		 ACCDET_BASE + 0x0162
#define TOP_RST_ACCDET_CLR 		 ACCDET_BASE + 0x0164

#define INT_CON_ACCDET           ACCDET_BASE + 0x016C
#define INT_CON_ACCDET_SET		 ACCDET_BASE + 0x016E  //6331 Design
#define INT_CON_ACCDET_CLR       ACCDET_BASE + 0x0170

#define INT_STATUS_ACCDET        ACCDET_BASE + 0x017E

//6331 clock register
#define TOP_CKPDN           	 ACCDET_BASE + 0x0138
#define TOP_CKPDN_SET            ACCDET_BASE + 0x013A
#define TOP_CKPDN_CLR            ACCDET_BASE + 0x013C


#define ACCDET_RSV               ACCDET_BASE + 0x077A
#define ACCDET_CTRL              ACCDET_BASE + 0x077C
#define ACCDET_STATE_SWCTRL      ACCDET_BASE + 0x077E
#define ACCDET_PWM_WIDTH         ACCDET_BASE + 0x0780
#define ACCDET_PWM_THRESH        ACCDET_BASE + 0x0782
#define ACCDET_EN_DELAY_NUM      ACCDET_BASE + 0x0784
#define ACCDET_DEBOUNCE0         ACCDET_BASE + 0x0786
#define ACCDET_DEBOUNCE1         ACCDET_BASE + 0x0788
#define ACCDET_DEBOUNCE2         ACCDET_BASE + 0x078A
#define ACCDET_DEBOUNCE3         ACCDET_BASE + 0x078C
#define ACCDET_DEBOUNCE4         ACCDET_BASE + 0x078E
#define ACCDET_DEFAULT_STATE_RG  ACCDET_BASE + 0x0790
#define ACCDET_IRQ_STS           ACCDET_BASE + 0x0792
#define ACCDET_CONTROL_RG        ACCDET_BASE + 0x0794
#define ACCDET_STATE_RG          ACCDET_BASE + 0x0796
#define ACCDET_EINT_CTL			 ACCDET_BASE + 0x0798
#define ACCDET_EINT_PWM_DELAY	 ACCDET_BASE + 0x079A
#define ACCDET_TEST_DEBUG	     ACCDET_BASE + 0x079C
#define ACCDET_EINT_NV           ACCDET_BASE + 0x079E
#define ACCDET_NEGV              ACCDET_BASE + 0x07A2
#define ACCDET_CUR_DEB			 ACCDET_BASE + 0x07A4
#define ACCDET_EINT_CUR_DEB		 ACCDET_BASE + 0x07A6
#define ACCDET_RSV_CON0			 ACCDET_BASE + 0x07A8
#define ACCDET_RSV_CON1			 ACCDET_BASE + 0x07AA

#define ACCDET_AUXADC_CTL	     ACCDET_BASE + 0x072C
#define ACCDET_AUXADC_CTL_SET	 ACCDET_BASE + 0x072E
#define ACCDET_AUXADC_REG	     ACCDET_BASE + 0x070A
#define ACCDET_AUXADC_AUTO_SPL   ACCDET_BASE + 0x074E

#define ACCDET_ADC_REG 			 ACCDET_BASE + 0x069C

//Register value define

#define ACCDET_AUXADC_AUTO_SET   (1<<9)
#define ACCDET_DATA_READY        (1<<15)
#define ACCDET_CH_REQ_EN         (1<<5)
#define ACCDET_DATA_MASK         (0x0FFF)

#define ACCDET_POWER_MOD         (1<<13)
#define ACCDET_MIC1_ON           (1<<7)
#define ACCDET_BF_ON           	 (1<<10)
#define ACCDET_BF_OFF            (0<<10)
#define ACCDET_BF_MOD            (1<<11)
#define ACCDET_INPUT_MICP        (1<<3)
#define ACCDET_EINT_CON_EN       (1<<14)
#define ACCDET_NEGV_DT_EN        (1<<13)

#define ACCDET_CTRL_EN           (1<<0)
#define ACCDET_EINT_EN           (1<<3)
#define ACCDET_NEGV_EN           (1<<2)
#define ACCDET_EINT_PWM_IDLE     (1<<7)
#define ACCDET_MIC_PWM_IDLE      (1<<6)
#define ACCDET_VTH_PWM_IDLE      (1<<5)
#define ACCDET_CMP_PWM_IDLE      (1<<4)
#define ACCDET_EINT_PWM_EN       (1<<3)
#define ACCDET_CMP_EN            (1<<0)
#define ACCDET_VTH_EN            (1<<1)
#define ACCDET_MICBIA_EN         (1<<2)


#define ACCDET_ENABLE			 (1<<0)
#define ACCDET_DISABLE			 (0<<0)

#define ACCDET_RESET_SET          (1<<3)
#define ACCDET_RESET_CLR          (1<<3)

#define IRQ_CLR_BIT              0x100
#define IRQ_EINT_CLR_BIT         0x400
#define IRQ_NEGV_CLR_BIT         0x200
#define IRQ_STATUS_BIT           (1<<0)
#define EINT_IRQ_STATUS_BIT      (1<<2)
#define NEGV_IRQ_STATUS_BIT      (1<<1)
#define EINT_IRQ_DE_OUT          0x40
#define EINT_IRQ_DE_IN           0x60
#define EINT_IRQ_POL_HIGH        (1<<15)
#define EINT_IRQ_POL_LOW         (1<<15)


#define RG_ACCDET_IRQ_SET        (1<<10)
#define RG_ACCDET_IRQ_CLR        (1<<10)
#define RG_ACCDET_IRQ_STATUS_CLR (1<<10)

#define RG_ACCDET_EINT_IRQ_SET        (1<<11)
#define RG_ACCDET_EINT_IRQ_CLR        (1<<11)
#define RG_ACCDET_EINT_IRQ_STATUS_CLR (1<<10)
#define RG_ACCDET_EINT_HIGH 		  (1<<15)
#define RG_ACCDET_EINT_LOW            (0<<15)

#define RG_ACCDET_NEGV_IRQ_SET        (1<<12)
#define RG_ACCDET_NEGV_IRQ_CLR        (1<<12)
#define RG_ACCDET_NEGV_IRQ_STATUS_CLR (1<<12)

//CLOCK
#define RG_ACCDET_CLK_SET        (1<<2)
#define RG_ACCDET_CLK_CLR        (1<<2)


#define ACCDET_PWM_EN_SW         (1<<15)
#define ACCDET_MIC_EN_SW         (1<<14)
#define ACCDET_VTH_EN_SW         (1<<13)
#define ACCDET_CMP_EN_SW         (1<<12)

#define ACCDET_SWCTRL_EN         0x07

#define ACCDET_IN_SW             0x10

#define ACCDET_DE4               0x42 //2ms
/*
#define ACCDET_PWM_SEL_CMP       0x00
#define ACCDET_PWM_SEL_VTH       0x01
#define ACCDET_PWM_SEL_MIC       0x10
#define ACCDET_PWM_SEL_SW        0x11



#define ACCDET_TEST_MODE5_ACCDET_IN_GPI        (1<<5)
#define ACCDET_TEST_MODE4_ACCDET_IN_SW        (1<<4)
#define ACCDET_TEST_MODE3_MIC_SW        (1<<3)
#define ACCDET_TEST_MODE2_VTH_SW        (1<<2)
#define ACCDET_TEST_MODE1_CMP_SW        (1<<1)
#define ACCDET_TEST_MODE0_GPI        (1<<0)
//#define ACCDET_DEFVAL_SEL        (1<<15)
*/
//power mode and auxadc switch on/off
#define ACCDET_1V9_MODE_OFF   0x1A10
#define ACCDET_2V8_MODE_OFF   0x5A10
#define ACCDET_1V9_MODE_ON   0x1E10
#define ACCDET_2V8_MODE_ON   0x5A20
#define ACCDET_SWCTRL_IDLE_EN    (0x07<<4)
