#
# Makefile for the STMicroelectronics device drivers.
#

obj-y					+= emi.o
obj-y					+= gpio.o
obj-y					+= pad.o
obj-y					+= sysconf.o

obj-$(CONFIG_STM_DMA)		+= fdma.o fdma-xbar.o
obj-$(CONFIG_STM_PIO10)		+= pio10.o
obj-$(CONFIG_STM_COPROCESSOR_SUPPORT)	+= copro-st_socs.o
obj-$(CONFIG_STM_COPROCESSOR)		+= stm-coprocessor.o
obj-$(CONFIG_STM_COPROCESSOR_FRMW)	+= stm-coprocessor_frmw.o
obj-$(CONFIG_STM_RNG)			+= rng.o
obj-$(CONFIG_PM)		+= platform-pm.o
obj-$(CONFIG_PMS)		+= pms.o

sata-$(CONFIG_SATA_STM)		:= sata-init.o

obj-$(CONFIG_CPU_SUBTYPE_STX5197)	+= stx5197.o
obj-$(CONFIG_CPU_SUBTYPE_STX7100)	+= stx7100.o stx7100_audio.o stx7100_comms.o stx7100_hispeed.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105)	+= stx7105.o stx7105_audio.o stx7105_comms.o stx7105_hispeed.o stx7105_pci.o $(sata-y)
obj-$(CONFIG_CPU_SUBTYPE_STX7111)	+= stx7111.o stx7111_audio.o stx7111_comms.o stx7111_hispeed.o stx7111_pci.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= stx7141.o stx7141_audio.o stx7141_comms.o stx7141_hispeed.o $(sata-y)
obj-$(CONFIG_CPU_SUBTYPE_STX7200)	+= stx7200.o stx7200_audio.o stx7200_comms.o stx7200_hispeed.o $(sata-y)
