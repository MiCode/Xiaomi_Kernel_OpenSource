#ifndef __UAPI_MSM_FD__
#define __UAPI_MSM_FD__

#include <linux/videodev2.h>
#include <linux/types.h>

/*
 * struct msm_fd_event - Structure contain event info.
 * @buf_index: Buffer index.
 * @frame_id: Frame id.
 * @face_cnt: Detected faces.
 */
struct msm_fd_event {
	__u32 buf_index;
	__u32 frame_id;
	__u32 face_cnt;
};

/*
 * enum msm_fd_pose - Face pose.
 */
enum msm_fd_pose {
	MSM_FD_POSE_FRONT,
	MSM_FD_POSE_RIGHT_DIAGONAL,
	MSM_FD_POSE_RIGHT,
	MSM_FD_POSE_LEFT_DIAGONAL,
	MSM_FD_POSE_LEFT,
};

/*
 * struct msm_fd_face_data - Structure contain detected face data.
 * @pose: refer to enum msm_fd_pose.
 * @angle: Face angle
 * @confidence: Face confidence level.
 * @reserved: Reserved data for future use.
 * @face: Face rectangle.
 */
struct msm_fd_face_data {
	__u32 pose;
	__u32 angle;
	__u32 confidence;
	__u32 reserved;
	struct v4l2_rect face;
};

/*
 * struct msm_fd_result - Structure contain detected faces result.
 * @frame_id: Frame id of requested result.
 * @face_cnt: Number of result faces, driver can modify this value (to smaller)
 * @face_data: Pointer to array of face data structures.
 *  Array size should not be smaller then face_cnt.
 */
struct msm_fd_result {
	__u32 frame_id;
	__u32 face_cnt;
	struct msm_fd_face_data __user *face_data;
};

/* MSM FD private ioctl ID */
#define VIDIOC_MSM_FD_GET_RESULT \
	_IOWR('V', BASE_VIDIOC_PRIVATE, struct msm_fd_result)

/* MSM FD event ID */
#define MSM_EVENT_FD (V4L2_EVENT_PRIVATE_START)

/* MSM FD control ID's */
#define V4L2_CID_FD_SPEED                (V4L2_CID_PRIVATE_BASE)
#define V4L2_CID_FD_FACE_ANGLE           (V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_FD_MIN_FACE_SIZE        (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_FD_FACE_DIRECTION       (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_FD_DETECTION_THRESHOLD  (V4L2_CID_PRIVATE_BASE + 4)
#define V4L2_CID_FD_WORK_MEMORY_SIZE     (V4L2_CID_PRIVATE_BASE + 5)
#define V4L2_CID_FD_WORK_MEMORY_FD       (V4L2_CID_PRIVATE_BASE + 6)

#endif
