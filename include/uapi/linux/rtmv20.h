#ifndef __RTMV20_H__
#define __RTMV20_H__

#define RTMV20_NAME "rtmv20"

#define RTMV_IOC_MAGIC 'Q'
#define RTMV_SL_PRIVATE    168

#define REG_DEVINFO             (0x00)
#define REG_PULSE_DLY1          (0x01)
#define REG_PULSE_DLY2          (0x02)
#define REG_PULSE_WIDTH1        (0x03)
#define REG_PULSE_WIDTH2        (0x04)
#define REG_LD_CTRL1            (0x05)
#define REG_ES_PULSE_WIDTH1     (0x06)
#define REG_ES_PULSE_WIDTH2     (0x07)
#define REG_ES_LD_CTRL1         (0x08)
#define REG_PULSE_REPT          (0x09)
#define REG_LBP                 (0x0A)
#define REG_LD_CTRL2            (0x0B)
#define REG_LD_CTRL3            (0x0C)
#define REG_FSIN1_CTRL1         (0x0D)
#define REG_FSIN1_CTRL2         (0x0E)
#define REG_FSIN1_CTRL3         (0x0F)
#define REG_FSIN2_CTRL1         (0x10)
#define REG_FSIN2_CTRL2         (0x11)
#define REG_FSIN2_CTRL3         (0x12)
#define REG_EN_CTRL             (0x13)
#define REG_FPS_CTRL            (0x14)
#define REG_ES_FPS_CTRL         (0x15)
#define REG_ADC_CTRL            (0x16)
#define REG_ADC_CTRL1           (0x17)
#define REG_VOUT_ADC1           (0x18)
#define REG_VOUT_ADC2           (0x19)
#define REG_VLD_ADC1            (0x1A)
#define REG_VLD_ADC2            (0x1B)
#define REG_VTS_ADC1            (0x1C)
#define REG_VTS_ADC2            (0x1D)
#define REG_ILD_ADC1            (0x1E)
#define REG_ILD_ADC2            (0x1F)
#define REG_VSYN1               (0x20)
#define REG_VSYN2               (0x21)
#define REG_STRB1               (0x22)
#define REG_STRB2               (0x23)
#define REG_INITIAL1            (0x24)
#define REG_INITIAL2            (0x25)
#define REG_VSYN_STRB1          (0x26)
#define REG_VSYN_STRB2          (0x27)
#define REG_STRB_VSYN1          (0x28)
#define REG_STRB_VSYN2          (0x29)
#define REG_LD_IRQ              (0x30)
#define REG_LD_STAT             (0x40)
#define REG_LD_MASK             (0x50)
#define REG_MAX                 (0x50)


struct rtmv20_platform_data {
	int enable_gpio;
	int ito_detect_gpio;
	int irq_gpio;
	int strobe_irq_gpio;
};

typedef struct {
	uint8_t reg_addr;
	uint8_t reg_data;
} rtmv20_reg_data;

typedef struct {
	rtmv20_reg_data reg_data[REG_MAX];
	uint8_t size;
} rtmv20_data;

typedef struct {
	int state;
	int ito_event;
	int ic_event;
	unsigned int strobe_event;
	unsigned int ic_exception;
} rtmv20_report_data;

typedef struct {
	unsigned int laser_enable;
	unsigned int laser_current;
	unsigned int laser_pulse;
	unsigned int laser_error;
	unsigned int flood_pulse;
	unsigned int flood_enable;
} rtmv20_info;

typedef enum {
	RTMV_IRQ_ES_LD_CTRL_EVT = 1 << 7,
	RTMV_IRQ_ES_PULSE_WIDTH_EVT = 1 << 6,
	RTMV_IRQ_ES_FPS_EVT = 1 << 5,
	RTMV_IRQ_OTP_EVT = 1 << 4,
	RTMV_IRQ_SHORT_EVT  = 1 << 3,
	RTMV_IRQ_OPEN_EVT = 1 << 2,
	RTMV_IRQ_LBP_EVT = 1 << 1,
	RTMV_IRQ_OCP_EVT = 1 << 0,
} rtmv_ic_error_enum;

#define RTMV_IOC_PWR_UP \
	_IO(RTMV_IOC_MAGIC, RTMV_SL_PRIVATE + 1)
#define RTMV_IOC_PWR_DOWN \
	_IO(RTMV_IOC_MAGIC, RTMV_SL_PRIVATE + 2)
#define RTMV_IOC_WRITE_DATA \
	_IOW(RTMV_IOC_MAGIC, RTMV_SL_PRIVATE + 3, rtmv20_data)
#define RTMV_IOC_READ_DATA \
	_IOR(RTMV_IOC_MAGIC, RTMV_SL_PRIVATE + 4, rtmv20_data)
#define RTMV_IOC_READ_INFO \
	_IOWR(RTMV_IOC_MAGIC, RTMV_SL_PRIVATE + 5, void*)


#endif /* __RTMV20_H__ */
