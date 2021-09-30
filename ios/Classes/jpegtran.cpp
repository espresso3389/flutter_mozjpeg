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

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <string>
#include <vector>
#include <memory>

#include "vector_dest_mgr.h"

class JpegTran
{
public:
    JpegTran(int argc, char **argv, void *context) : context(context)
    {
        size_t bufSize = 0;
        for (int i = 0; i < argc; i++)
            bufSize += strlen(argv[i]) + 1;
        argbuffer.resize(bufSize);
        char *p = &argbuffer[0];
        for (int i = 0; i < argc; i++)
        {
            this->argv.push_back(p);
            strcpy(p, argv[i]);
            p += strlen(argv[i]) + 1;
        }

        progname = this->argv[0];
        if (progname == NULL || progname[0] == 0)
            progname = "jpegtran"; // in case C library doesn't provide it

        init();
    }

    std::vector<char> argbuffer;
    std::vector<char *> argv;
    void *context;

    /*
     * Argument-parsing code.
     * The switch parser is designed to be useful with DOS-style command line
     * syntax, ie, intermixed switches and file names, where only the switches
     * to the left of a given file name affect processing of that file.
     * The main program in this file doesn't actually use this capability...
     */

    const char *progname; /* program name for error messages */
    char *outfilename;    /* for -outfile switch */
    unsigned char *image_ptr;
    size_t image_size;
    boolean strict;                      /* for -strict switch */
    boolean prefer_smallest;             /* use smallest of input or result file (if no image-changing options supplied) */
    JCOPY_OPTION copyoption;             /* -copy switch */
    jpeg_transform_info transformoption; /* image transformation options */

    void init()
    {
        outfilename = NULL;
        image_ptr = NULL;
        image_size = 0;
        strict = FALSE;
        copyoption = JCOPYOPT_DEFAULT;
        transformoption.transform = JXFORM_NONE;
        transformoption.perfect = FALSE;
        transformoption.trim = FALSE;
        transformoption.force_grayscale = FALSE;
        transformoption.crop = FALSE;
        transformoption.slow_hflip = FALSE;
        prefer_smallest = TRUE;
    }

