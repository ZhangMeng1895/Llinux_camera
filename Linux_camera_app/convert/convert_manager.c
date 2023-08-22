
#include <config.h>
#include <convert_manager.h>
#include <string.h>

struct convert_operations *cops_head = NULL;

int register_convert_ops(struct convert_operations *c_ops)
{
    struct convert_operations *ptTmp;

    if (!cops_head)
    {
        cops_head = c_ops;
        c_ops->next = NULL;
    }
    else
    {
        ptTmp = cops_head;
        while (ptTmp->next)
        {
            ptTmp = ptTmp->next;
        }
        ptTmp->next = c_ops;
        c_ops->next = NULL;
    }

    return 0;
}

void show_convert_ops(void)
{
    int i = 0;
    struct convert_operations *ptTmp = cops_head;

    while (ptTmp)
    {
        printf("%02d %s\n", i++, ptTmp->name);
        ptTmp = ptTmp->next;
    }
}

struct convert_operations *get_convert_ops(char *pcName)
{
    struct convert_operations *ptTmp = cops_head;

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

struct convert_operations *GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut)
{
    struct convert_operations *ptTmp = cops_head;

    while (ptTmp)
    {
        if (ptTmp->is_support(iPixelFormatIn, iPixelFormatOut))
        {
            return ptTmp;
        }
        ptTmp = ptTmp->next;
    }
    return NULL;
}

int VideoConvertInit(void)
{
    int iError;

    iError = Yuv2RgbInit();
    iError |= Mjpeg2RgbInit();
    iError |= Rgb2RgbInit();

    return iError;
}
