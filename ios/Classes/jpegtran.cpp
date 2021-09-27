/*
 * jpegtran.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1995-2019, Thomas G. Lane, Guido Vollbeding.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2010, 2014, 2017, 2019-2020, D. R. Commander.
 * mozjpeg Modifications:
 * Copyright (C) 2014, Mozilla Corporation.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a command-line user interface for JPEG transcoding.
 * It is very similar to cjpeg.c, and partly to djpeg.c, but provides
 * lossless transcoding between different JPEG file formats.  It also
 * provides some lossless and sort-of-lossless transformations of JPEG data.
 */

#include "cdjpeg.h" /* Common decls for cjpeg/djpeg applications */
#include "cdjapi.h"
#include "jconfigint.h"
#include "transupp.h" /* Support routines for jpegtran */

#include <vector>
#include <memory>
using FILE_PTR = std::unique_ptr<FILE, decltype(&fclose)>;

/*
 * Argument-parsing code.
 * The switch parser is designed to be useful with DOS-style command line
 * syntax, ie, intermixed switches and file names, where only the switches
 * to the left of a given file name affect processing of that file.
 * The main program in this file doesn't actually use this capability...
 */

static const char *progname;                /* program name for error messages */
JDIMENSION max_scans;                       /* for -maxscans switch */
static char *outfilename;                   /* for -outfile switch */
boolean strict;                             /* for -strict switch */
static boolean prefer_smallest;             /* use smallest of input or result file (if no image-changing options supplied) */
static JCOPY_OPTION copyoption;             /* -copy switch */
static jpeg_transform_info transformoption; /* image transformation options */
boolean memsrc = FALSE;                     /* for -memsrc switch */
#define INPUT_BUF_SIZE 4096

LOCAL(void)
usage(void)
/* complain about bad command line */
{
    debug_printf("usage: %s [switches] ", progname);
    debug_printf("[inputfile]\n");

    debug_printf("Switches (names may be abbreviated):\n");
    debug_printf("  -copy none     Copy no extra markers from source file\n");
    debug_printf("  -copy comments Copy only comment markers (default)\n");
    debug_printf("  -copy all      Copy all extra markers\n");
    debug_printf("  -optimize      Optimize Huffman table (smaller file, but slow compression, enabled by default)\n");
    debug_printf("  -progressive   Create progressive JPEG file (enabled by default)\n");
    debug_printf("  -revert        Revert to standard defaults (instead of mozjpeg defaults)\n");
    debug_printf("  -fastcrush     Disable progressive scan optimization\n");
    debug_printf("Switches for modifying the image:\n");
    debug_printf("  -crop WxH+X+Y  Crop to a rectangular region\n");
    debug_printf("  -flip [horizontal|vertical]  Mirror image (left-right or top-bottom)\n");
    debug_printf("  -grayscale     Reduce to grayscale (omit color data)\n");
    debug_printf("  -perfect       Fail if there is non-transformable edge blocks\n");
    debug_printf("  -rotate [90|180|270]         Rotate image (degrees clockwise)\n");
    debug_printf("  -transpose     Transpose image\n");
    debug_printf("  -transverse    Transverse transpose image\n");
    debug_printf("  -wipe WxH+X+Y  Wipe (gray out) a rectangular region\n");
    debug_printf("Switches for advanced users:\n");
    debug_printf("  -restart N     Set restart interval in rows, or in blocks with B\n");
    debug_printf("  -maxmemory N   Maximum memory to use (in kbytes)\n");
    debug_printf("  -maxscans N    Maximum number of scans to allow in input file\n");
    debug_printf("  -outfile name  Specify name for output file\n");
    debug_printf("  -strict        Treat all warnings as fatal\n");
    debug_printf("  -verbose  or  -debug   Emit debug output\n");
    jt_exit(EXIT_FAILURE);
}

LOCAL(void)
select_transform(JXFORM_CODE transform)
/* Silly little routine to detect multiple transform options,
 * which we can't handle.
 */
{
    if (transformoption.transform == JXFORM_NONE ||
        transformoption.transform == transform)
    {
        transformoption.transform = transform;
    }
    else
    {
        debug_printf("%s: can only do one image transformation at a time\n",
                     progname);
        usage();
    }
}

