#include "config.h"
#include "disp_manager.h"
#include <string.h>
#include <linux/videodev2.h>
#include <stdlib.h>

static struct disp_operations *g_ptDispOprHead;
static struct disp_operations *g_ptDefaultDispOpr;
static struct video_mem *g_ptVideoMemHead;

int register_disp_ops(struct disp_operations *ptDispOpr)
{
    struct disp_operations *ptTmp;

    if (!g_ptDispOprHead)
    {
        g_ptDispOprHead = ptDispOpr;
        ptDispOpr->next = NULL;
    }
    else
    {
        ptTmp = g_ptDispOprHead;
        while (ptTmp->next)
        {
            ptTmp = ptTmp->next;
        }
        ptTmp->next = ptDispOpr;
        ptDispOpr->next = NULL;
    }

    return 0;
}

/**********************************************************************
 * 函数名称： show_disp_ops
 * 功能描述： 显示本程序能支持的"显示模块"
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  韦东山	      创建
 ***********************************************************************/
void show_disp_ops(void)
{
    int i = 0;
    struct disp_operations *ptTmp = g_ptDispOprHead;

    while (ptTmp)
    {
        printf("%02d %s\n", i++, ptTmp->name);
        ptTmp = ptTmp->next;
    }
}

/**********************************************************************
 * 函数名称： get_disp_ops
 * 功能描述： 根据名字取出指定的"显示模块"
 * 输入参数： pcName - 名字
 * 输出参数： 无
 * 返 回 值： NULL   - 失败,没有指定的模块,
 *            非NULL - 显示模块的struct disp_operations *结构体指针
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 ***********************************************************************/
struct disp_operations *get_disp_ops(char *pcName)
{
    struct disp_operations *ptTmp = g_ptDispOprHead;

    while (ptTmp)
    {
        if (strcmp(ptTmp->name, pcName) == 0)
        {
            return ptTmp;
        }
        ptTmp = ptTmp->next;
    }
    return NULL;
}

/**********************************************************************
 * 函数名称： select_init_disp_dev
 * 功能描述： 根据名字取出指定的"显示模块", 调用它的初始化函数, 并且清屏
 * 输入参数： name - 名字
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 ***********************************************************************/
void select_init_disp_dev(char *name)
{
    g_ptDefaultDispOpr = get_disp_ops(name);
    if (g_ptDefaultDispOpr)
    {
        g_ptDefaultDispOpr->device_init();
        g_ptDefaultDispOpr->clean_screen(0);
    }
}

/**********************************************************************
 * 函数名称： get_default_disp_dev
 * 功能描述： 程序事先用SelectAndInitDefaultDispDev选择了显示模块,
 *            本函数返回该显示模块
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 显示模块的struct disp_operations *结构体指针
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 ***********************************************************************/
struct disp_operations *get_default_disp_dev(void)
{
    return g_ptDefaultDispOpr;
}

/**********************************************************************
 * 函数名称： get_dispdev_info
 * 功能描述： 获得所使用的显示设备的分辨率和BPP
 * 输入参数： 无
 * 输出参数： piXres - 存X分辨率
 *            piYres - 存X分辨率
 *            piBpp  - 存BPP
 * 返 回 值： 0  - 成功
 *            -1 - 失败(未使用SelectAndInitDefaultDispDev来选择显示模块)
 ***********************************************************************/
int get_dispdev_info(int *piXres, int *piYres, int *piBpp)
{
    if (g_ptDefaultDispOpr)
    {
        *piXres = g_ptDefaultDispOpr->x;
        *piYres = g_ptDefaultDispOpr->y;
        *piBpp = g_ptDefaultDispOpr->bpp;
        return 0;
    }
    else
    {
        return -1;
    }
}

int GetVideoBufForDisplay(struct videobuf *ptFrameBuf)
{
    ptFrameBuf->pixelformat = (g_ptDefaultDispOpr->bpp == 16) ? V4L2_PIX_FMT_RGB565 : (g_ptDefaultDispOpr->bpp == 32) ? V4L2_PIX_FMT_RGB32
                                                                                                                      : 0;
    ptFrameBuf->pdata.iWidth = g_ptDefaultDispOpr->x;
    ptFrameBuf->pdata.iHeight = g_ptDefaultDispOpr->y;
    ptFrameBuf->pdata.iBpp = g_ptDefaultDispOpr->bpp;
    ptFrameBuf->pdata.iLineBytes = g_ptDefaultDispOpr->line_bytes;
    ptFrameBuf->pdata.iTotalBytes = ptFrameBuf->pdata.iLineBytes * ptFrameBuf->pdata.iHeight;
    ptFrameBuf->pdata.aucPixelDatas = g_ptDefaultDispOpr->addr_mem;
    return 0;
}

