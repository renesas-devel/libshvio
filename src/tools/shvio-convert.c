/*
 * Tool to demonstrate VIO/VEU hardware acceleration of raw image scaling.
 *
 * The RGB/YCbCr source image is read from a file, scaled/rotated and then
 * output to another file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <uiomux/uiomux.h>
#if defined(USE_MERAM_RA) || defined(USE_MERAM_WB)
#include <meram/meram.h>
#endif

#include "shvio/shvio.h"

/* Rotation: default none */
static int rotation = SHVIO_NO_ROT;

static void
usage (const char * progname)
{
	printf ("Usage: %s [options] [input-filename [output-filename]]\n", progname);
	printf ("Convert raw image data using the SH-Mobile VIO/VEU.\n");
	printf ("\n");
	printf ("If no output filename is specified, data is output to stdout.\n");
	printf ("Specify '-' to force output to be written to stdout.\n");
	printf ("\n");
	printf ("If no input filename is specified, data is read from stdin.\n");
	printf ("Specify '-' to force input to be read from stdin.\n");
	printf ("\nInput options\n");
	printf ("  -c, --input-colorspace (RGB565, RGB888, BGR888, RGBx888, NV12, YV12, NV16, YV16, UYVY)\n");
	printf ("                         Specify input colorspace\n");
	printf ("  -s, --input-size       Set the input image size (qcif, cif, qvga, vga, d1, 720p)\n");
	printf ("\nOutput options\n");
	printf ("  -o filename, --output filename\n");
	printf ("                         Specify output filename (default: stdout)\n");
	printf ("  -C, --output-colorspace (RGB565, RGB888, BGR888, RGBx888, NV12, YV12, NV16, YV16, UYVY)\n");
	printf ("                         Specify output colorspace\n");
	printf ("  -O filename, --overlay filename\n");
	printf ("                         Specify overlayed filename (default: none)\n");
	printf ("\nTransform options\n");
	printf ("  Note that the VIO does not support combined rotation and scaling.\n");
	printf ("  -S, --output-size      Set the output image size (qcif, cif, qvga, vga, d1, 720p)\n");
	printf ("                         [default is same as input size, ie. no rescaling]\n");
	printf ("  -f, --filter	          Set the Filter Mode control register (see HW manual)\n");
	printf ("\nMiscellaneous options\n");
	printf ("  -l, --list             List VIO/VEU available and exit\n");
	printf ("  -u, --vio vio          Specify the name of VIO/VEU to use (default: any VEU)\n");
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
	{ "WVGA", 800, 450 },
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
	ren_vid_format_t fmt;
};

static const struct extensions_t exts[] = {
	{ "RGB565",   REN_RGB565 },
	{ "rgb",      REN_RGB565 },
	{ "RGB888",   REN_RGB24 },
	{ "888",      REN_RGB24 },
	{ "BGR888",   REN_BGR24 },
	{ "RGBx888",  REN_RGB32 },
	{ "x888",     REN_RGB32 },
	{ "YV12",     REN_YV12 },
	{ "NV12",     REN_NV12 },
	{ "420",      REN_NV12 },
	{ "yuv",      REN_NV12 },
	{ "YV16",     REN_YV16 },
	{ "NV16",     REN_NV16 },
	{ "UYVY",     REN_UYVY },
};

static int set_colorspace (char * arg, ren_vid_format_t * c)
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

static const char * show_colorspace (ren_vid_format_t c)
{
	int nr_exts = sizeof(exts) / sizeof(exts[0]);
	int i;

	for (i=0; i<nr_exts; i++) {
		if (c == exts[i].fmt)
			return exts[i].ext;
	}

	return "<Unknown colorspace>";
}

