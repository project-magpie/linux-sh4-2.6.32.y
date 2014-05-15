#
# Makefile for aotom frontpanel controler of Fulan spark 71xx set-top boxes
#
# See the README file in this directory for more info. <k4230r6@gmail.com>
#

aotom-objs += aotom_trace.o
aotom-objs += aotom_i2csoft.o
aotom-objs += aotom_vfd.o
aotom-objs += aotom_main.o

obj-m += aotom.o

ifeq ($(CONFIG_SPARK7111),y)
EXTRA_CFLAGS += -DSPARK
endif
ifeq ($(CONFIG_SPARK7162),y)
EXTRA_CFLAGS += -DSPARK7162
endif
