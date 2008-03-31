void epld_write(unsigned long value, unsigned long offset);
unsigned long epld_read(unsigned long offset);
void epld_early_init(struct platform_device *device);

void harp_init_irq(void);

struct plat_epld_data {
	int opsize;
};