void FlushPixelDatasToDev(struct PixelDatas *pdata)
{
    g_ptDefaultDispOpr->show_page(pdata);
}

/**********************************************************************
 * 函数名称： alloc_video_mem
 * 功能描述： VideoMem: 为加快显示速度,我们事先在缓存中构造好显示的页面的数据,
 *            (这个缓存称为VideoMem)
 *            显示时再把VideoMem中的数据复制到设备的显存上
 * 输入参数： iNum
 * 输出参数： 无
 * 返 回 值： 0  - 成功
 *            -1 - 失败(未使用SelectAndInitDefaultDispDev来选择显示模块)
 * -----------------------------------------------
 ***********************************************************************/
int alloc_video_mem(int iNum)
{
    int i;

    int x = 0;
    int y = 0;
    int bpp = 0;

    int videomem_size;
    int line_bytes;

    struct video_mem *ptNew;

    /* 确定VideoMem的大小
     */
    get_dispdev_info(&x, &y, &bpp);
    videomem_size = x * y * bpp / 8;
    line_bytes = x * bpp / 8;

    /* 先把设备本身的framebuffer放入链表
     * 分配一个T_VideoMem结构体, 注意我们没有分配里面的pdata.aucPixelDatas
     * 而是让pdata.aucPixelDatas指向显示设备的framebuffer
     */
    ptNew = malloc(sizeof(struct video_mem));
    if (ptNew == NULL)
    {
        return -1;
    }

    /* 指向framebuffer */
    ptNew->pdata.aucPixelDatas = g_ptDefaultDispOpr->addr_mem;

    ptNew->id = 0;
    ptNew->bDevFrameBuffer = 1; /* 表示这个VideoMem是设备本身的framebuffer, 而不是用作缓存作用的VideoMem */
    ptNew->vmstate = VMS_FREE;
    ptNew->vpstate = PS_BLANK;
    ptNew->pdata.iWidth = x;
    ptNew->pdata.iHeight = y;
    ptNew->pdata.iBpp = bpp;
    ptNew->pdata.iLineBytes = line_bytes;
    ptNew->pdata.iTotalBytes = videomem_size;

    if (iNum != 0)
    {
        /* 如果下面要分配用于缓存的VideoMem,
         * 把设备本身framebuffer对应的VideoMem状态设置为VMS_USED_FOR_CUR,
         * 表示这个VideoMem不会被作为缓存分配出去
         */
        ptNew->vmstate = VMS_USED_FOR_CUR;
    }

    /* 放入链表 */
    ptNew->next = g_ptVideoMemHead;
    g_ptVideoMemHead = ptNew;

    /*
     * 分配用于缓存的VideoMem
     */
    for (i = 0; i < iNum; i++)
    {
        /* 分配T_VideoMem结构体本身和"跟framebuffer同样大小的缓存" */
        ptNew = malloc(sizeof(struct video_mem) + videomem_size);
        if (ptNew == NULL)
        {
            return -1;
        }
        /* 在T_VideoMem结构体里记录上面分配的"跟framebuffer同样大小的缓存" */
        ptNew->pdata.aucPixelDatas = (unsigned char *)(ptNew + 1);

        ptNew->id = 0;
        ptNew->bDevFrameBuffer = 0;
        ptNew->vmstate = VMS_FREE;
        ptNew->vpstate = PS_BLANK;
        ptNew->pdata.iWidth = x;
        ptNew->pdata.iHeight = y;
        ptNew->pdata.iBpp = bpp;
        ptNew->pdata.iLineBytes = line_bytes;
        ptNew->pdata.iTotalBytes = videomem_size;

        /* 放入链表 */
        ptNew->next = g_ptVideoMemHead;
        g_ptVideoMemHead = ptNew;
    }

    return 0;
}

