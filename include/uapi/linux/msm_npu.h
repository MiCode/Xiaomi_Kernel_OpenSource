#ifndef _UAPI_MSM_NPU_H_
#define _UAPI_MSM_NPU_H_

#define MSM_NPU_IOCTL_MAGIC 'n'

/* get npu info */
#define MSM_NPU_GET_INFO \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 1, struct msm_npu_get_info_ioctl_t *)

/* map buf */
#define MSM_NPU_MAP_BUF \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 2, struct msm_npu_map_buf_ioctl_t *)

/* map buf */
#define MSM_NPU_UNMAP_BUF \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 3, struct msm_npu_unmap_buf_ioctl_t *)

/* load network */
#define MSM_NPU_LOAD_NETWORK \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 4, struct msm_npu_load_network_ioctl_t *)

/* unload network */
#define MSM_NPU_UNLOAD_NETWORK \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 5, struct msm_npu_unload_network_ioctl_t *)

/* exec network */
#define MSM_NPU_EXEC_NETWORK \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 6, struct msm_npu_exec_network_ioctl_t *)

struct msm_npu_map_buf_ioctl_t {
	/* buffer ion handle */
	void	*buf_ion_hdl;
	/* buffer size */
	uint32_t	size;
	/* iommu mapped physical address */
	void	*npu_phys_addr;
};

#define MSM_NPU_MAX_INPUT_LAYER_NUM 8
#define MSM_NPU_MAX_OUTPUT_LAYER_NUM 4
#define MSM_NPU_LAYER_NAME_SIZE 64

struct msm_npu_unmap_buf_ioctl_t {
	/* buffer ion handle */
	void	*buf_ion_hdl;
	/* iommu mapped physical address */
	void	*npu_phys_addr;
};

struct msm_npu_patch_info {
	/* chunk id */
	uint32_t	chunk_id;
	/* instruction size in bytes */
	uint16_t	instruction_size_in_bytes;
	/* variable size in bits */
	uint16_t	variable_size_in_bits;
	/* shift value in bits */
	uint16_t	shift_value_in_bits;
	/* location offset */
	uint32_t	loc_offset;
};

struct msm_npu_layer_t {
	/* layer id */
	uint32_t	layer_id;
	/* patch information*/
	struct msm_npu_patch_info patch_info;
	/* buffer handle */
	void	*buf_hdl;
	/* buffer size */
	uint32_t	buf_size;
};


struct msm_npu_get_info_ioctl_t {
	/* firmware version */
	uint32_t	firmware_version;
	/* reserved */
	uint32_t	flags;
};

struct msm_npu_load_network_ioctl_t {
	/* buffer ion handle */
	void	*buf_ion_hdl;
	/* physical address */
	void	*buf_phys_addr;
	/* buffer size */
	uint32_t	buf_size;
	/* first block size */
	uint32_t	first_block_size;
	/* reserved */
	uint32_t	flags;
	/* network handle */
	void	*network_hdl;
	/* aco buffer handle */
	void	*aco_hdl;
};

struct msm_npu_unload_network_ioctl_t {
	/* network handle */
	void	*network_hdl;
	/* aco buffer handle */
	void	*aco_hdl;
};

struct msm_npu_exec_network_ioctl_t {
	/* network handle */
	void	*network_hdl;
	/* input layer number */
	uint32_t	input_layer_num;
	/* input layer info */
	struct msm_npu_layer_t	input_layers[MSM_NPU_MAX_INPUT_LAYER_NUM];
	/* output layer number */
	uint32_t	output_layer_num;
	/* output layer info */
	struct msm_npu_layer_t	output_layers[MSM_NPU_MAX_OUTPUT_LAYER_NUM];
	/* patching is required */
	int32_t	patching_required;
	/* asynchronous execution */
	int32_t	async;
	/* reserved */
	uint32_t	flags;
};

#endif /*_UAPI_MSM_NPU_H_*/