static char * show_rotation (int r)
{
	switch (r) {
	case SHVIO_NO_ROT:
		return "None";
	case SHVIO_ROT_90:
		return "90 degrees clockwise";
	}

	return "<Unknown rotation>";
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

static off_t imgsize (ren_vid_format_t colorspace, int w, int h)
{
	return (off_t)(size_y(colorspace, w*h, 0) + size_c(colorspace, w*h, 0));
}

static int guess_colorspace (char * filename, ren_vid_format_t * c)
{
	char * ext;

	if (filename == NULL || !strcmp (filename, "-"))
		return -1;

	/* If the colorspace is already set (eg. explicitly by user args)
	 * then don't try to guess */
	if (*c != REN_UNKNOWN) return -1;

	ext = strrchr (filename, '.');
	if (ext == NULL) return -1;

	return set_colorspace(ext+1, c);
}

static int guess_size (char * filename, ren_vid_format_t colorspace, int * w, int * h)
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

int main (int argc, char * argv[])
{
	UIOMux * uiomux;
	uiomux_resource_t uiores;

	char * infilename[2] = {NULL, NULL}, * outfilename = NULL;
	FILE * infile[2], * outfile = NULL;
	size_t nread;
	size_t input_size[2], output_size;
	SHVIO *vio;
	struct ren_vid_surface src[2];
	const struct ren_vid_surface *srclist[2] = {
		&src[0], &src[1]
	};
	struct ren_vid_surface dst;
	void *inbuf[2], *outbuf;
	int ret;
	int frameno=0;

	int show_version = 0;
	int show_help = 0;
	int show_list_vio = 0;
	char * progname;
	char * viodev = NULL;
	int error = 0;

	int c;
	char * optstring = "hvo:O:c:s:C:S:f:u:l";

#ifdef HAVE_GETOPT_LONG
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"output", required_argument, 0, 'o'},
		{"overlay", required_argument, 0, 'O'},
		{"input-colorspace", required_argument, 0, 'c'},
		{"input-size", required_argument, 0, 's'},
		{"output-colorspace", required_argument, 0, 'C'},
		{"output-size", required_argument, 0, 'S'},
		{"filter", required_argument, 0, 'f'},
		{"vio", required_argument, 0, 'u'},
		{"list", no_argument, 0, 'l'},
		{NULL,0,0,0}
	};
#endif

#if defined(USE_MERAM_RA) || defined(USE_MERAM_WB)
#define ALIGN16(_x)	(((_x) + 15) / 16 * 16)
#define ADJUST_PITCH(_p, _w)			\
	{					\
		(_p) = ((_w) - 1) | 1023;	\
		(_p) = (_p) | ((_p) >> 1);	\
		(_p) = (_p) | ((_p) >> 2);	\
		(_p) += 1;			\
	}

	unsigned long val;
	MERAM *meram = meram_open();
	MERAM_REG *regs = meram_lock_reg(meram);
	size_t sz;
	unsigned long mblock;
	ICB *icbr, *icbw;
