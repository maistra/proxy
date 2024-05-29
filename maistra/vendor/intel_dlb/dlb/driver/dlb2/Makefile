# SPDX-License-Identifier: GPL-2.0-only
# Copyright(c) 2016-2020 Intel Corporation

ifneq ($(KERNELRELEASE),)

# building with Kbuild
ccflags-y := -DCONFIG_AS_SSE2 -I$(PWD)/hqmv2/linux -iquote $(PWD) -DCONFIG_INTEL_DLB2_DATAPATH

# build for the Frio environment (sets the device frequency macro to 2 MHz)
#ccflags-y += -DFPGA

RHEL_RELEASE := $(shell cat /etc/os-release |grep PRETTY_NAME|grep "Red Hat")

# Scalable IOV depends on MDEV and IMS support. No SIOV on RHEL.
ifneq (,$(filter $(CONFIG_VFIO_MDEV),y m))
 ifeq ($(RHEL_RELEASE),)
  # SIOV is not supported in kernel 5.16 and 5.17
  ccflags-y += -DCONFIG_INTEL_DLB2_SIOV
 endif
endif


obj-m := dlb2.o
dlb2-y := \
  dlb2_main.o          \
  dlb2_file.o          \
  dlb2_ioctl.o         \
  base/dlb2_resource.o \
  dlb2_pf_ops.o        \
  dlb2_intr.o          \
  dlb2_dp.o            \
  dlb2_sriov.o         \
  dlb2_vf_ops.o        \
  dlb2_vdcm.o          \
  dlb2_perf.o	       \

else

# building normally
KSRC ?= /lib/modules/`uname -r`/build
default: gen_base_files
	$(MAKE) -C $(KSRC) M=$(CURDIR) modules

modules_install:
	$(MAKE) -C $(KSRC) M=$(CURDIR) modules_install

# Driver packages will contain a pre-built register header file and no
# Makefile in the base/ directory.
gen_base_files gen_reg_header:
ifneq (,$(wildcard base/Makefile))
	$(MAKE) -C base $@
endif

clean:
	$(MAKE) -C $(KSRC) M=$(CURDIR) clean
ifneq (,$(wildcard base/Makefile))
	$(MAKE) -C base $@
endif

.PHONY: clean default modules_install gen_base_files gen_reg_header

endif
