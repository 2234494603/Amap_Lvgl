/**
 * @file lv_draw_sw_blend.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_sw.h"
#include "../../misc/lv_math.h"
#include "../../hal/lv_hal_disp.h"
#include "../../core/lv_refr.h"

/*********************
 *      DEFINES
 *********************/

#if LV_COLOR_DEPTH == 24
#define LV_BLEND_COLOR_EQ(a, b)  (lv_memcmp(&(a), &(b), sizeof(lv_color_t)) == 0)
#define LV_BLEND_COLOR_NEQ(a, b) (lv_memcmp(&(a), &(b), sizeof(lv_color_t)) != 0)
#define LV_BLEND_COLOR_COPY(dst, src) lv_memcpy(&(dst), &(src), sizeof(lv_color_t))
#else
#define LV_BLEND_COLOR_EQ(a, b)  ((a).full == (b).full)
#define LV_BLEND_COLOR_NEQ(a, b) ((a).full != (b).full)
#define LV_BLEND_COLOR_COPY(dst, src) ((dst).full = (src).full)
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void fill_set_px(lv_color_t * dest_buf, const lv_area_t * blend_area, lv_coord_t dest_stride,
                        lv_color_t color, lv_opa_t opa, const lv_opa_t * mask, lv_coord_t mask_stide);

static void /* LV_ATTRIBUTE_FAST_MEM */ fill_normal(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                                    lv_coord_t dest_stride, lv_color_t color, lv_opa_t opa,
                                                    const lv_opa_t * mask, lv_coord_t mask_stride);


#if LV_COLOR_SCREEN_TRANSP
static void /* LV_ATTRIBUTE_FAST_MEM */ fill_argb(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                                  lv_coord_t dest_stride, lv_color_t color, lv_opa_t opa,
                                                  const lv_opa_t * mask, lv_coord_t mask_stride);
#endif /*LV_COLOR_SCREEN_TRANSP*/

#if LV_DRAW_COMPLEX
static void fill_blended(lv_color_t * dest_buf, const lv_area_t * dest_area, lv_coord_t dest_stride, lv_color_t color,
                         lv_opa_t opa, const lv_opa_t * mask, lv_coord_t mask_stride, lv_blend_mode_t blend_mode);
#endif  /*LV_DRAW_COMPLEX*/

static void map_set_px(lv_color_t * dest_buf, const lv_area_t * dest_area, lv_coord_t dest_stride,
                       const lv_color_t * src_buf, lv_coord_t src_stride, lv_opa_t opa,
                       const lv_opa_t * mask, lv_coord_t mask_stride);

static void /* LV_ATTRIBUTE_FAST_MEM */ map_normal(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                                   lv_coord_t dest_stride, const lv_color_t * src_buf,
                                                   lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                                                   lv_coord_t mask_stride);

#if LV_COLOR_SCREEN_TRANSP
static void /* LV_ATTRIBUTE_FAST_MEM */ map_argb(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                                 lv_coord_t dest_stride, const lv_color_t * src_buf,
                                                 lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                                                 lv_coord_t mask_stride, lv_blend_mode_t blend_mode);

#endif /*LV_COLOR_SCREEN_TRANSP*/

#if LV_DRAW_COMPLEX
static void map_blended(lv_color_t * dest_buf, const lv_area_t * dest_area, lv_coord_t dest_stride,
                        const lv_color_t * src_buf, lv_coord_t src_stride, lv_opa_t opa,
                        const lv_opa_t * mask, lv_coord_t mask_stride, lv_blend_mode_t blend_mode);

static inline lv_color_t color_blend_true_color_additive(lv_color_t fg, lv_color_t bg, lv_opa_t opa);
static inline lv_color_t color_blend_true_color_subtractive(lv_color_t fg, lv_color_t bg, lv_opa_t opa);
static inline lv_color_t color_blend_true_color_multiply(lv_color_t fg, lv_color_t bg, lv_opa_t opa);
#endif /*LV_DRAW_COMPLEX*/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/
#define FILL_NORMAL_MASK_PX(color)                                                          \
    if(*mask == LV_OPA_COVER) *dest_buf = color;                                 \
    else *dest_buf = lv_color_mix(color, *dest_buf, *mask);            \
    mask++;                                                         \
    dest_buf++;

#define MAP_NORMAL_MASK_PX(x)                                                          \
    if(*mask_tmp_x) {          \
        if(*mask_tmp_x == LV_OPA_COVER) dest_buf[x] = src_buf[x];                                 \
        else dest_buf[x] = lv_color_mix(src_buf[x], dest_buf[x], *mask_tmp_x);            \
    }                                                                                               \
    mask_tmp_x++;


/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_sw_blend(lv_draw_ctx_t * draw_ctx, const lv_draw_sw_blend_dsc_t * dsc)
{
    /*Do not draw transparent things*/
    if(dsc->opa <= LV_OPA_MIN) return;

    lv_area_t blend_area;
    if(!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area)) return;

    if(draw_ctx->wait_for_finish) draw_ctx->wait_for_finish(draw_ctx);

    ((lv_draw_sw_ctx_t *)draw_ctx)->blend(draw_ctx, dsc);
}

