/*
 * @Author: Zhang Meng
 * @Date: 2023-08-17 15:06:57
 * @LastEditTime: 2023-08-18 21:19:01
 * @LastEditors: your name
 * @Description:
 * @FilePath: \video2lcd\render\operation\zoom.c
 */
#include <config.h>
#include <pic_operation.h>
#include <stdlib.h>
#include <string.h>

/**********************************************************************
 * 函数名称： PicZoom
 * 功能描述： 近邻取样插值方法缩放图片
 *            注意该函数会分配内存来存放缩放后的图片,用完后要用free函数释放掉
 *            "近邻取样插值"的原理请参考网友"lantianyu520"所著的"图像缩放算法"
 * 输入参数： ptOriginPic - 内含原始图片的象素数据
 *            ptBigPic    - 内含缩放后的图片的象素数据
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  韦东山	      创建
 ***********************************************************************/
int PicZoom(struct PixelDatas *ptOriginPic, struct PixelDatas *ptZoomPic)
{
    unsigned long dwDstWidth = ptZoomPic->iWidth;
    unsigned long *pdwSrcXTable;
    unsigned long x;
    unsigned long y;
    unsigned long dwSrcY;
    unsigned char *pucDest;
    unsigned char *pucSrc;
    unsigned long dwPixelBytes = ptOriginPic->iBpp / 8;

    DBG_PRINTF("src:\n");
    DBG_PRINTF("%d x %d, %d bpp, data: 0x%x\n", ptOriginPic->iWidth, ptOriginPic->iHeight, ptOriginPic->iBpp, (unsigned int)ptOriginPic->aucPixelDatas);

    DBG_PRINTF("dest:\n");
    DBG_PRINTF("%d x %d, %d bpp, data: 0x%x\n", ptZoomPic->iWidth, ptZoomPic->iHeight, ptZoomPic->iBpp, (unsigned int)ptZoomPic->aucPixelDatas);

    if (ptOriginPic->iBpp != ptZoomPic->iBpp)
    {
        return -1;
    }

    pdwSrcXTable = malloc(sizeof(unsigned long) * dwDstWidth);
    if (NULL == pdwSrcXTable)
    {
        DBG_PRINTF("malloc error!\n");
        return -1;
    }

    for (x = 0; x < dwDstWidth; x++) // 生成表 pdwSrcXTable
    {
        pdwSrcXTable[x] = (x * ptOriginPic->iWidth / ptZoomPic->iWidth);
    }

    for (y = 0; y < ptZoomPic->iHeight; y++)
    {
        dwSrcY = (y * ptOriginPic->iHeight / ptZoomPic->iHeight);

        pucDest = ptZoomPic->aucPixelDatas + y * ptZoomPic->iLineBytes;
        pucSrc = ptOriginPic->aucPixelDatas + dwSrcY * ptOriginPic->iLineBytes;

        for (x = 0; x < dwDstWidth; x++)
        {
            /* 原图座标: pdwSrcXTable[x]，srcy
             * 缩放座标: x, y
             */
            memcpy(pucDest + x * dwPixelBytes, pucSrc + pdwSrcXTable[x] * dwPixelBytes, dwPixelBytes);
        }
    }

    free(pdwSrcXTable);
    return 0;
}
