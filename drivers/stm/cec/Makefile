obj-m           += cec.o 
cec-objs        += cec_main.o cec_worker.o cec_opcodes.o cec_internal.o cec_proc.o cec_rc.o cec_dev.o

EXTRA_CFLAGS    := -Idrivers/dvb/drivers/media/dvb -Idrivers/stm/cec



ifdef CONFIG_CPU_SUBTYPE_STX7111
EXTRA_CFLAGS	+= -DSTx7111
endif

ifdef CONFIG_CPU_SUBTYPE_STX7105
EXTRA_CFLAGS	+= -DSTx7105
endif