void LV_ATTRIBUTE_FAST_MEM lv_draw_sw_blend_basic(lv_draw_ctx_t * draw_ctx,
                                                  const lv_draw_sw_blend_dsc_t * dsc)
{
    lv_opa_t * mask;
    if(dsc->mask_buf == NULL) mask = NULL;
    if(dsc->mask_buf && dsc->mask_res == LV_DRAW_MASK_RES_TRANSP) return;
    else if(dsc->mask_res == LV_DRAW_MASK_RES_FULL_COVER) mask = NULL;
    else mask = dsc->mask_buf;

    /* 源可能携带对齐字节行宽(padding)；下方 map/fill 系函数都按紧凑行排布，
       故按行切片递归，源指针每行前进 src_stride 字节 */
    if(dsc->src_buf != NULL && dsc->src_stride != 0 &&
       dsc->src_stride != (uint32_t)lv_area_get_width(dsc->blend_area) * sizeof(lv_color_t)) {
        lv_draw_sw_blend_dsc_t dsc_row = *dsc;
        lv_area_t row_area = *dsc->blend_area;
        const uint8_t * src8 = (const uint8_t *)dsc->src_buf;
        dsc_row.blend_area = &row_area;
        dsc_row.src_stride = 0;
        lv_coord_t y;
        for(y = dsc->blend_area->y1; y <= dsc->blend_area->y2; y++) {
            row_area.y1 = row_area.y2 = y;
            dsc_row.src_buf = (const lv_color_t *)src8;
            lv_draw_sw_blend_basic(draw_ctx, &dsc_row);
            src8 += dsc->src_stride;
        }
        return;
    }

    lv_coord_t dest_stride = lv_area_get_width(draw_ctx->buf_area);

    lv_area_t blend_area;
    if(!_lv_area_intersect(&blend_area, dsc->blend_area, draw_ctx->clip_area)) return;

    lv_disp_t * disp = _lv_refr_get_disp_refreshing();
    lv_color_t * dest_buf = draw_ctx->buf;
    if(disp->driver->set_px_cb == NULL) {
        if(disp->driver->screen_transp == 0) {
            dest_buf += dest_stride * (blend_area.y1 - draw_ctx->buf_area->y1) + (blend_area.x1 - draw_ctx->buf_area->x1);
        }
        else {
            /*With LV_COLOR_DEPTH 16 it means ARGB8565 (3 bytes format)*/
            uint8_t * dest_buf8 = (uint8_t *) dest_buf;
            dest_buf8 += dest_stride * (blend_area.y1 - draw_ctx->buf_area->y1) * LV_IMG_PX_SIZE_ALPHA_BYTE;
            dest_buf8 += (blend_area.x1 - draw_ctx->buf_area->x1) * LV_IMG_PX_SIZE_ALPHA_BYTE;
            dest_buf = (lv_color_t *)dest_buf8;
        }
    }

    const lv_color_t * src_buf = dsc->src_buf;
    lv_coord_t src_stride;
    if(src_buf) {
        src_stride = lv_area_get_width(dsc->blend_area);
        src_buf += src_stride * (blend_area.y1 - dsc->blend_area->y1) + (blend_area.x1 - dsc->blend_area->x1);
    }
    else {
        src_stride = 0;
    }

    lv_coord_t mask_stride;
    if(mask) {
        /*Round the values in the mask if anti-aliasing is disabled*/
        if(disp->driver->antialiasing == 0) {
            int32_t mask_size = lv_area_get_size(dsc->mask_area);
            int32_t i;
            for(i = 0; i < mask_size; i++) {
                mask[i] = mask[i] > 128 ? LV_OPA_COVER : LV_OPA_TRANSP;
            }
        }

        mask_stride = lv_area_get_width(dsc->mask_area);
        mask += mask_stride * (blend_area.y1 - dsc->mask_area->y1) + (blend_area.x1 - dsc->mask_area->x1);

    }
    else {
        mask_stride = 0;
    }

    lv_area_move(&blend_area, -draw_ctx->buf_area->x1, -draw_ctx->buf_area->y1);


    if(disp->driver->set_px_cb) {
        if(dsc->src_buf == NULL) {
            fill_set_px(dest_buf, &blend_area, dest_stride, dsc->color, dsc->opa, mask, mask_stride);
        }
        else {
            map_set_px(dest_buf, &blend_area, dest_stride, src_buf, src_stride, dsc->opa, mask, mask_stride);
        }
    }
#if LV_COLOR_SCREEN_TRANSP
    else if(disp->driver->screen_transp) {
        if(dsc->src_buf == NULL) {
            fill_argb(dest_buf, &blend_area, dest_stride, dsc->color, dsc->opa, mask, mask_stride);
        }
        else {
            map_argb(dest_buf, &blend_area, dest_stride, src_buf, src_stride, dsc->opa, mask, mask_stride, dsc->blend_mode);
        }
    }
#endif
    else if(dsc->blend_mode == LV_BLEND_MODE_NORMAL) {
        if(dsc->src_buf == NULL) {
            fill_normal(dest_buf, &blend_area, dest_stride, dsc->color, dsc->opa, mask, mask_stride);
        }
        else {
            map_normal(dest_buf, &blend_area, dest_stride, src_buf, src_stride, dsc->opa, mask, mask_stride);
        }
    }
    else {
#if LV_DRAW_COMPLEX
        if(dsc->src_buf == NULL) {
            fill_blended(dest_buf, &blend_area, dest_stride, dsc->color, dsc->opa, mask, mask_stride, dsc->blend_mode);
        }
        else {
            map_blended(dest_buf, &blend_area, dest_stride, src_buf, src_stride, dsc->opa, mask, mask_stride, dsc->blend_mode);
        }
#endif
    }
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

static void fill_set_px(lv_color_t * dest_buf, const lv_area_t * blend_area, lv_coord_t dest_stride,
                        lv_color_t color, lv_opa_t opa, const lv_opa_t * mask, lv_coord_t mask_stide)
{
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();

    int32_t x;
    int32_t y;

    if(mask == NULL) {
        for(y = blend_area->y1; y <= blend_area->y2; y++) {
            for(x = blend_area->x1; x <= blend_area->x2; x++) {
                disp->driver->set_px_cb(disp->driver, (void *)dest_buf, dest_stride, x, y, color, opa);
            }
        }
    }
    else {
        int32_t w = lv_area_get_width(blend_area);
        int32_t h = lv_area_get_height(blend_area);

        for(y = 0; y < h; y++) {
            for(x = 0; x < w; x++) {
                if(mask[x]) {


                    disp->driver->set_px_cb(disp->driver, (void *)dest_buf, dest_stride, blend_area->x1 + x, blend_area->y1 + y, color,
                                            (uint32_t)((uint32_t)opa * mask[x]) >> 8);
                }
            }
            mask += mask_stide;
        }
    }
}

static LV_ATTRIBUTE_FAST_MEM void fill_normal(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                              lv_coord_t dest_stride, lv_color_t color, lv_opa_t opa,
                                              const lv_opa_t * mask, lv_coord_t mask_stride)
{
    int32_t w = lv_area_get_width(dest_area);
    int32_t h = lv_area_get_height(dest_area);

    int32_t x;
    int32_t y;

    /*No mask*/
    if(mask == NULL) {
        if(opa >= LV_OPA_MAX) {
            for(y = 0; y < h; y++) {
                lv_color_fill(dest_buf, color, w);
                dest_buf += dest_stride;
            }
        }
        /*Has opacity*/
        else {
            lv_color_t last_dest_color = lv_color_black();
            lv_color_t last_res_color = lv_color_mix(color, last_dest_color, opa);

#if LV_COLOR_MIX_ROUND_OFS == 0 && LV_COLOR_DEPTH == 16
            /*lv_color_mix work with an optimized algorithm with 16 bit color depth.
             *However, it introduces some rounded error on opa.
             *Introduce the same error here too to make lv_color_premult produces the same result */
            #if LV_COLOR_16_ALPHA == 0
                opa = (uint32_t)((uint32_t)opa + 4) >> 3;
                opa = opa << 3;
            #elif LV_COLOR_16_ALPHA == 1 
                opa = (uint32_t)((uint32_t)opa + 4) >> 3;
                opa = opa << 3;
            #elif LV_COLOR_16_ALPHA == 4
                opa = (uint32_t)((uint32_t)opa + 8) >> 4;
                opa = opa << 4;
            #endif
#endif

            uint16_t color_premult[3];
            lv_color_premult(color, opa, color_premult);
            lv_opa_t opa_inv = 255 - opa;

            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    if(LV_BLEND_COLOR_NEQ(last_dest_color, dest_buf[x])) {
                        last_dest_color = dest_buf[x];
                        last_res_color = lv_color_mix_premult(color_premult, dest_buf[x], opa_inv);
                    }
                    dest_buf[x] = last_res_color;
                }
                dest_buf += dest_stride;
            }
        }
    }
    /*Masked*/
    else {
#if LV_COLOR_DEPTH == 16
        uint32_t c32 = color.full + ((uint32_t)color.full << 16);
#endif
        /*Only the mask matters*/
        if(opa >= LV_OPA_MAX) {
            int32_t x_end4 = w - 4;
            for(y = 0; y < h; y++) {
                for(x = 0; x < w && ((lv_uintptr_t)(mask) & 0x3); x++) {
                    FILL_NORMAL_MASK_PX(color)
                }

                for(; x <= x_end4; x += 4) {
                    uint32_t mask32 = *((uint32_t *)mask);
                    if(mask32 == 0xFFFFFFFF) {
#if LV_COLOR_DEPTH == 16
                        if((lv_uintptr_t)dest_buf & 0x3) {
                            *(dest_buf + 0) = color;
                            uint32_t * d = (uint32_t *)(dest_buf + 1);
                            *d = c32;
                            *(dest_buf + 3) = color;
                        }
                        else {
                            uint32_t * d = (uint32_t *)dest_buf;
                            *d = c32;
                            *(d + 1) = c32;
                        }
#else
                        dest_buf[0] = color;
                        dest_buf[1] = color;
                        dest_buf[2] = color;
                        dest_buf[3] = color;
#endif
                        dest_buf += 4;
                        mask += 4;
                    }
                    else if(mask32) {
                        FILL_NORMAL_MASK_PX(color)
                        FILL_NORMAL_MASK_PX(color)
                        FILL_NORMAL_MASK_PX(color)
                        FILL_NORMAL_MASK_PX(color)
                    }
                    else {
                        mask += 4;
                        dest_buf += 4;
                    }
                }

                for(; x < w ; x++) {
                    FILL_NORMAL_MASK_PX(color)
                }
                dest_buf += (dest_stride - w);
                mask += (mask_stride - w);
            }
        }
        /*With opacity*/
        else {
            /*Buffer the result color to avoid recalculating the same color*/
            lv_color_t last_dest_color;
            lv_color_t last_res_color;
            lv_opa_t last_mask = LV_OPA_TRANSP;
            LV_BLEND_COLOR_COPY(last_dest_color, dest_buf[0]);
            LV_BLEND_COLOR_COPY(last_res_color, dest_buf[0]);
            lv_opa_t opa_tmp = LV_OPA_TRANSP;

            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    if(*mask) {
                        if(*mask != last_mask) opa_tmp = *mask == LV_OPA_COVER ? opa :
                                                             (uint32_t)((uint32_t)(*mask) * opa) >> 8;
                        if(*mask != last_mask || LV_BLEND_COLOR_NEQ(last_dest_color, dest_buf[x])) {
                            if(opa_tmp == LV_OPA_COVER) last_res_color = color;
                            else last_res_color = lv_color_mix(color, dest_buf[x], opa_tmp);
                            last_mask = *mask;
                            LV_BLEND_COLOR_COPY(last_dest_color, dest_buf[x]);
                        }
                        dest_buf[x] = last_res_color;
                    }
                    mask++;
                }
                dest_buf += dest_stride;
                mask += (mask_stride - w);
            }
        }
    }
}