#endif /* defined(USE_MERAM_RA) || defined(USE_MERAM_WB) */

	src[0].w = -1;
	src[0].h = -1;
	dst.w = -1;
	dst.h = -1;
	src[0].format = REN_UNKNOWN;
	dst.format = REN_UNKNOWN;
	src[0].bpitchy = src[0].bpitchc = src[0].bpitcha = 0;
	dst.bpitchy = dst.bpitchc = dst.bpitcha = 0;

	memcpy((void *)&src[1], (void *)&src[0], sizeof(src[0]));

	progname = argv[0];

	if (argc < 2) {
		usage (progname);
		return (1);
	}

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
		case 'o': /* output */
			outfilename = optarg;
			break;
		case 'O': /* ovalery */
			infilename[1] = optarg;
			break;
		case 'c': /* input colorspace */
			set_colorspace (optarg, &src[0].format);
			break;
		case 's': /* input size */
			set_size (optarg, &src[0].w, &src[0].h);
			break;
		case 'C': /* output colorspace */
			set_colorspace (optarg, &dst.format);
			break;
		case 'S': /* output size */
			set_size (optarg, &dst.w, &dst.h);
			break;
		case 'f': /* filter mode */
			rotation = strtoul(optarg, NULL, 0);
			break;
		case 'l':
			show_list_vio = 1;
			break;
		case 'u':
			viodev = optarg;
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

	if (show_list_vio) {
		char **vio;
		int i, n;

		if (shvio_list_vio(&vio, &n) < 0) {
			printf ("Can't get a list of VIO available...\n");
		} else {
			for(i = 0; i < n; i++)
				printf("%s", vio[i]);
			printf("Total: %d VIOs available.\n", n);
		}
	}

	if (show_version || show_help || show_list_vio) {
		goto exit_ok;
	}

	if (optind >= argc) {
		usage (progname);
		goto exit_err;
	}

	infilename[0] = argv[optind++];

	if (optind < argc) {
		outfilename = argv[optind++];
	}

	printf ("Input file: %s\n", infilename[0]);
	if (infilename[1] != NULL)
		printf ("Overlay file: %s\n", infilename[1]);
	printf ("Output file: %s\n", outfilename);

	guess_colorspace (infilename[0], &src[0].format);
	if (infilename[1])
		guess_colorspace (infilename[1], &src[1].format);
	guess_colorspace (outfilename, &dst.format);
	/* If the output colorspace isn't given and can't be guessed, then default to
	 * the input colorspace (ie. no colorspace conversion) */
	if (dst.format == REN_UNKNOWN)
		dst.format = src[0].format;

	guess_size (infilename[0], src[0].format, &src[0].w, &src[0].h);
	if (rotation & 0xF) {
		/* Swap width/height for rotation */
		dst.w = src[0].h;
		dst.h = src[0].w;
	} else if (dst.w == -1 && dst.h == -1) {
		/* If the output size isn't given and can't be guessed, then default to
		 * the input size (ie. no rescaling) */
		dst.w = src[0].w;
		dst.h = src[0].h;
	}
	if (infilename[1])
		guess_size (infilename[1], src[1].format, &src[1].w, &src[1].h);

	/* Setup memory pitch */
	src[0].pitch = src[0].w;
	src[1].pitch = src[1].w;
	dst.pitch = dst.w;

	/* Check that all parameters are set */
	if (src[0].format == REN_UNKNOWN) {
		fprintf (stderr, "ERROR: Input colorspace unspecified\n");
		error = 1;
	}
	if (src[0].w == -1) {
		fprintf (stderr, "ERROR: Input width unspecified\n");
		error = 1;
	}
	if (src[0].h == -1) {
		fprintf (stderr, "ERROR: Input height unspecified\n");
		error = 1;
	}

	if (dst.format == REN_UNKNOWN) {
		fprintf (stderr, "ERROR: Output colorspace unspecified\n");
		error = 1;
	}
	if (dst.w == -1) {
		fprintf (stderr, "ERROR: Output width unspecified\n");
		error = 1;
	}
	if (dst.h == -1) {
		fprintf (stderr, "ERROR: Output height unspecified\n");
		error = 1;
	}

	if (error) goto exit_err;

	printf ("Input colorspace:\t%s\n", show_colorspace (src[0].format));
	printf ("Input size:\t\t%dx%d %s\n", src[0].w, src[0].h, show_size (src[0].w, src[0].h));
	printf ("Output colorspace:\t%s\n", show_colorspace (dst.format));
	printf ("Output size:\t\t%dx%d %s\n", dst.w, dst.h, show_size (dst.w, dst.h));
	printf ("Rotation:\t\t%s\n", show_rotation (rotation));

	input_size[0] = imgsize (src[0].format, src[0].w, src[0].h);
	if (infilename[1] != NULL)
		input_size[1] = imgsize (src[1].format, src[1].w, src[1].h);
	output_size = imgsize (dst.format, dst.w, dst.h);

	if (viodev) {
		const char *blocks[2] = { viodev, NULL };
		uiomux = uiomux_open_named(blocks);
		uiores = 1 << 0;
	} else {
		uiomux = uiomux_open ();
		uiores = UIOMUX_SH_VEU;
	}

	/* Set up memory buffers */
	src[0].py = inbuf[0] = uiomux_malloc (uiomux, uiores, input_size[0], 32);
	if (src[0].format == REN_RGB565) {
		src[0].pc = 0;
	} else if (src[0].format == REN_YV12) {
		src[0].pc2 = src[0].py + (src[0].w * src[0].h);	/* Cr(V) */
		src[0].pc = src[0].pc2 + (src[0].w * src[0].h) / 4;	/* Cb(U) */
	} else if (src[0].format == REN_YV16) {
		src[0].pc2 = src[0].py + (src[0].w * src[0].h);	/* Cr(V) */
		src[0].pc = src[0].pc2 + (src[0].w * src[0].h) / 2;	/* Cb(U) */
	} else {
		src[0].pc = src[0].py + (src[0].w * src[0].h);	/* CbCr(UV) */
	}

	if (infilename[1] != NULL) {
		src[1].py = inbuf[1] = uiomux_malloc (uiomux, uiores, input_size[1], 32);
		if (src[1].format == REN_RGB565) {
			src[1].pc = 0;
		} else if (src[1].format == REN_YV12) {
			src[1].pc2 = src[1].py + (src[1].w * src[1].h);	/* Cr(V) */
			src[1].pc = src[1].pc2 + (src[1].w * src[1].h) / 4;	/* Cb(U) */
		} else if (src[1].format == REN_YV16) {
			src[1].pc2 = src[1].py + (src[1].w * src[1].h);	/* Cr(V) */
			src[1].pc = src[1].pc2 + (src[1].w * src[1].h) / 2;	/* Cb(U) */
		} else {
			src[1].pc = src[1].py + (src[1].w * src[1].h);	/* CbCr(UV) */
		}
	}

	dst.py = outbuf = uiomux_malloc (uiomux, uiores, output_size, 32);
	if (dst.format == REN_RGB565) {
		dst.pc = 0;
	} else if (dst.format == REN_YV12) {
		dst.pc2 = dst.py + (dst.w * dst.h);	/* Cr(V) */
		dst.pc = dst.pc2 + (dst.w * dst.h) / 4;	/* Cb(U) */
	} else if (dst.format == REN_YV16) {
		dst.pc2 = dst.py + (dst.w * dst.h);	/* Cr(V) */
		dst.pc = dst.pc2 + (dst.w * dst.h) / 2;	/* Cb(U) */
	} else {
		dst.pc = dst.py + (dst.w * dst.h);	/* CbCr(UV) */
	}

