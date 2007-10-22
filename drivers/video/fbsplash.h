/* 
 *  linux/drivers/video/fbsplash.h -- Framebuffer splash headers
 *
 *  Copyright (C) 2004 Michal Januszewski <spock@gentoo.org>
 *
 */

#ifndef __FB_SPLASH_H
#define __FB_SPLASH_H

#ifndef _LINUX_FB_H
#include <linux/fb.h>
#endif

/* This is needed for vc_cons in fbcmap.c */
#include <linux/vt_kern.h>

struct fb_cursor;
struct fb_info;
struct vc_data;

#ifdef CONFIG_FB_SPLASH
/* fbsplash.c */
int fbsplash_init(void);
int fbsplash_exit(void);
int fbsplash_call_helper(char* cmd, unsigned short cons);
int fbsplash_disable(struct vc_data *vc, unsigned char redraw);

/* cfbsplash.c */
void fbsplash_putcs(struct vc_data *vc, struct fb_info *info, const unsigned short *s, int count, int yy, int xx);
void fbsplash_cursor(struct fb_info *info, struct fb_cursor *cursor);
void fbsplash_clear(struct vc_data *vc, struct fb_info *info, int sy, int sx, int height, int width);
void fbsplash_clear_margins(struct vc_data *vc, struct fb_info *info, int bottom_only);
void fbsplash_blank(struct vc_data *vc, struct fb_info *info, int blank);
void fbsplash_bmove_redraw(struct vc_data *vc, struct fb_info *info, int y, int sx, int dx, int width);
void fbsplash_copy(u8 *dst, u8 *src, int height, int width, int linebytes, int srclinesbytes, int bpp);
void fbsplash_fix_pseudo_pal(struct fb_info *info, struct vc_data *vc);

/* vt.c */
void acquire_console_sem(void);
void release_console_sem(void);
void do_unblank_screen(int entering_gfx);

/* struct vc_data *y */
#define fbsplash_active_vc(y) (y->vc_splash.state && y->vc_splash.theme) 

/* struct fb_info *x, struct vc_data *y */
#define fbsplash_active_nores(x,y) (x->splash.data && fbsplash_active_vc(y))

/* struct fb_info *x, struct vc_data *y */
#define fbsplash_active(x,y) (fbsplash_active_nores(x,y) &&		\
			      x->splash.width == x->var.xres && 	\
			      x->splash.height == x->var.yres &&	\
			      x->splash.depth == x->var.bits_per_pixel)


#else /* CONFIG_FB_SPLASH */

static inline void fbsplash_putcs(struct vc_data *vc, struct fb_info *info, const unsigned short *s, int count, int yy, int xx) {}
static inline void fbsplash_putc(struct vc_data *vc, struct fb_info *info, int c, int ypos, int xpos) {}
static inline void fbsplash_cursor(struct fb_info *info, struct fb_cursor *cursor) {}
static inline void fbsplash_clear(struct vc_data *vc, struct fb_info *info, int sy, int sx, int height, int width) {}
static inline void fbsplash_clear_margins(struct vc_data *vc, struct fb_info *info, int bottom_only) {}
static inline void fbsplash_blank(struct vc_data *vc, struct fb_info *info, int blank) {}
static inline void fbsplash_bmove_redraw(struct vc_data *vc, struct fb_info *info, int y, int sx, int dx, int width) {}
static inline void fbsplash_fix_pseudo_pal(struct fb_info *info, struct vc_data *vc) {}
static inline int fbsplash_call_helper(char* cmd, unsigned short cons) { return 0; }
static inline int fbsplash_init(void) { return 0; }
static inline int fbsplash_exit(void) { return 0; }
static inline int fbsplash_disable(struct vc_data *vc, unsigned char redraw) { return 0; }

#define fbsplash_active_vc(y) (0)
#define fbsplash_active_nores(x,y) (0)
#define fbsplash_active(x,y) (0)

#endif /* CONFIG_FB_SPLASH */

#endif /* __FB_SPLASH_H */
