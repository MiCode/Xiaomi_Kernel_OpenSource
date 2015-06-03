#ifndef _MSM_MDP_EXT_H_
#define _MSM_MDP_EXT_H_

#include <linux/msm_mdp.h>

#define MDP_IOCTL_MAGIC 'S'
/* atomic commit ioctl used for validate and commit request */
#define MSMFB_ATOMIC_COMMIT	_IOWR(MDP_IOCTL_MAGIC, 128, void *)

/*
 * Ioctl for updating the layer position asynchronously. Initially, pipes
 * should be configured with MDP_LAYER_ASYNC flag set during the atomic commit,
 * after which any number of position update calls can be made. This would
 * enable multiple position updates within a single vsync. However, the screen
 * update would happen only after vsync, which would pick the latest update.
 *
 * Limitations:
 * - Currently supported only for video mode panels with single LM or dual LM
 *   with source_split enabled.
 * - Only position update is supported with no scaling/cropping.
 * - Async layers should have unique z_order.
 */
#define MSMFB_ASYNC_POSITION_UPDATE _IOWR(MDP_IOCTL_MAGIC, 129, \
					struct mdp_position_update)

/**********************************************************************
LAYER FLAG CONFIGURATION
**********************************************************************/
/* left-right layer flip flag */
#define MDP_LAYER_FLIP_LR		0x1

/* up-down layer flip flag */
#define MDP_LAYER_FLIP_UD		0x2

/*
 * This flag enables pixel extension for the current layer. Validate/commit
 * call uses scale parameters when this flag is enabled.
 */
#define MDP_LAYER_ENABLE_PIXEL_EXT	0x4

/* Flag indicates that layer is foreground layer */
#define MDP_LAYER_FORGROUND		0x8

/* Flag indicates that layer is associated with secure session */
#define MDP_LAYER_SECURE_SESSION	0x10

/*
 * Flag indicates that layer is drawing solid fill. Validate/commit call
 * does not expect buffer when this flag is enabled.
 */
#define MDP_LAYER_SOLID_FILL		0x20

/* Layer format is deinterlace */
#define MDP_LAYER_DEINTERLACE		0x40

/* layer contains bandwidth compressed format data */
#define MDP_LAYER_BWC			0x80

/* layer is async position updatable */
#define MDP_LAYER_ASYNC			0x100

/* layer contains postprocessing configuration data */
#define MDP_LAYER_PP			0x200

/* Flag indicates that layer is associated with secure display session */
#define MDP_LAYER_SECURE_DISPLAY_SESSION 0x400

/**********************************************************************
VALIDATE/COMMIT FLAG CONFIGURATION
**********************************************************************/

/*
 * Client enables it to inform that call is to validate layers before commit.
 * If this flag is not set then driver will use MSMFB_ATOMIC_COMMIT for commit.
 */
#define MDP_VALIDATE_LAYER			0x01

/*
 * This flag is only valid for commit call. Commit behavior is synchronous
 * when this flag is defined. It blocks current call till processing is
 * complete. Behavior is asynchronous otherwise.
 */
#define MDP_COMMIT_WAIT_FOR_FINISH		0x02

/*
 * This flag is only valid for commit call and used for debugging purpose. It
 * forces the to wait for sync fences.
 */
#define MDP_COMMIT_SYNC_FENCE_WAIT		0x04

#define MDP_COMMIT_VERSION_1_0		0x00010000

/**********************************************************************
Configuration structures
All parameters are input to driver unless mentioned output parameter
explicitly.
**********************************************************************/
struct mdp_layer_plane {
	/* DMA buffer file descriptor information. */
	int fd;

	/* Pixel offset in the dma buffer. */
	uint32_t offset;

	/* Number of bytes in one scan line including padding bytes. */
	uint32_t stride;
};

struct mdp_layer_buffer {
	/* layer width in pixels. */
	uint32_t width;

	/* layer height in pixels. */
	uint32_t height;

	/*
	 * layer format in DRM-style fourcc, refer drm_fourcc.h for
	 * standard formats
	 */
	uint32_t format;

	/* plane to hold the fd, offset, etc for all color components */
	struct mdp_layer_plane planes[MAX_PLANES];

	/* valid planes count in layer planes list */
	uint32_t plane_count;

	/* compression ratio factor, value depends on the pixel format */
	struct mult_factor comp_ratio;

