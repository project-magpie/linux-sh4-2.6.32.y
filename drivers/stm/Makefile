#
# Makefile for the STMicroelectronics device drivers.
#

obj-$(CONFIG_STM_DMA)		+= 710x_fdma2.o
obj-$(CONFIG_STM_PIO)		+= pio.o
obj-$(CONFIG_STM_SSC)		+= stm_ssc.o
obj-$(CONFIG_STM_SPI)		+= stm_spi.o
obj-$(CONFIG_STM_COPROCESSOR)	+= stm-coprocessor.o
obj-$(CONFIG_STM_COPROCESSOR_FRMW)   += stm-coprocessor_frmw.o
ifneq ($(CONFIG_STM_COPROCESSOR)$(CONFIG_STM_COPROCESSOR_FRMW),nn)
obj-$(CONFIG_CPU_SUBTYPE_STB7100)	+= copro-stb7100.o
endif