#if LV_COLOR_SCREEN_TRANSP
static inline void set_px_argb(uint8_t * buf, lv_color_t color, lv_opa_t opa)
{
    lv_color_t bg_color;
    lv_color_t res_color;
    lv_opa_t bg_opa = buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1];
#if LV_COLOR_DEPTH == 8
    bg_color.full = buf[0];
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &buf[1]);
    if(buf[1] <= LV_OPA_MIN) return;
    buf[0] = res_color.full;
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 0
    bg_color.full = buf[0] + (buf[1] << 8);
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &buf[2]);
    if(buf[2] <= LV_OPA_MIN) return;
    buf[0] = res_color.full & 0xff;
    buf[1] = res_color.full >> 8;
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 1    // ARGB1555
    bg_color.full = buf[0] + (buf[1] << 8);
    bg_opa = buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] & 0x80 ? LV_OPA_COVER : LV_OPA_TRANSP;
    /* for color fomrat argb1555, res_opa will be 0 or 255 */
    lv_opa_t res_opa = LV_OPA_TRANSP;
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &res_opa);
    if (res_opa <= LV_OPA_MIN) return;
    buf[0] = res_color.full & 0xff;
    buf[1] = res_color.full >> 8;
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 4    // ARGB4444
    bg_color.full = buf[0] + (buf[1] << 8);
    bg_opa = buf[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] & 0xF0;
    bg_opa = bg_opa | bg_opa << 4;
    lv_opa_t res_opa = LV_OPA_TRANSP;
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &res_opa);
    if (res_opa <= LV_OPA_MIN) return;
    res_color.ch.alpha = (res_opa >> 4) & 0x0F;
    buf[0] = res_color.full & 0xff;
    buf[1] = res_color.full >> 8;
