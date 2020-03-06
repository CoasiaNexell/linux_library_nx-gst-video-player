#include <stdio.h>
#include <gst/gst.h>
#include "NX_GstLog.h"
#define LOG_TAG "[NX_StreamParser]"

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData
{
    GstElement *playbin;          /* Our one and only element */

    gint n_video;                 /* Number of embedded video streams */
    gint n_audio;                 /* Number of embedded audio streams */
    gint n_text;                  /* Number of embedded subtitle streams */

    gint current_video;           /* Currently playing video stream */
    gint current_audio;           /* Currently playing audio stream */
    gint current_text;            /* Currently playing subtitle stream */

    GMainLoop *main_loop;         /* GLib's Main Loop */
} CustomData;

/* playbin flags */
typedef enum
{
    GST_PLAY_FLAG_VIDEO = (1 << 0),       /* We want video output */
    GST_PLAY_FLAG_AUDIO = (1 << 1),       /* We want audio output */
    GST_PLAY_FLAG_TEXT = (1 << 2)           /* We want subtitle output */
} GstPlayFlags;

/* Forward definition for the message and keyboard processing functions */
static gboolean handle_message (GstBus * bus, GstMessage * msg,
            CustomData * data);

int start_parsing(const char* filePath)
{
    CustomData data;
    GstBus *bus;
    GstStateChangeReturn ret;
    gint flags;
    gchar *uri;

    NXGLOGI("START");

    /* Initialize GStreamer */
    gst_init (NULL, NULL);

    /* Create the elements */
    data.playbin = gst_element_factory_make ("playbin", "playbin");

    if (!data.playbin) {
    NXGLOGE("Not all elements could be created.");
        return -1;
    }
    if (gst_uri_is_valid (filePath))
    uri = g_strdup (filePath);
    else
    uri = gst_filename_to_uri (filePath, NULL);

    /* Set the URI to play */
    g_object_set (data.playbin, "uri", uri, NULL);
    g_free(uri);

    /* Set flags to show Audio and Video but ignore Subtitles */
    g_object_get (data.playbin, "flags", &flags, NULL);
    flags |= GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO;
    flags &= ~GST_PLAY_FLAG_TEXT;
    g_object_set (data.playbin, "flags", flags, NULL);

    /* Add a bus watch, so we get notified when a message arrives */
    bus = gst_element_get_bus (data.playbin);
    gst_bus_add_watch (bus, (GstBusFunc) handle_message, &data);

    /* Start playing */
    ret = gst_element_set_state (data.playbin, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        NXGLOGE("Unable to set the pipeline to the playing state.");
        gst_object_unref (data.playbin);
    return -1;
    }

    /* Create a GLib Main Loop and set it to run */
    data.main_loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (data.main_loop);

    /* Free resources */
    g_main_loop_unref (data.main_loop);
    //g_io_channel_unref (io_stdin);
    gst_object_unref (bus);
    gst_element_set_state (data.playbin, GST_STATE_NULL);
    gst_object_unref (data.playbin);
    return 0;
}

/* Extract some metadata from the streams and print it on the screen */
static void
analyze_streams (CustomData * data)
{
    gint i;
    GstTagList *tags;
    gchar *str;
    guint rate;

    /* Read some properties */
    g_object_get (data->playbin, "n-video", &data->n_video, NULL);
    g_object_get (data->playbin, "n-audio", &data->n_audio, NULL);
    g_object_get (data->playbin, "n-text", &data->n_text, NULL);

    NXGLOGI ("%d video stream(s), %d audio stream(s), %d text stream(s)\n",
    data->n_video, data->n_audio, data->n_text);

    NXGLOGI ("\n");
    for (i = 0; i < data->n_video; i++) {
        tags = NULL;
        /* Retrieve the stream's video tags */
        g_signal_emit_by_name (data->playbin, "get-video-tags", i, &tags);
        if (tags) {
            NXGLOGI ("video stream %d", i);
            gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str);
            NXGLOGI ("  codec: %s", str ? str : "unknown");
            g_free (str);
            gst_tag_list_unref (tags);
        }
    }

    NXGLOGI ("\n");
    for (i = 0; i < data->n_audio; i++) {
        tags = NULL;
        /* Retrieve the stream's audio tags */
        g_signal_emit_by_name (data->playbin, "get-audio-tags", i, &tags);
        if (tags) {
            NXGLOGI ("audio stream %d:", i);
            if (gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str)) {
                NXGLOGI ("  codec: %s", str);
                g_free (str);
            }
            if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
                NXGLOGI ("  language: %s", str);
                g_free (str);
            }
            if (gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate)) {
                NXGLOGI ("  bitrate: %d", rate);
            }
            gst_tag_list_unref (tags);
        }
    }

    NXGLOGI ("\n");
    for (i = 0; i < data->n_text; i++) {
        tags = NULL;
        /* Retrieve the stream's subtitle tags */
        g_signal_emit_by_name (data->playbin, "get-text-tags", i, &tags);
        if (tags) {
            NXGLOGI ("subtitle stream %d", i);
            if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
                NXGLOGI ("  language: %s", str);
                g_free (str);
            }
            gst_tag_list_unref (tags);
        }
    }

    g_object_get (data->playbin, "current-video", &data->current_video, NULL);
    g_object_get (data->playbin, "current-audio", &data->current_audio, NULL);
    g_object_get (data->playbin, "current-text", &data->current_text, NULL);

    NXGLOGI ("\n");
    NXGLOGI("Currently playing video stream %d, audio stream %d and text stream %d",
            data->current_video, data->current_audio, data->current_text);
}

/* Process messages from GStreamer */
static gboolean
handle_message (GstBus * bus, GstMessage * msg, CustomData * data)
{
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error (msg, &err, &debug_info);
            g_printerr ("Error received from element %s: %s\n",
            GST_OBJECT_NAME (msg->src), err->message);
            g_printerr ("Debugging information: %s\n",
            debug_info ? debug_info : "none");
            g_clear_error (&err);
            g_free (debug_info);
            g_main_loop_quit (data->main_loop);
            break;
        case GST_MESSAGE_EOS:
            NXGLOGI ("End-Of-Stream reached.\n");
            g_main_loop_quit (data->main_loop);
            break;
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (msg, &old_state, &new_state,
                        &pending_state);
            if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
                if (new_state == GST_STATE_PLAYING) {
                /* Once we are in the playing state, analyze the streams */
                analyze_streams (data);
                }
            }
        }
        break;
        default:
        break;
    }

    /* We want to keep receiving messages */
    return TRUE;
}