LOCAL(int)
parse_switches(j_compress_ptr cinfo, int argc, char **argv,
               int last_file_arg_seen, boolean for_real)
/* Parse optional switches.
 * Returns argv[] index of first file-name argument (== argc if none).
 * Any file names with indexes <= last_file_arg_seen are ignored;
 * they have presumably been processed in a previous iteration.
 * (Pass 0 for last_file_arg_seen on the first or only iteration.)
 */
{
    boolean simple_progressive = cinfo->num_scans == 0 ? FALSE : TRUE;

    max_scans = 0;
    outfilename = NULL;
    strict = FALSE;
    copyoption = JCOPYOPT_DEFAULT;
    transformoption.transform = JXFORM_NONE;
    transformoption.perfect = FALSE;
    transformoption.trim = FALSE;
    transformoption.force_grayscale = FALSE;
    transformoption.crop = FALSE;
    transformoption.slow_hflip = FALSE;
    cinfo->err->trace_level = 0;
    prefer_smallest = TRUE;

    /* Scan command line options, adjust parameters */
    int argn;
    char *arg;
    for (argn = 1; argn < argc; argn++)
    {
        arg = argv[argn];
        if (*arg != '-')
        {
            /* Not a switch, must be a file name argument */
            if (argn <= last_file_arg_seen)
            {
                outfilename = NULL; /* -outfile applies to just one input file */
                continue;           /* ignore this name if previously processed */
            }
            break; /* else done parsing switches */
        }
        arg++; /* advance past switch marker character */

        if (keymatch(arg, "copy", 2))
        {
            /* Select which extra markers to copy. */
            if (++argn >= argc) /* advance to next argument */
                usage();
            if (keymatch(argv[argn], "none", 1))
            {
                copyoption = JCOPYOPT_NONE;
            }
            else if (keymatch(argv[argn], "comments", 1))
            {
                copyoption = JCOPYOPT_COMMENTS;
            }
            else if (keymatch(argv[argn], "all", 1))
            {
                copyoption = JCOPYOPT_ALL;
            }
            else
                usage();
        }
        else if (keymatch(arg, "crop", 2))
        {
            /* Perform lossless cropping. */
            if (++argn >= argc) /* advance to next argument */
                usage();
            if (transformoption.crop /* reject multiple crop/drop/wipe requests */ ||
                !jtransform_parse_crop_spec(&transformoption, argv[argn]))
            {
                debug_printf("%s: bogus -crop argument '%s'\n",
                             progname, argv[argn]);
                jt_exit(EXIT_FAILURE);
            }
            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "debug", 1) || keymatch(arg, "verbose", 1))
        {
            /* Enable debug printouts. */
            //   /* On first -d, print version identification */
            //   static boolean printed_version = FALSE;

            //   if (!printed_version) {
            //     debug_printf("%s version %s (build %s)\n",
            //             PACKAGE_NAME, VERSION, BUILD);
            //     debug_printf("%s\n\n", JCOPYRIGHT);
            //     debug_printf("Emulating The Independent JPEG Group's software, version %s\n\n",
            //             JVERSION);
            //     printed_version = TRUE;
            //   }
            cinfo->err->trace_level++;
        }
        else if (keymatch(arg, "version", 4))
        {
            debug_printf("%s version %s (build %s)\n",
                         PACKAGE_NAME, VERSION, BUILD);
            jt_exit(EXIT_SUCCESS);
        }
        else if (keymatch(arg, "flip", 1))
        {
            /* Mirror left-right or top-bottom. */
            if (++argn >= argc) /* advance to next argument */
                usage();
            if (keymatch(argv[argn], "horizontal", 1))
                select_transform(JXFORM_FLIP_H);
            else if (keymatch(argv[argn], "vertical", 1))
                select_transform(JXFORM_FLIP_V);
            else
                usage();

            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "fastcrush", 4))
        {
            jpeg_c_set_bool_param(cinfo, JBOOLEAN_OPTIMIZE_SCANS, FALSE);
        }
        else if (keymatch(arg, "grayscale", 1) || keymatch(arg, "greyscale", 1))
        {
            /* Force to grayscale. */
            transformoption.force_grayscale = TRUE;
            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "maxmemory", 3))
        {
            /* Maximum memory in Kb (or Mb with 'm'). */
            long lval;
            char ch = 'x';

            if (++argn >= argc) /* advance to next argument */
                usage();
            if (sscanf(argv[argn], "%ld%c", &lval, &ch) < 1)
                usage();
            if (ch == 'm' || ch == 'M')
                lval *= 1000L;
            cinfo->mem->max_memory_to_use = lval * 1000L;
        }
        else if (keymatch(arg, "maxscans", 4))
        {
            if (++argn >= argc) /* advance to next argument */
                usage();
            if (sscanf(argv[argn], "%u", &max_scans) != 1)
                usage();
        }
        else if (keymatch(arg, "optimize", 1) || keymatch(arg, "optimise", 1))
        {
            /* Enable entropy parm optimization. */
            cinfo->optimize_coding = TRUE;
        }
        else if (keymatch(arg, "outfile", 4))
        {
            /* Set output file name. */
            if (++argn >= argc) /* advance to next argument */
                usage();
            outfilename = argv[argn]; /* save it away for later use */
        }
        else if (keymatch(arg, "perfect", 2))
        {
            /* Fail if there is any partial edge MCUs that the transform can't
       * handle. */
            transformoption.perfect = TRUE;
        }
        else if (keymatch(arg, "progressive", 2))
        {
            /* Select simple progressive mode. */
            simple_progressive = TRUE;
            prefer_smallest = FALSE;
            /* We must postpone execution until num_components is known. */
        }
        else if (keymatch(arg, "restart", 1))
        {
            /* Restart interval in MCU rows (or in MCUs with 'b'). */
            long lval;
            char ch = 'x';

            if (++argn >= argc) /* advance to next argument */
                usage();
            if (sscanf(argv[argn], "%ld%c", &lval, &ch) < 1)
                usage();
            if (lval < 0 || lval > 65535L)
                usage();
            if (ch == 'b' || ch == 'B')
            {
                cinfo->restart_interval = (unsigned int)lval;
                cinfo->restart_in_rows = 0; /* else prior '-restart n' overrides me */
            }
            else
            {
                cinfo->restart_in_rows = (int)lval;
                /* restart_interval will be computed during startup */
            }
        }
        else if (keymatch(arg, "revert", 3))
        {
            /* revert to old JPEG default */
            jpeg_c_set_int_param(cinfo, JINT_COMPRESS_PROFILE, JCP_FASTEST);
            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "rotate", 2))
        {
            /* Rotate 90, 180, or 270 degrees (measured clockwise). */
            if (++argn >= argc) /* advance to next argument */
                usage();
            if (keymatch(argv[argn], "90", 2))
                select_transform(JXFORM_ROT_90);
            else if (keymatch(argv[argn], "180", 3))
                select_transform(JXFORM_ROT_180);
            else if (keymatch(argv[argn], "270", 3))
                select_transform(JXFORM_ROT_270);
            else
                usage();

            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "strict", 2))
        {
            strict = TRUE;
        }
        else if (keymatch(arg, "transpose", 1))
        {
            /* Transpose (across UL-to-LR axis). */
            select_transform(JXFORM_TRANSPOSE);
            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "transverse", 6))
        {
            /* Transverse transpose (across UR-to-LL axis). */
            select_transform(JXFORM_TRANSVERSE);
            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "trim", 3))
        {
            /* Trim off any partial edge MCUs that the transform can't handle. */
            transformoption.trim = TRUE;
            prefer_smallest = FALSE;
        }
        else if (keymatch(arg, "wipe", 1))
        {
            if (++argn >= argc) /* advance to next argument */
                usage();
            if (transformoption.crop /* reject multiple crop/drop/wipe requests */ ||
                !jtransform_parse_crop_spec(&transformoption, argv[argn]))
            {
                debug_printf("%s: bogus -wipe argument '%s'\n",
                             progname, argv[argn]);
                jt_exit(EXIT_FAILURE);
            }
            select_transform(JXFORM_WIPE);
        }
        else
        {
            debug_printf("*** unknown/unsupported option: -%s\n", arg);
            usage(); /* bogus switch */
        }
    }

    /* Post-switch-scanning cleanup */

    if (for_real)
    {
        if (simple_progressive) /* process -progressive; -scans can override */
            jpeg_simple_progression(cinfo);
    }

    return argn; /* return index of next arg (file name) */
}