    void usage()
    /* complain about bad command line */
    {
        debug_printf("usage: %s [switches] [inputfile]\n", progname);
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
        debug_printf("  -rotate [90|180|270]\n"
                     "                 Rotate image (degrees clockwise)\n");
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

    void select_transform(JXFORM_CODE transform)
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
            debug_printf("%s: can only do one image transformation at a time\n", progname);
            usage();
        }
    }

    size_t parse_switches(j_compress_ptr cinfo, size_t argc, char **argv,
                          int last_file_arg_seen, boolean for_real)
    /* Parse optional switches.
 * Returns argv[] index of first file-name argument (== argc if none).
 * Any file names with indexes <= last_file_arg_seen are ignored;
 * they have presumably been processed in a previous iteration.
 * (Pass 0 for last_file_arg_seen on the first or only iteration.)
 */
    {
        boolean simple_progressive = cinfo->num_scans == 0 ? FALSE : TRUE;
        cinfo->err->trace_level = 0;

        /* Scan command line options, adjust parameters */
        size_t argn;
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
                /* On first -d, print version identification */
                // static boolean printed_version = FALSE;

                // if (!printed_version)
                // {
                //     debug_printf("%s version %s (build %s)\n",
                //                  PACKAGE_NAME, VERSION, BUILD);
                //     debug_printf("%s\n\n", JCOPYRIGHT);
                //     debug_printf("Emulating The Independent JPEG Group's software, version %s\n\n",
                //                  JVERSION);
                //     printed_version = TRUE;
                // }
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

    static void my_emit_message(j_common_ptr cinfo, int msg_level)
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

    int jpegtran()
    {
        std::vector<unsigned char> inbuffer;
        int result = 0;

        /* Initialize the JPEG decompression object with default error handling. */
        jpeg_decompress_struct srcinfo;
        memset(&srcinfo, 0, sizeof(srcinfo));
        jpeg_error_mgr jsrcerr;
        srcinfo.err = debug_foward_error(&jsrcerr);

        /* Initialize the JPEG compression object with default error handling. */
        jpeg_compress_struct dstinfo;
        memset(&dstinfo, 0, sizeof(dstinfo));
        jpeg_error_mgr jdsterr;
        dstinfo.err = debug_foward_error(&jdsterr);

        try
        {
            jpeg_create_decompress(&srcinfo);
            jpeg_create_compress(&dstinfo);

            /* Scan command line to find file names.
             * It is convenient to use just one switch-parsing routine, but the switch
             * values read here are mostly ignored; we will rescan the switches after
             * opening the input file.  Also note that most of the switches affect the
             * destination JPEG object, so we parse into that and then copy over what
             * needs to affect the source too.
             */

            size_t file_index = parse_switches(&dstinfo, argv.size(), &argv[0], 0, FALSE);
            jsrcerr.trace_level = jdsterr.trace_level;
            srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;

            if (strict)
                jsrcerr.emit_message = my_emit_message;

            /* Unix style: expect zero or one file name */
            if (file_index < argv.size() - 1)
            {
                debug_printf("%s: only one input file\n", progname);
                usage();
            }

            /* Open the input file. */
            if (file_index >= argv.size())
            {
                debug_printf("%s: no input file name.\n", progname);
                jt_exit(EXIT_FAILURE);
            }

            cdjpeg_progress_mgr dst_progress;
            start_progress_monitor((j_common_ptr)&dstinfo, &dst_progress, context);
            void *buf_address = 0;
            unsigned long long buf_size = 0;
            const char *input_filename = argv[file_index];
            bool mem_src_ok = false;
            if (strstr(input_filename, "@buffer@:") == 0) // special prefix to directly set image on memory
            {
                char *endp = (char *)input_filename + 9; // skip the prefix "@buffer@:"
                unsigned long long addr = strtoull(endp, &endp, 10);
                if ((addr != 0 && addr != ULLONG_MAX) && *endp == ',')
                {
                    buf_address = (void *)(size_t)addr;
                    buf_size = strtoull(endp, &endp, 10);
                    if (buf_size != 0 && buf_size != ULLONG_MAX)
                    {
                        jpeg_mem_src(&srcinfo, (const unsigned char *)buf_address, buf_size);
                        mem_src_ok = true;
                    }
                }
            }

            if (!mem_src_ok)
            {
                FILE *fp = fopen(input_filename, READ_BINARY);
                if (!fp)
                {
                    debug_printf("%s: can't open %s for reading\n", progname, input_filename);
                    jt_exit(EXIT_FAILURE);
                }
                struct stat st;
                if (fstat(fileno(fp), &st) != 0)
                {
                    fclose(fp);
                    debug_printf("%s: can't stat %s\n", progname, input_filename);
                    jt_exit(EXIT_FAILURE);
                }

                inbuffer.resize(st.st_size);
                if (fread(&inbuffer[0], inbuffer.size(), 1, fp) != 1)
                {
                    fclose(fp);
                    debug_printf("%s: can't read from %s\n", progname, input_filename);
                    jt_exit(EXIT_FAILURE);
                }
                fclose(fp);
                jpeg_mem_src(&srcinfo, &inbuffer[0], inbuffer.size());
                mem_src_ok = true;
            }

            /* Enable saving of extra markers that we want to copy */
            jcopy_markers_setup(&srcinfo, copyoption);

            /* Read file header */
            jpeg_read_header(&srcinfo, TRUE);

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
            jvirt_barray_ptr *src_coef_arrays = jpeg_read_coefficients(&srcinfo);

            /* Initialize destination compression parameters from source values */
            jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

            /* Adjust destination parameters if required by transform options;
             * also find out which set of coefficient arrays will hold the output.
             */
            jvirt_barray_ptr *dst_coef_arrays = jtransform_adjust_parameters(&srcinfo, &dstinfo,
                                                                             src_coef_arrays,
                                                                             &transformoption);

            /* Open the output file. */
            if (!buf_address && !outfilename)
            {
                debug_printf("%s: no output file name.\n", progname);
                jt_exit(EXIT_FAILURE);
            }

            /* Adjust default compression parameters by re-parsing the options */
            file_index = parse_switches(&dstinfo, argv.size(), &argv[0], 0, TRUE);

            /* Specify data destination for compression */
            std::vector<unsigned char> outbuffer;
            vector_dest_mgr::init(&dstinfo, outbuffer);

            // Start compressor (note no image data is actually written here)
            jpeg_write_coefficients(&dstinfo, dst_coef_arrays);

            // Copy to the output file any extra markers that we want to preserve
            jcopy_markers_execute(&srcinfo, &dstinfo, copyoption);

            // Execute image transformation, if any
            jtransform_execute_transformation(&srcinfo, &dstinfo, src_coef_arrays,
                                              &transformoption);

            jpeg_finish_compress(&dstinfo);

            if (buf_address && buf_size)
            {
                if (outbuffer.size() < buf_size)
                {
                    memcpy(buf_address, &outbuffer[0], outbuffer.size());
                    post_progress_monitor((j_common_ptr)&dstinfo, PROGRESS_PASS_OUTPUT_FILESIZE, PROGRESS_TPASS_OPTIMIZED, outbuffer.size());
                }
                else
                {
                    post_progress_monitor((j_common_ptr)&dstinfo, PROGRESS_PASS_OUTPUT_FILESIZE, PROGRESS_TPASS_ORIGINAL, buf_size); // return the original as is
                }
            }
            else
            {
                const auto &resultBuffer = prefer_smallest && inbuffer.size() < outbuffer.size() ? inbuffer : outbuffer;
                FILE *fp = fopen(outfilename, WRITE_BINARY);
                if (!fp)
                {
                    debug_printf("%s: can't open %s for writing\n", progname, outfilename);
                    jt_exit(EXIT_FAILURE);
                }
                size_t ret = fwrite(&resultBuffer[0], resultBuffer.size(), 1, fp);
                fclose(fp);
                if (ret != 1)
                {
                    debug_printf("%s: can't write to %s\n", progname, input_filename);
                    jt_exit(EXIT_FAILURE);
                }
            }
            result = jsrcerr.num_warnings + jdsterr.num_warnings ? EXIT_WARNING : EXIT_SUCCESS;
        }
        catch (int code)
        {
            result = code;
        }

        jpeg_destroy_decompress(&srcinfo);
        jpeg_destroy_compress(&dstinfo);
        post_progress_monitor((j_common_ptr)&dstinfo, PROGRESS_PASS_EXITCODE, 0, result);
        return result;
    }
};

extern "C" __attribute__((visibility("default"))) __attribute__((used)) int jpegtran(int argc, char **argv, void *context)
{
    return JpegTran(argc, argv, context).jpegtran();
}

#include <pthread.h>

static void *jpegtran_start(void *param)
{
    JpegTran *jt = (JpegTran *)param;
    int result = jt->jpegtran();
    delete jt;
    return (void *)(size_t)result;
}

extern "C" __attribute__((visibility("default"))) __attribute__((used)) int jpegtran_threaded(int argc, char **argv, void *context)
{
    JpegTran *jt = new JpegTran(argc, argv, context);
    pthread_t t;
    if (pthread_create(&t, NULL, jpegtran_start, (void *)jt) != 0)
    {
        notify_progress(context, PROGRESS_PASS_EXITCODE, -1, -1); // error
        return -1;
    }
    return 0;
}
