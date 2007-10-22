#ifndef _LINUX_CONSOLE_SPLASH_H_
#define _LINUX_CONSOLE_SPLASH_H_ 1

/* A structure used by the framebuffer splash code (drivers/video/fbsplash.c) */
struct vc_splash {
	__u8 bg_color;				/* The color that is to be treated as transparent */
	__u8 state;				/* Current splash state: 0 = off, 1 = on */
	__u16 tx, ty;				/* Top left corner coordinates of the text field */
	__u16 twidth, theight;			/* Width and height of the text field */
	char* theme;
};

#endif
