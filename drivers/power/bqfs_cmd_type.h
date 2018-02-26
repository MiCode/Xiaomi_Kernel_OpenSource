

#ifndef __BQFS_CMD_TYPE__
#define __BQFS_CMD_TYPE__


#define CMD_MAX_DATA_SIZE	110
#define RETRY_LIMIT		3
#define CMD_RETRY_DELAY		100 /* in ms */

#ifdef __GNUC__
#define __PACKED	__attribute__((packed))
#else
#error "Make sure structure cmd_t is packed"
#endif

typedef enum {
	CMD_INVALID = 0,
	CMD_R,	/* Read */
	CMD_W,	/* Write */
	CMD_C,	/* Compare */
	CMD_X,	/* Delay */
} cmd_type_t;


typedef struct {
	cmd_type_t cmd_type;
	u8 addr;
	u8 reg;
	union {
		u8 bytes[CMD_MAX_DATA_SIZE + 1];
		u16 delay;
	} data;
	u8  data_len;
	u16 line_num;
} __PACKED bqfs_cmd_t;


#endif
