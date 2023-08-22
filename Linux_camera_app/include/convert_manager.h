#ifndef _CONOVERT_MA_H
#define _CONOVERT_MA_H

#include "video_manager.h"

struct convert_operations
{
    char *name;
    int (*is_support)(int pixelformat_in, int pixelformat_out);
    int (*convert)(struct videobuf *vbuf_in, struct videobuf *vbuf_out);
    int (*convert_exit)(struct videobuf *vbuf_out);
    struct convert_operations *next;
};

int register_convert_ops(struct convert_operations *c_ops);

void show_convert_ops(void);

struct convert_operations *get_convert_ops(char *pcName);

struct convert_operations *GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut);

int VideoConvertInit(void);

#endif