#if defined(USE_MERAM_RA) || defined(USE_MERAM_WB)
	meram_read_reg(meram, regs, MEVCR1, &val);
	val |= 1 << 29;		/* use 0xc0000000-0xdfffffff */
	meram_write_reg(meram, regs, MEVCR1, val);
	meram_unlock_reg(meram, regs);
#endif /* defined(USE_MERAM_RA) || defined(USE_MERAM_WB) */

#if defined(USE_MERAM_RA)
	/* calcurate byte-pitch */
	src[0].bpitchy = size_y(src[0].format, src[0].pitch, 0);

	/* set up read-ahead cache for input */
	icbr = meram_lock_icb(meram, 0);
	val = (3 << 24) |		/* KRBNM: ((3+1) << 1) = 8 lines */
		((16 - 1) << 16);	/* BNM: 16 = KRBNM * 2 lines */
	ADJUST_PITCH(sz, src[0].bpitchy);
	sz *= 16;			/* 16 lines */
	if (src[0].format == REN_NV12) {
		val |= 2 << 12;	/* CPL: YCbCr420 */
		sz = sz * 3 / 2;
	} else if (src[0].format == REN_NV16) {
		val |= 3 << 12;	/* CPL: YCbCr422 */
		sz = sz * 2;
	}
	meram_write_icb(meram, icbr, MExxMCNF, val);

	sz = (sz + 1023) / 1024;
	mblock = meram_alloc_icb_memory(meram, icbr,
					    (sz == 0) ? 1 : sz);
	val = (1 << 28) |		/* BSZ: 2^1 line/block */
		(mblock << 16) |	/* MSAR */
		(3 << 9) |		/* WD: (constant) */
		(1 << 8) |		/* WS: (constant) */
		(1 << 3) |		/* CM: address mode 1 */
		1;			/* MD: read buffer mode */
	meram_write_icb(meram, icbr, MExxCTRL, val);

	val = ((src[0].h - 1) << 16) |	/* YSZM1 */
		(src[0].bpitchy - 1);	/* XSZM1 */
	meram_write_icb(meram, icbr, MExxBSIZE, val);
	val = ALIGN16(src[0].bpitchy);	/* SBSIZE: 16 bytes aligned */
	meram_write_icb(meram, icbr, MExxSBSIZE, val);

	ADJUST_PITCH(src[0].bpitchy, src[0].bpitchy);
	src[0].bpitchc = src[0].bpitcha = src[0].bpitchy;

	val = uiomux_all_virt_to_phys(src[0].py);
	meram_write_icb(meram, icbr, MExxSSARA, val);

	src[0].py = (void *)meram_get_icb_address(meram, icbr, 0);
	uiomux_register(src[0].py, (unsigned long)src[0].py, 8 << 20);
	if (is_ycbcr(src[0].format)) {
		val = uiomux_all_virt_to_phys(src[0].pc);
		meram_write_icb(meram, icbr, MExxSSARB, val);
		src[0].pc = (void *)meram_get_icb_address(meram, icbr, 1);
		uiomux_register(src[0].pc, (unsigned long)src[0].pc, 8 << 20);
	} else {
		meram_write_icb(meram, icbr, MExxSSARB, 0);
	}
