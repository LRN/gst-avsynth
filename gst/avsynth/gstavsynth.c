/*
 * GstAVSynth:
 * Copyright (C) 2009 LRN <lrn1986 _at_ gmail _dot_ com>
 *
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
 * http://www.gnu.org/copyleft/gpl.html .
 */

/**
 * SECTION:element-avsynth
 *
 * AVSynth is a wrapper that calls various AviSynth filters. As such, it does
 * nothing by itself.
 *
 * <refsect2>
 * <title>Launching sample AVSynth filter (overlays part of video stream#1 over video stream#0)</title>
 * |[
 * gst-launch-0.10 avsynth_SimpleSample name=filter SIZE=100 ! glimagesink videotestsrc ! filter.sink0 videotestsrc ! filter.sink1
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#if HAVE_ORC
#include <orc/orcprogram.h>
#endif

#include <liboil/liboil.h>

#include "gstavsynth.h"

GST_DEBUG_CATEGORY (gst_avsynth_debug);

#ifdef G_OS_WIN32
#include <windows.h>
static HMODULE gst_dll_handle;

BOOL WINAPI
DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
  if (fdwReason == DLL_PROCESS_ATTACH)
    gst_dll_handle = (HMODULE) hinstDLL;
  return TRUE;
}

#endif

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
avsynth_init (GstPlugin * avsynth)
{
  gchar *plugindirs = NULL;
  gboolean result = TRUE;
  GST_DEBUG_CATEGORY_INIT (gst_avsynth_debug, "avsynth",
      0, "AviSynth filter wrapper");

  plugindirs = g_strdup_printf ("%s", AVSYNTHPLUGINDIR);
#ifdef G_OS_WIN32
  {
    gchar *dir;
    gchar *lib_dir;
    gchar *plug_dir;
    gchar *oldplugindirs = plugindirs;

/*  FIXME: get gst_dll_handle from libgstreamer */
    plug_dir = g_win32_get_package_installation_directory_of_module (
       gst_dll_handle);

    lib_dir = g_path_get_dirname (plug_dir);

    dir = g_build_filename (lib_dir, "avsynth-0.10", NULL);

    plugindirs = g_strdup_printf ("%s;%s", oldplugindirs, dir);

    g_free (oldplugindirs);
    g_free (plug_dir);
    g_free (lib_dir);
    g_free (dir);
  }
#endif
  
  gst_plugin_add_dependency_simple (avsynth, NULL, plugindirs, NULL,
      GST_PLUGIN_DEPENDENCY_FLAG_NONE);

  GST_LOG("calling gst_avsynth_video_filter_register...");
  result = result && gst_avsynth_video_filter_register (avsynth, plugindirs);
  GST_LOG("gst_avsynth_video_filter_register result is %d", result);

  g_free (plugindirs);

#if HAVE_ORC
  orc_init ();
#endif

  oil_init();

  return TRUE;
}

/* gstreamer looks for this structure to register avsynths */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "avsynth",
    "Wrapper for AviSynth filters",
    avsynth_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://gstreamer.net/"
)
