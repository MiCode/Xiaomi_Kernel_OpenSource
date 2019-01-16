typedef struct {
	uint8_t pollIntervalMs;
	uint8_t status_0;
	uint8_t status_1;
	uint8_t connectedReady;
	uint8_t linkMode;
	uint8_t mhlHpdStatus;
	uint8_t mhlRequestWritePending;
	bool_t mhlConnectionEvent;
	uint8_t mhlConnected;
	uint8_t mhlHpdRSENflags;
	bool_t mscMsgArrived;
	uint8_t mscMsgSubCommand;
	uint8_t mscMsgData;
	uint8_t mscFeatureFlag;
	uint8_t cbusReferenceCount;
	uint8_t mscLastCommand;
	uint8_t mscLastOffset;
	uint8_t mscLastData;
	uint8_t mscMsgLastCommand;
	uint8_t mscMsgLastData;
	uint8_t mscSaveRcpKeyCode;
#define SCRATCHPAD_SIZE 16
	uint8_t localScratchPad[SCRATCHPAD_SIZE];
	uint8_t miscFlags;
	uint8_t ucDevCapCacheIndex;
	uint8_t aucDevCapCache[16];
	uint8_t rapFlags;
	uint8_t preferredClkMode;
} mhlTx_config_t;
typedef enum {
	MHL_HPD = 0x01, MHL_RSEN = 0x02
} MhlHpdRSEN_e;
typedef enum {
	FLAGS_SCRATCHPAD_BUSY = 0x01, FLAGS_REQ_WRT_PENDING = 0x02, FLAGS_WRITE_BURST_PENDING =
	    0x04, FLAGS_RCP_READY = 0x08, FLAGS_HAVE_DEV_CATEGORY =
	    0x10, FLAGS_HAVE_DEV_FEATURE_FLAGS = 0x20, FLAGS_SENT_DCAP_RDY =
	    0x40, FLAGS_SENT_PATH_EN = 0x80
} MiscFlags_e;
typedef enum {
	RAP_CONTENT_ON = 0x01
} rapFlags_e;

enum {
	MHL_RCP_CMD_SELECT = 0x00,
	MHL_RCP_CMD_UP = 0x01,
	MHL_RCP_CMD_DOWN = 0x02,
	MHL_RCP_CMD_LEFT = 0x03,
	MHL_RCP_CMD_RIGHT = 0x04,
	MHL_RCP_CMD_RIGHT_UP = 0x05,
	MHL_RCP_CMD_RIGHT_DOWN = 0x06,
	MHL_RCP_CMD_LEFT_UP = 0x07,
	MHL_RCP_CMD_LEFT_DOWN = 0x08,
	MHL_RCP_CMD_ROOT_MENU = 0x09,
	MHL_RCP_CMD_SETUP_MENU = 0x0A,
	MHL_RCP_CMD_CONTENTS_MENU = 0x0B,
	MHL_RCP_CMD_FAVORITE_MENU = 0x0C,
	MHL_RCP_CMD_EXIT = 0x0D,

	/* 0x0E - 0x1F are reserved */

	MHL_RCP_CMD_NUM_0 = 0x20,
	MHL_RCP_CMD_NUM_1 = 0x21,
	MHL_RCP_CMD_NUM_2 = 0x22,
	MHL_RCP_CMD_NUM_3 = 0x23,
	MHL_RCP_CMD_NUM_4 = 0x24,
	MHL_RCP_CMD_NUM_5 = 0x25,
	MHL_RCP_CMD_NUM_6 = 0x26,
	MHL_RCP_CMD_NUM_7 = 0x27,
	MHL_RCP_CMD_NUM_8 = 0x28,
	MHL_RCP_CMD_NUM_9 = 0x29,

	MHL_RCP_CMD_DOT = 0x2A,
	MHL_RCP_CMD_ENTER = 0x2B,
	MHL_RCP_CMD_CLEAR = 0x2C,

	/* 0x2D - 0x2F are reserved */

	MHL_RCP_CMD_CH_UP = 0x30,
	MHL_RCP_CMD_CH_DOWN = 0x31,
	MHL_RCP_CMD_PRE_CH = 0x32,
	MHL_RCP_CMD_SOUND_SELECT = 0x33,
	MHL_RCP_CMD_INPUT_SELECT = 0x34,
	MHL_RCP_CMD_SHOW_INFO = 0x35,
	MHL_RCP_CMD_HELP = 0x36,
	MHL_RCP_CMD_PAGE_UP = 0x37,
	MHL_RCP_CMD_PAGE_DOWN = 0x38,

	/* 0x39 - 0x40 are reserved */

	MHL_RCP_CMD_VOL_UP = 0x41,
	MHL_RCP_CMD_VOL_DOWN = 0x42,
	MHL_RCP_CMD_MUTE = 0x43,
	MHL_RCP_CMD_PLAY = 0x44,
	MHL_RCP_CMD_STOP = 0x45,
	MHL_RCP_CMD_PAUSE = 0x46,
	MHL_RCP_CMD_RECORD = 0x47,
	MHL_RCP_CMD_REWIND = 0x48,
	MHL_RCP_CMD_FAST_FWD = 0x49,
	MHL_RCP_CMD_EJECT = 0x4A,
	MHL_RCP_CMD_FWD = 0x4B,
	MHL_RCP_CMD_BKWD = 0x4C,

	/* 0x4D - 0x4F are reserved */

	MHL_RCP_CMD_ANGLE = 0x50,
	MHL_RCP_CMD_SUBPICTURE = 0x51,

	/* 0x52 - 0x5F are reserved */

	MHL_RCP_CMD_PLAY_FUNC = 0x60,
	MHL_RCP_CMD_PAUSE_PLAY_FUNC = 0x61,
	MHL_RCP_CMD_RECORD_FUNC = 0x62,
	MHL_RCP_CMD_PAUSE_REC_FUNC = 0x63,
	MHL_RCP_CMD_STOP_FUNC = 0x64,
	MHL_RCP_CMD_MUTE_FUNC = 0x65,
	MHL_RCP_CMD_UN_MUTE_FUNC = 0x66,
	MHL_RCP_CMD_TUNE_FUNC = 0x67,
	MHL_RCP_CMD_MEDIA_FUNC = 0x68,

	/* 0x69 - 0x70 are reserved */

	MHL_RCP_CMD_F1 = 0x71,
	MHL_RCP_CMD_F2 = 0x72,
	MHL_RCP_CMD_F3 = 0x73,
	MHL_RCP_CMD_F4 = 0x74,
	MHL_RCP_CMD_F5 = 0x75,

	/* 0x76 - 0x7D are reserved */

	MHL_RCP_CMD_VS = 0x7E,
	MHL_RCP_CMD_RSVD = 0x7F,
};
