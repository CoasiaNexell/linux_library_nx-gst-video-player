#include "CNX_Discover.h"
#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#define LOG_TAG "[NxGstVideoPlayer]"
#include <NX_Log.h>

/* Structure to contain all our information, so we can pass it around */
static GstDiscoverer *discoverer;

/* Structure to contain all our information, so we can pass it around */
typedef struct _DiscoverData {
  GstDiscoverer *discoverer;
  GMainLoop *loop;
} DiscoverData;

/* Print a tag in a human-readable format (name: value) */
static void print_tag_foreach (const GstTagList *tags, const gchar *tag, gpointer user_data) {
  GValue val = { 0, };
  gchar *str;
  gint depth = GPOINTER_TO_INT (user_data);

  gst_tag_list_copy_value (&val, tags, tag);

  if (G_VALUE_HOLDS_STRING (&val))
    str = g_value_dup_string (&val);
  else
    str = gst_value_serialize (&val);

  NXLOGI("%s(): %*s%s: %s\n", __FUNCTION__, 2 * depth, " ", gst_tag_get_nick (tag), str);
  g_free (str);

  g_value_unset (&val);
}

/* Print information regarding a stream */
static void print_stream_info (GstDiscovererStreamInfo *info, gint depth) {
  gchar *desc = NULL;
  GstCaps *caps;
  const GstTagList *tags;
  int width, height;

  caps = gst_discoverer_stream_info_get_caps (info);

  if (caps) {
    if (gst_caps_is_fixed (caps))
      desc = gst_pb_utils_get_codec_description (caps);
    else
      desc = gst_caps_to_string (caps);

    NXLOGI("%s(): start to get width/height", __FUNCTION__);
    GstStructure *s = gst_caps_get_structure(caps, 0);
      gboolean res;
      res = gst_structure_get_int (s, "width", &width);
      res |= gst_structure_get_int (s, "height", &height);
      if (!res) {
          NXLOGI("%s(): no dimensions", __FUNCTION__);
      } else {
          NXLOGI("%s(): width:%d, height:%d", __FUNCTION__, width, height);
      }
    gst_caps_unref (caps);
  }

  NXLOGI("%s(): %*s%s: %s\n",
         __FUNCTION__,
         2 * depth,
         " ",
         gst_discoverer_stream_info_get_stream_type_nick (info), (desc ? desc : ""));

  if (desc) {
    g_free (desc);
    desc = NULL;
  }

  tags = gst_discoverer_stream_info_get_tags (info);
  if (tags) {
    NXLOGI("%s(): %*sTags:", __FUNCTION__, 2 * (depth + 1), " ");
    gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (depth + 2));
  }
  NXLOGI("%s():  ** print_stream_info END **", __FUNCTION__);
}

/* Print information regarding a stream and its substreams, if any */
static void print_topology (GstDiscovererStreamInfo *info, gint depth) {
  GstDiscovererStreamInfo *next;

  if (!info)
    return;

  print_stream_info (info, depth);

  next = gst_discoverer_stream_info_get_next (info);
  if (next) {
    print_topology (next, depth + 1);
    gst_discoverer_stream_info_unref (next);
  } else if (GST_IS_DISCOVERER_CONTAINER_INFO (info)) {
    GList *tmp, *streams;

    streams = gst_discoverer_container_info_get_streams (GST_DISCOVERER_CONTAINER_INFO (info));
    for (tmp = streams; tmp; tmp = tmp->next) {
      GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
      print_topology (tmpinf, depth + 1);
    }
    gst_discoverer_stream_info_list_free (streams);
  }
}

/* This function is called every time the discoverer has information regarding
 * one of the URIs we provided.*/
