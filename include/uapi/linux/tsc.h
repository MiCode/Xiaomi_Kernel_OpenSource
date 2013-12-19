#ifndef TSC_H_
#define TSC_H_

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * ENUMS
 */
/* TSC sources that can transfer the TS out */
enum tsc_source {
	TSC_SOURCE_EXTERNAL0,
	TSC_SOURCE_EXTERNAL1,
	TSC_SOURCE_INTERNAL,
	TSC_SOURCE_CICAM
};

/* TSC destinations that can receive TS */
enum tsc_dest {
	TSC_DEST_TSPP0,
	TSC_DEST_TSPP1,
	TSC_DEST_CICAM
};

/** TSIF parameters **/
/* TSC data type - can be serial of parallel */
enum tsc_data_type {
	TSC_DATA_TYPE_SERIAL = 0,
	TSC_DATA_TYPE_PARALLEL = 1
};

/* TSC receive mode - determine the usage of the VALID and START bits */
enum tsc_receive_mode {
	TSC_RECEIVE_MODE_START_VALID = 0,
	TSC_RECEIVE_MODE_START_ONLY = 1,
	TSC_RECEIVE_MODE_VALID_ONLY = 2
};

/* TSC polarity - can be normal or inversed */
enum tsc_polarity {
	TSC_POLARITY_NORMAL = 0,
	TSC_POLARITY_INVERSED = 1,
};

/* TSC data swap - whether LSB or MSB is sent first */
enum tsc_data_swap {
	TSC_DATA_NORMAL = 0,
	TSC_DATA_SWAP = 1
};

/* TSC set error bit in ts header if ts_fail is enable */
enum tsc_set_error_bit {
	TSC_SET_ERROR_BIT_DISABLE = 0,
	TSC_SET_ERROR_BIT_ENABLE = 1
};

/* TSC CAM personality type */
enum tsc_cam_personality {
	TSC_CICAM_PERSONALITY_DISABLE = 0,
	TSC_CICAM_PERSONALITY_CI = 1,
	TSC_CICAM_PERSONALITY_CIPLUS = 2,
};

/* TSC CAM card status */
enum tsc_card_status {
	TSC_CARD_STATUS_DETECTED,
	TSC_CARD_STATUS_NOT_DETECTED,
	TSC_CARD_STATUS_FAILURE
};

/* TSC transaction error types */
enum tsc_transcation_error {
	TSC_TRANSACTION_ERROR_ERR,
	TSC_TRANSACTION_ERROR_RETRY,
	TSC_TRANSACTION_ERROR_SPLIT
};


/*
 * STRUCTS
 */
/* TSC route - configure a TS transfer from source to dest */
struct tsc_route {
	enum tsc_source source;
	enum tsc_dest dest;
};

/* TSIF parameters to configure the source TSIF */
struct tsc_tsif_params {
	enum tsc_source source;
	enum tsc_receive_mode receive_mode;
	enum tsc_data_type data_type;
	enum tsc_polarity clock_polarity;
	enum tsc_polarity data_polarity;
	enum tsc_polarity start_polarity;
	enum tsc_polarity valid_polarity;
	enum tsc_polarity error_polarity;
	enum tsc_data_swap data_swap;
	enum tsc_set_error_bit set_error;
};


/* Parameters to perform single byte data transaction */
struct tsc_single_byte_mode {
	__u16 address;
	__u8 data;
	int timeout; /* in msec */
};

/* Parameters to perform buffer data transaction */
struct tsc_buffer_mode {
	int buffer_fd;
	__u16 buffer_size;
	int timeout; /* in msec */
};

/*
 * defines for IOCTL functions
 * read Documentation/ioctl-number.txt
 * some random number to avoid coinciding with other ioctl numbers
 */
#define TSC_IOCTL_BASE					0xBA

/* TSC Mux IOCTLs */
#define TSC_CONFIG_ROUTE			\
	_IOW(TSC_IOCTL_BASE, 0, struct tsc_route)
#define TSC_ENABLE_INPUT			\
	_IOW(TSC_IOCTL_BASE, 1, enum tsc_source)
#define TSC_DISABLE_INPUT			\
	_IOW(TSC_IOCTL_BASE, 2, enum tsc_source)
#define TSC_SET_TSIF_CONFIG			\
	_IOW(TSC_IOCTL_BASE, 3, struct tsc_tsif_params)
#define TSC_CLEAR_RATE_MISMATCH_IRQ		\
	_IO(TSC_IOCTL_BASE, 4)
#define TSC_CICAM_SET_CLOCK			\
	_IOW(TSC_IOCTL_BASE, 5, int)

/* TSC CI Card IOCTLs */
#define TSC_CAM_RESET				\
	_IO(TSC_IOCTL_BASE, 6)
#define TSC_CICAM_PERSONALITY_CHANGE\
	_IOW(TSC_IOCTL_BASE, 7, enum tsc_cam_personality)
#define TSC_GET_CARD_STATUS			\
	_IOR(TSC_IOCTL_BASE, 8, enum tsc_card_status)

/* TSC CI Data IOCTLs */
#define TSC_READ_CAM_MEMORY			\
	_IOWR(TSC_IOCTL_BASE, 9, struct tsc_single_byte_mode)
#define TSC_WRITE_CAM_MEMORY		\
	_IOW(TSC_IOCTL_BASE, 10, struct tsc_single_byte_mode)
#define TSC_READ_CAM_IO				\
	_IOWR(TSC_IOCTL_BASE, 11, struct tsc_single_byte_mode)
#define TSC_WRITE_CAM_IO			\
	_IOW(TSC_IOCTL_BASE, 12, struct tsc_single_byte_mode)
#define TSC_READ_CAM_BUFFER			\
	_IOWR(TSC_IOCTL_BASE, 13, struct tsc_buffer_mode)
#define TSC_WRITE_CAM_BUFFER		\
	_IOW(TSC_IOCTL_BASE, 14, struct tsc_buffer_mode)

#endif /* TSC_H_ */