#elif LV_COLOR_DEPTH == 24
    bg_color.full[0] = buf[0];
    bg_color.full[1] = buf[1];
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &buf[2]);
    if(buf[2] <= LV_OPA_MIN) return;
    buf[0] = res_color.full[0];
    buf[1] = res_color.full[1];
#elif LV_COLOR_DEPTH == 32
    bg_color = *((lv_color_t *)buf);
    lv_color_mix_with_alpha(bg_color, bg_opa, color, opa, &res_color, &buf[3]);
    if(buf[3] <= LV_OPA_MIN) return;
    buf[0] = res_color.ch.blue;
    buf[1] = res_color.ch.green;
    buf[2] = res_color.ch.red;
#endif
}

static inline void set_px_argb_blend(uint8_t * buf, lv_color_t color, lv_opa_t opa, lv_color_t (*blend_fp)(lv_color_t,
                                                                                                           lv_color_t, lv_opa_t))
{
    static lv_color_t last_dest_color;
    static lv_color_t last_src_color;
    static lv_color_t last_res_color;
    static uint32_t last_opa = 0xffff; /*Set to an invalid value for first*/

    lv_color_t bg_color;

    /*Get the BG color*/
#if LV_COLOR_DEPTH == 8
    if(buf[1] <= LV_OPA_MIN) return;
    bg_color.full = buf[0];
#elif LV_COLOR_DEPTH == 16
    if(buf[2] <= LV_OPA_MIN) return;
    bg_color.full = buf[0] + (buf[1] << 8);
#elif LV_COLOR_DEPTH == 24
    if(buf[2] <= LV_OPA_MIN) return;
    bg_color.full[0] = buf[0];
    bg_color.full[1] = buf[1];
#elif LV_COLOR_DEPTH == 32
    if(buf[3] <= LV_OPA_MIN) return;
    bg_color = *((lv_color_t *)buf);
#endif

    /*Get the result color*/
    if(LV_BLEND_COLOR_NEQ(last_dest_color, bg_color) || LV_BLEND_COLOR_NEQ(last_src_color, color) || last_opa != opa) {
        LV_BLEND_COLOR_COPY(last_dest_color, bg_color);
        LV_BLEND_COLOR_COPY(last_src_color, color);
        last_opa = opa;
        last_res_color = blend_fp(last_src_color, last_dest_color, last_opa);
    }

    /*Set the result color*/
#if LV_COLOR_DEPTH == 8
    buf[0] = last_res_color.full;
#elif LV_COLOR_DEPTH == 16
    buf[0] = last_res_color.full & 0xff;
    buf[1] = last_res_color.full >> 8;
#elif LV_COLOR_DEPTH == 24
    buf[0] = last_res_color.full[0];
    buf[1] = last_res_color.full[1];
#elif LV_COLOR_DEPTH == 32
    buf[0] = last_res_color.ch.blue;
    buf[1] = last_res_color.ch.green;
    buf[2] = last_res_color.ch.red;
#endif

}

static void LV_ATTRIBUTE_FAST_MEM fill_argb(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                            lv_coord_t dest_stride, lv_color_t color, lv_opa_t opa,
                                            const lv_opa_t * mask, lv_coord_t mask_stride)
{
    uint8_t * dest_buf8 = (uint8_t *) dest_buf;
    int32_t w = lv_area_get_width(dest_area);
    int32_t h = lv_area_get_height(dest_area);

    int32_t x;
    int32_t y;

#if (LV_COLOR_DEPTH == 16) && (LV_COLOR_16_ALPHA == 1) 
    uint8_t ctmp[LV_IMG_PX_SIZE_ALPHA_BYTE];
    lv_memcpy(ctmp, &color, sizeof(lv_color_t));
#elif (LV_COLOR_DEPTH == 16) && (LV_COLOR_16_ALPHA == 4) 
    uint8_t ctmp[LV_IMG_PX_SIZE_ALPHA_BYTE];
    lv_memcpy(ctmp, &color, sizeof(lv_color_t));
#else
    uint8_t ctmp[LV_IMG_PX_SIZE_ALPHA_BYTE];
    lv_memcpy(ctmp, &color, sizeof(lv_color_t));
    ctmp[LV_IMG_PX_SIZE_ALPHA_BYTE - 1] = opa;
#endif
    /*No mask*/
    if(mask == NULL) {
        if(opa >= LV_OPA_MAX) {
            for(x = 0; x < w; x++) {
                lv_memcpy(dest_buf8, ctmp, LV_IMG_PX_SIZE_ALPHA_BYTE);
                dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
            }

            dest_buf8 += (dest_stride - w) * LV_IMG_PX_SIZE_ALPHA_BYTE;

            for(y = 1; y < h; y++) {
                lv_memcpy(dest_buf8, (uint8_t *) dest_buf, w * LV_IMG_PX_SIZE_ALPHA_BYTE);
                dest_buf8 += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
            }
        }
        /*Has opacity*/
        else {
            uint8_t * dest_buf8_row = dest_buf8;
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    set_px_argb(dest_buf8, color, opa);
                    dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                }
                dest_buf8_row += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
                dest_buf8 = dest_buf8_row;
            }
        }
    }
    /*Masked*/
    else {
        /*Only the mask matters*/
        if(opa >= LV_OPA_MAX) {
            uint8_t * dest_buf8_row = dest_buf8;
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    set_px_argb(dest_buf8, color,  *mask);
                    mask++;
                    dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                }
                dest_buf8_row += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
                dest_buf8 = dest_buf8_row;
            }
        }
        /*With opacity*/
        else {
            /*Buffer the result color to avoid recalculating the same color*/
            lv_opa_t last_mask = LV_OPA_TRANSP;
            lv_opa_t opa_tmp = LV_OPA_TRANSP;

            uint8_t * dest_buf8_row = dest_buf8;
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    if(*mask) {
                        if(*mask != last_mask) opa_tmp = *mask == LV_OPA_COVER ? opa :
                                                             (uint32_t)((uint32_t)(*mask) * opa) >> 8;

                        set_px_argb(dest_buf8, color,  opa_tmp);
                    }
                    dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                    mask++;
                }
                dest_buf8_row += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
                dest_buf8 = dest_buf8_row;
                mask += (mask_stride - w);
            }
        }
    }
}
#endif

