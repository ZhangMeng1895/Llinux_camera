#include "convert_manager.h"
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>

static int isSupportRgb2Rgb(int pixelformatIn, int pixelformatOut)
{
    if (pixelformatIn != V4L2_PIX_FMT_RGB565)
        return 0;
    if ((pixelformatOut != V4L2_PIX_FMT_RGB565) && (pixelformatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}

static int Rgb2RgbConvert(struct videobuf *vbuf_in, struct videobuf *vbuf_out)
{
    struct PixelDatas *pdata_in = &vbuf_in->pdata;
    struct PixelDatas *pdata_out = &vbuf_out->pdata;

    int x, y;
    int r, g, b;
    int color;
    unsigned short *pwSrc = (unsigned short *)pdata_in->aucPixelDatas;
    unsigned int *pdwDest;

    if (vbuf_in->pixelformat != V4L2_PIX_FMT_RGB565)
    {
        return -1;
    }

    if (vbuf_out->pixelformat == V4L2_PIX_FMT_RGB565)
    {
        pdata_out->iWidth = pdata_in->iWidth;
        pdata_out->iHeight = pdata_in->iHeight;
        pdata_out->iBpp = 16;
        pdata_out->iLineBytes = pdata_out->iWidth * pdata_out->iBpp / 8;
        pdata_out->iTotalBytes = pdata_out->iLineBytes * pdata_out->iHeight;
        if (!pdata_out->aucPixelDatas)
        {
            pdata_out->aucPixelDatas = malloc(pdata_out->iTotalBytes);
        }

        memcpy(pdata_out->aucPixelDatas, pdata_in->aucPixelDatas, pdata_out->iTotalBytes);
        return 0;
    }
    else if (vbuf_out->pixelformat == V4L2_PIX_FMT_RGB32)
    {
        pdata_out->iWidth = pdata_in->iWidth;
        pdata_out->iHeight = pdata_in->iHeight;
        pdata_out->iBpp = 32;
        pdata_out->iLineBytes = pdata_out->iWidth * pdata_out->iBpp / 8;
        pdata_out->iTotalBytes = pdata_out->iLineBytes * pdata_out->iHeight;
        if (!pdata_out->aucPixelDatas)
        {
            pdata_out->aucPixelDatas = malloc(pdata_out->iTotalBytes);
        }

        pdwDest = (unsigned int *)pdata_out->aucPixelDatas;

        for (y = 0; y < pdata_out->iHeight; y++)
        {
            for (x = 0; x < pdata_out->iWidth; x++)
            {
                color = *pwSrc++;
                /* 从RGB565格式的数据中提取出R,G,B */
                r = color >> 11;
                g = (color >> 5) & (0x3f);
                b = color & 0x1f;

                /* 把r,g,b转为0x00RRGGBB的32位数据 */
                color = ((r << 3) << 16) | ((g << 2) << 8) | (b << 3);

                *pdwDest = color;
                pdwDest++;
            }
        }
        return 0;
    }

    return -1;
}

static int Rgb2RgbConvertExit(struct videobuf *vbuf_out)
{
    if (vbuf_out->pdata.aucPixelDatas)
    {
        free(vbuf_out->pdata.aucPixelDatas);
        vbuf_out->pdata.aucPixelDatas = NULL;
    }
    return 0;
}

/* 构造 */
static struct convert_operations rgb_c_ops = {
    .name = "rgb2rgb",
    .is_support = isSupportRgb2Rgb,
    .convert = Rgb2RgbConvert,
    .convert_exit = Rgb2RgbConvertExit,
};

/* 注册 */
int Rgb2RgbInit(void)
{
    return register_convert_ops(&rgb_c_ops);
}
