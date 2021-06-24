/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chris-YC Chen <chris-yc.chen@mediatek.com>
 */
#include "tile_driver.h"
#include "tile_mdp_func.h"
#include "tile_utility.h"
#include "tile_mdp_reg.h"

#include "mtk-mml-color.h"
#include "mtk-mml-core.h"
#include "mtk-mml.h"

#define UNUSED(expr) do { (void)(expr); } while (0)

#ifndef MAX
    #define MAX(x, y)   ((x) >= (y))? (x): (y)
#endif // MAX

#ifndef MIN
    #define MIN(x, y)   ((x) <= (y))? (x): (y)
#endif  // MIN

/* mdp lut function */

/* prototype init */
static ISP_TILE_MESSAGE_ENUM tile_rdma_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_hdr_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_aal_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_prz_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_tdshp_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_wrot_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
/* prototype for */
static ISP_TILE_MESSAGE_ENUM tile_rdma_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_hdr_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_aal_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_prz_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_wrot_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
/* prototype back */
static ISP_TILE_MESSAGE_ENUM tile_rdma_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
//static ISP_TILE_MESSAGE_ENUM tile_aal_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_prz_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);
static ISP_TILE_MESSAGE_ENUM tile_wrot_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map);

bool tile_init_mdp_func_property(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    bool found_flag = false;

    MDP_TILE_INIT_PROPERTY_LUT(INIT_TILE_FUNC, ptr_func, found_flag, ptr_tile_reg_map, TILE_MDP_GROUP_NUM,
        0, 0,
        ptr_tile_reg_map->max_input_width, ptr_tile_reg_map->max_input_height);
    return found_flag;
}

ISP_TILE_MESSAGE_ENUM tile_lut_mdp_func_output_disable(int module_no, TILE_FUNC_ENABLE_STRUCT *ptr_func_en,
                                                       TILE_REG_MAP_STRUCT *ptr_tile_reg_map)
{
    ISP_TILE_MESSAGE_ENUM result = ISP_MESSAGE_TILE_OK;

    int i;
    int module_last_no = 0;
    for (i=0;i<module_no;i++)
    {
        TILE_FUNC_ENABLE_STRUCT *ptr_func_en_list = ptr_func_en + i;
        /* init default val */
        ptr_func_en_list->output_disable_flag = false;
        MDP_TILE_FUNC_OUTPUT_DISABLE_LUT(TILE_OUTPUT_DISABLE_CHECK, ptr_func_en_list,
            module_last_no, ptr_tile_reg_map, result,);
        /* return when error occurs */
        TILE_CHECK_RESULT(result);
    }
    return result;
}

