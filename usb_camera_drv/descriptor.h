#ifndef _DES_P_H_
#define _DES_P_H_

const char *get_guid(const unsigned char *buf);

void parse_videocontrol_interface(struct usb_interface *intf,
                                  unsigned char *buf, int buflen);

void parse_videostreaming_interface(struct usb_interface *intf,
                                    unsigned char *buf, int buflen);

void dump_endpoint(const struct usb_endpoint_descriptor *endpoint);


#endif
