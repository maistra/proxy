*******************************************************************************

	    Intel Dynamic Load Balancer 2.x Driver Release 8.0.0

********************************************************************************
Disclaimer: This code is being provided to potential customers of Intel DLB to
enable the use of Intel DLB well ahead of the kernel.org and DPDK.org
upstreaming process. Please be aware that based on the open source community
feedback, the design of the code module can change considerably, including API
interface definitions. If the open source implementation differs from what is
presented in this release, Intel may reserve the right to update the
implementation to align with the open source version at a later time and stop
supporting this early enablement version.

===================
Table of Contents
===================

* Intel Dynamic Load Balancer
* Intel DLB Software Installation Notes
* Kernel and DPDK Versions
* Build and Installation
* Examples / Test Cases
* Alerts & Notices
* Known Issues

=========================================
Intel Dynamic Load Balancer (Intel DLB)
=========================================

Intel DLB 2.x is a PCIe device that provides load-balanced, prioritized
scheduling of events across CPU cores enabling efficient core-to-core
communication. It is an accelerator for the event-driven programming model of
DPDK's Event Device Library. This library is used in packet processing pipelines
for multi-core scalability, dynamic load-balancing, and variety of packet
distribution and synchronization schemes.

=======================================
Intel DLB Software Installation Notes
=======================================

The Intel DLB software comes in the following package:

    dlb_linux_src_release_<rel-id>_<rel-date>.txz

The package contains the Intel DLB kernel driver, DPDK Patch for Intel DLB 2.x
PMD (Poll Mode Driver) with other required changes to standard DPDK, and libdlb
client library for non-dpdk applications. The package can be extracted using
the following command:

    $ tar xfJ dlb_linux_src_release_<rel-id>_<rel-date>.txz

DLB Software feature details can be found in DLB Programmer Guides.

============================
Kernel and DPDK Versions
============================

Linux Kernel Version: Tested with Linux Kernel versions 5.15 and 5.19.

DPDK Base Version: v21.11

===========================
Build and Installation
===========================

Refer to document "DLB_Driver_User_Guide.pdf" for detailed instructions on build
and installation.

==========================
Examples / Test Cases
==========================

This Intel DLB release contains libdlb, the client library for building Intel
DLB applications which do not require complete DPDK framework. It also includes
a set of sample tests to demonstrate various features. Accompanying User Guide
has details on the included samples.

==============================
Changes in release 8.0.0
==============================

- HQM-335: Implement QEMU VFIO framework for VM Live Migration support in
  DLB2.x kernel driver.
- HQM-605: Support for DLB 2.5 QIDX selection WRR scheduler weight programming.
- HQM-681: Enable SIOV support in intel-next 5.19 for GNR Power-on in the new
  MDEV/IOMMUFD framework.
- HQM-703, 704: Fix and add error checkings for port allocation from default
  Class of Service.
- Add clang build support for DLB driver and libdlb.
- Replace dlb2_movdir64 with dlb2_movntdq on unsupported platforms.

=====================
Alerts & Notices
=====================

- The software distributed in this release is not intended for secure use
  and should not be used to transmit or store security sensitive or privacy
  protected data.
- DLB2.x application only supports simultaneous use of either Bi-furcated mode
  devices  (bound to dlb2 driver) or PF PMD mode devices (bound to vfio-pci).
  Application should use all devices of the same type when some devices are
  bound to dlb2 driver and some to vfio-pci module. Different application can
  use different types and run at the same time. To avoid picking devices bound
  to vfio-pci while using devices bound to dlb2 driver, use the --no-pci
  option.
- HQM-417: For older kernels (pre 5.15) with some specific application patterns
  and kernel configuration, DLB device binding to vfio-pci driver results in a
  crash. In such cases, "uio_pci_generic" or "igb_uio" module can be used in
  place of "vfio-pci" (with IOMMU disabled or in passthrough mode) or Bifurcated
  mode can be used by linking device to DLB driver. This issue is not observed
  with kernels 5.15 and later.

=====================
Known Issues
=====================
