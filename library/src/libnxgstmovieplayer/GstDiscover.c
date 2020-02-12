//------------------------------------------------------------------------------
// Refer to basic tutorial 9
//------------------------------------------------------------------------------
#include <string.h>
#include <gst/pbutils/pbutils.h>

#include "GstDiscover.h"
#include "NX_GstLog.h"
#define LOG_TAG "[GstDiscover]"

//#define ASYNC_DISCOVER

/* Structure to contain all our information, so we can pass it around */
typedef struct _DiscoverData {
    GstDiscoverer *discoverer;
    GMainLoop *loop;
} DiscoverData;

/* Print a tag in a human-readable format (name: value) */
static void print_tag_foreach (const GstTagList *tags, const gchar *tag, gpointer user_data)
{
    FUNC_IN();

    GValue val = { 0, };
    gchar *str;
    gint depth = GPOINTER_TO_INT(user_data);

    gst_tag_list_copy_value(&val, tags, tag);

    if (G_VALUE_HOLDS_STRING(&val))
    {
        str = g_value_dup_string(&val);
    }
    else
    {
        str = gst_value_serialize(&val);
    }

    NXGLOGI("%*s%s: %s", 2 * depth, " ", gst_tag_get_nick(tag), str);

    g_free(str);
    g_value_unset(&val);

    FUNC_OUT();
}

/* Print information regarding a stream */
static void print_stream_info (GstDiscovererStreamInfo *info, gint depth,
                                struct GST_MEDIA_INFO* pMediaInfo)
{
    FUNC_IN();

    gchar *desc = NULL;
    GstCaps *caps;

    // TODO: Need to display the log after fixing the crash when playing NTT DoCoMo
    //const gchar *stream_type = gst_discoverer_stream_info_get_stream_type_nick(info);
    caps = gst_discoverer_stream_info_get_caps(info);
    if (caps)
    {
        if (gst_caps_is_fixed(caps))
            desc = gst_pb_utils_get_codec_description(caps);
        else
            desc = gst_caps_to_string(caps);

        gst_caps_unref(caps);
    }

    // TODO: Need to display the log after fixing the crash when playing NTT DoCoMo
    //NXGLOGI("## %*s%s: %s ==> %s", 2 * depth, " ",
    //        (stream_type ? stream_type : ""), (desc ? desc : ""));

    if (desc)
    {
        g_free(desc);
        desc = NULL;
    }

    const GstTagList *tags = gst_discoverer_stream_info_get_tags(info);
    if (tags) {
        NXGLOGI ("** %*sTags:", 2 * (depth + 1), " ");
        gst_tag_list_foreach(tags, print_tag_foreach, GINT_TO_POINTER(depth + 2));
    }

    FUNC_OUT();
}

static void  get_gst_stream_info(GstDiscovererStreamInfo *sinfo, gint depth,
                                   struct GST_MEDIA_INFO* pMediaInfo)
{
    FUNC_IN();

    gchar *desc = NULL;
    GstCaps *caps;
    gint video_mpegversion = 0;
    gint audio_mpegversion = 0;

    const gchar *stream_type = gst_discoverer_stream_info_get_stream_type_nick(sinfo);
    caps = gst_discoverer_stream_info_get_caps(sinfo);
    if (caps) {
        if (gst_caps_is_fixed (caps))
            desc = gst_pb_utils_get_codec_description(caps);
        else
            desc = gst_caps_to_string (caps);
    }

    GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (structure == NULL)
    {
        NXGLOGE("Failed to get current caps");
        gst_caps_unref (caps);
        return;
    }
    gst_caps_unref (caps);

    const gchar *mime_type = gst_structure_get_name(structure);
    NXGLOGV("%*s%s: %s ==> %s",
            (2 * 1), " ", (stream_type ? stream_type : ""),
            (desc ? desc : ""), (mime_type ? mime_type : ""));
    if (desc)
    {
        g_free (desc);
        desc = NULL;
    }

