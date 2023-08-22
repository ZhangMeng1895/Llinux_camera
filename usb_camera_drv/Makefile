KERN_DIR = /usr/src/linux-headers-5.4.0-150-generic

# KERN_VER = $(shell uname -r)
# KERN_DIR = /lib/modules/$(KERN_VER)/build

all:
	make -C $(KERN_DIR) M=`pwd` modules 
	
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

myuvc_drv-y=myuvc.o descriptor.o video.o video_stream.o
obj-m	+= myuvc_drv.o 

