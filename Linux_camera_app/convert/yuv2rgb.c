#include "convert_manager.h"
#include <linux/videodev2.h>
#include <stdlib.h>
#include "color.h"

int yuv_is_support(int pixelformat_in, int pixelformat_out)
{
    if (pixelformat_in != V4L2_PIX_FMT_YUYV)
        return 0;
    if (pixelformat_out != V4L2_PIX_FMT_RGB565 && pixelformat_out != V4L2_PIX_FMT_RGB32)
        return 0;
    return 1;
}

/* translate YUV422Packed to rgb24 */
static unsigned int
Pyuv422torgb565(unsigned char *input_ptr, unsigned char *output_ptr, unsigned int image_width, unsigned int image_height)
{
    unsigned int i, size;
    unsigned char Y, Y1, U, V;
    unsigned char *buff = input_ptr;
    unsigned char *output_pt = output_ptr;

    unsigned int r, g, b;
    unsigned int color;

    size = image_width * image_height / 2;
    for (i = size; i > 0; i--)
    {
        /* bgr instead rgb ?? */
        Y = buff[0];
        U = buff[1];
        Y1 = buff[2];
        V = buff[3];
        buff += 4;
        r = R_FROMYV(Y, V);
        g = G_FROMYUV(Y, U, V); // b
        b = B_FROMYU(Y, U);     // v

        /* 把r,g,b三色构造为rgb565的16位值 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;

        r = R_FROMYV(Y1, V);
        g = G_FROMYUV(Y1, U, V); // b
        b = B_FROMYU(Y1, U);     // v

        /* 把r,g,b三色构造为rgb565的16位值 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        *output_pt++ = color & 0xff;
        *output_pt++ = (color >> 8) & 0xff;
    }

    return 0;
}

/* translate YUV422Packed to rgb24 */

static unsigned int
Pyuv422torgb32(unsigned char *input_ptr, unsigned char *output_ptr, unsigned int image_width, unsigned int image_height)
{
    unsigned int i, size;
    unsigned char Y, Y1, U, V;
    unsigned char *buff = input_ptr;
    unsigned int *output_pt = (unsigned int *)output_ptr;

    unsigned int r, g, b;
    unsigned int color;

    size = image_width * image_height / 2;
    for (i = size; i > 0; i--)
    {
        /* bgr instead rgb ?? */
        Y = buff[0];
        U = buff[1];
        Y1 = buff[2];
        V = buff[3];
        buff += 4;

        r = R_FROMYV(Y, V);
        g = G_FROMYUV(Y, U, V); // b
        b = B_FROMYU(Y, U);     // v
        /* rgb888 */
        color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;

        r = R_FROMYV(Y1, V);
        g = G_FROMYUV(Y1, U, V); // b
        b = B_FROMYU(Y1, U);     // v
        color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;
    }

    return 0;
}

/* yuv2rgb函数 */
int yuv_convert(struct videobuf *vbuf_in, struct videobuf *vbuf_out)
{
    struct PixelDatas *pdata_in = &(vbuf_in->pdata);
    struct PixelDatas *pdata_out = &(vbuf_out->pdata);

    pdata_out->iWidth = pdata_in->iWidth;
    pdata_out->iHeight = pdata_in->iHeight;

    if (vbuf_out->pixelformat == V4L2_PIX_FMT_RGB565)
    {
        pdata_out->iBpp = 16;
        pdata_out->iLineBytes = pdata_out->iWidth * pdata_out->iHeight / 8;
        pdata_out->iTotalBytes = pdata_out->iLineBytes * pdata_out->iHeight;
        if (!pdata_out->aucPixelDatas)
        {
            pdata_out->aucPixelDatas = malloc(pdata_out->iTotalBytes);
        }
        Pyuv422torgb565(pdata_in->aucPixelDatas, pdata_out->aucPixelDatas, pdata_out->iWidth, pdata_out->iHeight);
    }
    else if (vbuf_out->pixelformat == V4L2_PIX_FMT_RGB32)
    {
        pdata_out->iBpp = 32;
        pdata_out->iLineBytes = pdata_out->iWidth * pdata_out->iHeight / 8;
        pdata_out->iTotalBytes = pdata_out->iLineBytes * pdata_out->iHeight;
        if (!pdata_out->aucPixelDatas)
        {
            pdata_out->aucPixelDatas = malloc(pdata_out->iTotalBytes);
        }
        Pyuv422torgb32(pdata_in->aucPixelDatas, pdata_out->aucPixelDatas, pdata_out->iWidth, pdata_out->iHeight);
    }
    return 0;
}

int yuv_convert_exit(struct videobuf *vbuf_out)
{
    if (vbuf_out->pdata.aucPixelDatas)
    {
        free(vbuf_out->pdata.aucPixelDatas);
        vbuf_out->pdata.aucPixelDatas = NULL;
    }
    return 0;
}

/* 构造convert_ops结构体 */

static struct convert_operations yuv_c_ops = {
    .is_support = yuv_is_support,
    .convert = yuv_convert,
    .convert_exit = yuv_convert_exit,
    .name = "yuv_convert",
};

extern void initLut(void);

/* 注册 */
int Yuv2RgbInit(void)
{
    initLut();
    return register_convert_ops(&yuv_c_ops);
}