	/*
	 * SyncFence associated with this buffer. It is used in two ways.
	 *
	 * 1. Driver waits to consume the buffer till producer signals in case
	 * of primary and external display.
	 *
	 * 2. Writeback device uses buffer structure for output buffer where
	 * driver is producer. However, client sends the fence with buffer to
	 * indicate that consumer is still using the buffer and it is not ready
	 * for new content.
	 */
	int	 fence;

	/* 32bits reserved value for future usage. */
	uint32_t reserved;
};

/*
 * One layer holds configuration for one pipe. If client wants to stage single
 * layer on two pipes then it should send two different layers with relative
 * (x,y) information. Client must send same information during validate and
 * commit call. Commit call may fail if client sends different layer information
 * attached to same pipe during validate and commit. Device invalidate the pipe
 * once it receives the vsync for that commit.
 */
struct mdp_input_layer {
	/*
	 * Flag to enable/disable properties for layer configuration. Refer
	 * layer flag configuration section for all possible flags.
	 */
	uint32_t		flags;

	/*
	 * Pipe selection for this layer by client. Client provides the index
	 * in validate and commit call. Device reserves the pipe once validate
	 * is successful. Device only uses validated pipe during commit call.
	 * If client sends different layer/pipe configuration in validate &
	 * commit then commit may fail.
	 */
	uint32_t		pipe_ndx;

	/*
	 * Horizontal decimation value, this indicates the amount of pixels
	 * dropped for each pixel that is fetched from a line. It does not
	 * result in bandwidth reduction because pixels are still fetched from
	 * memory but dropped internally by hardware.
	 * The decimation value given should be power of two of decimation
	 * amount.
	 * 0: no decimation
	 * 1: decimate by 2 (drop 1 pixel for each pixel fetched)
	 * 2: decimate by 4 (drop 3 pixels for each pixel fetched)
	 * 3: decimate by 8 (drop 7 pixels for each pixel fetched)
	 * 4: decimate by 16 (drop 15 pixels for each pixel fetched)
	 */
	uint8_t			horz_deci;

	/*
	 * Vertical decimation value, this indicates the amount of lines
	 * dropped for each line that is fetched from overlay. It saves
	 * bandwidth because decimated pixels are not fetched.
	 * The decimation value given should be power of two of decimation
	 * amount.
	 * 0: no decimation
	 * 1: decimation by 2 (drop 1 line for each line fetched)
	 * 2: decimation by 4 (drop 3 lines for each line fetched)
	 * 3: decimation by 8 (drop 7 lines for each line fetched)
	 * 4: decimation by 16 (drop 15 lines for each line fetched)
	 */
	uint8_t			vert_deci;

	/*
	 * Used to set plane opacity. The range can be from 0-255, where
	 * 0 means completely transparent and 255 means fully opaque.
	 */
	uint8_t			alpha;

	/*
	 * Blending stage to occupy in display, if multiple layers are present,
	 * highest z_order usually means the top most visible layer. The range
	 * acceptable is from 0-7 to support blending up to 8 layers.
	 */
	uint16_t		z_order;

	/*
	 * Color used as color key for transparency. Any pixel in fetched
	 * image matching this color will be transparent when blending.
	 * The color should be in same format as the source image format.
	 */
	uint32_t		transp_mask;

	/*
	 * Solid color used to fill the overlay surface when no source
	 * buffer is provided.
	 */
	uint32_t		bg_color;

	/* blend operation defined in "mdss_mdp_blend_op" enum. */
	enum mdss_mdp_blend_op		blend_op;

	/* color space of the source */
	enum mdp_color_space	color_space;

	/*
	 * Source crop rectangle, portion of image that will be fetched. This
	 * should always be within boundaries of source image.
	 */
	struct mdp_rect		src_rect;

	/*
	 * Destination rectangle, the position and size of image on screen.
	 * This should always be within panel boundaries.
	 */
	struct mdp_rect		dst_rect;

	/* Scaling parameters. */
	struct mdp_scale_data __user	*scale;

	/* Buffer attached with each layer. Device uses it for commit call. */
	struct mdp_layer_buffer	buffer;

	/*
	 * Source side post processing configuration information for each
	 * layer.
	 */
	void __user		*pp_info;

	/*
	 * This is an output parameter.
	 *
	 * Only for validate call. Frame buffer device sets error code
	 * based on validate call failure scenario.
	 */
	int			error_code;

	/* 32bits reserved value for future usage. */
	uint32_t		reserved[6];
};

