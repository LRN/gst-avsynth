/* GStreamer
 *
 * volume.c: sample application to change the volume of a pipeline
 *
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gst/video/video.h>

/* global pointer for the scale widget */
GtkWidget *elapsed;
GtkWidget *scale;
GstFormat fmt = GST_FORMAT_DEFAULT;
gint64 duration;

#ifndef M_LN10
#define M_LN10 (log(10.0))
#endif

void
handoff (GstElement *fakesink, GstBuffer *buffer, GstPad *pad, gpointer user_data)
{
  gchar *filename;
  static gint64 counter = 0;
  gint width, height;
  GdkPixbuf *pb;
  GstCaps *caps;
  GstStructure *str;
  guint size;
  caps = gst_buffer_get_caps (buffer);
  size = gst_caps_get_size (caps);

  g_print ("Dumping frame %" G_GUINT64_FORMAT "\n", GST_BUFFER_OFFSET (buffer));
/*
  filename = g_strdup_printf ("f:/dump/counter(%08" G_GINT64_FORMAT ")_frame(%08" G_GUINT64_FORMAT ").png", counter++, GST_BUFFER_OFFSET (buffer));

  if (size == 1)
  {
    gchar *sstr;
    str = gst_caps_get_structure (caps, 0);

    sstr = gst_structure_to_string (str);
    g_print ("Caps: %s\n", sstr);
    g_free (sstr);

    if (g_utf8_collate (gst_structure_get_name (str), "video/x-raw-rgb") == 0)
    {
      GstVideoFormat fmt;
      gst_video_format_parse_caps (caps, &fmt, &width, &height);

      if (fmt == GST_VIDEO_FORMAT_RGBx || fmt == GST_VIDEO_FORMAT_RGBA || fmt == GST_VIDEO_FORMAT_RGB)
      {
        pb = gdk_pixbuf_new_from_data (GST_BUFFER_DATA (buffer), GDK_COLORSPACE_RGB, fmt == GST_VIDEO_FORMAT_RGBA, 8, width, height, gst_video_format_get_row_stride (fmt, 0, width), NULL, NULL);

        gdk_pixbuf_save (pb, filename, "png", NULL, NULL);

        g_object_unref (pb);
      }
    }
  }
  g_free (filename);
*/  
  gst_caps_unref (caps);
  if (duration == 0)
  {
    if (gst_element_query_duration (fakesink, &fmt , &duration))
      gtk_range_set_range (GTK_RANGE (scale), 0, duration);
  }
/*
  {
    gchar *newlabel = g_strdup_printf ("%" G_GUINT64_FORMAT, GST_BUFFER_OFFSET (buffer));
    gtk_label_set_text (GTK_LABEL (elapsed), newlabel);
    g_free (newlabel);
  }
*/
  //gtk_range_set_value (GTK_RANGE (scale), GST_BUFFER_OFFSET (buffer));
}

static void
value_changed_callback (GtkWidget * widget, GstElement * fakesink)
{
  gdouble value;
  gint64 pos;

  value = gtk_range_get_value (GTK_RANGE (widget));
  g_print ("Frame: %f\n", value);
  //g_object_set (, "volume", level, NULL);
  pos = value;
  gst_element_seek_simple (fakesink, GST_FORMAT_DEFAULT, GST_SEEK_FLAG_NONE, pos);
}

static void
setup_gui (GstElement * fakesink)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label, *hbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);

  vbox = gtk_vbox_new (TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* elapsed widget */
  hbox = gtk_hbox_new (TRUE, 0);
  label = gtk_label_new ("Frame");
  elapsed = gtk_label_new ("0");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), elapsed, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  /* volume */
  hbox = gtk_hbox_new (TRUE, 0);
  label = gtk_label_new ("Seek");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  
  scale = gtk_hscale_new_with_range (0, 1, 1.0);
  gtk_range_set_value (GTK_RANGE (scale), 0.0);
  gtk_widget_set_size_request (scale, 100, -1);
  gtk_box_pack_start (GTK_BOX (hbox), scale, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  g_signal_connect (scale, "value-changed",
      G_CALLBACK (value_changed_callback), fakesink);

  gtk_widget_show_all (GTK_WIDGET (window));
}

static void
message_received (GstBus * bus, GstMessage * message, GstPipeline * pipeline)
{
  const GstStructure *s;

  s = gst_message_get_structure (message);
  g_print ("message from \"%s\" (%s): ",
      GST_STR_NULL (GST_ELEMENT_NAME (GST_MESSAGE_SRC (message))),
      gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
  if (s) {
    gchar *sstr;

    sstr = gst_structure_to_string (s);
    g_print ("%s\n", sstr);
    g_free (sstr);
  } else {
    g_print ("no message details\n");
  }
}

static void
eos_message_received (GstBus * bus, GstMessage * message,
    GstPipeline * pipeline)
{
  message_received (bus, message, pipeline);
  gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
  GstStateChangeReturn ret;
  GstElement *pipeline = NULL;

#ifndef GST_DISABLE_PARSE
  GError *error = NULL;
#endif
  GstElement *fakesink;
  GstBus *bus;

#ifdef GST_DISABLE_PARSE
  g_print ("GStreamer was built without pipeline parsing capabilities.\n");
  g_print
      ("Please rebuild GStreamer with pipeline parsing capabilities activated to use this example.\n");
  return 1;
#else
  gst_init (&argc, &argv);
  gtk_init (&argc, &argv);

  pipeline = gst_parse_launchv ((const gchar **) &argv[1], &error);
  if (error) {
    g_print ("pipeline could not be constructed: %s\n", error->message);
    g_print ("Please give a complete pipeline  with a 'fakesink' element.\n");
    g_print ("Example: uridecodebin uri=file:///usr/file.avi ! ffmpegcolorspace ! fakesink\n");
    g_error_free (error);
    return 1;
  }
#endif
  fakesink = gst_bin_get_by_name (GST_BIN (pipeline), "fakesink0");
  if (fakesink == NULL) {
    g_print ("Please give a pipeline with a 'fakesink' element in it\n");
    return 1;
  }

  /* setup message handling */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch_full (bus, G_PRIORITY_HIGH);
  g_signal_connect (bus, "message", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::error", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::warning", (GCallback) message_received,
      pipeline);
  g_signal_connect (bus, "message::eos", (GCallback) eos_message_received,
      pipeline);
  g_signal_connect (fakesink, "handoff", (GCallback) handoff,
      pipeline);
  g_object_set (G_OBJECT (fakesink), "signal-handoffs", TRUE, NULL);

  setup_gui (fakesink);

  if ((ret = gst_element_set_state (pipeline, GST_STATE_PLAYING)) == GST_STATE_CHANGE_ASYNC)
  {
    GstState state;
    GstState pending;
    while ((ret = gst_element_get_state (pipeline, &state, &pending, GST_CLOCK_TIME_NONE)) == GST_STATE_CHANGE_ASYNC);
    if (ret != GST_STATE_CHANGE_SUCCESS)
      return 0;
  }
  else if (ret != GST_STATE_CHANGE_SUCCESS)
    return 0;

  /* setup GUI */
  if (gst_element_query_duration (fakesink, &fmt , &duration))
    gtk_range_set_range (GTK_RANGE (scale), 0, duration);
  else
    return 0;

  /* go to main loop */
//  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  gtk_main ();
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  return 0;
}
