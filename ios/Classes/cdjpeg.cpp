/*
 * cdjpeg.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * libjpeg-turbo Modifications:
 * Copyright (C) 2019, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file contains common support routines used by the IJG application
 * programs (cjpeg, djpeg, jpegtran).
 */

#include <stdio.h>
#include <stdarg.h>
#include "cdjpeg.h" /* Common decls for cjpeg/djpeg applications */
#include "cdjapi.h"
#include <ctype.h> /* to declare isupper(), tolower() */

/*
 * Optional progress monitor: display a percent-done figure on stderr.
 */

METHODDEF(void)
progress_monitor(j_common_ptr cinfo)
{
  cd_progress_ptr prog = (cd_progress_ptr)cinfo->progress;

  if (prog->max_scans != 0 && cinfo->is_decompressor)
  {
    int scan_no = ((j_decompress_ptr)cinfo)->input_scan_number;

    if (scan_no > (int)prog->max_scans)
    {
      debug_printf("Scan number %d exceeds maximum scans (%d)\n", scan_no,
                   prog->max_scans);
      jt_exit(EXIT_FAILURE);
    }
  }

  int total_passes = prog->pub.total_passes + prog->total_extra_passes;
  int percent_done =
      (int)(prog->pub.pass_counter * 100L / prog->pub.pass_limit);

  if (percent_done != prog->percent_done)
  {
    prog->percent_done = percent_done;
    notify_progress(prog->context, prog->pub.completed_passes + prog->completed_extra_passes + 1,
                    total_passes, percent_done);
  }
}

GLOBAL(void)
start_progress_monitor(j_common_ptr cinfo, cd_progress_ptr progress, void *context)
{
  /* Enable progress display, unless trace output is on */
  if (cinfo->err->trace_level == 0)
  {
    progress->pub.progress_monitor = progress_monitor;
    progress->completed_extra_passes = 0;
    progress->total_extra_passes = 0;
    progress->max_scans = 0;
    progress->percent_done = -1;
    progress->context = context;
    cinfo->progress = &progress->pub;
  }
}

GLOBAL(void)
post_progress_monitor(j_common_ptr cinfo, int pass, int totalPass, size_t percentage)
{
  cd_progress_ptr progress = (cd_progress_ptr)cinfo->progress;
  if (progress)
    notify_progress(progress->context, pass, totalPass, percentage); // finalization
}

/*
 * Case-insensitive matching of possibly-abbreviated keyword switches.
 * keyword is the constant keyword (must be lower case already),
 * minchars is length of minimum legal abbreviation.
 */

GLOBAL(boolean)
keymatch(char *arg, const char *keyword, int minchars)
{
  int ca, ck;
  int nmatched = 0;

  while ((ca = *arg++) != '\0')
  {
    if ((ck = *keyword++) == '\0')
      return FALSE;  /* arg longer than keyword, no good */
    if (isupper(ca)) /* force arg to lcase (assume ck is already) */
      ca = tolower(ca);
    if (ca != ck)
      return FALSE; /* no good */
    nmatched++;     /* count matched characters */
  }
  /* reached end of argument; fail if it's too short for unique abbrev */
  if (nmatched < minchars)
    return FALSE;
  return TRUE; /* A-OK */
}

static void error_exit(j_common_ptr cinfo)
{
  (*cinfo->err->output_message)(cinfo);
  jpeg_destroy(cinfo);
  jt_exit(EXIT_FAILURE);
}

static void output_message(j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, buffer);
  debug_print(buffer);
}

jpeg_error_mgr *debug_foward_error(jpeg_error_mgr *err)
{
  jpeg_std_error(err);

  // replaces two vital callbacks
  err->error_exit = error_exit;
  err->output_message = output_message;
  return err;
}
