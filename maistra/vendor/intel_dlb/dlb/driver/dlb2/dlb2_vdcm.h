/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright(c) 2018-2020 Intel Corporation
 */

#ifndef __DLB2_VDCM_H
#define __DLB2_VDCM_H

#include <linux/version.h>

#if KERNEL_VERSION(5, 16, 0) <= LINUX_VERSION_CODE
#ifndef CONFIG_IOMMUFD
#undef CONFIG_INTEL_DLB2_SIOV
#else
#define DLB2_NEW_MDEV_IOMMUFD
#endif
#endif

#ifdef CONFIG_INTEL_DLB2_SIOV

#include <linux/pci.h>

#define DLB2_SIOV_IMS_WORKAROUND

struct dlb2;
int dlb2_vdcm_init(struct dlb2 *dlb2);
void dlb2_vdcm_exit(struct pci_dev *pdev);
void dlb2_save_cmd_for_migration(struct dlb2 *dlb2, int vdev_id, u8 *data, int data_size);
void dlb2_handle_migration_cmds(struct dlb2 *dlb2, int vdev_id, u8 *data);
#endif /* CONFIG_INTEL_DLB2_SIOV */
#endif /* __DLB2_VDCM_H */
