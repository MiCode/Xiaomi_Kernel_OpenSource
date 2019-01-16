#include "tmNxTypes.h"


/* States of transmission */
#define  INIT                   0xFF
#define  START                  0xF0
#define  CORRECT                0x00
#define  BUS_ERROR              0x01
#define  NO_ANSWER_SLAVE        0x20
#define  UNCORRECT              0x30
#define  BUS_LOST               0x38

/* version of infrastructure */
#define COMPATIBILITY_INFRA     0U
#define MAJOR_VERSION_INFRA     1U
#define MINOR_VERSION_INFRA     02U

/* i2c address slave */
#define slaveAddressTDA9975A      0x4C
#define slaveAddressTDA8778       0x74
#define slaveAddressUDA1355H      0x1A
#define slaveAddressMAX4562       0x4E
#define slaveAddressTDA9984       0x70
#define slaveAddressTDA9989       0x70
#define slaveAddressDriverHdmiCEC 0x34
#define slaveAddressPCA9536       0x41

typedef enum {
	reg_TDA8778,
	reg_UDA1355H,
	reg_MAX4562,
	reg_TDA997X,
	reg_TDA998X,
	reg_TDA9989_CEC,
	reg_TDA9950,
	reg_PCA9536
} i2cRegisterType_t;

typedef struct {
	UInt8 slaveAddr;
	UInt8 firstRegister;
	UInt8 lenData;
	UInt8 *pData;
} tmbslHdmiSysArgs_t;


extern volatile unsigned char transmission;
    /* Count of repeated starts within frame, used as index into arrays within interrupt routines */
extern unsigned char nb_byte[3];	/* Array of byte counts, allows multi-part messages with repeated starts */
extern unsigned char slave[3];	/* Array of slave addresses, allows multi-part messages with repeated starts to different slaves or W/R */
extern unsigned char *pt_mtd[3], *pt_mrd[3];	/* Array of read/write pointers, allows multi-part messages with repeated starts */
extern unsigned char rep_start_cntr;

tmErrorCode_t Init_i2c(void);

unsigned char Read_at_i2c(unsigned char address, unsigned char pos, unsigned char nb_char,
			  unsigned char *ptr);
unsigned char Write_i2c(unsigned char address, unsigned char *ptr, unsigned char nb_char);
unsigned char Read_edid(unsigned char seg_addr, unsigned char seg_ptr, unsigned char data_addr,
			unsigned char word_offset, unsigned char nb_char, unsigned char *ptr);

tmErrorCode_t i2cWrite(i2cRegisterType_t type_register, tmbslHdmiSysArgs_t *pSysArgs);
tmErrorCode_t i2cRead(i2cRegisterType_t type_register, tmbslHdmiSysArgs_t *pSysArgs);

unsigned char i2cReadEdid(unsigned char seg_addr, unsigned char seg_ptr,
			  unsigned char data_addr, unsigned char word_offset,
			  unsigned char nb_char, unsigned char *ptr);
