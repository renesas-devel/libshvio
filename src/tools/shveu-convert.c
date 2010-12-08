/*
 * Tool to demonstrate VEU hardware acceleration of raw image scaling.
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

#include "shveu/shveu.h"

/* Rotation: default none */
static int rotation = SHVEU_NO_ROT;

static void
usage (const char * progname)
{
	printf ("Usage: %s [options] [input-filename [output-filename]]\n", progname);
	printf ("Convert raw image data using the SH-Mobile VEU.\n");
	printf ("\n");
	printf ("If no output filename is specified, data is output to stdout.\n");
	printf ("Specify '-' to force output to be written to stdout.\n");
	printf ("\n");
	printf ("If no input filename is specified, data is read from stdin.\n");
	printf ("Specify '-' to force input to be read from stdin.\n");
	printf ("\nInput options\n");
	printf ("  -c, --input-colorspace (RGB565, RGB888, RGBx888, NV12, YCbCr420, NV16, YCbCr422)\n");
	printf ("                         Specify input colorspace\n");
	printf ("  -s, --input-size       Set the input image size (qcif, cif, qvga, vga, d1, 720p)\n");
	printf ("\nOutput options\n");
	printf ("  -o filename, --output filename\n");
	printf ("                         Specify output filename (default: stdout)\n");
	printf ("  -C, --output-colorspace (RGB565, RGB888, RGBx888, NV12, YCbCr420, NV16, YCbCr422)\n");
	printf ("                         Specify output colorspace\n");
	printf ("\nTransform options\n");
	printf ("  Note that the VEU does not support combined rotation and scaling.\n");
	printf ("  -S, --output-size      Set the output image size (qcif, cif, qvga, vga, d1, 720p)\n");
	printf ("                         [default is same as input size, ie. no rescaling]\n");
	printf ("  -r, --rotate	          Rotate the image 90 degrees clockwise\n");
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
		if (!strncasecmp (arg, sizes[i].name, strlen(sizes[i].name))) {
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

static int set_colorspace (char * arg, ren_vid_format_t * c)
{
	int nr_exts = sizeof(exts) / sizeof(exts[0]);
	int i;

	if (!arg)
		return -1;

	for (i=0; i<nr_exts; i++) {
		if (!strncasecmp (arg, exts[i].ext, strlen(exts[i].ext))) {
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
	case SHVEU_NO_ROT:
		return "None";
	case SHVEU_ROT_90:
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
	return (off_t)(size_y(colorspace, w*h) + size_c(colorspace, w*h));
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

	char * infilename = NULL, * outfilename = NULL;
	FILE * infile, * outfile = NULL;
	size_t nread;
	size_t input_size, output_size;
	SHVEU *veu;
	struct ren_vid_surface src;
	struct ren_vid_surface dst;
	int ret;
	int frameno=0;

	int show_version = 0;
	int show_help = 0;
	char * progname;
	int error = 0;

	int c;
	char * optstring = "hvo:c:s:C:S:r";

#ifdef HAVE_GETOPT_LONG
	static struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"output", required_argument, 0, 'o'},
		{"input-colorspace", required_argument, 0, 'c'},
		{"input-size", required_argument, 0, 's'},
		{"output-colorspace", required_argument, 0, 'C'},
		{"output-size", required_argument, 0, 'S'},
		{"rotate", no_argument, 0, 'r'},
		{NULL,0,0,0}
	};
#endif

	src.w = -1;
	src.h = -1;
	dst.w = -1;
	dst.h = -1;
	src.format = REN_UNKNOWN;
	dst.format = REN_UNKNOWN;


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
		case 'c': /* input colorspace */
			set_colorspace (optarg, &src.format);
			break;
		case 's': /* input size */
			set_size (optarg, &src.w, &src.h);
			break;
		case 'C': /* output colorspace */
			set_colorspace (optarg, &dst.format);
			break;
		case 'S': /* output size */
			set_size (optarg, &dst.w, &dst.h);
			break;
		case 'r': /* rotate */
			rotation = SHVEU_ROT_90;
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

	if (optind >= argc) {
		usage (progname);
		goto exit_err;
	}

	infilename = argv[optind++];
	
	if (optind < argc) {
		outfilename = argv[optind++];
	}

	printf ("Input file: %s\n", infilename);
	printf ("Output file: %s\n", outfilename);

	guess_colorspace (infilename, &src.format);
	guess_colorspace (outfilename, &dst.format);
	/* If the output colorspace isn't given and can't be guessed, then default to
	 * the input colorspace (ie. no colorspace conversion) */
	if (dst.format == REN_UNKNOWN)
		dst.format = src.format;

	guess_size (infilename, src.format, &src.w, &src.h);
	/* If the output size isn't given and can't be guessed, then default to
	 * the input size (ie. no rescaling) */
	if (dst.w == -1 && dst.h == -1) {
		if (rotation == SHVEU_NO_ROT) {
			dst.w = src.w;
			dst.h = src.h;
		} else {
			/* Swap width/height for rotation */
			dst.w = src.h;
			dst.h = src.w;
		}
	}

	/* Setup memory pitch */
	src.pitch = src.w;
	dst.pitch = dst.w;

	/* Check that all parameters are set */
	if (src.format == REN_UNKNOWN) {
		fprintf (stderr, "ERROR: Input colorspace unspecified\n");
		error = 1;
	}
	if (src.w == -1) {
		fprintf (stderr, "ERROR: Input width unspecified\n");
		error = 1;
	}
	if (src.h == -1) {
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

	printf ("Input colorspace:\t%s\n", show_colorspace (src.format));
	printf ("Input size:\t\t%dx%d %s\n", src.w, src.h, show_size (src.w, src.h));
	printf ("Output colorspace:\t%s\n", show_colorspace (dst.format));
	printf ("Output size:\t\t%dx%d %s\n", dst.w, dst.h, show_size (dst.w, dst.h));
	printf ("Rotation:\t\t%s\n", show_rotation (rotation));

	input_size = imgsize (src.format, src.w, src.h);
	output_size = imgsize (dst.format, dst.w, dst.h);

	uiomux = uiomux_open ();

	/* Set up memory buffers */
	src.py = uiomux_malloc (uiomux, UIOMUX_SH_VEU, input_size, 32);
	if (src.format == REN_RGB565) {
		src.pc = 0;
	} else {
		src.pc = src.py + (src.w * src.h);
	}

	dst.py = uiomux_malloc (uiomux, UIOMUX_SH_VEU, output_size, 32);
	if (dst.format == REN_RGB565) {
		dst.pc = 0;
	} else {
		dst.pc = dst.py + (dst.w * dst.h);
	}

	if (strcmp (infilename, "-") == 0) {
		infile = stdin;
	} else {
		infile = fopen (infilename, "rb");
		if (infile == NULL) {
			fprintf (stderr, "%s: unable to open input file %s\n",
				 progname, infilename);
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

	if ((veu = shveu_open ()) == 0) {
		fprintf (stderr, "Error opening VEU\n");
		goto exit_err;
	}

	while (1) {
#ifdef DEBUG
		fprintf (stderr, "%s: Converting frame %d\n", progname, frameno);
#endif

		/* Read input */
		if ((nread = fread (src.py, 1, input_size, infile)) != input_size) {
			if (nread == 0 && feof (infile)) {
				break;
			} else {
				fprintf (stderr, "%s: error reading input file %s\n",
					 progname, infilename);
			}
		}

		if (rotation) {
			ret = shveu_rotate(veu, &src, &dst, rotation);
		} else {
			ret = shveu_resize(veu, &src, &dst);
		}

		/* Write output */
		if (outfile && fwrite (dst.py, 1, output_size, outfile) != output_size) {
				fprintf (stderr, "%s: error writing input file %s\n",
					 progname, outfilename);
		}

		frameno++;
	}

	shveu_close (veu);

	uiomux_free (uiomux, UIOMUX_SH_VEU, src.py, input_size);
	uiomux_free (uiomux, UIOMUX_SH_VEU, dst.py, output_size);
	uiomux_close (uiomux);

	if (infile != stdin) fclose (infile);

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