    // TODO:
/*	tags = gst_discoverer_stream_info_get_tags (sinfo);
    if (tags) {
        NXGLOGI("** %s() %*sTags:", 2 * (depth + 1), " ");
        gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER (depth + 2));
    }
*/
    if (GST_IS_DISCOVERER_CONTAINER_INFO (sinfo))
    {
        pMediaInfo->n_container++;
        //gst_stream_container_information(sinfo, pMediaInfo);
        if (pMediaInfo->container_format) {
            g_free (pMediaInfo->container_format);
            pMediaInfo->container_format = NULL;
        }
        pMediaInfo->container_format = g_strdup(mime_type);
        /*gchar *container_format;
        if (tags) gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &container_format);

        NXGLOGI("n_container(%d) container_format(%s)"
                , pMediaInfo->n_container
                , pMediaInfo->container_format);

        if (container_format)
            g_free(container_format);*/
    }
    else if (GST_IS_DISCOVERER_VIDEO_INFO (sinfo))
    {
        pMediaInfo->n_video++;
        //gst_stream_video_information(sinfo, pMediaInfo);
        if (pMediaInfo->video_mime_type) {
            g_free (pMediaInfo->video_mime_type);
            pMediaInfo->video_mime_type = NULL;
        }
        pMediaInfo->video_mime_type = g_strdup(mime_type);

        GstDiscovererVideoInfo *video_info = (GstDiscovererVideoInfo *) sinfo;
        pMediaInfo->iWidth = gst_discoverer_video_info_get_width (video_info);
        pMediaInfo->iHeight = gst_discoverer_video_info_get_height (video_info);

        if ((structure != NULL) && (g_str_has_prefix(mime_type, "video/mpeg")))
        {
            gst_structure_get_int (structure, "mpegversion", &video_mpegversion);
            pMediaInfo->video_mpegversion = video_mpegversion;
        }

        NXGLOGI("n_video(%u), video_mime_type(%s)(%d), iWidth(%d), iHeight(%d)"
                , pMediaInfo->n_video, pMediaInfo->video_mime_type
                , pMediaInfo->video_mpegversion
                , pMediaInfo->iWidth, pMediaInfo->iHeight);
    }
    else if (GST_IS_DISCOVERER_AUDIO_INFO (sinfo))
    {
        pMediaInfo->n_audio++;
        //gst_stream_audio_information(sinfo, pMediaInfo);
        if (pMediaInfo->audio_mime_type) {
            g_free (pMediaInfo->audio_mime_type);
            pMediaInfo->audio_mime_type = NULL;
        }
        pMediaInfo->audio_mime_type = g_strdup(mime_type);

        if ((structure != NULL) && (g_str_has_prefix(mime_type, "audio/mpeg")))
        {
            gst_structure_get_int (structure, "mpegversion", &audio_mpegversion);
            pMediaInfo->audio_mpegversion = audio_mpegversion;
        }
        NXGLOGI("n_audio(%d) audio_mime_type(%s)(%d)",
                pMediaInfo->n_audio,
                pMediaInfo->audio_mime_type, pMediaInfo->audio_mpegversion);
    }
    else if (GST_IS_DISCOVERER_SUBTITLE_INFO (sinfo))
    {
        pMediaInfo->n_subtitle++;
        //gst_stream_subtitle_information(sinfo, pMediaInfo);
        NXGLOGI("n_subtitle(%d)", pMediaInfo->n_subtitle);
    }

    FUNC_OUT();
}

/* Print information regarding a stream and its substreams, if any */
static void print_topology(GstDiscovererStreamInfo *sinfo, gint depth,
            struct GST_MEDIA_INFO* pMediaInfo)
{
    if (!sinfo)
    {
        NXGLOGE("GstDiscovererStreamInfo is NULL");
        return;
    }

    // Print Stream Info
    print_stream_info (sinfo, depth, pMediaInfo);

    // Get Stream Info
    get_gst_stream_info(sinfo, depth, pMediaInfo);

