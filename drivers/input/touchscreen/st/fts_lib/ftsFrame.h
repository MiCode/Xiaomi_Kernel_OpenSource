/*

**************************************************************************
**						STMicroelectronics						**
**************************************************************************
**						marco.cali@st.com				**
**************************************************************************
*																		*
*				  FTS functions for getting frames			 *
*																		*
**************************************************************************
**************************************************************************

*/

#include "ftsSoftware.h"

/* Number of data bytes for each node */
#define BYTES_PER_NODE				2
#define OFFSET_LENGTH				2
#define FRAME_DATA_HEADER			8
#define FRAME_HEADER_SIGNATURE			0xB5
#define FRAME_DATA_READ_RETRY			2

typedef struct {
	DataHeader header;
	short *node_data;
	int node_data_size;
} MutualSenseFrame;

typedef struct {
	DataHeader header;
	short *force_data;
	short *sense_data;
} SelfSenseFrame;

/* int getOffsetFrame(u16 address, u16 *offset); */
int getChannelsLength(void);
int getFrameData(u16 address, int size, short **frame);
/* int getMSFrame(u16 type, short **frame, int keep_first_row); */
/* int getMSKeyFrame(u16 type, short **frame); */
/* int getSSFrame(u16 type, short **frame); */
/* int getNmsFrame(u16 type, short ***frames, int * sizes, int keep_first_row, int fs, int n); */
int getSenseLen(void);
int getForceLen(void);
int requestFrame(u16 type);
int readFrameDataHeader(u16 type, DataHeader *header);
int getMSFrame2(u16 type, MutualSenseFrame *frame);
int getSSFrame2(u16 type, SelfSenseFrame *frame);
