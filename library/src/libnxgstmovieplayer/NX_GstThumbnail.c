/* GStreamer snapshot example
 * Copyright (C) <2007> Wim Taymans <wim.taymans@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <stdlib.h>
#include "NX_DbgMsg.h"

#define LOG_TAG "[LibGst][Thumbnail]"
#include <NX_Log.h>

const char *
makeThumbnail(const gchar *uri, gint64 pos_msec, gint width)
{
  GstElement *pipeline, *sink;
  gint video_width, video_height;
  GstSample *sample;
  gchar *descr;
  GError *error = NULL;
  GdkPixbuf *pixbuf;
  gint64 duration, position;
  GstStateChangeReturn ret;
  gboolean res;
  GstMapInfo map;
  gchar *caps;
  gchar* type = "jpeg";
  const char* filepath = "/nexell/daudio/NxGstVideoPlayer/snapshot.jpg";

  FUNC_IN();

  NXLOGI("%s uri(%s), pos_msec(%lld), width(%d)", __FUNCTION__, uri, pos_msec, width);

  gboolean isGstInitialized = gst_is_initialized();
  NXLOGI("%s isGstInitialized(%s)", __FUNCTION__, isGstInitialized?"initialized":"uninitialized");
  if (!isGstInitialized)
  {
    gst_init(NULL, NULL);
  }

  // caps filter
  caps = g_strdup_printf("video/x-raw,format=RGB,width=%d,pixel-aspect-ratio=1/1", width);
  NXLOGI("%s caps(%s)", __FUNCTION__, caps);

  // The pipeline for gst-launch-1.0
  descr = g_strdup_printf ("uridecodebin uri=file://%s ! videoconvert ! videoscale ! appsink name=sink caps=%s", uri, caps);
NXLOGI("%s descr(%s)", __FUNCTION__, descr);
  pipeline = gst_parse_launch (descr, &error);

  g_free(caps);
  g_free(descr);

  if (error != NULL) {
    NXLOGE("could not construct pipeline: %s", error->message);
    g_error_free (error);
    return "";
  }

  /* get sink */
  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");

  /* set to PAUSED to make the first frame arrive in the sink */
  ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
  switch (ret) {
    case GST_STATE_CHANGE_FAILURE:
      NXLOGE("failed to play the file");
      return "";
    case GST_STATE_CHANGE_NO_PREROLL:
      /* for live sources, we need to set the pipeline to PLAYING before we can
        * receive a buffer. We don't do that yet */
      NXLOGE("live sources not supported yet");
      return "";
    default:
      break;
  }

  /* This can block for up to 5 seconds. If your machine is really overloaded,
    * it might time out before the pipeline prerolled and we generate an error. A
    * better way is to run a mainloop and catch errors there. */
  ret = gst_element_get_state (pipeline, NULL, NULL, 5 * GST_SECOND);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    NXLOGE("failed to play the file");
    return "";
  }

  /* get the duration */
  gst_element_query_duration (pipeline, GST_FORMAT_TIME, &duration);
  position = pos_msec * 1000 * 1000;

  /* seek to the a position in the file. Most files have a black first frame so
    * by seeking to somewhere else we have a bigger chance of getting something
    * more interesting. An optimisation would be to detect black images and then
    * seek a little more */
  gst_element_seek_simple (pipeline, GST_FORMAT_TIME,
      GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH, position);

  /* get the preroll buffer from appsink, this block untils appsink really
    * prerolls */
  g_signal_emit_by_name (sink, "pull-preroll", &sample, NULL);
  gst_object_unref (sink);

  /* if we have a buffer now, convert it to a pixbuf. It's possible that we
    * don't have a buffer because we went EOS right away or had an error. */
  if (sample) {
    GstBuffer *buffer;
    GstCaps *caps;
    GstStructure *s;

    /* get the snapshot buffer format now. We set the caps on the appsink so
      * that it can only be an rgb buffer. The only thing we have not specified
      * on the caps is the height, which is dependent on the pixel-aspect-ratio
      * of the source material */
    caps = gst_sample_get_caps (sample);
    if (!caps) {
      NXLOGE("could not get snapshot format");
      return "";
    }
    s = gst_caps_get_structure (caps, 0);

    /* we need to get the final caps on the buffer to get the size */
    res = gst_structure_get_int (s, "width", &video_width);
    res |= gst_structure_get_int (s, "height", &video_height);
    if (!res) {
      NXLOGE("could not get snapshot dimension");
      return "";
    }

    /* create pixmap from buffer and save, gstreamer video buffers have a stride
      * that is rounded up to the nearest multiple of 4 */
    buffer = gst_sample_get_buffer (sample);
    gst_buffer_map (buffer, &map, GST_MAP_READ);
    pixbuf = gdk_pixbuf_new_from_data (map.data,
        GDK_COLORSPACE_RGB, FALSE, 8, video_width, video_height,
        GST_ROUND_UP_4 (video_width * 3), NULL, NULL);

    /* save the pixbuf */
    gdk_pixbuf_save (pixbuf, filepath, type, &error, NULL);
    gst_buffer_unmap (buffer, &map);
  } else {
    NXLOGE("could not make snapshot");
  }

  /* cleanup and exit */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  FUNC_OUT();

  NXLOGI("%s() snapshot filepath: %s", __FUNCTION__, filepath);

  return filepath;
}