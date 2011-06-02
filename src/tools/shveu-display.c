/*
 * Tool to demonstrate VEU hardware acceleration of raw image scaling.
 *
 * The RGB/YCbCr source image is read from a file, scaled and displayed on the
 * framebuffer. It uses an ncurses interface to allow the user to zoom in/out
 * and pan the output image.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

//#define BUNDLE_MODE
//#undef HAVE_NCURSES
#ifdef HAVE_NCURSES
#ifdef HAVE_NCURSES_SUBDIR
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif
#endif
#include <uiomux/uiomux.h>
#include "shveu/shveu.h"
#include "display.h"

/* RGB565 colors */
#define BLACK 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F

static void
usage (const char * progname)
{
	printf ("Usage: %s [options] [input-filename]\n", progname);
	printf ("Processes raw image data using the SH-Mobile VEU and displays on screen.\n");
	printf ("\n");
	printf ("If no input filename is specified, a simple image will be created.\n");
	printf ("\nInput options\n");
	printf ("  -c, --input-colorspace (RGB565, RGB888, BGR888, RGBx888, NV12, YCbCr420, NV16, YCbCr422)\n");
	printf ("                         Specify input colorspace\n");
	printf ("  -s, --input-size       Set the input image size (qcif, cif, qvga, vga, d1, 720p)\n");
	printf ("  -W, --width            Set the input image width\n");
	printf ("  -H, --height           Set the input image height\n");
	printf ("\nControl keys\n");
	printf ("  +/-                    Zoom in/out\n");
	printf ("  Cursor keys            Pan\n");
	printf ("  =                      Reset zoom and panning\n");
	printf ("  q                      Quit\n");
	printf ("\nMiscellaneous options\n");
	printf ("  -h, --help             Display this help and exit\n");
	printf ("  -v, --version          Output version information and exit\n");
	printf ("\nFile extensions are interpreted as follows unless otherwise specified:\n");
	printf ("  .yuv    YCbCr420\n");
	printf ("  .rgb    RGB565\n");
	printf ("  .888    RGB888\n");
	printf ("\n");
	printf ("Please report bugs to <linux-sh@vger.kernel.org>\n");
}

struct sizes_t {
	const char *name;
	int w;
	int h;
};

static const struct sizes_t sizes[] = {
	{ "QCIF", 176,  144 },
	{ "CIF",  352,  288 },
	{ "QVGA", 320,  240 },
	{ "VGA",  640,  480 },
	{ "D1",   720,  480 },
	{ "720p", 1280, 720 },
};

static int set_size (char * arg, int * w, int * h)
{
	int nr_sizes = sizeof(sizes) / sizeof(sizes[0]);
	int i;

	if (!arg)
		return -1;

	for (i=0; i<nr_sizes; i++) {
		if (!strcasecmp (arg, sizes[i].name)) {
			*w = sizes[i].w;
			*h = sizes[i].h;
			return 0;
		}
	}

	return -1;
}

static const char * show_size (int w, int h)
{
	int nr_sizes = sizeof(sizes) / sizeof(sizes[0]);
	int i;

	for (i=0; i<nr_sizes; i++) {
		if (w == sizes[i].w && h == sizes[i].h)
			return sizes[i].name;
	}

	return "";
}

struct extensions_t {
	const char *ext;
	int fmt;
};

static const struct extensions_t exts[] = {
	{ "RGB565",   REN_RGB565 },
	{ "rgb",      REN_RGB565 },
	{ "RGB888",   REN_RGB24 },
	{ "888",      REN_RGB24 },
	{ "BGR888",   REN_BGR24 },
	{ "RGBx888",  REN_RGB32 },
	{ "x888",     REN_RGB32 },
	{ "YCbCr420", REN_NV12 },
	{ "420",      REN_NV12 },
	{ "yuv",      REN_NV12 },
	{ "NV12",     REN_NV12 },
	{ "YCbCr422", REN_NV16 },
	{ "422",      REN_NV16 },
	{ "NV16",     REN_NV16 },
};

static int set_colorspace (char * arg, int * c)
{
	int nr_exts = sizeof(exts) / sizeof(exts[0]);
	int i;

	if (!arg)
		return -1;

	for (i=0; i<nr_exts; i++) {
		if (!strcasecmp (arg, exts[i].ext)) {
			*c = exts[i].fmt;
			return 0;
		}
	}

	return -1;
}

