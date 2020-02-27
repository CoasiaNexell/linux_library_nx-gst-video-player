#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

#include "NX_TypeFind.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NX_TypeFind]"

typedef struct TypeFindSt {
    GMainLoop *loop;
    GstBus *bus;
    GstElement *pipeline;
    GstElement *filesrc;
    GstElement *typefind;
    GstElement *fakesink;
    CONTAINER_TYPE  container_type;
} TypeFindSt;

static gboolean
idle_exit_loop (gpointer data)
{
    g_main_loop_quit ((GMainLoop *) data);

    /* once */
    return FALSE;
}

static gboolean
my_bus_callback (GstBus * bus, GstMessage * msg, gpointer data)
{
    NXGLOGI("Got %s msg\n", GST_MESSAGE_TYPE_NAME (msg));

    GMainLoop *loop = (GMainLoop *)data;
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:{
            GError *err;
            gchar *debug;

            gst_message_parse_error(msg, &err, &debug);
            NXGLOGE("Error: %s\n", err->message);
            g_error_free (err);
            g_free (debug);

            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_EOS:
            /* end-of-stream */
            g_main_loop_quit(loop);
            break;
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
            NXGLOGI("Element '%s' changed state from  '%s' to '%s'"
                       , GST_OBJECT_NAME (msg->src)
                       , gst_element_state_get_name (old_state)
                       , gst_element_state_get_name (new_state));
            break;
        }
        default:
            /* unhandled message */
            break;
    }

    /* we want to be notified again the next time there is a message
    * on the bus, so returning TRUE (FALSE means we want to stop watching
    * for messages on the bus and our callback should not be called again)
    */
    return TRUE;
}

static void
cb_typefound (GstElement *typefind,
          guint       probability,
          GstCaps    *caps,
          gpointer    data)
{
    TypeFindSt *handle = (TypeFindSt *)data;

    gchar *type = gst_caps_to_string (caps);
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mime_type = gst_structure_get_name(structure);
    handle->container_type = get_container_type(mime_type);
    NXGLOGI("container_type (%d) Media type %s found, probability %d%%",
            handle->container_type, type, probability);

    g_free (type);

    /* since we connect to a signal in the pipeline thread context, we need
    * to set an idle handler to exit the main loop in the mainloop context.
    * Normally, your app should not need to worry about such things. */
    g_idle_add (idle_exit_loop, handle->loop);
}

gint start_typefind (const char* filePath, CONTAINER_TYPE *type)
{
    // Initialize struct 'TypeFindSt'
    TypeFindSt handle;
    memset(&handle, 0, sizeof(TypeFindSt));
    handle.container_type = *type;

    NXGLOGI();

    // init GStreamer
    if(!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }
    handle.loop = g_main_loop_new (NULL, FALSE);

    // Create a new pipeline to hold the elements
    handle.pipeline = gst_pipeline_new("pipe");

    handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
    gst_bus_add_watch (handle.bus, my_bus_callback, handle.loop);
    gst_object_unref (handle.bus);

    // Create file source and typefind element
    handle.filesrc = gst_element_factory_make ("filesrc", "source");
    g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);
    handle.typefind = gst_element_factory_make ("typefind", "typefinder");
    g_signal_connect (handle.typefind, "have-type", G_CALLBACK (cb_typefound), &handle);
    handle.fakesink = gst_element_factory_make ("fakesink", "sink");

    // Add & link elements
    gst_bin_add_many (GST_BIN (handle.pipeline), handle.filesrc, handle.typefind, handle.fakesink, NULL);
    gst_element_link_many (handle.filesrc, handle.typefind, handle.fakesink, NULL);

    // Set the state to PLAYING
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_PLAYING);
    g_main_loop_run (handle.loop);

    // "Release"
    *type = handle.container_type;
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (handle.pipeline));

    return 0;
}