#if LV_DRAW_COMPLEX
static void fill_blended(lv_color_t * dest_buf, const lv_area_t * dest_area,
                         lv_coord_t dest_stride, lv_color_t color, lv_opa_t opa, const lv_opa_t * mask, lv_coord_t mask_stride,
                         lv_blend_mode_t blend_mode)
{

    int32_t w = lv_area_get_width(dest_area);
    int32_t h = lv_area_get_height(dest_area);

    int32_t x;
    int32_t y;

    lv_color_t (*blend_fp)(lv_color_t, lv_color_t, lv_opa_t);
    switch(blend_mode) {
        case LV_BLEND_MODE_ADDITIVE:
            blend_fp = color_blend_true_color_additive;
            break;
        case LV_BLEND_MODE_SUBTRACTIVE:
            blend_fp = color_blend_true_color_subtractive;
            break;
        case LV_BLEND_MODE_MULTIPLY:
            blend_fp = color_blend_true_color_multiply;
            break;
        default:
            LV_LOG_WARN("fill_blended: unsupported blend mode");
            return;
    }

    /*Simple fill (maybe with opacity), no masking*/
    if(mask == NULL) {
        lv_color_t last_dest_color = dest_buf[0];
        lv_color_t last_res_color = blend_fp(color, dest_buf[0], opa);
        for(y = 0; y < h; y++) {
            for(x = 0; x < w; x++) {
                if(LV_BLEND_COLOR_NEQ(last_dest_color, dest_buf[x])) {
                    last_dest_color = dest_buf[x];
                    last_res_color = blend_fp(color, dest_buf[x], opa);
                }
                dest_buf[x] = last_res_color;
            }
            dest_buf += dest_stride;
        }
    }
    /*Masked*/
    else {
        /*Buffer the result color to avoid recalculating the same color*/
        lv_color_t last_dest_color;
        lv_color_t last_res_color;
        lv_opa_t last_mask = LV_OPA_TRANSP;
        last_dest_color = dest_buf[0];
        lv_opa_t opa_tmp = mask[0] >= LV_OPA_MAX ? opa : (uint32_t)((uint32_t)mask[0] * opa) >> 8;
        last_res_color = blend_fp(color, last_dest_color, opa_tmp);

        for(y = 0; y < h; y++) {
            for(x = 0; x < w; x++) {
                if(mask[x] == 0) continue;
                if(mask[x] != last_mask || LV_BLEND_COLOR_NEQ(last_dest_color, dest_buf[x])) {
                    opa_tmp = mask[x] >= LV_OPA_MAX ? opa : (uint32_t)((uint32_t)mask[x] * opa) >> 8;

                    last_res_color = blend_fp(color, dest_buf[x], opa_tmp);
                    last_mask = mask[x];
                    LV_BLEND_COLOR_COPY(last_dest_color, dest_buf[x]);
                }
                dest_buf[x] = last_res_color;
            }
            dest_buf += dest_stride;
            mask += mask_stride;
        }
    }
}
#endif

static void map_set_px(lv_color_t * dest_buf, const lv_area_t * dest_area, lv_coord_t dest_stride,
                       const lv_color_t * src_buf, lv_coord_t src_stride, lv_opa_t opa,
                       const lv_opa_t * mask, lv_coord_t mask_stride)

{
    lv_disp_t * disp = _lv_refr_get_disp_refreshing();

    int32_t w = lv_area_get_width(dest_area);
    int32_t h = lv_area_get_height(dest_area);

    int32_t x;
    int32_t y;

    if(mask == NULL) {
        for(y = 0; y < h; y++) {
            for(x = 0; x < w; x++) {
                disp->driver->set_px_cb(disp->driver, (void *)dest_buf, dest_stride, dest_area->x1 + x, dest_area->y1 + y, src_buf[x],
                                        opa);
            }
            src_buf += src_stride;
        }
    }
    else {
        for(y = 0; y < h; y++) {
            for(x = 0; x < w; x++) {
                if(mask[x]) {
                    disp->driver->set_px_cb(disp->driver, (void *)dest_buf, dest_stride, dest_area->x1 + x, dest_area->y1 + y, src_buf[x],
                                            (uint32_t)((uint32_t)opa * mask[x]) >> 8);
                }
            }
            mask += mask_stride;
            src_buf += src_stride;
        }
    }
}

static void LV_ATTRIBUTE_FAST_MEM map_normal(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                             lv_coord_t dest_stride, const lv_color_t * src_buf,
                                             lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                                             lv_coord_t mask_stride)

