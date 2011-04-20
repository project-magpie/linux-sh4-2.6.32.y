
#
# (C) COPYRIGHT 2010 STMicroelectronics R&D Ltd
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file COPYING in the main directory of this archive for
# more details.
#
MALI_DIR="drivers/stm/mali"
# Get ARM relase version string
SVN_REV:=$(shell [ -e $(MALI_DIR)/version ] && cat $(MALI_DIR)/version)

# Get API version of user-kernel interface by extracting it from the mali_kernel_ioctl.h file
API_VERSION:=$(shell grep "\#define _MALI_API_VERSION" $(MALI_DIR)/common/mali_uk_types.h | cut -d' ' -f 3 )

# Source files which always are included in a build
OSK_OBJS=$(addprefix linux/, \
        mali_osk_atomics.o \
        mali_osk_irq.o \
        mali_osk_locks.o \
        mali_osk_low_level_mem.o \
        mali_osk_math.o \
        mali_osk_memory.o \
        mali_osk_misc.o \
        mali_osk_mali.o \
        mali_osk_notification.o \
        mali_osk_time.o \
        mali_osk_timers.o)

UKK_OBJS=$(addprefix linux/,\
        mali_ukk_mem.o \
        mali_ukk_gp.o \
        mali_ukk_pp.o \
        mali_ukk_core.o)

LINUX_OBJS = $(addprefix linux/, \
	mali_kernel_linux.o \
	mali_osk_indir_mmap.o)

COMMON_OBJS = $(addprefix common/, \
	mali_kernel_core.o \
	mali_kernel_rendercore.o \
	mali_kernel_descriptor_mapping.o)

PLATFORM_OBJS = $(addprefix platform/default/, \
	mali_platform.o )

ifeq ($(CONFIG_CPU_SUBTYPE_STX7108)$(CONFIG_CPU_SUBTYPE_FLI7510),y)
SOC=stx7108
USING_MMU = 1
USING_UMP = 0
USING_MALI200 = 0
USING_MALI400 = 1
USING_MALI400_L2_CACHE = 1
USING_GP2 = 1

MALI_OBJS = $(OSK_OBJS) $(UKK_OBJS) $(COMMON_OBJS) $(LINUX_OBJS) $(PLATFORM_OBJS)
MALI_OBJS += $(addprefix common/, \
        mali_kernel_MALI200.o \
	mali_kernel_GP2.o \
	mali_kernel_mem_mmu.o \
	mali_kernel_memory_engine.o \
	mali_block_allocator.o \
	mali_kernel_mem_os.o \
	mali_kernel_l2_cache.o)

endif

MALI_OBJS += malidrv_build_info.o


# Driver include paths
EXTRA_CFLAGS += -I $(MALI_DIR) -I $(MALI_DIR)/common
EXTRA_CFLAGS += -I $(MALI_DIR)/linux -I $(MALI_DIR)/platform
EXTRA_CFLAGS += -I $(MALI_DIR)/linux/license/gpl

# Mali driver defines
EXTRA_CFLAGS += -DUSING_MMU=$(USING_MMU) \
		-DMALI_USE_UNIFIED_MEMORY_PROVIDER=0 \
		-D_MALI_OSK_SPECIFIC_INDIRECT_MMAP \
		-DMALI_UKK_HAS_IMPLICIT_MMAP_CLEANUP \
		-DUSING_MALI400=$(USING_MALI400) \
		-DUSING_MALI400_L2_CACHE=$(USING_MALI400_L2_CACHE) \
		-DUSING_MALI_PMM=0 \
		-DSVN_REV=$(SVN_REV) \
		-DSVN_REV_STRING=\"$(SVN_REV)\"

ifeq ($(CONFIG_DEBUG_MALI),n)
EXTRA_CFLAGS += -DNDEBUG
endif

BUILD_DATE = $(shell /bin/date)

CFLAGS_malidrv_build_info.o := -DMALIDRV_SOC="KBUILD_STR($(SOC))"     \
		-DMALIDRV_KERNELDIR="KBUILD_STR($(KERNELDIR))" \
		-DMALIDRV_USING_MMU="KBUILD_STR($(USING_MMU))" \
		-DMALIDRV_USING_UMP="KBUILD_STR($(USING_UMP))" \
		-DMALIDRV_MMAP_CACHED_PAGES="KBUILD_STR($(MMAP_CACHED_PAGES))" \
		-DMALIDRV_USING_MALI200="KBUILD_STR($(USING_MALI200))" \
		-DMALIDRV_USING_MALI400="KBUILD_STR($(USING_MALI400))" \
		-DMALIDRV_USING_MALI400L2="KBUILD_STR($(USING_MALI400L2))" \
		-DMALIDRV_USING_MALIGP2="KBUILD_STR($(USING_MALIGP2))" \
		-DMALIDRV_API_VERSION="KBUILD_STR($(API_VERSION))" \
		-DMALIDRV_SVN_REV="KBUILD_STR($(SVN_REV))" \
		-DMALIDRV_BUILD_DATE="KBUILD_STR($(BUILD_DATE))"

obj-$(CONFIG_STM_MALI) += mali.o
mali-objs := $(MALI_OBJS)

