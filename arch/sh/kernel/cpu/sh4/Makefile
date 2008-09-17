#
# Makefile for the Linux/SuperH SH-4 backends.
#

obj-y	:= probe.o common.o
common-y	+= $(addprefix ../sh3/, entry.o ex.o)

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
obj-$(CONFIG_CPU_SUBTYPE_STB7100)	+= setup-stb7100.o
obj-$(CONFIG_CPU_SUBTYPE_STX7105)	+= setup-stx7105.o
obj-$(CONFIG_CPU_SUBTYPE_STX7111)	+= setup-stx7111.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= setup-stx7141.o
obj-$(CONFIG_CPU_SUBTYPE_STX7200)	+= setup-stx7200.o

# Primary on-chip clocks (common)
ifndef CONFIG_CPU_SH4A
clock-$(CONFIG_CPU_SH4)			:= clock-sh4.o
clock-$(CONFIG_CPU_SUBTYPE_STB7100)	:= clock-stb7100.o
clock-$(CONFIG_CPU_SUBTYPE_STX7105)	:= clock-stx7105.o
clock-$(CONFIG_CPU_SUBTYPE_STX7111)	:= clock-stx7111.o
clock-$(CONFIG_CPU_SUBTYPE_STX7141)	:= clock-stx7141.o
clock-$(CONFIG_CPU_SUBTYPE_STX7200)	:= clock-stx7200.o
endif

# Additional clocks by subtype
clock-$(CONFIG_CPU_SUBTYPE_SH4_202)	+= clock-sh4-202.o

obj-y	+= $(clock-y)

ifdef CONFIG_STM_CPU_FREQ
obj-y					+= cpufreq-stm.o
obj-$(CONFIG_CPU_SUBTYPE_STB7100)	+= cpufreq-stb7100.o
obj-$(CONFIG_CPU_SUBTYPE_STX7111)	+= cpufreq-stx7111.o
obj-$(CONFIG_CPU_SUBTYPE_STX7141)	+= cpufreq-stx7141.o
obj-$(CONFIG_CPU_SUBTYPE_STX7200)	+= cpufreq-stx7200.o
endif

