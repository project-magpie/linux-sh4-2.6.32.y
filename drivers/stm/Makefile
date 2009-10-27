#
# Makefile for the STMicroelectronics device drivers.
#

obj-y					+= pad.o

obj-y				+= sysconf.o
obj-$(CONFIG_STM_DMA)		+= fdma.o fdma-xbar.o
obj-$(CONFIG_STM_PIO)		+= pio.o
obj-$(CONFIG_STM_COPROCESSOR_SUPPORT)	+= copro-st_socs.o
obj-$(CONFIG_STM_COPROCESSOR)		+= stm-coprocessor.o
obj-$(CONFIG_STM_COPROCESSOR_FRMW)	+= stm-coprocessor_frmw.o
obj-$(CONFIG_STM_RNG)		+= stm_rng.o
obj-y				+= emi.o
obj-$(CONFIG_PM)		+= platform-pm.o
obj-$(CONFIG_PMS)		+= pms.o