struct mdp_output_layer {
	/*
	 * Flag to enable/disable properties for layer configuration. Refer
	 * layer flag config section for all possible flags.
	 */
	uint32_t			flags;

	/*
	 * Writeback destination selection for output. Client provides the index
	 * in validate and commit call.
	 */
	uint32_t			writeback_ndx;

	/* Buffer attached with output layer. Device uses it for commit call */
	struct mdp_layer_buffer		buffer;

	/* 32bits reserved value for future usage. */
	uint32_t			reserved[6];
};

/*
 * Commit structure holds layer stack send by client for validate and commit
 * call. If layers are different between validate and commit call then commit
 * call will also do validation. In such case, commit may fail.
 */
struct mdp_layer_commit_v1 {
	/*
	 * Flag to enable/disable properties for commit/validate call. Refer
	 * validate/commit flag config section for all possible flags.
	 */
	uint32_t		flags;

	/*
	 * This is an output parameter.
	 *
	 * Frame buffer device provides release fence handle to client. It
	 * triggers release fence when display hardware has consumed all the
	 * buffers attached to this commit call and buffer is ready for reuse
	 * for primary and external. For writeback case, it triggers it when
	 * output buffer is ready for consumer.
	 */
	int			release_fence;

	/*
	 * Left_roi is optional configuration. Client configures it only when
	 * partial update is enabled. It defines the "region of interest" on
	 * left part of panel when it is split display. For non-split display,
	 * it defines the "region of interest" on the panel.
	 */
	struct mdp_rect		left_roi;

	/*
	 * Right_roi is optional configuration. Client configures it only when
	 * partial update is enabled. It defines the "region of interest" on
	 * right part of panel for split display configuration. It is not
	 * required for non-split display.
	 */
	struct mdp_rect		right_roi;

	 /* Pointer to a list of input layers for composition. */
	struct mdp_input_layer __user *input_layers;

	/* Input layer count present in input list */
	uint32_t		input_layer_cnt;

	/*
	 * Output layer for writeback display. It supports only one
	 * layer as output layer. This is not required for primary
	 * and external displays
	 */
	struct mdp_output_layer __user *output_layer;

	/*
	 * This is an output parameter.
	 *
	 * Frame buffer device provides retire fence handle if
	 * COMMIT_RETIRE_FENCE flag is set in commit call. It triggers
	 * retire fence when current layers are swapped with new layers
	 * on display hardware. For video mode panel and writeback,
	 * retire fence and release fences are triggered at the same
	 * time while command mode panel triggers release fence first
	 * (on pingpong done) and retire fence (on rdptr done)
	 * after that.
	 */
	int			retire_fence;

	/* 32-bits reserved value for future usage. */
	uint32_t		reserved[6];
};

/*
 * mdp_overlay_list - argument for ioctl MSMFB_ATOMIC_COMMIT
 */
struct mdp_layer_commit {
	/*
	 * 32bit version indicates the commit structure selection
	 * from union. Lower 16bits indicates the minor version while
	 * higher 16bits indicates the major version. It selects the
	 * commit structure based on major version selection. Minor version
	 * indicates that reserved fields are in use.
	 *
	 * Current supported version is 1.0 (Major:1 Minor:0)
	 */
	uint32_t version;
	union {
		/* Layer commit/validate definition for V1 */
		struct mdp_layer_commit_v1 commit_v1;
	};
};

struct mdp_point {
	uint32_t x;
	uint32_t y;
};

/*
 * Async updatable layers. One layer holds configuration for one pipe.
 */
struct mdp_async_layer {
	/*
	 * Flag to enable/disable properties for layer configuration. Refer
	 * layer flag config section for all possible flags.
	 */
	uint32_t flags;

	/*
	 * Pipe selection for this layer by client. Client provides the
	 * pipe index that the device reserved during ATOMIC_COMMIT.
	 */
	uint32_t		pipe_ndx;

	/* Source start x,y. */
	struct mdp_point	src;

	/* Destination start x,y. */
	struct mdp_point	dst;

	/*
	 * This is an output parameter.
	 *
	 * Frame buffer device sets error code based on the failure.
	 */
	int			error_code;

	uint32_t		reserved[3];
};

/*
 * mdp_position_update - argument for ioctl MSMFB_ASYNC_POSITION_UPDATE
 */
struct mdp_position_update {
	 /* Pointer to a list of async updatable input layers */
	struct mdp_async_layer __user *input_layers;

	/* Input layer count present in input list */
	uint32_t input_layer_cnt;
};

#endif
