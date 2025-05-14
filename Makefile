# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2025 Intel Corporation
# These contents may have been developed with support from one or more
# Intel-operated generative artificial intelligence solutions.

# Define the kernel source directory
KERNELDIR ?= /lib/modules/$(shell uname -r)/build

# Define the module name
MODULE_NAME := acomp-dpusm

# Define the home path for the DPUSM and ZFS source directories
HOME_PATH ?= /devel/zfs

# Define the include paths for DPUSM
DPUSM_INCLUDE := $(HOME_PATH)/dpusm/include

# Define the include paths for ZFS
ZFS_INCLUDE := $(HOME_PATH)/zfs/include

# Define the source files for the module
SRC_FILES := provider.c compress.c

# Define the object files for the module
OBJ_FILES := $(SRC_FILES:.c=.o)

# Define the location of the module symvers file for DPUSM
KBUILD_EXTRA_SYMBOLS := $(HOME_PATH)/dpusm/Module.symvers

ifneq ($(KERNELRELEASE),)
ccflags-y := -I$(DPUSM_INCLUDE) -I$(ZFS_INCLUDE)
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := $(OBJ_FILES)
else
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) W=1 modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif

.PHONY: clean
