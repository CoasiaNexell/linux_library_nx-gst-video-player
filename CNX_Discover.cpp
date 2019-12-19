#include "CNX_Discover.h"
#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#define LOG_TAG "[NxGstVideoPlayer]"
#include <NX_Log.h>

//#define ASYNC_DISCOVER

/* Structure to contain all our information, so we can pass it around */
typedef struct _DiscoverData {
  GstDiscoverer *discoverer;
  GMainLoop *loop;
} DiscoverData;

/* Print a tag in a human-readable format (name: value) */
static void print_tag_foreach (const GstTagList *tags, const gchar *tag, gpointer user_data)
{
	//NXLOGE("%s() ++++++++++++++++++++++++++", __FUNCTION__);
	GValue val = { 0, };
	gchar *str;
	gint depth = GPOINTER_TO_INT (user_data);

	gst_tag_list_copy_value (&val, tags, tag);

	if (G_VALUE_HOLDS_STRING (&val))
		str = g_value_dup_string (&val);
	else
		str = gst_value_serialize (&val);

	NXLOGI("%s(): %*s%s: %s", __FUNCTION__, 2 * depth, " ", gst_tag_get_nick (tag), str);

	g_free (str);
	g_value_unset (&val);
	//NXLOGE("%s() ---------------------------", __FUNCTION__);
}

/* Print information regarding a stream */
static void print_stream_info (GstDiscovererStreamInfo *info, gint depth, struct GST_MEDIA_INFO* pMediaInfo)
{
	gchar *desc = NULL;
	GstCaps *caps;
	const GstTagList *tags;
	gint width = -1;
	gint height = -1;
	const gchar *stream_type = NULL;
	gchar *dbg_msg = NULL;
	const gchar *mime_type;

	//NXLOGE("%s() ++++++++++++++++++++++++++", __FUNCTION__);
	stream_type = gst_discoverer_stream_info_get_stream_type_nick (info);

	caps = gst_discoverer_stream_info_get_caps (info);
	if (caps) {
		if (gst_caps_is_fixed (caps))
			desc = gst_pb_utils_get_codec_description (caps);
		else
			desc = gst_caps_to_string (caps);

		GstStructure *structure = gst_caps_get_structure(caps, 0);
		if (structure == NULL) {
			NXLOGE("%s() Failed to get current caps", __FUNCTION__);
			return;
		}
		mime_type = gst_structure_get_name(structure);

		gboolean res;
		res = gst_structure_get_int (structure, "width", &width);
		res |= gst_structure_get_int (structure, "height", &height);
		if (!res) {
			//NXLOGI("%s() no dimensions", __FUNCTION__);
		} else {
			pMediaInfo->iWidth = width;
			pMediaInfo->iHeight = height;
		}
		gst_caps_unref (caps);
	}

	NXLOGI("## %s() %*s%s: %s ==> %s"
		   , __FUNCTION__, 2 * depth, " "
		   , (stream_type ? stream_type : ""), (desc ? desc : "")
		   , (mime_type ? mime_type : ""));

	if (g_strcmp0(stream_type, TOPOLOGY_TYPE_CONTAINER) == 0) {
		if (desc) {
			if (pMediaInfo->container_format) {
				g_free (pMediaInfo->container_format);
				pMediaInfo->container_format = NULL;
			}
			pMediaInfo->container_format = g_strdup(mime_type);
		}
	} else if (g_strcmp0(stream_type, TOPOLOGY_TYPE_VIDEO) == 0) {
		if (desc) {
			if (pMediaInfo->video_codec) {
				g_free (pMediaInfo->video_codec);
				pMediaInfo->video_codec = NULL;
			}
			pMediaInfo->video_codec = g_strdup(mime_type);
		}
	} else if (g_strcmp0(stream_type, TOPOLOGY_TYPE_AUDIO) == 0) {
		if (desc) {
			if (pMediaInfo->audio_codec) {
				g_free (pMediaInfo->audio_codec);
				pMediaInfo->audio_codec = NULL;
			}
			pMediaInfo->audio_codec = g_strdup(mime_type);
		}
	}

	if (desc) {
		g_free (desc);
		desc = NULL;
	}

	tags = gst_discoverer_stream_info_get_tags (info);
	if (tags) {
		NXLOGI("** %s() %*sTags:", __FUNCTION__, 2 * (depth + 1), " ");
		gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (depth + 2));
	}

	/*
	// https://git.collabora.com/cgit/user/kakaroto/gst-plugins-base.git/plain/tools/gst-discoverer.c
	if (1) {
	  if (GST_IS_DISCOVERER_AUDIO_INFO (info))
		dbg_msg =
			gst_stream_audio_information_to_string (info,
			GPOINTER_TO_INT (depth) + 1);
	  else if (GST_IS_DISCOVERER_VIDEO_INFO (info))
		dbg_msg =
			gst_stream_video_information_to_string (info,
			GPOINTER_TO_INT (depth) + 1);
	  else if (GST_IS_DISCOVERER_SUBTITLE_INFO (info))
		dbg_msg =
			gst_stream_subtitle_information_to_string (info,
			GPOINTER_TO_INT (depth) + 1);
	  if (desc) {
		NXLOGE ("%s() %s", __FUNCTION__, dbg_msg);
		g_free (dbg_msg);
	  }
	*/
}