/**********************************************************************
 * 函数名称： get_video_mem
 * 功能描述： 获得一块可操作的VideoMem(它用于存储要显示的数据),
 *            用完后用put_video_mem来释放
 * 输入参数： id  - ID值,先尝试在众多VideoMem中找到ID值相同的
 *            bCur - 1表示当前程序马上要使用VideoMem,无法如何都要返回一个VideoMem
 *                   0表示这是为了改进性能而提前取得VideoMem,不是必需的
 * 输出参数： 无
 * 返 回 值： NULL   - 失败,没有可用的VideoMem
 *            非NULL - 成功,返回struct  video_mem *结构体
 ***********************************************************************/
struct video_mem *get_video_mem(int id, int bCur)
{
    struct video_mem *ptTmp = g_ptVideoMemHead;

    /* 1. 优先: 取出空闲的、ID相同的videomem */
    while (ptTmp)
    {
        if ((ptTmp->vmstate == VMS_FREE) && (ptTmp->id == id))
        {
            ptTmp->vmstate = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
            return ptTmp;
        }
        ptTmp = ptTmp->next;
    }

    /* 2. 如果前面不成功, 取出一个空闲的并且里面没有数据(ptVideoMem->vpstate = PS_BLANK)的VideoMem */
    ptTmp = g_ptVideoMemHead;
    while (ptTmp)
    {
        if ((ptTmp->vmstate == VMS_FREE) && (ptTmp->vpstate == PS_BLANK))
        {
            ptTmp->id = id;
            ptTmp->vmstate = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
            return ptTmp;
        }
        ptTmp = ptTmp->next;
    }

    /* 3. 如果前面不成功: 取出任意一个空闲的VideoMem */
    ptTmp = g_ptVideoMemHead;
    while (ptTmp)
    {
        if (ptTmp->vmstate == VMS_FREE)
        {
            ptTmp->id = id;
            ptTmp->vpstate = PS_BLANK;
            ptTmp->vmstate = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
            return ptTmp;
        }
        ptTmp = ptTmp->next;
    }

    /* 4. 如果没有空闲的VideoMem并且bCur为1, 则取出任意一个VideoMem(不管它是否空闲) */
    if (bCur)
    {
        ptTmp = g_ptVideoMemHead;
        ptTmp->id = id;
        ptTmp->vpstate = PS_BLANK;
        ptTmp->vmstate = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
        return ptTmp;
    }

    return NULL;
}

/**********************************************************************
 * 函数名称： put_video_mem
 * 功能描述： 使用GetVideoMem获得的VideoMem, 用完时用put_video_mem释放掉
 * 输入参数： ptVideoMem - 使用完毕的VideoMem
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void put_video_mem(struct video_mem *ptVideoMem)
{
    ptVideoMem->vmstate = VMS_FREE; /* 设置VideoMem状态为空闲 */
    if (ptVideoMem->id == -1)
    {
        ptVideoMem->vpstate = PS_BLANK; /* 表示里面的数据没有用了 */
    }
}

/**********************************************************************
 * 函数名称： GetDevVideoMem
 * 功能描述： 获得显示设备的显存, 在该显存上操作就可以直接在LCD上显示出来
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 显存对应的VideoMem结构体指针
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  韦东山	      创建
 ***********************************************************************/
struct video_mem *get_dev_video_mem(void)
{
    struct video_mem *ptTmp = g_ptVideoMemHead;

    while (ptTmp)
    {
        if (ptTmp->bDevFrameBuffer)
        {
            return ptTmp;
        }
        ptTmp = ptTmp->next;
    }
    return NULL;
}

/**********************************************************************
 * 函数名称： ClearVideoMem
 * 功能描述： 把VideoMem中内存全部清为某种颜色
 * 输入参数： ptVideoMem - VideoMem结构体指针, 内含要操作的内存
 *            dwColor    - 设置为该颜色
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  韦东山	      创建
 ***********************************************************************/
