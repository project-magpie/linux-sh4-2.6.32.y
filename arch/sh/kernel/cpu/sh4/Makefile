#
# Makefile for the Linux/SuperH SH-4 backends.
#

obj-y	:= probe.o common.o
common-y	+= entry.o

obj-$(CONFIG_SH_FPU)			+= fpu.o softfloat.o
obj-$(CONFIG_SH_STORE_QUEUES)		+= sq.o

# CPU subtype setup
obj-$(CONFIG_CPU_SUBTYPE_SH7750)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7750R)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7750S)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7091)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751R)	+= setup-sh7750.o
obj-$(CONFIG_CPU_SUBTYPE_SH7760)	+= setup-sh7760.o
obj-$(CONFIG_CPU_SUBTYPE_SH4_202)	+= setup-sh4-202.o
obj-$(CONFIG_CPU_SUBTYPE_ST40STB1)	+= setup-st40ra.o
obj-$(CONFIG_CPU_SUBTYPE_STX5197)	+= setup-stx5197.o
obj-$(CONFIG_CPU_SUBTYPE_STB7100)	+= setup-stb7100.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105)	+= setup-stx7105.o
obj-$(CONFIG_CPU_SUBTYPE_STX7111)	+= setup-stx7111.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= setup-stx7141.o
obj-$(CONFIG_CPU_SUBTYPE_STX7200)	+= setup-stx7200.o

# Primary on-chip clocks (common)
ifndef CONFIG_CPU_SH4A
clock-$(CONFIG_CPU_SH4)			:= clock-sh4.o
clock-$(CONFIG_CPU_SUBTYPE_STX5197)	:= clock-stx5197.o
clock-$(CONFIG_CPU_SUBTYPE_STB7100)	:= clock-stb7100.o
clock-$(CONFIG_CPU_SUBTYPE_STX7105)	:= clock-stx7105.o
clock-$(CONFIG_CPU_SUBTYPE_STX7111)	:= clock-stx7111.o
clock-$(CONFIG_CPU_SUBTYPE_STX7141)	:= clock-stx7141.o
clock-$(CONFIG_CPU_SUBTYPE_STX7200)	:= clock-stx7200.o
endif

# Additional clocks by subtype
clock-$(CONFIG_CPU_SUBTYPE_SH4_202)	+= clock-sh4-202.o

obj-y	+= $(clock-y)

obj-$(CONFIG_STM_CPU_FREQ)		+= cpufreq-stm.o

ifdef CONFIG_SUSPEND
obj-y					+= suspend.o suspend-core.o
obj-$(CONFIG_CPU_SUBTYPE_STX5197)	+= suspend-stx5197.o
obj-$(CONFIG_CPU_SUBTYPE_STB7100)	+= suspend-stb7100.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105)	+= suspend-stx7105.o
obj-$(CONFIG_CPU_SUBTYPE_STX7111)	+= suspend-stx7111.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= suspend-stx7141.o
obj-$(CONFIG_CPU_SUBTYPE_STX7200)	+= suspend-stx7200.o
endif