/* Print information regarding a stream and its substreams, if any */
static void print_topology (GstDiscovererStreamInfo *info, gint depth, struct GST_MEDIA_INFO* pMediaInfo)
{
	GstDiscovererStreamInfo *next;

	if (!info) {
		NXLOGE("%s() GstDiscovererStreamInfo is NULL", __FUNCTION__);
		return;
	}

	//NXLOGE("%s() ++++++++++++++++++++++++++", __FUNCTION__);
	print_stream_info (info, depth, pMediaInfo);

	next = gst_discoverer_stream_info_get_next (info);
	if (next) {
		print_topology (next, depth + 1, pMediaInfo);
		gst_discoverer_stream_info_unref (next);
	} else if (GST_IS_DISCOVERER_CONTAINER_INFO (info)) {
		GList *tmp, *streams;

		streams = gst_discoverer_container_info_get_streams (GST_DISCOVERER_CONTAINER_INFO (info));
		for (tmp = streams; tmp; tmp = tmp->next) {
			GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
			print_topology (tmpinf, depth + 1, pMediaInfo);
		}
		gst_discoverer_stream_info_list_free (streams);
	}
	//NXLOGE("%s() --------------------------", __FUNCTION__);
}

void parse_GstDiscovererInfo(GstDiscovererInfo *info, GError *err, struct GST_MEDIA_INFO* pMediaInfo)
{
	const gchar *uri;
	const GstTagList *tags;
	GstDiscovererStreamInfo *sinfo;
	GstDiscovererResult result;
	gboolean isSeekable = false;
	gint64 duration;

	uri = gst_discoverer_info_get_uri (info);
	result = gst_discoverer_info_get_result (info);
	switch (result) {
		case GST_DISCOVERER_URI_INVALID:
			NXLOGI("%s() Invalid URI '%s'", __FUNCTION__, uri);
			break;
		case GST_DISCOVERER_ERROR:
			if(err != NULL)
				NXLOGI("%s() Discoverer error: %s",  __FUNCTION__, err->message);
			else
				NXLOGI("%s() Discoverer error",  __FUNCTION__);
			break;
		case GST_DISCOVERER_TIMEOUT:
				NXLOGI("%s() Timeout", __FUNCTION__);
				break;
		case GST_DISCOVERER_BUSY:
				NXLOGI("%s() Busy", __FUNCTION__);
				break;
		case GST_DISCOVERER_MISSING_PLUGINS:{
			const GstStructure *s;
			gchar *str;

			s = gst_discoverer_info_get_misc (info);
			str = gst_structure_to_string (s);

			NXLOGI("%s() Missing plugins: %s", __FUNCTION__, str);
			g_free (str);
			break;
		}
		case GST_DISCOVERER_OK:
			NXLOGI("%s() Discovered '%s'", __FUNCTION__, uri);
			break;
	}

	if (result != GST_DISCOVERER_OK) {
		NXLOGE("%s() This URI cannot be played", __FUNCTION__);
		return;
	}

	duration = gst_discoverer_info_get_duration (info);
	/* If we got no error, show the retrieved information */
	NXLOGI("%s() Duration: %" GST_TIME_FORMAT, __FUNCTION__,
		   GST_TIME_ARGS (duration));

	tags = gst_discoverer_info_get_tags (info);
	if (tags) {
		NXLOGI("%s() Tags:", __FUNCTION__);
		gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (1));
	}

	isSeekable = gst_discoverer_info_get_seekable (info);
	NXLOGI("%s() Seekable: %s", __FUNCTION__, (isSeekable ? "yes" : "no"));

	sinfo = gst_discoverer_info_get_stream_info (info);
	if (!sinfo) {
		NXLOGE("%s(): Failed to get stream info", __FUNCTION__);
		return;
	}

	NXLOGI("%s() Stream information:", __FUNCTION__);

	print_topology (sinfo, 1, pMediaInfo);

	gst_discoverer_stream_info_unref (sinfo);

	pMediaInfo->isSeekable = isSeekable;
	pMediaInfo->iDuration = duration;
}