void clear_video_mem(struct video_mem *ptVideoMem, unsigned int dwColor)
{
    unsigned char *pucVM;
    unsigned short *pwVM16bpp;
    unsigned int *pdwVM32bpp;
    unsigned short wColor16bpp; /* 565 */
    int iRed;
    int iGreen;
    int iBlue;
    int i = 0;

    pucVM = ptVideoMem->pdata.aucPixelDatas;
    pwVM16bpp = (unsigned short *)pucVM;
    pdwVM32bpp = (unsigned int *)pucVM;

    switch (ptVideoMem->pdata.iBpp)
    {
    case 8:
    {
        memset(pucVM, dwColor, ptVideoMem->pdata.iTotalBytes);
        break;
    }
    case 16:
    {
        /* 先根据32位的dwColor构造出16位的wColor16bpp */
        iRed = (dwColor >> (16 + 3)) & 0x1f;
        iGreen = (dwColor >> (8 + 2)) & 0x3f;
        iBlue = (dwColor >> 3) & 0x1f;
        wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
        while (i < ptVideoMem->pdata.iTotalBytes)
        {
            *pwVM16bpp = wColor16bpp;
            pwVM16bpp++;
            i += 2;
        }
        break;
    }
    case 32:
    {
        while (i < ptVideoMem->pdata.iTotalBytes)
        {
            *pdwVM32bpp = dwColor;
            pdwVM32bpp++;
            i += 4;
        }
        break;
    }
    default:
    {
        DBG_PRINTF("can't support %d bpp\n", ptVideoMem->pdata.iBpp);
        return;
    }
    }
}

/**********************************************************************
 * 函数名称： ClearVideoMemRegion
 * 功能描述： 把VideoMem中内存的指定区域全部清为某种颜色
 * 输入参数： ptVideoMem - VideoMem结构体指针, 内含要操作的内存
 *            ptLayout   - 矩形区域, 指定了左上角,右在角的坐标
 *            dwColor    - 设置为该颜色
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  韦东山	      创建
 ***********************************************************************/
void clear_video_mem_region(struct video_mem *ptVideoMem, struct layout *ptLayout, unsigned int dwColor)
{
    unsigned char *pucVM;
    unsigned short *pwVM16bpp;
    unsigned int *pdwVM32bpp;
    unsigned short wColor16bpp; /* 565 */
    int iRed;
    int iGreen;
    int iBlue;
    int iX;
    int iY;
    int iLineBytesClear;
    int i;

    pucVM = ptVideoMem->pdata.aucPixelDatas + ptLayout->top_left_y * ptVideoMem->pdata.iLineBytes + ptLayout->top_left_x * ptVideoMem->pdata.iBpp / 8;
    pwVM16bpp = (unsigned short *)pucVM;
    pdwVM32bpp = (unsigned int *)pucVM;

    iLineBytesClear = (ptLayout->bot_right_x - ptLayout->top_left_x + 1) * ptVideoMem->pdata.iBpp / 8;

    switch (ptVideoMem->pdata.iBpp)
    {
    case 8:
    {
        for (iY = ptLayout->top_left_y; iY <= ptLayout->bot_right_y; iY++)
        {
            memset(pucVM, dwColor, iLineBytesClear);
            pucVM += ptVideoMem->pdata.iLineBytes;
        }
        break;
    }
    case 16:
    {
        /* 先根据32位的dwColor构造出16位的wColor16bpp */
        iRed = (dwColor >> (16 + 3)) & 0x1f;
        iGreen = (dwColor >> (8 + 2)) & 0x3f;
        iBlue = (dwColor >> 3) & 0x1f;
        wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
        for (iY = ptLayout->top_left_y; iY <= ptLayout->bot_right_y; iY++)
        {
            i = 0;
            for (iX = ptLayout->top_left_x; iX <= ptLayout->bot_right_x; iX++)
            {
                pwVM16bpp[i++] = wColor16bpp;
            }
            pwVM16bpp = (unsigned short *)((unsigned int)pwVM16bpp + ptVideoMem->pdata.iLineBytes);
        }
        break;
    }
    case 32:
    {
        for (iY = ptLayout->top_left_y; iY <= ptLayout->bot_right_y; iY++)
        {
            i = 0;
            for (iX = ptLayout->top_left_x; iX <= ptLayout->bot_right_x; iX++)
            {
                pdwVM32bpp[i++] = dwColor;
            }
            pdwVM32bpp = (unsigned int *)((unsigned int)pdwVM32bpp + ptVideoMem->pdata.iLineBytes);
        }
        break;
    }
    default:
    {
        DBG_PRINTF("can't support %d bpp\n", ptVideoMem->pdata.iBpp);
        return;
    }
    }
}

/**********************************************************************
 * 函数名称： DisplayInit
 * 功能描述： 注册显示设备
 * 输入参数： 无
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 ***********************************************************************/
int DisplayInit(void)
{
    int iError;

    iError = FBInit();

    return iError;
}