METHODDEF(void)
my_emit_message(j_common_ptr cinfo, int msg_level)
{
    if (msg_level < 0)
    {
        /* Treat warning as fatal */
        cinfo->err->error_exit(cinfo);
    }
    else
    {
        if (cinfo->err->trace_level >= msg_level)
            cinfo->err->output_message(cinfo);
    }
}

/*
 * The main program.
 */

extern "C" __attribute__((visibility("default"))) __attribute__((used)) int jpegtran(int argc, char **argv, void *context)
{
    std::vector<unsigned char> inbuffer;
    std::vector<unsigned char> outbuffer_v(4096);
    unsigned char *outbuffer = &outbuffer_v[0];
    unsigned long outsize = outbuffer_v.size();
    try
    {
        jvirt_barray_ptr *src_coef_arrays;
        jvirt_barray_ptr *dst_coef_arrays;

        progname = argv[0];
        if (progname == NULL || progname[0] == 0)
            progname = "jpegtran"; /* in case C library doesn't provide it */

        /* Initialize the JPEG decompression object with default error handling. */
        jpeg_decompress_struct srcinfo;
        jpeg_error_mgr jsrcerr;
        srcinfo.err = jpeg_std_error(&jsrcerr);
        jpeg_create_decompress(&srcinfo);

        /* Initialize the JPEG compression object with default error handling. */
        jpeg_compress_struct dstinfo;
        jpeg_error_mgr jdsterr;
        dstinfo.err = jpeg_std_error(&jdsterr);
        jpeg_create_compress(&dstinfo);

        /* Scan command line to find file names.
         * It is convenient to use just one switch-parsing routine, but the switch
         * values read here are mostly ignored; we will rescan the switches after
         * opening the input file.  Also note that most of the switches affect the
         * destination JPEG object, so we parse into that and then copy over what
         * needs to affect the source too.
         */

        int file_index = parse_switches(&dstinfo, argc, argv, 0, FALSE);
        jsrcerr.trace_level = jdsterr.trace_level;
        srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;

        if (strict)
            jsrcerr.emit_message = my_emit_message;

        /* Unix style: expect zero or one file name */
        if (file_index < argc - 1)
        {
            debug_printf("%s: only one input file\n", progname);
            usage();
        }

        /* Open the input file. */
        if (file_index >= argc)
        {
            debug_printf("%s: no input file name.\n", progname);
            jt_exit(EXIT_FAILURE);
        }

        {
            FILE_PTR fp(fopen(argv[file_index], READ_BINARY), fclose);
            if (!fp)
            {
                debug_printf("%s: can't open %s for reading\n", progname,
                             argv[file_index]);
                jt_exit(EXIT_FAILURE);
            }

            cdjpeg_progress_mgr dst_progress;
            start_progress_monitor((j_common_ptr)&dstinfo, &dst_progress, context);

            /* Specify data source for decompression */
            if (jpeg_c_int_param_supported(&dstinfo, JINT_COMPRESS_PROFILE) &&
                jpeg_c_get_int_param(&dstinfo, JINT_COMPRESS_PROFILE) == JCP_MAX_COMPRESSION)
                memsrc = TRUE; /* needed to revert to original */

            if (memsrc)
            {
                unsigned long insize = 0;
                size_t nbytes;
                do
                {
                    inbuffer.resize(insize + INPUT_BUF_SIZE);
                    nbytes = JFREAD(fp.get(), &inbuffer[insize], INPUT_BUF_SIZE);
                    if (nbytes < INPUT_BUF_SIZE && ferror(fp.get()))
                    {
                        if (file_index < argc)
                            debug_printf("%s: can't read from %s\n", progname,
                                         argv[file_index]);
                        else
                            debug_printf("%s: can't read from stdin\n", progname);
                    }
                    insize += (unsigned long)nbytes;
                } while (nbytes == INPUT_BUF_SIZE);
                inbuffer.resize(insize);
                jpeg_mem_src(&srcinfo, &inbuffer[0], insize);
            }
            else
                jpeg_stdio_src(&srcinfo, fp.get());

            /* Enable saving of extra markers that we want to copy */
            jcopy_markers_setup(&srcinfo, copyoption);

            /* Read file header */
            (void)jpeg_read_header(&srcinfo, TRUE);

            /* Any space needed by a transform option must be requested before
         * jpeg_read_coefficients so that memory allocation will be done right.
         */
            /* Fail right away if -perfect is given and transformation is not perfect. */
            if (!jtransform_request_workspace(&srcinfo, &transformoption))
            {
                debug_printf("%s: transformation is not perfect\n", progname);
                jt_exit(EXIT_FAILURE);
            }

            /* Read source file as DCT coefficients */
            src_coef_arrays = jpeg_read_coefficients(&srcinfo);

            /* Initialize destination compression parameters from source values */
            jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

            /* Adjust destination parameters if required by transform options;
         * also find out which set of coefficient arrays will hold the output.
         */
            dst_coef_arrays = jtransform_adjust_parameters(&srcinfo, &dstinfo,
                                                           src_coef_arrays,
                                                           &transformoption);

            /* Close input file, if we opened it.
         * Note: we assume that jpeg_read_coefficients consumed all input
         * until JPEG_REACHED_EOI, and that jpeg_finish_decompress will
         * only consume more while (!cinfo->inputctl->eoi_reached).
         * We cannot call jpeg_finish_decompress here since we still need the
         * virtual arrays allocated from the source object for processing.
         */
        }

        /* Open the output file. */
        if (!outfilename)
        {
            debug_printf("%s: no output file name.\n", progname);
            jt_exit(EXIT_FAILURE);
        }
        FILE_PTR fpOut(fopen(outfilename, WRITE_BINARY), fclose);
        if (!fpOut)
        {
            debug_printf("%s: can't open %s for writing\n", progname, outfilename);
            jt_exit(EXIT_FAILURE);
        }

        /* Adjust default compression parameters by re-parsing the options */
        file_index = parse_switches(&dstinfo, argc, argv, 0, TRUE);

        /* Specify data destination for compression */
        if (jpeg_c_int_param_supported(&dstinfo, JINT_COMPRESS_PROFILE) &&
            jpeg_c_get_int_param(&dstinfo, JINT_COMPRESS_PROFILE) == JCP_MAX_COMPRESSION)
            jpeg_mem_dest(&dstinfo, &outbuffer, &outsize);
        else
            jpeg_stdio_dest(&dstinfo, fpOut.get());

        /* Start compressor (note no image data is actually written here) */
        jpeg_write_coefficients(&dstinfo, dst_coef_arrays);

        /* Copy to the output file any extra markers that we want to preserve */
        jcopy_markers_execute(&srcinfo, &dstinfo, copyoption);

        /* Execute image transformation, if any */
        jtransform_execute_transformation(&srcinfo, &dstinfo, src_coef_arrays,
                                          &transformoption);

        /* Finish compression and release memory */
        jpeg_finish_compress(&dstinfo);

        if (jpeg_c_int_param_supported(&dstinfo, JINT_COMPRESS_PROFILE) &&
            jpeg_c_get_int_param(&dstinfo, JINT_COMPRESS_PROFILE) == JCP_MAX_COMPRESSION)
        {
            size_t nbytes;

            unsigned char *buffer = outbuffer;
            unsigned long size = outsize;
            if (prefer_smallest && inbuffer.size() < size)
            {
                size = inbuffer.size();
                buffer = &inbuffer[0];
            }

            nbytes = JFWRITE(fpOut.get(), buffer, size);
            if (nbytes < size && ferror(fpOut.get()))
            {
                if (file_index < argc)
                    debug_printf("%s: can't write to %s\n", progname,
                                 argv[file_index]);
                else
                    debug_printf("%s: can't write to stdout\n", progname);
            }
        }

        jpeg_destroy_compress(&dstinfo);
        (void)jpeg_finish_decompress(&srcinfo);
        jpeg_destroy_decompress(&srcinfo);

        end_progress_monitor((j_common_ptr)&dstinfo);

        /* All done. */
        jt_exit(jsrcerr.num_warnings + jdsterr.num_warnings ? EXIT_WARNING : EXIT_SUCCESS);
        return 0; /* suppress no-return-value warnings */
    }
    catch (int code)
    {
        return code;
    }
}

extern "C" void jt_exit(int code)
{
    throw code;
}