{
    int32_t w = lv_area_get_width(dest_area);
    int32_t h = lv_area_get_height(dest_area);

    int32_t x;
    int32_t y;

    /*Simple fill (maybe with opacity), no masking*/
    if(mask == NULL) {
        if(opa >= LV_OPA_MAX) {
            for(y = 0; y < h; y++) {
                lv_memcpy(dest_buf, src_buf, w * sizeof(lv_color_t));
                dest_buf += dest_stride;
                src_buf += src_stride;
            }
        }
        else {
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    dest_buf[x] = lv_color_mix(src_buf[x], dest_buf[x], opa);
                }
                dest_buf += dest_stride;
                src_buf += src_stride;
            }
        }
    }
    /*Masked*/
    else {
        /*Only the mask matters*/
        if(opa > LV_OPA_MAX) {
            int32_t x_end4 = w - 4;

            for(y = 0; y < h; y++) {
                const lv_opa_t * mask_tmp_x = mask;
#if 0
                for(x = 0; x < w; x++) {
                    MAP_NORMAL_MASK_PX(x);
                }
#else
                for(x = 0; x < w && ((lv_uintptr_t)mask_tmp_x & 0x3); x++) {
                    MAP_NORMAL_MASK_PX(x)
                }

                uint32_t * mask32 = (uint32_t *)mask_tmp_x;
                for(; x < x_end4; x += 4) {
                    if(*mask32) {
                        if((*mask32) == 0xFFFFFFFF) {
                            dest_buf[x] = src_buf[x];
                            dest_buf[x + 1] = src_buf[x + 1];
                            dest_buf[x + 2] = src_buf[x + 2];
                            dest_buf[x + 3] = src_buf[x + 3];
                        }
                        else {
                            mask_tmp_x = (const lv_opa_t *)mask32;
                            MAP_NORMAL_MASK_PX(x)
                            MAP_NORMAL_MASK_PX(x + 1)
                            MAP_NORMAL_MASK_PX(x + 2)
                            MAP_NORMAL_MASK_PX(x + 3)
                        }
                    }
                    mask32++;
                }

                mask_tmp_x = (const lv_opa_t *)mask32;
                for(; x < w ; x++) {
                    MAP_NORMAL_MASK_PX(x)
                }
#endif
                dest_buf += dest_stride;
                src_buf += src_stride;
                mask += mask_stride;
            }
        }
        /*Handle opa and mask values too*/
        else {
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    if(mask[x]) {
                        lv_opa_t opa_tmp = mask[x] >= LV_OPA_MAX ? opa : ((opa * mask[x]) >> 8);
                        dest_buf[x] = lv_color_mix(src_buf[x], dest_buf[x], opa_tmp);
                    }
                }
                dest_buf += dest_stride;
                src_buf += src_stride;
                mask += mask_stride;
            }
        }
    }
}



#if LV_COLOR_SCREEN_TRANSP
static void LV_ATTRIBUTE_FAST_MEM map_argb(lv_color_t * dest_buf, const lv_area_t * dest_area,
                                           lv_coord_t dest_stride, const lv_color_t * src_buf,
                                           lv_coord_t src_stride, lv_opa_t opa, const lv_opa_t * mask,
                                           lv_coord_t mask_stride, lv_blend_mode_t blend_mode)

{
    uint8_t * dest_buf8 = (uint8_t *) dest_buf;

    int32_t w = lv_area_get_width(dest_area);
    int32_t h = lv_area_get_height(dest_area);

    int32_t x;
    int32_t y;

    lv_color_t (*blend_fp)(lv_color_t, lv_color_t, lv_opa_t);
    switch(blend_mode) {
        case LV_BLEND_MODE_ADDITIVE:
            blend_fp = color_blend_true_color_additive;
            break;
        case LV_BLEND_MODE_SUBTRACTIVE:
            blend_fp = color_blend_true_color_subtractive;
            break;
        case LV_BLEND_MODE_MULTIPLY:
            blend_fp = color_blend_true_color_multiply;
            break;
        default:
            blend_fp = NULL;
    }

    /*Simple fill (maybe with opacity), no masking*/
    if(mask == NULL) {
        if(opa >= LV_OPA_MAX) {
            if(blend_fp == NULL && LV_COLOR_DEPTH == 32) {
                for(y = 0; y < h; y++) {
                    lv_memcpy(dest_buf, src_buf, w * sizeof(lv_color_t));
                    dest_buf += dest_stride;
                    src_buf += src_stride;
                }
            }
            else {
                uint8_t * dest_buf8_row = dest_buf8;
                for(y = 0; y < h; y++) {
                    if(blend_fp == NULL) {
                        for(x = 0; x < w; x++) {
                            set_px_argb(dest_buf8, src_buf[x], LV_OPA_COVER);
                            dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                        }
                    }
                    else {
                        for(x = 0; x < w; x++) {
                            set_px_argb_blend(dest_buf8, src_buf[x], LV_OPA_COVER, blend_fp);
                            dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                        }
                    }

                    dest_buf8_row += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
                    dest_buf8 = dest_buf8_row;
                    src_buf += src_stride;
                }
            }
        }
        /*No mask but opacity*/
        else {
            uint8_t * dest_buf8_row = dest_buf8;
            for(y = 0; y < h; y++) {
                if(blend_fp == NULL) {
                    for(x = 0; x < w; x++) {
                        set_px_argb(dest_buf8, src_buf[x], opa);
                        dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                    }
                }
                else {
                    for(x = 0; x < w; x++) {
                        set_px_argb_blend(dest_buf8, src_buf[x], opa, blend_fp);
                        dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                    }
                }

                dest_buf8_row += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
                dest_buf8 = dest_buf8_row;
                src_buf += src_stride;
            }
        }
    }
    /*Masked*/
    else {
        /*Only the mask matters*/
        if(opa > LV_OPA_MAX) {
            uint8_t * dest_buf8_row = dest_buf8;
            for(y = 0; y < h; y++) {
                if(blend_fp == NULL) {
                    for(x = 0; x < w; x++) {
                        set_px_argb(dest_buf8, src_buf[x], mask[x]);
                        dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                    }
                }
                else {
                    for(x = 0; x < w; x++) {
                        set_px_argb_blend(dest_buf8, src_buf[x], mask[x], blend_fp);
                        dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                    }
                }
                dest_buf8_row += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
                dest_buf8 = dest_buf8_row;
                src_buf += src_stride;
                mask += mask_stride;
            }
        }
        /*Handle opa and mask values too*/
        else {
            uint8_t * dest_buf8_row = dest_buf8;
            for(y = 0; y < h; y++) {
                if(blend_fp == NULL) {
                    for(x = 0; x < w; x++) {
                        if(mask[x]) {
                            lv_opa_t opa_tmp = mask[x] >= LV_OPA_MAX ? opa : ((opa * mask[x]) >> 8);
                            set_px_argb(dest_buf8, src_buf[x], opa_tmp);
                        }
                        dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                    }
                }
                else {
                    for(x = 0; x < w; x++) {
                        if(mask[x]) {
                            lv_opa_t opa_tmp = mask[x] >= LV_OPA_MAX ? opa : ((opa * mask[x]) >> 8);
                            set_px_argb_blend(dest_buf8, src_buf[x], opa_tmp, blend_fp);
                        }
                        dest_buf8 += LV_IMG_PX_SIZE_ALPHA_BYTE;
                    }
                }
                dest_buf8_row += dest_stride * LV_IMG_PX_SIZE_ALPHA_BYTE;
                dest_buf8 = dest_buf8_row;
                src_buf += src_stride;
                mask += mask_stride;
            }
        }
    }
}
#endif