    GstDiscovererStreamInfo *next = gst_discoverer_stream_info_get_next(sinfo);
    if (next)
    {
        print_topology(next, depth + 1, pMediaInfo);
        gst_discoverer_stream_info_unref(next);
    }
    else if (GST_IS_DISCOVERER_CONTAINER_INFO(sinfo))
    {
        GList *tmp, *streams;

        // Get Container Info from Stream Info
        streams = gst_discoverer_container_info_get_streams(GST_DISCOVERER_CONTAINER_INFO(sinfo));
        for (tmp = streams; tmp; tmp = tmp->next)
        {
            GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *) tmp->data;
            print_topology(tmpinf, depth + 1, pMediaInfo);
        }
        gst_discoverer_stream_info_list_free(streams);
    }
}

void parse_GstDiscovererInfo(GstDiscovererInfo *info, GError *err,
                        struct GST_MEDIA_INFO* pMediaInfo)
{
    const gchar *uri;
    const GstTagList *tags;
    GstDiscovererStreamInfo *sinfo;
    GstDiscovererResult result;
    gboolean isSeekable = FALSE;
    gint64 duration;

    uri = gst_discoverer_info_get_uri(info);
    result = gst_discoverer_info_get_result(info);
    switch (result) {
        case GST_DISCOVERER_URI_INVALID:
            NXGLOGE("Invalid URI '%s'", uri);
            break;
        case GST_DISCOVERER_ERROR:
            if (err != NULL)
                NXGLOGE("Discoverer error: %s",  err->message);
            else
                NXGLOGE("Discoverer error",  __FUNCTION__);
            break;
        case GST_DISCOVERER_TIMEOUT:
                NXGLOGE("Timeout");
                break;
        case GST_DISCOVERER_BUSY:
                NXGLOGE("Busy");
                break;
        case GST_DISCOVERER_MISSING_PLUGINS:{
            const GstStructure *s;
            gchar *str;

            s = gst_discoverer_info_get_misc (info);
            str = gst_structure_to_string (s);

            NXGLOGE("Missing plugins: %s", str);
            g_free (str);
            break;
        }
        case GST_DISCOVERER_OK:
            NXGLOGI("Discovered '%s'", uri);
            break;
    }

    if (result != GST_DISCOVERER_OK) {
        NXGLOGE("This URI cannot be played");
        return;
    }