static void on_discovered_cb (GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, DiscoverData *data) {
  GstDiscovererResult result;
  const gchar *uri;
  const GstTagList *tags;
  GstDiscovererStreamInfo *sinfo;

  uri = gst_discoverer_info_get_uri (info);
  result = gst_discoverer_info_get_result (info);
  switch (result) {
    case GST_DISCOVERER_URI_INVALID:
      NXLOGI("%s(): Invalid URI '%s'", __FUNCTION__, uri);
      break;
    case GST_DISCOVERER_ERROR:
      NXLOGI("%s(): Discoverer error: %s",  __FUNCTION__, err->message);
      break;
    case GST_DISCOVERER_TIMEOUT:
      NXLOGI("%s(): Timeout", __FUNCTION__);
      break;
    case GST_DISCOVERER_BUSY:
      NXLOGI("%s(): Busy", __FUNCTION__);
      break;
    case GST_DISCOVERER_MISSING_PLUGINS:{
      const GstStructure *s;
      gchar *str;

      s = gst_discoverer_info_get_misc (info);
      str = gst_structure_to_string (s);

      NXLOGI("%s(): Missing plugins: %s", __FUNCTION__, str);
      g_free (str);
      break;
    }
    case GST_DISCOVERER_OK:
      NXLOGI("%s(): Discovered '%s'", __FUNCTION__, uri);
      break;
  }

  if (result != GST_DISCOVERER_OK) {
    NXLOGE("%s(): This URI cannot be played", __FUNCTION__);
    return;
  }

  /* If we got no error, show the retrieved information */

  NXLOGI("%s(): nDuration: %" GST_TIME_FORMAT, __FUNCTION__, GST_TIME_ARGS (gst_discoverer_info_get_duration (info)));

  tags = gst_discoverer_info_get_tags (info);
  if (tags) {
    NXLOGI("%s(): Tags:", __FUNCTION__);
    gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (1));
  }

  NXLOGI("%s(): Seekable: %s", __FUNCTION__, (gst_discoverer_info_get_seekable (info) ? "yes" : "no"));
    sinfo = gst_discoverer_info_get_stream_info (info);
    if (!sinfo) {
        NXLOGE("%s(): Failed to get stream info", __FUNCTION__);
        return;
    }

  NXLOGI("%s(): Stream information:", __FUNCTION__);

  print_topology (sinfo, 1);

  gst_discoverer_stream_info_unref (sinfo);

  NXLOGI("%s(): ", __FUNCTION__);
}

/* This function is called when the discoverer has finished examining
 * all the URIs we provided.*/
static void on_finished_cb (GstDiscoverer *discoverer, DiscoverData *data) {
  NXLOGI("%s(): Finished discovering", __FUNCTION__);

  g_main_loop_quit (data->loop);
}

static int startDiscover (const char* pUri) {
  DiscoverData data;
  GError *err = NULL;

  //gst_init (&argc, &argv);
  memset (&data, 0, sizeof (data));

  NXLOGI("%s(): Discovering '%s'", __FUNCTION__, pUri);

  /* Instantiate the Discoverer */
  data.discoverer = gst_discoverer_new (5 * GST_SECOND, &err);
  if (!data.discoverer) {
    NXLOGI("%s(): Error creating discoverer instance: %s\n", __FUNCTION__, err->message);
    g_clear_error (&err);
    return -1;
  }

  /* Connect to the interesting signals */
  g_signal_connect (data.discoverer, "discovered", G_CALLBACK (on_discovered_cb), &data);
  g_signal_connect (data.discoverer, "finished", G_CALLBACK (on_finished_cb), &data);

  NXLOGI("%s(): Discovering '%s'", __FUNCTION__, pUri);
  /* Start the discoverer process (nothing to do yet) */
  gst_discoverer_start (data.discoverer);

  /* Add a request to process asynchronously the URI passed through the command line */
  if (!gst_discoverer_discover_uri_async (discoverer, pUri)) {
    NXLOGI("%s(): Failed to start discovering URI '%s'\n", __FUNCTION__, pUri);
    g_object_unref (data.discoverer);
    return -1;
  }

  /* Create a GLib Main Loop and set it to run, so we can wait for the signals */
  data.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (data.loop);

  /* Stop the discoverer process */
  gst_discoverer_stop (data.discoverer);

  /* Free resources */
  g_object_unref (data.discoverer);
  g_main_loop_unref (data.loop);

  return 0;
}

CNX_Discover::CNX_Discover()
{

}

CNX_Discover::~CNX_Discover()
{

}

void CNX_Discover::StartDiscover(const char* pUri)
{
    NXLOGI("%s(): pUri: %s", __FUNCTION__, pUri);
    startDiscover(pUri);
}