#if LV_DRAW_COMPLEX
static void map_blended(lv_color_t * dest_buf, const lv_area_t * dest_area, lv_coord_t dest_stride,
                        const lv_color_t * src_buf, lv_coord_t src_stride, lv_opa_t opa,
                        const lv_opa_t * mask, lv_coord_t mask_stride, lv_blend_mode_t blend_mode)
{

    int32_t w = lv_area_get_width(dest_area);
    int32_t h = lv_area_get_height(dest_area);

    int32_t x;
    int32_t y;

    lv_color_t (*blend_fp)(lv_color_t, lv_color_t, lv_opa_t);
    switch(blend_mode) {
        case LV_BLEND_MODE_ADDITIVE:
            blend_fp = color_blend_true_color_additive;
            break;
        case LV_BLEND_MODE_SUBTRACTIVE:
            blend_fp = color_blend_true_color_subtractive;
            break;
        case LV_BLEND_MODE_MULTIPLY:
            blend_fp = color_blend_true_color_multiply;
            break;
        default:
            LV_LOG_WARN("fill_blended: unsupported blend mode");
            return;
    }

    lv_color_t last_dest_color;
    lv_color_t last_src_color;
    /*Simple fill (maybe with opacity), no masking*/
    if(mask == NULL) {
        last_dest_color = dest_buf[0];
        last_src_color = src_buf[0];
        lv_color_t last_res_color = blend_fp(last_src_color, last_dest_color, opa);
        for(y = 0; y < h; y++) {
            for(x = 0; x < w; x++) {
                if(LV_BLEND_COLOR_NEQ(last_src_color, src_buf[x]) || LV_BLEND_COLOR_NEQ(last_dest_color, dest_buf[x])) {
                    LV_BLEND_COLOR_COPY(last_dest_color, dest_buf[x]);
                    LV_BLEND_COLOR_COPY(last_src_color, src_buf[x]);
                    last_res_color = blend_fp(last_src_color, last_dest_color, opa);
                }
                dest_buf[x] = last_res_color;
            }
            dest_buf += dest_stride;
            src_buf += src_stride;
        }
    }
    /*Masked*/
    else {
        last_dest_color = dest_buf[0];
        last_src_color = src_buf[0];
        lv_opa_t last_opa = mask[0] >= LV_OPA_MAX ? opa : ((opa * mask[0]) >> 8);
        lv_color_t last_res_color = blend_fp(last_src_color, last_dest_color, last_opa);
        for(y = 0; y < h; y++) {
            for(x = 0; x < w; x++) {
                if(mask[x] == 0) continue;
                lv_opa_t opa_tmp = mask[x] >= LV_OPA_MAX ? opa : ((opa * mask[x]) >> 8);
                if(LV_BLEND_COLOR_NEQ(last_src_color, src_buf[x]) || LV_BLEND_COLOR_NEQ(last_dest_color, dest_buf[x]) || last_opa != opa_tmp) {
                    LV_BLEND_COLOR_COPY(last_dest_color, dest_buf[x]);
                    LV_BLEND_COLOR_COPY(last_src_color, src_buf[x]);
                    last_opa = opa_tmp;
                    last_res_color = blend_fp(last_src_color, last_dest_color, last_opa);
                }
                dest_buf[x] = last_res_color;
            }
            dest_buf += dest_stride;
            src_buf += src_stride;
            mask += mask_stride;
        }
    }
}

