#include "config.h"
#include "disp_manager.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>

static int g_fd;

static struct disp_operations fb_dispops;

static struct fb_var_screeninfo g_tFBVar;
static struct fb_fix_screeninfo g_tFBFix;
static unsigned char *g_pucFBMem;
static unsigned int g_dwScreenSize;

static unsigned int g_dwLineWidth;
static unsigned int g_dwPixelWidth;

/* 显示设备初始化 */
int fb_device_init(void)
{
    int ret;

    g_fd = open(FB_DEVICE_NAME, O_RDWR);
    if (0 > g_fd)
    {
        DBG_PRINTF("can't open %s\n", FB_DEVICE_NAME);
    }

    ret = ioctl(g_fd, FBIOGET_VSCREENINFO, &g_tFBVar);
    if (ret < 0)
    {
        DBG_PRINTF("can't get fb's var\n");
        return -1;
    }

    ret = ioctl(g_fd, FBIOGET_FSCREENINFO, &g_tFBFix);
    if (ret < 0)
    {
        DBG_PRINTF("can't get fb's fix\n");
        return -1;
    }

    g_dwScreenSize = g_tFBVar.xres * g_tFBVar.yres * g_tFBVar.bits_per_pixel / 8;
    g_pucFBMem = (unsigned char *)mmap(NULL, g_dwScreenSize, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (0 > g_pucFBMem)
    {
        DBG_PRINTF("can't mmap\n");
        return -1;
    }

    fb_dispops.x = g_tFBVar.xres;
    fb_dispops.y = g_tFBVar.yres;
    fb_dispops.bpp = g_tFBVar.bits_per_pixel;
    fb_dispops.line_bytes = g_tFBVar.xres * fb_dispops.bpp / 8;
    fb_dispops.addr_mem = g_pucFBMem;

    g_dwLineWidth = g_tFBVar.xres * g_tFBVar.bits_per_pixel / 8;
    g_dwPixelWidth = g_tFBVar.bits_per_pixel / 8;

    return 0;
    return 0;
}

/* 清屏为某种颜色 */
int fb_clean_screen(unsigned int back_color)
{
    unsigned char *pucFB;
    unsigned short *pwFB16bpp;
    unsigned int *pdwFB32bpp;
    unsigned short wColor16bpp; /* 565 */
    int iRed;
    int iGreen;
    int iBlue;
    int i = 0;

    pucFB = g_pucFBMem;
    pwFB16bpp = (unsigned short *)pucFB;
    pdwFB32bpp = (unsigned int *)pucFB;

    switch (g_tFBVar.bits_per_pixel)
    {
    case 8:
    {
        memset(g_pucFBMem, back_color, g_dwScreenSize);
        break;
    }
    case 16:
    {
        /* 从dwBackColor中取出红绿蓝三原色,
         * 构造为16Bpp的颜色
         */
        iRed = (back_color >> (16 + 3)) & 0x1f;
        iGreen = (back_color >> (8 + 2)) & 0x3f;
        iBlue = (back_color >> 3) & 0x1f;
        wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
        while (i < g_dwScreenSize)
        {
            *pwFB16bpp = wColor16bpp;
            pwFB16bpp++;
            i += 2;
        }
        break;
    }
    case 32:
    {
        while (i < g_dwScreenSize)
        {
            *pdwFB32bpp = back_color;
            pdwFB32bpp++;
            i += 4;
        }
        break;
    }
    default:
    {
        DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
        return -1;
    }
    }
    return 0;
}

/* 设置某点像素为某个颜色 */
int fb_show_pixel(int x, int y, unsigned int color)
{
    unsigned char *pucFB;
    unsigned short *pwFB16bpp;
    unsigned int *pdwFB32bpp;
    unsigned short wColor16bpp; /* 565 */
    int iRed;
    int iGreen;
    int iBlue;

    if ((x >= g_tFBVar.xres) || (y >= g_tFBVar.yres))
    {
        DBG_PRINTF("out of region\n");
        return -1;
    }

    pucFB = g_pucFBMem + g_dwLineWidth * y + g_dwPixelWidth * x;
    pwFB16bpp = (unsigned short *)pucFB;
    pdwFB32bpp = (unsigned int *)pucFB;

    switch (g_tFBVar.bits_per_pixel)
    {
    case 8:
    {
        *pucFB = (unsigned char)color;
        break;
    }
    case 16:
    {
        /* 从dwBackColor中取出红绿蓝三原色,
         * 构造为16Bpp的颜色
         */
        iRed = (color >> (16 + 3)) & 0x1f;
        iGreen = (color >> (8 + 2)) & 0x3f;
        iBlue = (color >> 3) & 0x1f;
        wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
        *pwFB16bpp = wColor16bpp;
        break;
    }
    case 32:
    {
        *pdwFB32bpp = color;
        break;
    }
    default:
    {
        DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
        return -1;
    }
    }
    return 0;
}

/* 显示一页 */
int fb_show_page(struct PixelDatas *pdata)
{
    if (fb_dispops.addr_mem != pdata->aucPixelDatas)
    {
        memcpy(fb_dispops.addr_mem, pdata->aucPixelDatas, pdata->iTotalBytes);
    }
    return 0;
}

static struct disp_operations fb_dispops = {
    .name = "fb",
    .device_init = fb_device_init,
    .show_pixel = fb_show_pixel,
    .clean_screen = fb_clean_screen,
    .show_page = fb_show_page,
};

int FBInit(void)
{
    return register_disp_ops(&fb_dispops);
}
