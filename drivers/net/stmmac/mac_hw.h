#ifdef CONFIG_STMMAC_ETH
#include "mac100.h"
#elif CONFIG_STMGMAC_ETH
#include "gmac.h"
#endif

/* MAC HW structure */
struct stmmmac_driver {
	char *name;
	/* It has been added to cover some issues about the HW setup
	 * especially in the stb7109 Ethernet. */
	int have_hw_fix;
	/* Initial setup of the MAC controller */
	void (*core_init) (struct net_device * dev);
	/* Dump MAC CORE registers */
	void (*mac_registers) (unsigned long ioaddr);
	/* Dump DMA registers */
	void (*dma_registers) (unsigned long ioaddr);
	/* Return zero if no error is happened during the transmission */
	int (*check_tx_summary) (void *p, unsigned int status);
	/* Check if the frame was not successfully received */
	int (*check_rx_summary) (void *p, unsigned int status);
	/* Verify the TX checksum */
	void (*tx_checksum) (struct sk_buff * skb);
	/* Verifies the RX checksum */
	void (*rx_checksum) (struct sk_buff * skb, int status);
	/* Enable/Disable Multicast filtering */
	void (*set_filter) (struct net_device * dev);
};