static inline lv_color_t color_blend_true_color_additive(lv_color_t fg, lv_color_t bg, lv_opa_t opa)
{

    if(opa <= LV_OPA_MIN) return bg;

    uint32_t tmp;
#if LV_COLOR_DEPTH == 1
    tmp = bg.full + fg.full;
    fg.full = LV_MIN(tmp, 1);
#else
    /* R 通道: LV_COLOR_GET_R/SET_R 宏在 SWAP 下透明处理, 无需特判 */
    tmp = LV_COLOR_GET_R(bg) + LV_COLOR_GET_R(fg);
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 4
    LV_COLOR_SET_R(fg, LV_MIN(tmp, 15));        /* ARGB4444: 4-bit */
#elif LV_COLOR_DEPTH == 8
    LV_COLOR_SET_R(fg, LV_MIN(tmp, 7));         /* RGB332: 3-bit */
#elif LV_COLOR_DEPTH == 32
    LV_COLOR_SET_R(fg, LV_MIN(tmp, 255));       /* ARGB8888: 8-bit */
#else
    LV_COLOR_SET_R(fg, LV_MIN(tmp, 31));        /* 16(ALPHA 0/1) / 24: 5-bit */
#endif

    /* G 通道: 宏内部已处理 LV_COLOR_16_SWAP 的 green_h/green_l 拆分 */
    tmp = LV_COLOR_GET_G(bg) + LV_COLOR_GET_G(fg);
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 4
    LV_COLOR_SET_G(fg, LV_MIN(tmp, 15));        /* ARGB4444: 4-bit */
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 1
    LV_COLOR_SET_G(fg, LV_MIN(tmp, 31));        /* ARGB1555: 5-bit */
#elif LV_COLOR_DEPTH == 8
    LV_COLOR_SET_G(fg, LV_MIN(tmp, 7));         /* RGB332: 3-bit */
#elif LV_COLOR_DEPTH == 32
    LV_COLOR_SET_G(fg, LV_MIN(tmp, 255));       /* ARGB8888: 8-bit */
#else
    LV_COLOR_SET_G(fg, LV_MIN(tmp, 63));        /* 16(ALPHA 0) / 24: 6-bit */
#endif

    /* B 通道 */
    tmp = LV_COLOR_GET_B(bg) + LV_COLOR_GET_B(fg);
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 4
    LV_COLOR_SET_B(fg, LV_MIN(tmp, 15));        /* ARGB4444: 4-bit */
#elif LV_COLOR_DEPTH == 8
    LV_COLOR_SET_B(fg, LV_MIN(tmp, 3));         /* RGB332: 2-bit */
#elif LV_COLOR_DEPTH == 32
    LV_COLOR_SET_B(fg, LV_MIN(tmp, 255));       /* ARGB8888: 8-bit */
#else
    LV_COLOR_SET_B(fg, LV_MIN(tmp, 31));        /* 16(ALPHA 0/1) / 24: 5-bit */
#endif
#endif

    if(opa == LV_OPA_COVER) return fg;

    return lv_color_mix(fg, bg, opa);
}

static inline lv_color_t color_blend_true_color_subtractive(lv_color_t fg, lv_color_t bg, lv_opa_t opa)
{
    if(opa <= LV_OPA_MIN) return bg;

    int32_t tmp;
    tmp = (int32_t)LV_COLOR_GET_R(bg) - (int32_t)LV_COLOR_GET_R(fg);
    LV_COLOR_SET_R(fg, LV_MAX(tmp, 0));

    /* G 通道: 宏内部已处理 LV_COLOR_16_SWAP, 无需特判 */
    tmp = (int32_t)LV_COLOR_GET_G(bg) - (int32_t)LV_COLOR_GET_G(fg);
    LV_COLOR_SET_G(fg, LV_MAX(tmp, 0));

    tmp = (int32_t)LV_COLOR_GET_B(bg) - (int32_t)LV_COLOR_GET_B(fg);
    LV_COLOR_SET_B(fg, LV_MAX(tmp, 0));

    if(opa == LV_OPA_COVER) return fg;

    return lv_color_mix(fg, bg, opa);
}

static inline lv_color_t color_blend_true_color_multiply(lv_color_t fg, lv_color_t bg, lv_opa_t opa)
{
    if(opa <= LV_OPA_MIN) return bg;

    /* 移位值 = 通道位宽; 宏内部已处理 LV_COLOR_16_SWAP */
#if LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 4      /* ARGB4444: 4/4/4 */
    LV_COLOR_SET_R(fg, (LV_COLOR_GET_R(fg) * LV_COLOR_GET_R(bg)) >> 4);
    LV_COLOR_SET_G(fg, (LV_COLOR_GET_G(fg) * LV_COLOR_GET_G(bg)) >> 4);
    LV_COLOR_SET_B(fg, (LV_COLOR_GET_B(fg) * LV_COLOR_GET_B(bg)) >> 4);
#elif LV_COLOR_DEPTH == 16 && LV_COLOR_16_ALPHA == 1    /* ARGB1555: 5/5/5 */
    LV_COLOR_SET_R(fg, (LV_COLOR_GET_R(fg) * LV_COLOR_GET_R(bg)) >> 5);
    LV_COLOR_SET_G(fg, (LV_COLOR_GET_G(fg) * LV_COLOR_GET_G(bg)) >> 5);
    LV_COLOR_SET_B(fg, (LV_COLOR_GET_B(fg) * LV_COLOR_GET_B(bg)) >> 5);
#elif LV_COLOR_DEPTH == 8                                /* RGB332: 3/3/2 */
    LV_COLOR_SET_R(fg, (LV_COLOR_GET_R(fg) * LV_COLOR_GET_R(bg)) >> 3);
    LV_COLOR_SET_G(fg, (LV_COLOR_GET_G(fg) * LV_COLOR_GET_G(bg)) >> 3);
    LV_COLOR_SET_B(fg, (LV_COLOR_GET_B(fg) * LV_COLOR_GET_B(bg)) >> 2);
#elif LV_COLOR_DEPTH == 32                               /* ARGB8888: 8/8/8 */
    LV_COLOR_SET_R(fg, (LV_COLOR_GET_R(fg) * LV_COLOR_GET_R(bg)) >> 8);
    LV_COLOR_SET_G(fg, (LV_COLOR_GET_G(fg) * LV_COLOR_GET_G(bg)) >> 8);
    LV_COLOR_SET_B(fg, (LV_COLOR_GET_B(fg) * LV_COLOR_GET_B(bg)) >> 8);
#else   /* 16(ALPHA 0 RGB565) / 24(ARGB8565): 5/6/5 */
    LV_COLOR_SET_R(fg, (LV_COLOR_GET_R(fg) * LV_COLOR_GET_R(bg)) >> 5);
    LV_COLOR_SET_G(fg, (LV_COLOR_GET_G(fg) * LV_COLOR_GET_G(bg)) >> 6);
    LV_COLOR_SET_B(fg, (LV_COLOR_GET_B(fg) * LV_COLOR_GET_B(bg)) >> 5);
#endif

    if(opa == LV_OPA_COVER) return fg;

    return lv_color_mix(fg, bg, opa);
}

#endif

