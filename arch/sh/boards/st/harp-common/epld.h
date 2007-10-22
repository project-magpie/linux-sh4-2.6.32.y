void epld_write(unsigned long value, unsigned long offset);
unsigned long epld_read(unsigned long offset);
int harp_configure_epld(struct platform_device *device);

struct plat_epld_data {
	int opsize;
};
