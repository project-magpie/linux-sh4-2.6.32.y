/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: David Mckay <david.mckay@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Arch specific glue to join up the main stm pci driver in drivers/stm/
 * to the specific PCI arch code.
 *
 */

#ifndef __LINUX_STM_PCI_GLUE_H
#define __LINUX_STM_PCI_GLUE_H

/*
 * This function takes the information filled in by the arch independant code
 * and hooks it up to the arch specific stuff. The arch independant code lives
 * in drivers/stm/pcie.c or drivers/stm/pci.c
 *
 * If this function needs to allocate memory it should use devm_kzalloc()
 */

enum stm_pci_type {STM_PCI_EMISS};

int __devinit stm_pci_register_controller(struct platform_device *pdev,
					  struct pci_ops *config_ops,
					  enum stm_pci_type type);

/*
 * Given a pci bus, return the corresponding platform device that created it
 * in the first place. Must be called with a bus that has as it's root
 * controller an interface registered via above mechanism
 */
struct platform_device *stm_pci_bus_to_platform(struct pci_bus *bus);

#endif