static const char * show_colorspace (int c)
{
	int nr_exts = sizeof(exts) / sizeof(exts[0]);
	int i;

	for (i=0; i<nr_exts; i++) {
		if (c == exts[i].fmt)
			return exts[i].ext;
	}

	return "<Unknown colorspace>";
}

static off_t filesize (char * filename)
{
	struct stat statbuf;

	if (filename == NULL || !strcmp (filename, "-"))
		return -1;

	if (stat (filename, &statbuf) == -1) {
		perror (filename);
		return -1;
	}

	return statbuf.st_size;
}

static off_t imgsize (int colorspace, int w, int h)
{
	return (off_t)(size_y(colorspace, w*h) + size_c(colorspace, w*h));
}

static int guess_colorspace (char * filename, int * c)
{
	char * ext;

	if (filename == NULL || !strcmp (filename, "-"))
		return -1;

	/* If the colorspace is already set (eg. explicitly by user args)
	 * then don't try to guess */
	if (*c != -1) return -1;

	ext = strrchr (filename, '.');
	if (ext == NULL) return -1;

	return set_colorspace(ext+1, c);
}

static int guess_size (char * filename, int colorspace, int * w, int * h)
{
	off_t size;

	if ((size = filesize (filename)) == -1) {
		return -1;
	}

	if (*w==-1 && *h==-1) {
		/* Image size unspecified */
		int nr_sizes = sizeof(sizes) / sizeof(sizes[0]);
		int i;

		for (i=0; i<nr_sizes; i++) {

			if (size == imgsize(colorspace, sizes[i].w, sizes[i].h)) {
				*w = sizes[i].w;
				*h = sizes[i].h;
				return 0;
			}
		}
	}

	return -1;
}


static void draw_rect_rgb565(void *surface, uint16_t color, int x, int y, int w, int h, int span)
{
	uint16_t *pix = (uint16_t *)surface + y*span + x;
	int xi, yi;

	for (yi=0; yi<h; yi++) {
		for (xi=0; xi<w; xi++) {
			*pix++ = color;
		}
		pix += (span-w);
	}
}

#define U_SEC_PER_SEC 1000000
#define N_SEC_PER_SEC 1000000000

/* Total microseconds elapsed */
static long
elapsed_us (struct timespec * start)
{
	struct timespec curr;
	long secs, nsecs;
	int ret;

	ret = clock_gettime(CLOCK_MONOTONIC, &curr);
	if (ret == -1) return ret;

	secs = curr.tv_sec - start->tv_sec;
	nsecs = curr.tv_nsec - start->tv_nsec;
	if (nsecs < 0) {
		secs--;
		nsecs += N_SEC_PER_SEC;
	}

	return (secs*U_SEC_PER_SEC) + nsecs/1000;
}

static int nr_scales = 0;
static long time_total_us = 0;

