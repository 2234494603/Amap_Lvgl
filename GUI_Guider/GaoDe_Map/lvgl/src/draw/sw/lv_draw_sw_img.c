/**
 * @file lv_draw_img.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_sw.h"
#include "../lv_img_cache.h"
#include "../../hal/lv_hal_disp.h"
#include "../../misc/lv_log.h"
#include "../../core/lv_refr.h"
#include "../../misc/lv_mem.h"
#include "../../misc/lv_math.h"

/*********************
 *      DEFINES
 *********************/
#define MAX_BUF_SIZE (uint32_t) lv_disp_get_hor_res(_lv_refr_get_disp_refreshing())

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void convert_cb(const lv_area_t * dest_area, const void * src_buf, lv_coord_t src_w, lv_coord_t src_h,
                       uint32_t src_stride, const lv_draw_img_dsc_t * draw_dsc, lv_img_cf_t cf, lv_color_t * cbuf, lv_opa_t * abuf);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/


void LV_ATTRIBUTE_FAST_MEM lv_draw_sw_img_decoded(struct _lv_draw_ctx_t * draw_ctx,
                                                  const lv_draw_img_dsc_t * draw_dsc,
                                                  const lv_area_t * coords, const uint8_t * src_buf,
                                                  lv_img_cf_t cf)
{
    /*Use the clip area as draw area*/
    lv_area_t draw_area;
    lv_area_copy(&draw_area, draw_ctx->clip_area);

    bool mask_any = lv_draw_mask_is_any(&draw_area);
    bool transform = draw_dsc->angle != 0 || draw_dsc->zoom != LV_IMG_ZOOM_NONE ? true : false;

    /* draw_dsc->stride 是源图字节行宽（lv_img_stride_get() 已按 header.reserved 完成 0/1/2/3 → 紧凑/4/8/16 字节对齐）。
       TRUE_COLOR 快速路径经 dsc->src_stride 透传给 blend（硬件 blit 或 SW 行切片）；
       mask 面快速路径（ALPHA_8BIT/RGB565A8）仍要求紧凑；
       逐像素路径（convert_cb/lv_draw_transform）按字节域 stride 处理 */
    uint32_t px_size_bytes = lv_img_cf_get_px_size(cf) >> 3;
    lv_coord_t src_w = lv_area_get_width(coords);
    uint32_t src_stride_bytes = (draw_dsc->stride != 0) ? draw_dsc->stride : (uint32_t)src_w * px_size_bytes;
    if(src_stride_bytes == 0) src_stride_bytes = (uint32_t)src_w * sizeof(lv_color_t); /* RGB565A8 等 px_size=0 的格式按颜色面行宽处理 */
    bool stride_aligned = (draw_dsc->stride != 0) && (draw_dsc->stride != src_w * px_size_bytes);

    lv_area_t blend_area;
    lv_draw_sw_blend_dsc_t blend_dsc;

    lv_memset_00(&blend_dsc, sizeof(lv_draw_sw_blend_dsc_t));
    blend_dsc.opa = draw_dsc->opa;
    blend_dsc.blend_mode = draw_dsc->blend_mode;
    blend_dsc.blend_area = &blend_area;

    /*The simplest case just copy the pixels into the draw_buf.
      含 padding 的图像也直接走 blend：字节行宽经 dsc->src_stride 透传，
      由 draw_ctx->blend（lv_draw_gdma_blend）的硬件 blit 按 src_w 跳行处理*/
    if(!mask_any && !transform && cf == LV_IMG_CF_TRUE_COLOR && draw_dsc->recolor_opa == LV_OPA_TRANSP) {
        blend_dsc.src_buf = (const lv_color_t *)src_buf;
        blend_dsc.src_stride = src_stride_bytes;

        blend_dsc.blend_area = coords;
        lv_draw_sw_blend(draw_ctx, &blend_dsc);
    }
    else if(!mask_any && !transform && !stride_aligned && cf == LV_IMG_CF_ALPHA_8BIT) {
        lv_area_t clipped_coords;
        if(!_lv_area_intersect(&clipped_coords, coords, draw_ctx->clip_area)) return;

        blend_dsc.mask_buf = (lv_opa_t *)src_buf;
        blend_dsc.mask_area = coords;
        blend_dsc.src_buf = NULL;
        blend_dsc.color = draw_dsc->recolor;
        blend_dsc.mask_res = LV_DRAW_MASK_RES_CHANGED;

        blend_dsc.blend_area = coords;
        lv_draw_sw_blend(draw_ctx, &blend_dsc);
    }
#if LV_COLOR_DEPTH == 16
    else if(!mask_any && !transform && !stride_aligned && cf == LV_IMG_CF_RGB565A8 && draw_dsc->recolor_opa == LV_OPA_TRANSP) {
        lv_coord_t src_h = lv_area_get_height(coords);
        blend_dsc.src_buf = (const lv_color_t *)src_buf;
        blend_dsc.mask_buf = (lv_opa_t *)src_buf;
        blend_dsc.mask_buf += sizeof(lv_color_t) * src_w * src_h;
        blend_dsc.blend_area = coords;
        blend_dsc.mask_area = coords;
        blend_dsc.mask_res = LV_DRAW_MASK_RES_CHANGED;
        lv_draw_sw_blend(draw_ctx, &blend_dsc);
    }
#endif
    /*In the other cases every pixel need to be checked one-by-one*/
    else {
        blend_area.x1 = draw_ctx->clip_area->x1;
        blend_area.x2 = draw_ctx->clip_area->x2;
        blend_area.y1 = draw_ctx->clip_area->y1;
        blend_area.y2 = draw_ctx->clip_area->y2;

        lv_coord_t src_h = lv_area_get_height(coords);
        lv_coord_t blend_h = lv_area_get_height(&blend_area);
        lv_coord_t blend_w = lv_area_get_width(&blend_area);

        uint32_t max_buf_size = MAX_BUF_SIZE;
        uint32_t blend_size = lv_area_get_size(&blend_area);
        uint32_t buf_h;
        uint32_t buf_w = blend_w;
        if(blend_size <= max_buf_size) {
            buf_h = blend_h;
        }
        else {
            /*Round to full lines*/
            buf_h = max_buf_size / blend_w;
        }

        /*Create buffers and masks*/
        uint32_t buf_size = buf_w * buf_h;

        lv_color_t * rgb_buf = lv_mem_buf_get(buf_size * sizeof(lv_color_t));
        lv_opa_t * mask_buf = lv_mem_buf_get(buf_size);
        blend_dsc.mask_buf = mask_buf;
        blend_dsc.mask_area = &blend_area;
        blend_dsc.mask_res = LV_DRAW_MASK_RES_CHANGED;
        blend_dsc.src_buf = rgb_buf;
        lv_coord_t y_last = blend_area.y2;
        blend_area.y2 = blend_area.y1 + buf_h - 1;

        lv_draw_mask_res_t mask_res_def = (cf != LV_IMG_CF_TRUE_COLOR || draw_dsc->angle ||
                                           draw_dsc->zoom != LV_IMG_ZOOM_NONE) ?
                                          LV_DRAW_MASK_RES_CHANGED : LV_DRAW_MASK_RES_FULL_COVER;
        blend_dsc.mask_res = mask_res_def;

        while(blend_area.y1 <= y_last) {
            /*Apply transformations if any or separate the channels*/
            lv_area_t transform_area;
            lv_area_copy(&transform_area, &blend_area);
            lv_area_move(&transform_area, -coords->x1, -coords->y1);
            if(transform) {
                lv_draw_transform(draw_ctx, &transform_area, src_buf, src_w, src_h, src_stride_bytes,
                                  draw_dsc, cf, rgb_buf, mask_buf);
            }
            else {
                convert_cb(&transform_area, src_buf, src_w, src_h, src_stride_bytes, draw_dsc, cf, rgb_buf, mask_buf);
            }

            /*Apply recolor*/
            if(draw_dsc->recolor_opa > LV_OPA_MIN) {
                uint16_t premult_v[3];
                lv_opa_t recolor_opa = draw_dsc->recolor_opa;
                lv_color_t recolor = draw_dsc->recolor;
                lv_color_premult(recolor, recolor_opa, premult_v);
                recolor_opa = 255 - recolor_opa;
                uint32_t i;
                for(i = 0; i < buf_size; i++) {
                    rgb_buf[i] = lv_color_mix_premult(premult_v, rgb_buf[i], recolor_opa);
                }
            }
#if LV_DRAW_COMPLEX
            /*Apply the masks if any*/
            if(mask_any) {
                lv_coord_t y;
                lv_opa_t * mask_buf_tmp = mask_buf;
                for(y = blend_area.y1; y <= blend_area.y2; y++) {
                    lv_draw_mask_res_t mask_res_line;
                    mask_res_line = lv_draw_mask_apply(mask_buf_tmp, blend_area.x1, y, blend_w);

                    if(mask_res_line == LV_DRAW_MASK_RES_TRANSP) {
                        lv_memset_00(mask_buf_tmp, blend_w);
                        blend_dsc.mask_res = LV_DRAW_MASK_RES_CHANGED;
                    }
                    else if(mask_res_line == LV_DRAW_MASK_RES_CHANGED) {
                        blend_dsc.mask_res = LV_DRAW_MASK_RES_CHANGED;
                    }
                    mask_buf_tmp += blend_w;
                }
            }
#endif

            /*Blend*/
            lv_draw_sw_blend(draw_ctx, &blend_dsc);

            /*Go the the next lines*/
            blend_area.y1 = blend_area.y2 + 1;
            blend_area.y2 = blend_area.y1 + buf_h - 1;
            if(blend_area.y2 > y_last) blend_area.y2 = y_last;
        }

        lv_mem_buf_release(mask_buf);
        lv_mem_buf_release(rgb_buf);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Separate the image channels to RGB and Alpha to match LV_COLOR_DEPTH settings
 * @param src_stride 源图字节行宽（可含对齐 padding）*/
static void convert_cb(const lv_area_t * dest_area, const void * src_buf, lv_coord_t src_w, lv_coord_t src_h,
                       uint32_t src_stride, const lv_draw_img_dsc_t * draw_dsc, lv_img_cf_t cf, lv_color_t * cbuf, lv_opa_t * abuf)
{
    LV_UNUSED(draw_dsc);
    LV_UNUSED(src_h);
    LV_UNUSED(src_w);

    const uint8_t * src_tmp8 = (const uint8_t *)src_buf;
    lv_coord_t y;
    lv_coord_t x;

    if(cf == LV_IMG_CF_TRUE_COLOR || cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED) {
        uint32_t px_cnt = lv_area_get_size(dest_area);
        lv_memset_ff(abuf, px_cnt);

        /* src_stride 为字节行宽, 直接以字节偏移 */
        src_tmp8 += src_stride * dest_area->y1 + dest_area->x1 * sizeof(lv_color_t);
        uint32_t dest_w = lv_area_get_width(dest_area);
        uint32_t dest_w_byte = dest_w * sizeof(lv_color_t);

        lv_coord_t src_stride_byte = src_stride;
        lv_color_t * cbuf_tmp = cbuf;
        for(y = dest_area->y1; y <= dest_area->y2; y++) {
            lv_memcpy(cbuf_tmp, src_tmp8, dest_w_byte);
            src_tmp8 += src_stride_byte;
            cbuf_tmp += dest_w;
        }

        /*Make "holes" for with Chroma keying*/
        if(cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED) {
            uint32_t i;
            lv_color_t chk = LV_COLOR_CHROMA_KEY;
#if LV_COLOR_DEPTH == 8 || LV_COLOR_DEPTH == 1
            uint8_t * cbuf_uint = (uint8_t *)cbuf;
            uint8_t chk_v = chk.full;
#elif LV_COLOR_DEPTH == 16
            uint16_t * cbuf_uint = (uint16_t *)cbuf;
            uint16_t chk_v = chk.full;
#elif LV_COLOR_DEPTH == 24
            for(i = 0; i < px_cnt; i++) {
                if(lv_memcmp(&chk, &cbuf[i], sizeof(lv_color_t)) == 0) abuf[i] = 0x00;
            }
#elif LV_COLOR_DEPTH == 32
            uint32_t * cbuf_uint = (uint32_t *)cbuf;
            uint32_t chk_v = chk.full;
#endif
#if LV_COLOR_DEPTH != 24
            for(i = 0; i < px_cnt; i++) {
                if(chk_v == cbuf_uint[i]) abuf[i] = 0x00;
            }
#endif
        }
    }
    else if(cf == LV_IMG_CF_TRUE_COLOR_ALPHA) {
        /* src_stride 为字节行宽, 直接以字节偏移 */
        src_tmp8 += src_stride * dest_area->y1 + dest_area->x1 * LV_IMG_PX_SIZE_ALPHA_BYTE;

        lv_coord_t src_new_line_step_byte = src_stride - lv_area_get_width(dest_area) * LV_IMG_PX_SIZE_ALPHA_BYTE;

        lv_coord_t dest_h = lv_area_get_height(dest_area);
        lv_coord_t dest_w = lv_area_get_width(dest_area);
        for(y = 0; y < dest_h; y++) {
            for(x = 0; x < dest_w; x++) {
                abuf[x] = LV_IMG_PX_ALPHA8(src_tmp8);
#if LV_COLOR_DEPTH == 8 || LV_COLOR_DEPTH == 1
                cbuf[x].full = *src_tmp8;
#elif LV_COLOR_DEPTH == 16
                cbuf[x].full = *src_tmp8 + ((*(src_tmp8 + 1)) << 8);
#elif LV_COLOR_DEPTH == 24
                cbuf[x].full[0] = src_tmp8[0];
                cbuf[x].full[1] = src_tmp8[1];
                cbuf[x].full[2] = 0xFF;
#elif LV_COLOR_DEPTH == 32
                cbuf[x] = *((lv_color_t *) src_tmp8);
                cbuf[x].ch.alpha = 0xff;
#endif
                src_tmp8 += LV_IMG_PX_SIZE_ALPHA_BYTE;

            }
            cbuf += dest_w;
            abuf += dest_w;
            src_tmp8 += src_new_line_step_byte;
        }
    }
    else if(cf == LV_IMG_CF_RGB565A8) {
        /* RGB565A8 不支持 stride 对齐（lv_img_stride_get 返回 0），此处传入颜色面字节行宽，
           换算回像素行宽按原逻辑处理 */
        lv_coord_t src_stride_px = src_stride / sizeof(lv_color_t);
        src_tmp8 += (src_stride_px * dest_area->y1 * sizeof(lv_color_t)) + dest_area->x1 * sizeof(lv_color_t);

        lv_coord_t src_stride_byte = src_stride_px * sizeof(lv_color_t);

        lv_coord_t dest_h = lv_area_get_height(dest_area);
        lv_coord_t dest_w = lv_area_get_width(dest_area);
        for(y = 0; y < dest_h; y++) {
            lv_memcpy(cbuf, src_tmp8, dest_w * sizeof(lv_color_t));
            cbuf += dest_w;
            src_tmp8 += src_stride_byte;
        }

        src_tmp8 = (const uint8_t *)src_buf;
        src_tmp8 += sizeof(lv_color_t) * src_w * src_h;
        src_tmp8 += src_stride_px * dest_area->y1 + dest_area->x1;
        for(y = 0; y < dest_h; y++) {
            lv_memcpy(abuf, src_tmp8, dest_w);
            abuf += dest_w;
            src_tmp8 += src_stride_px;
        }
    }
}