static ISP_TILE_MESSAGE_ENUM tile_rdma_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct rdma_tile_data *data = (struct rdma_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_RDMA_NULL_DATA;
    }

    // RDMA support crop capability
    ptr_func->type |= TILE_TYPE_CROP_EN;

    // Specific constraints implied by different formats

    /* For AFBC mode end x may be extend to block size
     * and may exceed max tile width 640. So reduce width
     * to prevent it.
     */
    if (MML_FMT_COMPRESS(data->src_fmt) &&
        (MML_FMT_HW_FORMAT(data->src_fmt) == 2 ||
        MML_FMT_HW_FORMAT(data->src_fmt) == 3))
    {
        ptr_func->in_tile_width = ((data->max_width >> 5) - 1) << 5;
    }
    /* In tile constraints
     * Input Format | Tile Width
     * -------------|-----------
     *   Block mode | L
     *       YUV420 | L
     *       YUV422 | L * 2
     * YUV444/RGB/Y | L * 4
     */
    else if (MML_FMT_BLOCK(data->src_fmt) ||  // Block mode
        (MML_FMT_H_SUBSAMPLE(data->src_fmt) &&     // YUV420
         MML_FMT_V_SUBSAMPLE(data->src_fmt)) ||
        MML_FMT_COMPRESS(data->src_fmt))
    {
        ptr_func->in_tile_width = data->max_width;
    }
    else if (MML_FMT_H_SUBSAMPLE(data->src_fmt) && // YUV422
        !MML_FMT_V_SUBSAMPLE(data->src_fmt))
    {
        ptr_func->in_tile_width = data->max_width * 2;
    }
    else
    {
        ptr_func->in_tile_width = data->max_width * 4;
    }

    if (MML_FMT_H_SUBSAMPLE(data->src_fmt))        // YUV422
    {
        // Tile alignment constraints
        ptr_func->in_const_x = 2;

        if (MML_FMT_V_SUBSAMPLE(data->src_fmt) &&  // YUV420
            !MML_FMT_INTERLACED(data->src_fmt))
        {
            ptr_func->in_const_y = 2;
        }
    }

    ptr_func->in_tile_height  = 16000;
    ptr_func->out_tile_height = 16000;

    if (!MML_FMT_BLOCK(data->src_fmt))
    {
        // Enable MAV for MDP tile
        ptr_tile_reg_map->tdr_ctrl_en = true;
    }

    if (data->alpharot) //argb rot
    {
        ptr_func->crop_bias_x = data->crop.left;
        ptr_func->crop_bias_y = data->crop.top;
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_hdr_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct hdr_tile_data *data = (struct hdr_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_HDR_NULL_DATA;
    }

    ptr_func->in_tile_width   = 8191;
    ptr_func->out_tile_width  = 8191;
    ptr_func->in_tile_height  = 16000;
    ptr_func->out_tile_height = 16000;

    if (!data->relay_mode)
    {
        ptr_func->type |= TILE_TYPE_CROP_EN;
        ptr_func->in_min_width    = data->min_width;
        ptr_func->l_tile_loss     = 8;
        ptr_func->r_tile_loss     = 8;
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_aal_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct aal_tile_data *data = (struct aal_tile_data*)(ptr_func->func_data);

    UNUSED(ptr_tile_reg_map);

    if (NULL == data)
    {
        return MDP_MESSAGE_AAL_NULL_DATA;
    }

    ptr_func->in_tile_width   = data->max_width;
    ptr_func->out_tile_width  = data->max_width;
    // AAL_TILE_WIDTH > tile > AAL_HIST_MIN_WIDTH for histogram update, unless AAL_HIST_MIN_WIDTH > frame > AAL_MIN_WIDTH
    ptr_func->in_min_width    = MAX(MIN(data->min_hist_width, ptr_func->full_size_x_in), data->min_width);
    ptr_func->in_tile_height  = 16000;
    ptr_func->out_tile_height = 16000;
    ptr_func->l_tile_loss     = 8;
    ptr_func->r_tile_loss     = 8;

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_prz_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct rsz_tile_data *data = (struct rsz_tile_data*)(ptr_func->func_data);

    UNUSED(ptr_tile_reg_map);

    if (NULL == data)
    {
        return MDP_MESSAGE_PRZ_NULL_DATA;
    }

    // SCL support crop capability
    ptr_func->type |= TILE_TYPE_CROP_EN;

    // drs: C42 downsampler output frame width
    data->c42_out_frame_w = (ptr_func->full_size_x_in + 0x01) & ~0x01;

    // prz
    if (data->vir_scale)
    {
        /* Line buffer size constraints
         * Horz. Scale   | Vert. First | Vert. Scale    | Vert. Acc. | Tile Width
         * --------------|-------------|----------------|------------|---------------
         * (inf, 1x]     |     Yes     |   (inf, 1x]    | 6-tap      | Input = L
         *               |             |    (1x, 1/2x)  | 6/4n-tap   | Input = L
         *               |             |  [1/2x, 1/inf) | 4n-tap/src | Input = L / 2
         *   (1x, 1/2x)  |     No      |   (inf, 1x]    | 6-tap      | Output = L
         *               |             |    (1x, 1/2x)  | 6/4n-tap   | Output = L
         *               |             |  [1/2x, 1/inf) | 4n-tap/src | Output = L / 2
         * [1/2x, 1/inf) |     No      |   (inf, 1x]    | 6-tap      | Output = L
         *               |             |    (1x, 1/2x)  | 6/4n-tap   | Output = L / 2
         *               |             |  [1/2x, 1/inf) | 4n-tap/src | Output = L / 2
         */
        if (data->ver_first) /* vertical first */
        {
            if (SCALER_6_TAPS == data->ver_algo ||
                data->ver_cubic_trunc)
            {
                ptr_func->in_tile_width = data->max_width;    // FIFO 544
            }
            else
            {
                ptr_func->in_tile_width = data->max_width >> 1;    // FIFO 272
            }
        }
        else
        {
            if (SCALER_6_TAPS == data->ver_algo ||
                data->ver_cubic_trunc)
            {
                ptr_func->out_tile_width = data->max_width - 2;   // FIFO 544
                data->out_tile_w = data->max_width;
            }
            else
            {
                ptr_func->out_tile_width = (data->max_width >> 1) - 2;   // FIFO 272
                data->out_tile_w = data->max_width >> 1;
            }
        }
    }

    ptr_func->in_tile_height  = 16000;
    ptr_func->out_tile_height = 16000;

    if (ptr_func->in_stream_order & TILE_ORDER_BOTTOM_TO_TOP)   // For Y Flip read, and then t bottom to top
    {
        int32_t bias_y;
        int32_t offset_y;

        if (data->crop.y_sub_px)
        {
            bias_y = ptr_func->full_size_y_in - data->crop.r.height - data->crop.r.top - 1;
            offset_y = (1 << TILE_SCALER_SUBPIXEL_SHIFT) - data->crop.y_sub_px;
        }
        else
        {
            bias_y = ptr_func->full_size_y_in - data->crop.r.height - data->crop.r.top;
            offset_y = 0;
        }

        data->crop.r.top = bias_y;
        data->crop.y_sub_px = offset_y;
    }

    // urs: C24 upsampler input frame width
    data->c24_in_frame_w = (ptr_func->full_size_x_out + 0x01) & ~0x1;

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_tdshp_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct tdshp_tile_data *data = (struct tdshp_tile_data*)(ptr_func->func_data);

    UNUSED(ptr_tile_reg_map);

    if (NULL == data)
    {
        return MDP_MESSAGE_PRZ_NULL_DATA;
    }

    ptr_func->type |= TILE_TYPE_CROP_EN;
    ptr_func->in_tile_width   = data->max_width;
    ptr_func->out_tile_width  = data->max_width;
    ptr_func->in_tile_height  = 16000;
    ptr_func->out_tile_height = 16000;
    ptr_func->l_tile_loss     = 3;
    ptr_func->r_tile_loss     = 3;
    ptr_func->t_tile_loss     = 2;
    ptr_func->b_tile_loss     = 2;

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_wrot_init(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct wrot_tile_data *data = (struct wrot_tile_data*)(ptr_func->func_data);

    UNUSED(ptr_tile_reg_map);

    if (NULL == data)
    {
        return MDP_MESSAGE_WROT_NULL_DATA;
    }

    // WROT support crop capability
    ptr_func->type |= TILE_TYPE_CROP_EN;

    if ((MML_ROT_90  == data->rotate) ||
        (MML_ROT_270 == data->rotate) ||
        data->flip) /* 90, 270 degrees and flip */
    {
        ptr_func->out_tile_width = data->max_width;
    }
    else
    {
        ptr_func->out_tile_width = data->max_width * 2;
    }

    if (MML_FMT_COMPRESS(data->dest_fmt))
    {
        ptr_func->out_tile_width = 128;
    }

    if (MML_FMT_H_SUBSAMPLE(data->dest_fmt) &&
        !MML_FMT_V_SUBSAMPLE(data->dest_fmt))
    {
        // For tile calculation
        if ((MML_ROT_90  == data->rotate) ||
            (MML_ROT_270 == data->rotate)) /* 90, 270 degrees & YUV422 */
        {
            /* To update with rotation */
            ptr_func->out_const_x = 2;
            ptr_func->out_const_y = 2;
        }
        else
        {
            /* To update with rotation */
            ptr_func->out_const_x = 2;
        }
    }
    else if (MML_FMT_H_SUBSAMPLE(data->dest_fmt) &&
        MML_FMT_V_SUBSAMPLE(data->dest_fmt))
    {
        // For tile calculation
        ptr_func->out_const_x = 2;
        ptr_func->out_const_y = 2;
    }
    else if ((data->dest_fmt != MML_FMT_GREY) && !MML_FMT_IS_RGB(data->dest_fmt))
    {
        // TODO: set FIFO and Line max for YUV444 (DP_COLOR_I444, DP_COLOR_YV24)
        ASSERT(0);
        return MDP_MESSAGE_WROT_INVALID_FORMAT;
    }

    ptr_func->in_tile_height  = 16000;
    ptr_func->out_tile_height = 16000;

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_rdma_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct rdma_tile_data *data = (struct rdma_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_RDMA_NULL_DATA;
    }

    if(data->alpharot)
    {
        if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
        {
            ptr_func->out_pos_xs = ptr_func->in_pos_xs - ptr_func->crop_bias_x;
            ptr_func->out_pos_xe = ptr_func->in_pos_xe - ptr_func->crop_bias_x;
        }

        if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
        {
            ptr_func->out_pos_ys = ptr_func->in_pos_ys - ptr_func->crop_bias_y;
            ptr_func->out_pos_ye = ptr_func->in_pos_ye - ptr_func->crop_bias_y;
        }
    }

    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {
        if (ptr_func->backward_output_xs_pos >= ptr_func->out_pos_xs)
        {
            ptr_func->bias_x = ptr_func->backward_output_xs_pos - ptr_func->out_pos_xs;
            ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
        }

        if (ptr_func->out_pos_xe > ptr_func->backward_output_xe_pos)
        {
            ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
        }
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
        if (ptr_func->backward_output_ys_pos >= ptr_func->out_pos_ys)
        {
            ptr_func->bias_y = ptr_func->backward_output_ys_pos - ptr_func->out_pos_ys;
            ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;
        }

        if (ptr_func->out_pos_ye > ptr_func->backward_output_ye_pos)
        {
            ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
        }
    }

    // skip frame mode
    if (ptr_tile_reg_map->first_frame)
    {
        return ISP_MESSAGE_TILE_OK;
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_hdr_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    // skip frame mode
    if (ptr_tile_reg_map->first_frame)
    {
        return ISP_MESSAGE_TILE_OK;
    }

    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {
        if (ptr_func->backward_output_xs_pos >= ptr_func->out_pos_xs)
        {
            ptr_func->bias_x = ptr_func->backward_output_xs_pos - ptr_func->out_pos_xs;
            ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
        }
        else
        {
            return MDP_MESSAGE_TDSHP_BACK_LT_FORWARD;
        }

        // Should we crop the extra output?
        if (ptr_func->out_pos_xe > ptr_func->backward_output_xe_pos)
        {
            ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
        }
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
        if (ptr_func->backward_output_ys_pos >= ptr_func->out_pos_ys)
        {
            ptr_func->bias_y = ptr_func->backward_output_ys_pos - ptr_func->out_pos_ys;
            ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;
        }
        else
        {
            return MDP_MESSAGE_TDSHP_BACK_LT_FORWARD;
        }

        // Should we crop the extra output?
        if (ptr_func->out_pos_ye > ptr_func->backward_output_ye_pos)
        {
            ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
        }
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_aal_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    // skip frame mode
    if (ptr_tile_reg_map->first_frame)
    {
        return ISP_MESSAGE_TILE_OK;
    }

    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {

        if (ptr_func->out_tile_width)
        {

            if (ptr_func->out_pos_xe + 1 > ptr_func->out_pos_xs + ptr_func->out_tile_width)
            {
                ptr_func->out_pos_xe = ptr_func->out_pos_xs + ptr_func->out_tile_width - 1;
                ptr_func->h_end_flag = false;
            }
        }
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {

        if (ptr_func->out_tile_height)
        {
            if (ptr_func->out_pos_ye + 1 > ptr_func->out_pos_ys + ptr_func->out_tile_height)
            {
                ptr_func->out_pos_ye = ptr_func->out_pos_ys + ptr_func->out_tile_height - 1;
            }
        }
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_prz_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    int32_t         C42OutXLeft;
    int32_t         C42OutXRight;
    int32_t         C24InXLeft;
    int32_t         C24InXRight;
#if USE_DP_TILE_SCALER

#else
    ISP_TILE_MESSAGE_ENUM result;
    TILE_RESIZER_FORWARD_CAL_ARG_STRUCT *ptr_for_arg = &ptr_tile_reg_map->for_arg;
#endif
		struct rsz_tile_data *data = (struct rsz_tile_data*)(ptr_func->func_data);

    if (NULL == data)
    {
        return MDP_MESSAGE_PRZ_NULL_DATA;
    }

    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {
        // drs: C42 downsampler forward
        if (data->use_121filter && (ptr_func->in_pos_xs > 0))
        {
            C42OutXLeft = ptr_func->in_pos_xs + 2;  // Fixed 2 column tile loss for 121 filter
        }
        else
        {
            C42OutXLeft = ptr_func->in_pos_xs;
        }

        if ((ptr_func->in_pos_xe + 1) >= ptr_func->full_size_x_in)
        {
            C42OutXRight = data->c42_out_frame_w - 1;
        }
        else
        {
            C42OutXRight = ptr_func->in_pos_xe;

            /* tile calculation: xe not end position & is odd
             HW behavior: prz in x size = drs out x size = (drs in x size + 0x01) & ~0x01
                          prz out x size = urs in x size = (urs out x size + 0x01) & ~0x01
             can match tile caculation
             HW only needs to fill in drs in x size & urs out x size */
            if (0 == (ptr_func->in_pos_xe & 0x1))
            {
                C42OutXRight -= 1;
            }
        }

        // prz
#if USE_DP_TILE_SCALER
        switch (data->hor_algo)
        {
            case SCALER_6_TAPS:
                forward_6_taps(C42OutXLeft,    // C42 out = Scaler input
                               C42OutXRight,   // C42 out = Scaler input
                               data->c42_out_frame_w - 1,
                               data->coef_step_x,
                               data->precision_x,
                               data->crop.r.left,
                               data->crop.x_sub_px,
                               data->c24_in_frame_w - 1,
                               2,
                               data->back_xs,
                               ptr_func->out_cal_order,
                               &C24InXLeft,     // C24 in = Scaler output
                               &C24InXRight,    // C24 in = Scaler output
                               &ptr_func->bias_x,
                               &ptr_func->offset_x,
                               &ptr_func->bias_x_c,
                               &ptr_func->offset_x_c);
                break;
            case SCALER_SRC_ACC:
                forward_src_acc(C42OutXLeft,   // C42 out = Scaler input
                                C42OutXRight,  // C42 out = Scaler input
                                data->c42_out_frame_w - 1,
                                data->coef_step_x,
                                data->precision_x,
                                data->crop.r.left,
                                data->crop.x_sub_px,
                                data->c24_in_frame_w - 1,
                                2,
                                data->back_xs,
                                ptr_func->out_cal_order,
                                &C24InXLeft,    // C24 in = Scaler output
                                &C24InXRight,   // C24 in = Scaler output
                                &ptr_func->bias_x,
                                &ptr_func->offset_x,
                                &ptr_func->bias_x_c,
                                &ptr_func->offset_x_c);
                break;
            case SCALER_CUB_ACC:
                forward_cub_acc(C42OutXLeft,   // C42 out = Scaler input
                                C42OutXRight,  // C42 out = Scaler input
                                data->c42_out_frame_w - 1,
                                data->coef_step_x,
                                data->precision_x,
                                data->crop.r.left,
                                data->crop.x_sub_px,
                                data->c24_in_frame_w - 1,
                                2,
                                data->back_xs,
                                ptr_func->out_cal_order,
                                &C24InXLeft,    // C24 in = Scaler output
                                &C24InXRight,   // C24 in = Scaler output
                                &ptr_func->bias_x,
                                &ptr_func->offset_x,
                                &ptr_func->bias_x_c,
                                &ptr_func->offset_x_c);
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_RESIZER_SCALING_ERROR;
                break;
        }
#else
        switch (data->hor_algo)
        {
            case SCALER_6_TAPS:
                ptr_for_arg->mode = TILE_RESIZER_MODE_MDP_6_TAPES;
                ptr_for_arg->prec_bits = TILE_RESIZER_N_TP_PREC_BITS;
                break;
            case SCALER_SRC_ACC:
                ptr_for_arg->mode = TILE_RESIZER_MODE_SRC_ACC;
                ptr_for_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            case SCALER_CUB_ACC:
                ptr_for_arg->mode = TILE_RESIZER_MODE_CUBIC_ACC;
                ptr_for_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_UNKNOWN_ERROR;
        }
        ptr_for_arg->coeff_step = data->coef_step_x;
        ptr_for_arg->dir_mode = CAM_DIR_X;/* x dir */
        ptr_for_arg->uv_flag = CAM_UV_422_FLAG;/* 422 */
        result = tile_for_comp_resizer(ptr_for_arg, ptr_func, &ptr_tile_reg_map->back_arg);
        TILE_CHECK_RESULT(result);
#endif

        if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
        {
            C24InXRight = data->back_xe;

            if (C24InXLeft < data->back_xs)
            {
                C24InXLeft = data->back_xs;
            }
        }
        else
        {
            C24InXLeft = data->back_xs;

            if (C24InXRight > data->back_xe)
            {
                C24InXRight = data->back_xe;
            }
        }

        // urs: C24 upsampler forward
        ptr_func->out_pos_xs = C24InXLeft;
        ptr_func->out_pos_xe = C24InXRight - 1; // Fixed 1 column tile loss for C24 upsampling while end is even

        if (C24InXRight >= (data->c24_in_frame_w - 1))
        {
            ptr_func->out_pos_xe = ptr_func->full_size_x_out - 1;
        }

        if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
        {
            if (ptr_func->out_pos_xs < ptr_func->backward_output_xs_pos)
            {
                ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
            }
        }
        else
        {
            if (ptr_func->out_pos_xe > ptr_func->backward_output_xe_pos)
            {
                ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
            }
        }

#if USE_DP_TILE_SCALER

#endif
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
        // drs: C42 downsampler forward
        // prz
#if USE_DP_TILE_SCALER
        switch (data->ver_algo)
        {
            case SCALER_6_TAPS:
                forward_6_taps(ptr_func->in_pos_ys,
                               ptr_func->in_pos_ye,
                               ptr_func->full_size_y_in - 1,
                               data->coef_step_y,
                               data->precision_y,
                               data->crop.r.top,
                               data->crop.y_sub_px,
                               ptr_func->full_size_y_out - 1,
                               ptr_func->out_const_y,
                               ptr_func->backward_output_ys_pos,
                               ptr_func->out_cal_order,
                               &ptr_func->out_pos_ys,
                               &ptr_func->out_pos_ye,
                               &ptr_func->bias_y,
                               &ptr_func->offset_y,
                               &ptr_func->bias_y_c,
                               &ptr_func->offset_y_c);
                break;
            case SCALER_SRC_ACC:
                forward_src_acc(ptr_func->in_pos_ys,
                                ptr_func->in_pos_ye,
                                ptr_func->full_size_y_in - 1,
                                data->coef_step_y,
                                data->precision_y,
                                data->crop.r.top,
                                data->crop.y_sub_px,
                                ptr_func->full_size_y_out - 1,
                                ptr_func->out_const_y,
                                ptr_func->backward_output_ys_pos,
                                ptr_func->out_cal_order,
                                &ptr_func->out_pos_ys,
                                &ptr_func->out_pos_ye,
                                &ptr_func->bias_y,
                                &ptr_func->offset_y,
                                &ptr_func->bias_y_c,
                                &ptr_func->offset_y_c);
                break;
            case SCALER_CUB_ACC:
                forward_cub_acc(ptr_func->in_pos_ys,
                                ptr_func->in_pos_ye,
                                ptr_func->full_size_y_in - 1,
                                data->coef_step_y,
                                data->precision_y,
                                data->crop.r.top,
                                data->crop.y_sub_px,
                                ptr_func->full_size_y_out - 1,
                                ptr_func->out_const_y,
                                ptr_func->backward_output_ys_pos,
                                ptr_func->out_cal_order,
                                &ptr_func->out_pos_ys,
                                &ptr_func->out_pos_ye,
                                &ptr_func->bias_y,
                                &ptr_func->offset_y,
                                &ptr_func->bias_y_c,
                                &ptr_func->offset_y_c);
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_RESIZER_SCALING_ERROR;
                break;
        }
#else
        switch (data->ver_algo)
        {
            case SCALER_6_TAPS:
                ptr_for_arg->mode = TILE_RESIZER_MODE_MDP_6_TAPES;
                ptr_for_arg->prec_bits = TILE_RESIZER_N_TP_PREC_BITS;
                break;
            case SCALER_SRC_ACC:
                ptr_for_arg->mode = TILE_RESIZER_MODE_SRC_ACC;
                ptr_for_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            case SCALER_CUB_ACC:
                ptr_for_arg->mode = TILE_RESIZER_MODE_CUBIC_ACC;
                ptr_for_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_UNKNOWN_ERROR;
        }
        ptr_for_arg->coeff_step = data->coef_step_y;
        ptr_for_arg->dir_mode = CAM_DIR_Y;/* y dir */
        ptr_for_arg->uv_flag = CAM_UV_444_FLAG;/* 444 */
        result = tile_for_comp_resizer(ptr_for_arg, ptr_func, &ptr_tile_reg_map->back_arg);
        TILE_CHECK_RESULT(result);
#endif

        ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;

        if (ptr_func->out_pos_ye > ptr_func->backward_output_ye_pos)
        {
            ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
        }

        // urs: C24 upsampler forward

#if USE_DP_TILE_SCALER

#endif
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_wrot_for(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    int32_t remain;
    struct wrot_tile_data *data = (struct wrot_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_WROT_NULL_DATA;
    }

    // frame mode
    if (ptr_tile_reg_map->first_frame)
    {
        if (data->enable_crop &&
            (false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
        {
            if (ptr_func->min_out_pos_xs > ptr_func->out_pos_xs)
            {
                ptr_func->out_pos_xs = ptr_func->min_out_pos_xs;
            }
            if (ptr_func->out_pos_xe > ptr_func->max_out_pos_xe)
            {
                ptr_func->out_pos_xe = ptr_func->max_out_pos_xe;
            }
        }
        return ISP_MESSAGE_TILE_OK;
    }

    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {
        ptr_func->out_pos_xs = ptr_func->in_pos_xs;
        ptr_func->out_pos_xe = ptr_func->in_pos_xe;

        if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
        {
            if (ptr_func->backward_output_xe_pos < ptr_func->out_pos_xe)
            {
                ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
            }

            if (ptr_func->out_pos_xs < ptr_func->backward_output_xs_pos )
            {
                ptr_func->bias_x = ptr_func->backward_output_xs_pos - ptr_func->out_pos_xs;
                ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
            }
            else
            {
                ptr_func->bias_x = 0;
                /* Check out xs alignment */
                if (1 < ptr_func->out_const_x)
                {
                    remain = TILE_MOD(ptr_func->out_pos_xs, ptr_func->out_const_x);
                    if (0 != remain)
                    {
                        ptr_func->out_pos_xs += ptr_func->out_const_x - remain;
                        ptr_func->bias_x = ptr_func->out_const_x - remain;
                    }
                }
            }
        }
        else //Normal order
        {
            if (ptr_func->backward_output_xs_pos >= ptr_func->out_pos_xs)
            {
                ptr_func->bias_x = ptr_func->backward_output_xs_pos - ptr_func->out_pos_xs;
                ptr_func->out_pos_xs = ptr_func->backward_output_xs_pos;
            }

            if (ptr_func->out_pos_xe > ptr_func->backward_output_xe_pos )
            {
                ptr_func->out_pos_xe = ptr_func->backward_output_xe_pos;
            }
            else
            {
                /* Check out xe alignment */
                if (1 < ptr_func->out_const_x)
                {
                    remain = TILE_MOD(ptr_func->out_pos_xe + 1, ptr_func->out_const_x);
                    if (0 != remain)
                    {
                        ptr_func->out_pos_xe -= remain;
                    }
                }

                /* Check out width alignment */
                if (MML_FMT_10BIT_PACKED(data->dest_fmt))
                {
                    remain = 0;
                    if ((MML_ROT_0 == data->rotate && data->flip) ||
                        (MML_ROT_180 == data->rotate && !data->flip) ||
                        (MML_ROT_270 == data->rotate))
                    {
                        // first tile padding
                        if (0 == ptr_func->out_pos_xs)
                        {
                            remain = TILE_MOD(ptr_func->full_size_x_out - ptr_func->out_pos_xe - 1, 4);
                            if (0 != remain)
                            {
                                remain = 4 - remain;
                            }
                        }
                        else
                        {
                            remain = TILE_MOD(ptr_func->out_pos_xe - ptr_func->out_pos_xs + 1, 4);
                        }
                    }
                    else
                    {
                        // last tile padding
                        if ((ptr_func->out_pos_xe + 1) < ptr_func->full_size_x_out)
                        {
                            remain = TILE_MOD(ptr_func->out_pos_xe - ptr_func->out_pos_xs + 1, 4);
                        }
                    }
                    if (0 != remain)
                    {
                        ptr_func->out_pos_xe = ptr_func->out_pos_xe - remain;
                    }
                }
            }
        }
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
        ptr_func->out_pos_ys =  ptr_func->in_pos_ys;
        ptr_func->out_pos_ye =  ptr_func->in_pos_ye;

        if (ptr_func->backward_output_ys_pos >= ptr_func->out_pos_ys)
        {
            ptr_func->bias_y = ptr_func->backward_output_ys_pos - ptr_func->out_pos_ys;
            ptr_func->out_pos_ys = ptr_func->backward_output_ys_pos;
        }

        if (ptr_func->out_pos_ye > ptr_func->backward_output_ye_pos )
        {
            ptr_func->out_pos_ye = ptr_func->backward_output_ye_pos;
        }
        else
        {
            /* Check out ye alignment */
            if (1 < ptr_func->out_const_y)
            {
                remain = TILE_MOD(ptr_func->out_pos_ye + 1, ptr_func->out_const_y);
                if (0 != remain)
                {
                    ptr_func->out_pos_ye -= remain;
                }
            }
        }
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_rdma_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct rdma_tile_data *data = (struct rdma_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_RDMA_NULL_DATA;
    }

    if (data->alpharot)
    {
        if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
        {
            ptr_func->in_pos_xs = ptr_func->out_pos_xs + ptr_func->crop_bias_x;
            ptr_func->in_pos_xe = ptr_func->out_pos_xe + ptr_func->crop_bias_x;
        }

        if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
        {
            ptr_func->in_pos_ys = ptr_func->out_pos_ys + ptr_func->crop_bias_y;
            ptr_func->in_pos_ye = ptr_func->out_pos_ye + ptr_func->crop_bias_y;
        }
    }

    // frame mode
    if (ptr_tile_reg_map->first_frame)
    {
#if RDMA_CROP_LEFT_TOP
        if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
        {
            if (ptr_func->in_pos_xs < data->crop.left)
            {
                ptr_func->in_pos_xs = data->crop.left;
            }
        }

        if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
        {
            if (ptr_func->in_pos_ys < data->crop.top)
            {
                ptr_func->in_pos_ys = data->crop.top;
            }
        }
#endif

        // Specific handle for block and ring buffer mode
        if ((ptr_func->in_pos_xe + 1) > (int)(data->crop.left + data->crop.width))
        {
            ptr_func->in_pos_xe = data->crop.left + data->crop.width - 1;
        }

        if (MML_FMT_BLOCK(data->src_fmt))
        {
            ptr_func->in_pos_xe = ((1 + (ptr_func->in_pos_xe >> data->blk_shift_w)) << data->blk_shift_w) - 1;  // Alignment X right in block boundary

            if ((ptr_func->in_pos_xe + 1) > ptr_func->full_size_x_in)
            {
                ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
            }
        }

        if (1 != ptr_func->in_const_x)
        {
            int32_t remain;
            remain = TILE_MOD(ptr_func->in_pos_xe + 1, ptr_func->in_const_x);
            if (0 != remain)
            {
                ptr_func->in_pos_xe += remain;
            }
        }

        if ((ptr_func->in_pos_ye + 1) > (int)(data->crop.top + data->crop.height))
        {
            ptr_func->in_pos_ye = data->crop.top + data->crop.height - 1;
        }

        if (MML_FMT_BLOCK(data->src_fmt))
        {
            ptr_func->in_pos_ye = ((1 + (ptr_func->in_pos_ye >> data->blk_shift_h)) << data->blk_shift_h) - 1;  // Alignment Y bottom in block boundary

            if ((ptr_func->in_pos_ye + 1) > ptr_func->full_size_y_in)
            {
                ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
            }
        }

        if (1 != ptr_func->in_const_y)
        {
            int32_t remain;
            remain = TILE_MOD(ptr_func->in_pos_ye + 1, ptr_func->in_const_y);
            if (0 != remain)
            {
                ptr_func->in_pos_ye += remain;
            }
        }
        return ISP_MESSAGE_TILE_OK;
    }

    // Specific handle for block and ring buffer mode
    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {
        if ((ptr_func->in_pos_xe + 1) > (int)(data->crop.left + data->crop.width))
        {
            ptr_func->in_pos_xe = data->crop.left + data->crop.width - 1;
        }

        if (MML_FMT_BLOCK(data->src_fmt))
        {
            int32_t tmpLeft;
            tmpLeft = ((ptr_func->in_pos_xs >> data->blk_shift_w) << data->blk_shift_w);   // Alignment X left in block boundary

            // For video block mode, FIFO limit is before crop
            if ((ptr_func->in_pos_xe + 1) > (tmpLeft + ptr_func->in_tile_width))
            {
                ptr_func->in_pos_xe = tmpLeft + ptr_func->in_tile_width - 1;
            }

            ptr_func->in_pos_xe = ((1 + (ptr_func->in_pos_xe >> data->blk_shift_w)) << data->blk_shift_w) - 1;  // Alignment X right in block boundary

            if ((ptr_func->in_pos_xe + 1) > ptr_func->full_size_x_in)
            {
                ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
            }
        }

        if (1 != ptr_func->in_const_x)
        {
            int32_t remain;
            remain = TILE_MOD(ptr_func->in_pos_xe + 1, ptr_func->in_const_x);
            if (0 != remain)
            {
                ptr_func->in_pos_xe += remain;
            }
        }
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
        if ((ptr_func->in_pos_ye + 1) > (int)(data->crop.top + data->crop.height))
        {
            ptr_func->in_pos_ye = data->crop.top + data->crop.height - 1;
        }

        if (MML_FMT_BLOCK(data->src_fmt))
        {
            int32_t tempTop;
            tempTop = ((ptr_func->in_pos_ys >> data->blk_shift_h) << data->blk_shift_h);   // Alignment Y top in block boundary

            // For video block mode, FIFO limit is before crop
            if ((ptr_func->in_pos_ye + 1) > (tempTop + ptr_func->in_tile_height))
            {
                ptr_func->in_pos_ye = tempTop + ptr_func->in_tile_height - 1;
            }

            ptr_func->in_pos_ye = ((1 + (ptr_func->in_pos_ye >> data->blk_shift_h)) << data->blk_shift_h) - 1;  // Alignment Y bottom in block boundary

            if ((ptr_func->in_pos_ye + 1) > ptr_func->full_size_y_in)
            {
                ptr_func->in_pos_ye = ptr_func->full_size_y_in - 1;
            }
        }

        if (1 != ptr_func->in_const_y)
        {
            int32_t remain;
            remain = TILE_MOD(ptr_func->in_pos_ye + 1, ptr_func->in_const_y);
            if (0 != remain)
            {
                ptr_func->in_pos_ye += remain;
            }
        }
    }
    return ISP_MESSAGE_TILE_OK;
}

/*
static ISP_TILE_MESSAGE_ENUM tile_aal_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    struct aal_tile_data *data = (struct aal_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_AAL_NULL_DATA;
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
        if (data->m_YCropFromFrameTop)
        {
            // Read from top
            ptr_func->in_pos_ys = 0;
            ptr_func->out_pos_ys = 0;
        }
    }

    return ISP_MESSAGE_TILE_OK;
}
*/

static ISP_TILE_MESSAGE_ENUM tile_prz_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
		int32_t         C24InXLeft;
    int32_t         C24InXRight;
    int32_t         C42OutXLeft;
    int32_t         C42OutXRight;
#if USE_DP_TILE_SCALER

#else
    ISP_TILE_MESSAGE_ENUM result;
    TILE_RESIZER_BACKWARD_CAL_ARG_STRUCT *ptr_back_arg = &ptr_tile_reg_map->back_arg;
#endif
    struct rsz_tile_data *data = (struct rsz_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_PRZ_NULL_DATA;
    }

    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {
        // urs: C24 upsampler backward
        C24InXLeft  = ptr_func->out_pos_xs;

        if (C24InXLeft & 0x1)
        {
            C24InXLeft -= 1;
        }

        if (ptr_func->out_tile_width)
        {
            if (ptr_func->out_pos_xe + 1 > C24InXLeft + ptr_func->out_tile_width)
            {
                ptr_func->out_pos_xe = C24InXLeft + ptr_func->out_tile_width - 1;
                ptr_func->h_end_flag = false;
            }
        }

        if ((ptr_func->out_pos_xe + 1) >= ptr_func->full_size_x_out)
        {
            C24InXRight = data->c24_in_frame_w - 1;
        }
        else
        {
            C24InXRight = ptr_func->out_pos_xe + 2; // Fixed 2 column tile loss for C24 upsampling while end is odd

            if (0 == (ptr_func->out_pos_xe & 0x1))
            {
                C24InXRight -= 1;
            }
        }

        // prz
        if (data->out_tile_w && ptr_func->out_tile_width)
        {
            if (C24InXRight + 1 > C24InXLeft + data->out_tile_w)
            {
                if (ptr_func->out_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
                {
                    C24InXLeft = C24InXRight - data->out_tile_w + 1;
                }
                else
                {
                    C24InXRight = C24InXLeft + data->out_tile_w - 1;
                }
            }
        }

        if (C24InXRight + 1 > data->c24_in_frame_w)
        {
            C24InXRight = data->c24_in_frame_w - 1;
        }
        if (C24InXLeft < 0)
        {
            C24InXLeft = 0;
        }

#if USE_DP_TILE_SCALER
        switch (data->hor_algo)
        {
            case SCALER_6_TAPS:
                backward_6_taps(C24InXLeft,        // C24 in = Scaler output
                                C24InXRight,       // C24 in = Scaler output
                                data->c24_in_frame_w - 1,
                                data->coef_step_x,
                                data->precision_x,
                                data->crop.r.left,
                                data->crop.x_sub_px,
                                data->c42_out_frame_w - 1,
                                2,
                                &C42OutXLeft,       // C42 out = Scaler input
                                &C42OutXRight);     // C42 out = Scaler input
                break;
            case SCALER_SRC_ACC:
                backward_src_acc(C24InXLeft,       // C24 in = Scaler output
                                 C24InXRight,      // C24 in = Scaler output
                                 data->c24_in_frame_w - 1,
                                 data->coef_step_x,
                                 data->precision_x,
                                 data->crop.r.left,
                                 data->crop.x_sub_px,
                                 data->c42_out_frame_w - 1,
                                 2,
                                 &C42OutXLeft,      // C42 out = Scaler input
                                 &C42OutXRight);    // C42 out = Scaler input
                break;
            case SCALER_CUB_ACC:
                backward_cub_acc(C24InXLeft,       // C24 in = Scaler output
                                 C24InXRight,      // C24 in = Scaler output
                                 data->c24_in_frame_w - 1,
                                 data->coef_step_x,
                                 data->precision_x,
                                 data->crop.r.left,
                                 data->crop.x_sub_px,
                                 data->c42_out_frame_w - 1,
                                 2,
                                 &C42OutXLeft,      // C42 out = Scaler input
                                 &C42OutXRight);    // C42 out = Scaler input
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_RESIZER_SCALING_ERROR;
                break;
        }
#else
        switch (data->hor_algo)
        {
            case SCALER_6_TAPS:
                ptr_back_arg->mode = TILE_RESIZER_MODE_MDP_6_TAPES;
                ptr_back_arg->prec_bits = TILE_RESIZER_N_TP_PREC_BITS;
                break;
            case SCALER_SRC_ACC:
                ptr_back_arg->mode = TILE_RESIZER_MODE_SRC_ACC;
                ptr_back_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            case SCALER_CUB_ACC:
                ptr_back_arg->mode = TILE_RESIZER_MODE_CUBIC_ACC;
                ptr_back_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_UNKNOWN_ERROR;
        }
        ptr_back_arg->coeff_step = data->coef_step_x;
        ptr_back_arg->dir_mode = CAM_DIR_X;/* x dir */
        ptr_back_arg->uv_flag = CAM_UV_422_FLAG;/* 422 */
        result = tile_back_comp_resizer(ptr_back_arg, ptr_func);
        TILE_CHECK_RESULT(result);
#endif

        if (ptr_func->in_tile_width)
        {
            if (C42OutXRight + 1 > C42OutXLeft + ptr_func->in_tile_width)
            {
                if (ptr_func->in_cal_order & TILE_ORDER_RIGHT_TO_LEFT)
                {
                    C42OutXLeft = C42OutXRight - ptr_func->in_tile_width + 1;
                }
                else
                {
                    C42OutXRight = C42OutXLeft + ptr_func->in_tile_width - 1;
                }
            }
        }
        data->back_xs  = C24InXLeft;
        data->back_xe = C24InXRight;

        // drs: C42 downsampler backward
        ptr_func->in_pos_xs = C42OutXLeft;
        ptr_func->in_pos_xe = C42OutXRight;

        if (data->use_121filter)
        {
            ptr_func->in_pos_xs -= 2;   // Fixed 2 column tile loss for 121 filter
        }

        if (ptr_func->in_pos_xs < 0)
        {
            ptr_func->in_pos_xs = 0;
        }
        if (ptr_func->in_pos_xe + 1 > ptr_func->full_size_x_in)
        {
            ptr_func->in_pos_xe = ptr_func->full_size_x_in - 1;
        }

#if USE_DP_TILE_SCALER

#endif
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
#if 0
        // urs: C24 upsampler backward
        prz_out_pos_ys = ptr_func->out_pos_ys;
        prz_out_pos_ye = ptr_func->out_pos_ye;

        // prz
        if (ptr_func->out_tile_height)
        {
            if (prz_out_pos_ye + 1 > prz_out_pos_ys + ptr_func->out_tile_height)
            {
                prz_out_pos_ye = prz_out_pos_ys + ptr_func->out_tile_height - 1;
            }
        }

        if (prz_out_pos_ye + 1 > prz_full_size_y_out)
        {
            prz_out_pos_ye = prz_full_size_y_out - 1;
        }
        if (prz_out_pos_ys < 0)
        {
            prz_out_pos_ys = 0;
        }
#endif

#if USE_DP_TILE_SCALER
        switch (data->ver_algo)
        {
            case SCALER_6_TAPS:
                backward_6_taps(ptr_func->out_pos_ys,
                                ptr_func->out_pos_ye,
                                ptr_func->full_size_y_out - 1,
                                data->coef_step_y,
                                data->precision_y,
                                data->crop.r.top,
                                data->crop.y_sub_px,
                                ptr_func->full_size_y_in - 1,
                                ptr_func->in_const_y,
                                &ptr_func->in_pos_ys,
                                &ptr_func->in_pos_ye);
                break;
            case SCALER_SRC_ACC:
                backward_src_acc(ptr_func->out_pos_ys,
                                 ptr_func->out_pos_ye,
                                 ptr_func->full_size_y_out - 1,
                                 data->coef_step_y,
                                 data->precision_y,
                                 data->crop.r.top,
                                 data->crop.y_sub_px,
                                 ptr_func->full_size_y_in - 1,
                                 ptr_func->in_const_y,
                                 &ptr_func->in_pos_ys,
                                 &ptr_func->in_pos_ye);
                break;
            case SCALER_CUB_ACC:
                backward_cub_acc(ptr_func->out_pos_ys,
                                 ptr_func->out_pos_ye,
                                 ptr_func->full_size_y_out - 1,
                                 data->coef_step_y,
                                 data->precision_y,
                                 data->crop.r.top,
                                 data->crop.y_sub_px,
                                 ptr_func->full_size_y_in - 1,
                                 ptr_func->in_const_y,
                                 &ptr_func->in_pos_ys,
                                 &ptr_func->in_pos_ye);
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_RESIZER_SCALING_ERROR;
                break;
        }
#else
        switch (data->ver_algo)
        {
            case SCALER_6_TAPS:
                ptr_back_arg->mode = TILE_RESIZER_MODE_MDP_6_TAPES;
                ptr_back_arg->prec_bits = TILE_RESIZER_N_TP_PREC_BITS;
                break;
            case SCALER_SRC_ACC:
                ptr_back_arg->mode = TILE_RESIZER_MODE_SRC_ACC;
                ptr_back_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            case SCALER_CUB_ACC:
                ptr_back_arg->mode = TILE_RESIZER_MODE_CUBIC_ACC;
                ptr_back_arg->prec_bits = TILE_RESIZER_ACC_PREC_BITS;
                break;
            default:
                ASSERT(0);
                return MDP_MESSAGE_UNKNOWN_ERROR;
        }
        ptr_back_arg->coeff_step = data->coef_step_y;
        ptr_back_arg->dir_mode = CAM_DIR_Y;/* y dir */
        ptr_back_arg->uv_flag = CAM_UV_444_FLAG;/* 444 */
        result = tile_back_comp_resizer(ptr_back_arg, ptr_func);
        TILE_CHECK_RESULT(result);
#endif

        // drs: C42 downsampler backward

#if USE_DP_TILE_SCALER

#endif
    }

    return ISP_MESSAGE_TILE_OK;
}

static ISP_TILE_MESSAGE_ENUM tile_wrot_back(TILE_FUNC_BLOCK_STRUCT *ptr_func, TILE_REG_MAP_STRUCT* ptr_tile_reg_map)
{
    int32_t remain;
    int32_t alignment = 1;
    struct wrot_tile_data *data = (struct wrot_tile_data*)(ptr_func->func_data);
    if (NULL == data)
    {
        return MDP_MESSAGE_WROT_NULL_DATA;
    }

    // frame mode
    if (ptr_tile_reg_map->first_frame)
    {
        if (data->enable_crop &&
            (false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
        {
            ptr_func->out_pos_xs = data->crop_left;
            ptr_func->out_pos_xe = data->crop_left + data->crop_width - 1;
            ptr_func->in_pos_xs = ptr_func->out_pos_xs;
            ptr_func->in_pos_xe = ptr_func->out_pos_xe;
            ptr_func->min_out_pos_xs = ptr_func->out_pos_xs;
            ptr_func->max_out_pos_xe = ptr_func->out_pos_xe;
        }
        return ISP_MESSAGE_TILE_OK;
    }

    if ((false == ptr_tile_reg_map->skip_x_cal) && (false == ptr_func->tdr_h_disable_flag))
    {
        int full_size_x_out = ptr_func->full_size_x_out;

        if (data->enable_crop)
        {
            if (ptr_func->valid_h_no == 0)
            {
                /* first tile */
                ptr_func->out_pos_xs = data->crop_left;
                ptr_func->in_pos_xs = ptr_func->out_pos_xs;
            }
            if (ptr_func->out_tile_width)
            {
                ptr_func->out_pos_xe = ptr_func->out_pos_xs + ptr_func->out_tile_width - 1;
                ptr_func->in_pos_xe = ptr_func->out_pos_xe;
            }

            full_size_x_out = data->crop_left + data->crop_width;

            if ((ptr_func->out_pos_xe + 1) >= full_size_x_out)
            {
                ptr_func->in_pos_xe = full_size_x_out - 1;
                //ptr_func->h_end_flag = true;
            }
        }

        if (data->alpharot)
        {
            if ((ptr_func->out_pos_xe + 1) < full_size_x_out)
            {
                if (ptr_func->out_pos_xe + 9 + 1 > full_size_x_out)
                {
                    if (ptr_func->out_pos_xe != ptr_func->out_pos_xs)
                    {
                        ptr_func->out_pos_xe = full_size_x_out - 9 - 1;
                        ptr_func->in_pos_xe  = full_size_x_out - 9 - 1;

                        ptr_func->out_pos_xe = ((ptr_func->out_pos_xe + 1) >> 2 << 2) - 1;
                        ptr_func->in_pos_xe  = ((ptr_func->in_pos_xe + 1) >> 2 << 2) - 1;
                    }
                }
            }
        }

        /* Check out width alignment */
        if (MML_FMT_COMPRESS(data->dest_fmt))
        {
            alignment = 32;
        }
        else if (MML_FMT_10BIT_PACKED(data->dest_fmt))
        {
            alignment = 4;
        }

        if (1 < alignment)
        {
            remain = 0;
            if ((MML_ROT_0 == data->rotate && data->flip) ||
                (MML_ROT_180 == data->rotate && !data->flip) ||
                (MML_ROT_270 == data->rotate))
            {
                // first tile padding
                if (0 == ptr_func->out_pos_xs)
                {
                    remain = TILE_MOD(full_size_x_out - ptr_func->out_pos_xe - 1, alignment);
                    if (0 != remain)
                    {
                        remain = alignment - remain;
                    }
                }
                else
                {
                    remain = TILE_MOD(ptr_func->out_pos_xe - ptr_func->out_pos_xs + 1, alignment);
                }
            }
            else
            {
                // last tile padding
                if ((ptr_func->out_pos_xe + 1) < full_size_x_out)
                {
                    remain = TILE_MOD(ptr_func->out_pos_xe - ptr_func->out_pos_xs + 1, alignment);
                }
            }
            if (0 != remain)
            {
                ptr_func->out_pos_xe = ptr_func->out_pos_xe - remain;
            }
        }
    }

    if ((false == ptr_tile_reg_map->skip_y_cal) && (false == ptr_func->tdr_v_disable_flag))
    {
    }

    return ISP_MESSAGE_TILE_OK;
}