/* This function is called every time the discoverer has information regarding
 * one of the URIs we provided.*/
static void on_discovered_cb (GstDiscoverer *discoverer,
				GstDiscovererInfo *info, GError *err, DiscoverData *data, struct GST_MEDIA_INFO* pMediaInfo)
{
	parse_GstDiscovererInfo(info, err, pMediaInfo);
}

/* This function is called when the discoverer has finished examining
 * all the URIs we provided.*/
static void on_finished_cb (GstDiscoverer *discoverer, DiscoverData *data)
{
	NXLOGI("%s() Finished discovering", __FUNCTION__);

	g_main_loop_quit (data->loop);
}

static int start_discover (const char* pUri, struct GST_MEDIA_INFO *pMediaInfo)
{
	GError *err = NULL;
	gchar *uri = g_strconcat("file://", pUri, NULL);
	DiscoverData data;

	memset (&data, 0, sizeof (data));

	NXLOGI("%s() Start to discover '%s'", __FUNCTION__, pUri);

	/* Instantiate the Discoverer */
	data.discoverer = gst_discoverer_new (5 * GST_SECOND, &err);
	if (!data.discoverer) {
		NXLOGI("%s(): Error creating discoverer instance: %s\n", __FUNCTION__, err->message);
		g_clear_error (&err);
		return -1;
	}

#ifdef ASYNC_DISCOVER
	/* Connect to the interesting signals */
	g_signal_connect (data.discoverer, "discovered", G_CALLBACK (on_discovered_cb), &data);
	g_signal_connect (data.discoverer, "finished", G_CALLBACK (on_finished_cb), &data);

	/* Start the discoverer process (nothing to do yet) */
	gst_discoverer_start (data.discoverer);

	/* Add a request to process asynchronously the URI passed through the command line */
	if (!gst_discoverer_discover_uri_async (data.discoverer, uri)) {
		NXLOGI("%s(): Failed to start async discovering URI '%s'\n", __FUNCTION__, uri);
		g_free (uri);
		g_object_unref (data.discoverer);
		return -1;
	}
#else
	GstDiscovererInfo *pDiscInfo = gst_discoverer_discover_uri (data.discoverer, uri, &err);
	if (!pDiscInfo) {
		NXLOGI("%s(): Failed to start sync discovering URI '%s'\n", __FUNCTION__, uri);
		g_free (uri);
		g_object_unref (data.discoverer);
		return -1;
	}
	parse_GstDiscovererInfo(pDiscInfo, NULL, pMediaInfo);
	//GList *audio_list = gst_discoverer_info_get_audio_streams(pDiscInfo);
	gst_discoverer_info_unref(pDiscInfo);
#endif
	g_free (uri);

#ifdef ASYNC_DISCOVER
	/* Create a GLib Main Loop and set it to run, so we can wait for the signals */
	data.loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (data.loop);

	/* Stop the discoverer process */
	gst_discoverer_stop (data.discoverer);
#endif
	/* Free resources */
	g_object_unref (data.discoverer);
#ifdef ASYNC_DISCOVER
	g_main_loop_unref (data.loop);
#endif

	return 0;
}

CNX_Discover::CNX_Discover()
{

}

CNX_Discover::~CNX_Discover()
{

}

int CNX_Discover::StartDiscover(const char* pUri, struct GST_MEDIA_INFO *pInfo)
{
	return start_discover(pUri, pInfo);
}