static void scale(
	SHVEU *veu,
	DISPLAY *display,
	float scale,
	void *py,
	void *pc,
	unsigned long w,
	unsigned long h,
	int x,	/* Centre co-ordinates */
	int y,
	int src_fmt)
{
	unsigned char *lcd_buf = display_get_back_buff_virt(display);
	int lcd_w = display_get_width(display);
	int lcd_h = display_get_height(display);
	struct ren_vid_surface src_surface;
	struct ren_vid_surface src_surface2;
	struct ren_vid_surface dst_surface;
	struct ren_vid_surface dst_surface2;
	struct ren_vid_rect src_sel;
	struct ren_vid_rect dst_sel;
	struct timespec start;

	/* Clear the back buffer */
	draw_rect_rgb565(lcd_buf, BLACK, 0, 0, lcd_w, lcd_h, lcd_w);

	src_surface.format = src_fmt;
	src_surface.py = py;
	src_surface.pc = pc;
	src_surface.pa = 0;
	src_surface.w = w;
	src_surface.h = h;
	src_surface.pitch = w;

	dst_surface.format = REN_RGB565;
	dst_surface.py = lcd_buf;
	dst_surface.pc = 0;
	dst_surface.pa = 0;
	dst_surface.w = lcd_w;
	dst_surface.h = lcd_h;
	dst_surface.pitch = lcd_w;

	src_surface2 = src_surface;
	dst_surface2 = dst_surface;

	clock_gettime(CLOCK_MONOTONIC, &start);

#ifndef BUNDLE_MODE
	src_sel.w = w;
	src_sel.h = h;
	src_sel.x = 0;
	src_sel.y = 0;

	dst_sel.x = x;
	dst_sel.y = y;

	/* Handle output off-surface to the left or above by using part of the input */
	if (x < 0) {
		src_sel.w = w + (int)(x/scale);
		src_sel.x = w - src_sel.w;
		dst_sel.x = 0;
	}
	if (y < 0) {
		src_sel.h = h + (int)(y/scale);
		src_sel.y = h - src_sel.h;
		dst_sel.y = 0;
	}
	dst_sel.w = (int) (src_sel.w * scale) & ~1;
	dst_sel.h = (int) (src_sel.h * scale) & ~1;


	/* Handle output off-surface to the right or below by cropping the input & output */
	if ((dst_sel.x + dst_sel.w) > lcd_w) {
		src_sel.w = (int)((lcd_w - dst_sel.x) / scale);
		dst_sel.w = lcd_w - dst_sel.x;
	}
	if ((dst_sel.y + dst_sel.h) > lcd_h) {
		src_sel.h = (int)((lcd_h - dst_sel.y) / scale);
		dst_sel.h = lcd_h - dst_sel.y;
	}

	if (src_sel.w <= 0 || src_sel.h <= 0)
		return;
	if (dst_sel.w <= 0 || dst_sel.h <= 0)
		return;

	get_sel_surface(&src_surface2, &src_surface, &src_sel);
	get_sel_surface(&dst_surface2, &dst_surface, &dst_sel);
#endif	/* !BUNDLE_MODE */

	shveu_setup(
		veu,
		&src_surface2,
		&dst_surface2,
		SHVEU_NO_ROT);

#ifdef BUNDLE_MODE
	{
		int end = 0;
		int nr_lines = 16;

		while (end == 0) {
			shveu_set_src(veu, py, pc);
			shveu_set_dst(veu, lcd_buf, 0);
			py += offset_y(src_surface.format, 0, nr_lines, src_surface.pitch);
			pc += offset_c(src_surface.format, 0, nr_lines, src_surface.pitch);
			lcd_buf += offset_y(dst_surface.format, 0, nr_lines, dst_surface.pitch);
			shveu_start_bundle(veu, nr_lines);
			end = shveu_wait(veu);
		}
	}
#else
	shveu_start(veu);
	shveu_wait(veu);
#endif	/* BUNDLE_MODE */

	time_total_us += elapsed_us(&start);
	nr_scales++;

	display_flip(display);
}


int main (int argc, char * argv[])
{
	UIOMux *uiomux = NULL;
	SHVEU *veu = NULL;
	DISPLAY *display = NULL;
	char * infilename = NULL;
	FILE * infile = NULL;
	size_t nread;
	size_t input_size;
	int input_w = -1;
	int input_h = -1;
	int input_colorspace = -1;
	unsigned char *src_virt;
	void *src_py, *src_pc;
	int ret;
	float scale_factor=1.0;
	int read_image = 1;
	int x=0, y=0;
	int key;
	int run = 1;

	int show_version = 0;
	int show_help = 0;
	char * progname;
	int error = 0;

	int c;
	char * optstring = "hvc:s:W:H:";

#ifdef HAVE_GETOPT_LONG
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"input-colorspace", required_argument, 0, 'c'},
		{"input-size", required_argument, 0, 's'},
		{"width", required_argument, 0, 'W'},
		{"height", required_argument, 0, 'H'},
		{NULL,0,0,0}
	};