    duration = gst_discoverer_info_get_duration(info);
    /* If we got no error, show the retrieved information */
    NXGLOGI("Duration: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (duration));

    tags = gst_discoverer_info_get_tags(info);
    if (tags) {
        NXGLOGI("Tags:");
        gst_tag_list_foreach (tags, print_tag_foreach, GINT_TO_POINTER(1));
    }

    isSeekable = gst_discoverer_info_get_seekable(info);
    NXGLOGI("Seekable: %s", (isSeekable ? "yes" : "no"));

    sinfo = gst_discoverer_info_get_stream_info(info);
    if (!sinfo) {
        NXGLOGE("%s(): Failed to get stream info");
        return;
    }

    NXGLOGI("Stream information:");

    print_topology(sinfo, 1, pMediaInfo);

    gst_discoverer_stream_info_unref(sinfo);

    pMediaInfo->isSeekable = isSeekable;
    pMediaInfo->iDuration = duration;
}

#ifdef ASYNC_DISCOVER
/* This function is called every time the discoverer has information regarding
 * one of the URIs we provided.*/
static void on_discovered_cb(GstDiscoverer *discoverer, GstDiscovererInfo *info,
                GError *err, /*DiscoverData *data, */struct GST_MEDIA_INFO* pMediaInfo)
{
    parse_GstDiscovererInfo(info, err, pMediaInfo);
}

/* This function is called when the discoverer has finished examining
 * all the URIs we provided.*/
static void on_finished_cb (GstDiscoverer *discoverer, DiscoverData *data)
{
    NXGLOGI("Finished discovering");

    g_main_loop_quit(data->loop);
}
#endif

static int start_discover(const char* filePath, struct GST_MEDIA_INFO *pMediaInfo)
{
    FUNC_IN();

    GError *err = NULL;
    gchar *uri = g_strconcat("file://", filePath, NULL);

    DiscoverData data;
    memset (&data, 0, sizeof (data));

    NXGLOGI("Start to discover '%s'", uri);

    /* Instantiate the Discoverer */
    data.discoverer = gst_discoverer_new(5 * GST_SECOND, &err);
    if (!data.discoverer) {
        NXGLOGI("%s(): Error creating discoverer instance: %s\n", err->message);
        g_clear_error(&err);
        return -1;
    }

#ifdef ASYNC_DISCOVER
    /* Connect to the interesting signals */
    g_signal_connect(data.discoverer, "discovered", G_CALLBACK (on_discovered_cb), &data);
    g_signal_connect(data.discoverer, "finished", G_CALLBACK (on_finished_cb), &data);

    /* Start the discoverer process (nothing to do yet) */
    gst_discoverer_start(data.discoverer);

    /* Add a request to process asynchronously the URI passed through the command line */
    if (!gst_discoverer_discover_uri_async(data.discoverer, uri))
    {
        NXGLOGI("%s(): Failed to start async discovering URI '%s'\n", uri);
        g_free (uri);
        g_object_unref (data.discoverer);
        return -1;
    }
#else
    GstDiscovererInfo *pDiscInfo = gst_discoverer_discover_uri(data.discoverer, uri, &err);
    if (!pDiscInfo)
    {
        NXGLOGI("%s(): Failed to start sync discovering URI '%s'\n", uri);
        g_free (uri);
        g_object_unref (data.discoverer);
        return -1;
    }
    parse_GstDiscovererInfo(pDiscInfo, NULL, pMediaInfo);
    gst_discoverer_info_unref(pDiscInfo);
#endif
    g_free (uri);

#ifdef ASYNC_DISCOVER
    /* Create a GLib Main Loop and set it to run, so we can wait for the signals */
    data.loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(data.loop);

    /* Stop the discoverer process */
    gst_discoverer_stop(data.discoverer);
#endif
    /* Free resources */
    g_object_unref(data.discoverer);
#ifdef ASYNC_DISCOVER
    g_main_loop_unref(data.loop);
#endif

    FUNC_OUT();

    return 0;
}

gboolean isSupportedMimeType(const gchar* mimeType)
{
    /* Quicktime, 3GP, Matroska, AVI, MPEG (vob) */
    if ((g_strcmp0(mimeType, "video/quicktime") == 0) ||
        (g_strcmp0(mimeType, "application/x-3gp") == 0) ||
        (g_strcmp0(mimeType, "video/x-matroska") == 0) ||
        (g_strcmp0(mimeType, "video/x-msvideo") == 0) ||
        (g_strcmp0(mimeType, "avidemux") == 0) ||
        (g_strcmp0(mimeType, "video/x-flv") == 0) ||
        (g_strcmp0(mimeType, "video/mpeg") == 0))
    {
        return TRUE;
    }
    else
    {
        NXGLOGE("Not supported mime-type(%s)", mimeType);
        return FALSE;
    }
}

enum NX_GST_ERROR StartDiscover(const char* pUri, struct GST_MEDIA_INFO **pInfo)
{
    enum NX_GST_ERROR ret = 0;

    NXGLOGI();

    ret = start_discover(pUri, *pInfo);
    if (ret < 0)
    {
        NXGLOGE("Failed to discover");
        return NX_GST_ERROR_DISCOVER_FAILED;
    }
    if (FALSE == isSupportedMimeType((*pInfo)->container_format))
    {
        return NX_GST_ERROR_NOT_SUPPORTED_CONTENTS;
    }

    NXGLOGI("Done to discover");

    return NX_GST_ERROR_NONE;
}