#endif /* defined(USE_MERAM_RA) */

#if defined(USE_MERAM_WB)
	/* calcurate byte-pitch */
	dst.bpitchy = size_y(dst.format, dst.pitch, 0);

	/* set up write-back cache for input */
	icbw = meram_lock_icb(meram, 1);
	val = (3 << 28) |		/* KWBNM: ((3+1) << 1) = 8 lines */
		((16 - 1) << 16);	/* BNM: 16 = KWBNM * 2 lines */
	ADJUST_PITCH(sz, dst.bpitchy);
	sz *= 16;			/* 16 lines */
	if (dst.format == REN_NV12) {
		val |= 2 << 12;	/* CPL: YCbCr420 */
		sz = sz * 3 / 2;
	} else if (dst.format == REN_NV16) {
		val |= 3 << 12;	/* CPL: YCbCr422 */
		sz = sz * 2;
	}
	meram_write_icb(meram, icbw, MExxMCNF, val);
	sz = (sz + 1023) / 1024;
	mblock = meram_alloc_icb_memory(meram, icbw,
					(sz == 0) ? 1 : sz);
	val = (1 << 28) |		/* BSZ: 2^1 line/block */
		(mblock << 16) |	/* MSAR */
		(3 << 9) |		/* WD: (constant) */
		(1 << 8) |		/* WS: (constant) */
		(1 << 3) |		/* CM: address mode 1 */
		2;			/* MD: write buffer mode */
	meram_write_icb(meram, icbw, MExxCTRL, val);

	val = ((dst.h - 1) << 16) |	/* YSZM1 */
		(dst.bpitchy - 1);	/* XSZM1 */
	meram_write_icb(meram, icbw, MExxBSIZE, val);
	val = ALIGN16(dst.bpitchy);	/* SBSIZE: 16 bytes aligned */
	meram_write_icb(meram, icbw, MExxSBSIZE, val);

	ADJUST_PITCH(dst.bpitchy, dst.bpitchy);
	dst.bpitchc = dst.bpitcha = dst.bpitchy;

	val = uiomux_all_virt_to_phys(dst.py);
	meram_write_icb(meram, icbw, MExxSSARA, val);

	dst.py = (void *)meram_get_icb_address(meram, icbw, 0);
	uiomux_register(dst.py, (unsigned long)dst.py, 8 << 20);
	if (is_ycbcr(dst.format)) {
		val = uiomux_all_virt_to_phys(dst.pc);
		meram_write_icb(meram, icbw, MExxSSARB, val);
		dst.pc = (void *)meram_get_icb_address(meram, icbw, 1);
		uiomux_register(dst.pc, (unsigned long)dst.pc, 8 << 20);
	} else {
		meram_write_icb(meram, icbw, MExxSSARB, 0);
	}
