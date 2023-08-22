#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <poll.h>
#include "video_manager.h"
#include "disp_manager.h"
#include "convert_manager.h"
#include "render.h"
#include <stdlib.h>

/* video2lcd /dev/video0 */
int main(int argc, char **argv)
{
    int ret;
    struct videobuf buf;
    struct videobuf cv_buf;
    struct videobuf zoombuf;
    struct videobuf fb_vdbuf;
    /* 显示设备的参数 */
    int lcd_w, lcd_h, lcd_bpp;

    if (argc != 2)
    {
        printf("input error!\r\n");
        return 0;
    }

    /* 一系列初始化操作 */
    /* 显示模块（LCD）初始化*/
    ret = DisplayInit(); /* 该函数内部调用fb_init函数 fb_init内部调用register_disp_ops */

    /* 选择使用fb作为显示设备
    fb_init函数内部其实可以支持多个显示设备的注册，
    然后通过select_init_disp_dev函数选择使用哪一个显示设备*/
    select_init_disp_dev("fb");                 /* 会调用选择的设备的device_init函数和clean_screen函数 */
    get_dispdev_info(&lcd_w, &lcd_h, &lcd_bpp); /* 获取显示设备的信息 */
    GetVideoBufForDisplay(&fb_vdbuf);

    /* 转化模块初始化 该函数注册三个转化模块*/
    VideoConvertInit();

    /* 摄像头初始化 注册video_ops*/
    video_init();

    struct camera_device my_camera;
    camera_device_init(argv[1], &my_camera);

    /* 注册了三个转化模块 需要根据我们需求的输入和输出格式确定选择哪一个转化模块 */
    struct convert_operations *cv_ops = GetVideoConvertForFormats(my_camera.pixelformat, fb_vdbuf.pixelformat);

    /* 启动摄像头 */
    (my_camera.v_ops)->start_device(&my_camera);

    memset(&buf, 0, sizeof(struct videobuf));
    memset(&cv_buf, 0, sizeof(struct videobuf));
    memset(&zoombuf, 0, sizeof(struct videobuf));
    cv_buf.pdata.iBpp = lcd_bpp;

    while (1)
    {
        /* 读入摄像头数据 */
        my_camera.v_ops->get_frame(&my_camera, &buf);

        /* 转化为RGB数据 */
        cv_ops->convert(&buf, &cv_buf);

        /* 如果图像分辨率大于LCD的分辨率 进行缩放 */
        if (cv_buf.pdata.iWidth > lcd_w || cv_buf.pdata.iHeight > lcd_h)
        {
            /* 确定分辨率 */
            float k = (float)cv_buf.pdata.iHeight / cv_buf.pdata.iWidth;
            zoombuf.pdata.iWidth = lcd_w;
            zoombuf.pdata.iHeight = lcd_w * k;
            if (zoombuf.pdata.iHeight > lcd_h)
            {
                zoombuf.pdata.iWidth = lcd_h / k;
                zoombuf.pdata.iHeight = lcd_h;
            }
            zoombuf.pdata.iBpp = lcd_bpp;
            zoombuf.pdata.iLineBytes = zoombuf.pdata.iWidth * zoombuf.pdata.iBpp / 8;
            zoombuf.pdata.iTotalBytes = zoombuf.pdata.iLineBytes * zoombuf.pdata.iHeight;

            if (!zoombuf.pdata.aucPixelDatas)
            {
                zoombuf.pdata.aucPixelDatas = malloc(zoombuf.pdata.iTotalBytes);
            }
            PicZoom(&cv_buf.pdata, &zoombuf.pdata);
        }
        /* 将数据合并到framebuffer中 */
        int top_left_x = (lcd_w - cv_buf.pdata.iWidth) / 2;
        int top_left_y = (lcd_h - cv_buf.pdata.iHeight) / 2;

        PicMerge(top_left_x, top_left_y, &cv_buf.pdata, &fb_vdbuf.pdata);

        /* 将framebuffer的数据显示到LCD上显示 */
        FlushPixelDatasToDev(&fb_vdbuf.pdata);

        my_camera.v_ops->put_frame(&my_camera, &buf);
    }

    return 0;
}