#endif

	progname = argv[0];

	while (1) {
#ifdef HAVE_GETOPT_LONG
		c = getopt_long (argc, argv, optstring, long_options, NULL);
#else
		c = getopt (argc, argv, optstring);
#endif
		if (c == -1) break;
		if (c == ':') {
			usage (progname);
			goto exit_err;
		}

		switch (c) {
		case 'h': /* help */
			show_help = 1;
			break;
		case 'v': /* version */
			show_version = 1;
			break;
		case 'c': /* input colorspace */
			set_colorspace (optarg, &input_colorspace);
			break;
		case 's': /* input size */
			set_size (optarg, &input_w, &input_h);
			break;
		case 'W': /* input size */
			input_w = strtoul(optarg, NULL, 10);
			break;
		case 'H': /* input size */
			input_h = strtoul(optarg, NULL, 10);
			break;
		default:
			break;
		}
	}

	if (show_version) {
		printf ("%s version " VERSION "\n", progname);
	}

	if (show_help) {
		usage (progname);
	}

	if (show_version || show_help) {
		goto exit_ok;
	}

	if (optind > argc) {
		usage (progname);
		goto exit_err;
	}

	if (optind < argc) {
		infilename = argv[optind++];
		printf ("Input file: %s\n", infilename);

		guess_colorspace (infilename, &input_colorspace);
		guess_size (infilename, input_colorspace, &input_w, &input_h);
	} else {
		printf ("No input file specified, drawing simple image\n");
		if (input_w == -1) {
			input_w = 320;
			input_h = 240;
		}
		input_colorspace = REN_RGB565;
	}

	/* Check that all parameters are set */
	if (input_colorspace == -1) {
		fprintf (stderr, "ERROR: Input colorspace unspecified\n");
		error = 1;
	}
	if (input_w == -1) {
		fprintf (stderr, "ERROR: Input width unspecified\n");
		error = 1;
	}
	if (input_h == -1) {
		fprintf (stderr, "ERROR: Input height unspecified\n");
		error = 1;
	}

	if (error) goto exit_err;

	printf ("Input colorspace:\t%s\n", show_colorspace (input_colorspace));
	printf ("Input size:\t\t%dx%d %s\n", input_w, input_h, show_size (input_w, input_h));

	input_size = imgsize (input_colorspace, input_w, input_h);

	if (infilename != NULL) {
		infile = fopen (infilename, "rb");
		if (infile == NULL) {
			fprintf (stderr, "%s: unable to open input file %s\n",
				 progname, infilename);
			goto exit_err;
		}
	}

	if ((uiomux = uiomux_open()) == 0) {
		fprintf (stderr, "Error opening UIOmux\n");
		goto exit_err;
	}

	if ((veu = shveu_open()) == 0) {
		fprintf (stderr, "Error opening VEU\n");
		goto exit_err;
	}

	if ((display = display_open()) == 0) {
		fprintf (stderr, "Error opening display\n");
		goto exit_err;
	}


	/* Set up memory buffers */
	src_py = uiomux_malloc (uiomux, UIOMUX_SH_VEU, input_size, 32);
	if (!src_py) {
		perror("uiomux_malloc");
		goto exit_err;
	}
	src_pc = src_py + (input_w * input_h);

	if (!infilename) {
		/* Draw a simple picture */
		draw_rect_rgb565(src_py, BLACK, 0,         0,         input_w,   input_h,   input_w);
		draw_rect_rgb565(src_py, BLUE,  input_w/4, input_h/4, input_w/4, input_h/2, input_w);
		draw_rect_rgb565(src_py, RED,   input_w/2, input_h/4, input_w/4, input_h/2, input_w);
	}


#ifdef HAVE_NCURSES
	/* ncurses init */
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
#endif

	while (run) {

		if (infilename && read_image) {
			read_image = 0;
			/* Read input */
			if ((nread = fread (src_py, 1, input_size, infile)) != input_size) {
				if (nread == 0 && feof (infile)) {
					break;
				} else {
					fprintf (stderr, "%s: error reading input file %s\n",
						 progname, infilename);
				}
			}
		}

		scale (veu, display, scale_factor, src_py, src_pc, input_w, input_h, x, y, input_colorspace);

#ifdef HAVE_NCURSES
		key = getch();
		switch (key)
		{
		case '+':
			scale_factor += 0.01;
			break;
		case '-':
			scale_factor -= 0.01;
			break;
		case '=':
			scale_factor = 1.0;
			x = 0;
			y = 0;
			break;
		case KEY_UP:
			y -= 1;
			break;
		case KEY_DOWN:
			y += 1;
			break;
		case KEY_LEFT:
			x -= 1;
			break;
		case KEY_RIGHT:
			x += 1;
			break;
		case ' ':
			read_image = 1;
			break;
		case 'q':
			run = 0;
			break;
		}
#else
		read_image = 1;
#endif
	}

#ifdef HAVE_NCURSES
	/* ncurses close */
	clrtoeol();
	refresh();
	endwin();
#endif

	display_close(display);
	shveu_close(veu);
	uiomux_free (uiomux, UIOMUX_SH_VEU, src_py, input_size);
	uiomux_close (uiomux);

	printf("Average time for scale is %luus\n", time_total_us/nr_scales);

exit_ok:
	exit (0);

exit_err:
	if (display) display_close(display);
	if (veu)     shveu_close(veu);
	if (uiomux)  uiomux_close (uiomux);
	exit (1);
}