#endif /* defined(USE_MERAM_WB) */

	if (strcmp (infilename[0], "-") == 0) {
		infile[0] = stdin;
	} else {
		infile[0] = fopen (infilename[0], "rb");
		if (infile[0] == NULL) {
			fprintf (stderr, "%s: unable to open input file %s\n",
				 progname, infilename[0]);
			goto exit_err;
		}
	}

	if (infilename[1] != NULL) {
		infile[1] = fopen (infilename[1], "rb");
		if (infile[1] == NULL) {
			fprintf (stderr, "%s: unable to open input file %s\n",
				 progname, infilename[1]);
			goto exit_err;
		}
	}

	if (outfilename != NULL) {
		if (strcmp (outfilename, "-") == 0) {
			outfile = stdout;
		} else {
			outfile = fopen (outfilename, "wb");
			if (outfile == NULL) {
				fprintf (stderr, "%s: unable to open output file %s\n",
					 progname, outfilename);
				goto exit_err;
			}
		}
	}

	if (!viodev)
		vio = shvio_open();
	else
		vio = shvio_open_named(viodev);

	if (vio == 0) {
		fprintf (stderr, "Error opening VIO\n");
		goto exit_err;
	}

	while (1) {
#ifdef DEBUG
		fprintf (stderr, "%s: Converting frame %d\n", progname, frameno);
#endif

		/* Read input */
		if ((nread = fread (inbuf[0], 1, input_size[0], infile[0])) != input_size[0]) {
			if (nread == 0 && feof (infile[0])) {
				break;
			} else {
				fprintf (stderr, "%s: error reading input file %s\n",
					 progname, infilename[0]);
			}
		}

		if (infilename[1] != NULL) {
			if ((nread = fread (inbuf[1], 1, input_size[1], infile[1])) != input_size[1]) {
				if (nread == 0 && feof (infile[1])) {
					break;
				} else {
					fprintf (stderr, "%s: error reading input file %s\n",
						 progname, infilename[1]);
				}
			}

			printf("invoke shvio_setup_blend()...\n");
			ret = shvio_setup_blend(vio, NULL, srclist, 2, &dst);
			shvio_start(vio);
			printf("shvio_start_blend() = %d\n", ret);
			ret = shvio_wait(vio);
		} else {
			if (rotation) {
				ret = shvio_rotate(vio, &src[0], &dst, rotation);
			} else {
				ret = shvio_resize(vio, &src[0], &dst);
			}
		}

#if defined(USE_MERAM_WB)
		meram_read_icb(meram, icbw, MExxCTRL, &val);
		val |= 1 << 5;	/* WF: flush data */
		meram_write_icb(meram, icbw, MExxCTRL, val);
#endif
#if defined(USE_MERAM_RA)
		meram_read_icb(meram, icbr, MExxCTRL, &val);
		val |= 1 << 4;	/* RF: flush data */
		meram_write_icb(meram, icbr, MExxCTRL, val);
#endif

		/* Write output */
		if (outfile && fwrite (outbuf, 1, output_size, outfile) != output_size) {
				fprintf (stderr, "%s: error writing input file %s\n",
					 progname, outfilename);
		}

		frameno++;
	}

	shvio_close (vio);

#if defined(USE_MERAM_RA)
	/* finialize the read-ahead cache */
	uiomux_unregister(src[0].py);
	if (is_ycbcr(src[0].format))
		uiomux_unregister(src[0].pc);
	meram_free_icb_memory(meram, icbr);
	meram_unlock_icb(meram, icbr);
#endif
#if defined(USE_MERAM_WB)
	/* finialize the write-back cache */
	uiomux_unregister(dst.py);
	if (is_ycbcr(dst.format))
		uiomux_unregister(dst.pc);
	meram_free_icb_memory(meram, icbw);
	meram_unlock_icb(meram, icbw);
#endif
#if defined(USE_MERAM_RA) || defined(USE_MERAM_WB)
	meram_close(meram);
#endif

	uiomux_free (uiomux, uiores, src[0].py, input_size[0]);
	if (infilename[1] != NULL)
		uiomux_free (uiomux, uiores, src[1].py, input_size[1]);
	uiomux_free (uiomux, uiores, dst.py, output_size);
	uiomux_close (uiomux);

	if (infile[0] != stdin) fclose (infile[0]);
	if (infilename[1] != NULL)
		fclose (infile[1]);

	if (outfile == stdout) {
		fflush (stdout);
	} else if (outfile) {
		fclose (outfile);
	}

	printf ("Frames:\t\t%d\n", frameno);

exit_ok:
	exit (0);

exit_err:
	exit (1);
}
