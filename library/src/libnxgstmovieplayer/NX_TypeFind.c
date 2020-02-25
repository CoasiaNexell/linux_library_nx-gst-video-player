#include <gst/gst.h>
#include "NX_GstLog.h"
#define LOG_TAG "[NX_TypeFind]"

static void
print_tag_foreach (const GstTagList * tags, const gchar * tag,
    gpointer user_data)
{
    GValue val = { 0, };
    gchar *str;
    gint depth = GPOINTER_TO_INT (user_data);

    if (!gst_tag_list_copy_value (&val, tags, tag))
        return;

    if (G_VALUE_HOLDS_STRING (&val))
        str = g_value_dup_string (&val);
    else
        str = gst_value_serialize (&val);

    NXGLOGI ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), str);
    g_free (str);

    g_value_unset (&val);
}

static void
dump_collection (GstStreamCollection * collection)
{
  guint i;
  GstTagList *tags;
  GstCaps *caps;

  for (i = 0; i < gst_stream_collection_get_size (collection); i++) {
    GstStream *stream = gst_stream_collection_get_stream (collection, i);
    NXGLOGI (" Stream %u type %s flags 0x%x\n", i,
        gst_stream_type_get_name (gst_stream_get_stream_type (stream)),
        gst_stream_get_stream_flags (stream));
    NXGLOGI ("  ID: %s\n", gst_stream_get_stream_id (stream));

    caps = gst_stream_get_caps (stream);
    if (caps) {
      gchar *caps_str = gst_caps_to_string (caps);
      NXGLOGI ("  caps: %s\n", caps_str);
      g_free (caps_str);
      gst_caps_unref (caps);
    }

    tags = gst_stream_get_tags (stream);
    if (tags) {
      NXGLOGI ("  tags:\n");
      gst_tag_list_foreach (tags, print_tag_foreach, GUINT_TO_POINTER (3));
      gst_tag_list_unref (tags);
    }
  }
}

static gboolean
my_bus_callback (GstBus * bus, GstMessage * message, gpointer data)
{
    NXGLOGI("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

    GMainLoop *loop = (GMainLoop *)data;
    switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ERROR:{
            GError *err;
            gchar *debug;

            gst_message_parse_error (message, &err, &debug);
            NXGLOGE("Error: %s\n", err->message);
            g_error_free (err);
            g_free (debug);

            g_main_loop_quit (loop);
            break;
        }
        case GST_MESSAGE_EOS:
            /* end-of-stream */
            g_main_loop_quit (loop);
            break;
        case GST_MESSAGE_STREAM_COLLECTION:
        {
            GstStreamCollection *collection = NULL;
            GstObject *src = GST_MESSAGE_SRC (message);

            gst_message_parse_stream_collection (message, &collection);
            if (collection) {
                NXGLOGI ("Got a collection from %s:\n",
                    src ? GST_OBJECT_NAME (src) : "Unknown");
                dump_collection (collection);
            }
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

static gboolean
idle_exit_loop (gpointer data)
{
    g_main_loop_quit ((GMainLoop *) data);

    /* once */
    return FALSE;
}

static void
cb_typefound (GstElement *typefind,
          guint       probability,
          GstCaps    *caps,
          gpointer    data)
{
    GMainLoop *loop = data;
    gchar *type;

    type = gst_caps_to_string (caps);
    NXGLOGI("Media type %s found, probability %d%%\n", type, probability);
    g_free (type);

    /* since we connect to a signal in the pipeline thread context, we need
    * to set an idle handler to exit the main loop in the mainloop context.
    * Normally, your app should not need to worry about such things. */
    g_idle_add (idle_exit_loop, loop);
}

gint start_typefind (const char* filePath)
{
    GMainLoop *loop;
    GstElement *pipeline, *filesrc, *typefind, *fakesink;
    GstBus *bus;

    /* init GStreamer */
    if(!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }
    loop = g_main_loop_new (NULL, FALSE);

    NXGLOGI("create a new pipeline to hold the elements");
    pipeline = gst_pipeline_new ("pipe");

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, my_bus_callback, loop);
    gst_object_unref (bus);

    NXGLOGI("create file source and typefind element");
    filesrc = gst_element_factory_make ("filesrc", "source");
    g_object_set (G_OBJECT (filesrc), "location", filePath, NULL);
    typefind = gst_element_factory_make ("typefind", "typefinder");
    g_signal_connect (typefind, "have-type", G_CALLBACK (cb_typefound), loop);
    fakesink = gst_element_factory_make ("fakesink", "sink");

    NXGLOGI("Add elements");
    gst_bin_add_many (GST_BIN (pipeline), filesrc, typefind, fakesink, NULL);
    gst_element_link_many (filesrc, typefind, fakesink, NULL);
    gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
    g_main_loop_run (loop);

    NXGLOGI("unset");
    gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (pipeline));

    return 0;
}

static void on_pad_added_demux(GstElement *element, 
                        GstPad *pad, gpointer data)
{
    NXGLOGI();
}

#include <gst/mpegts/mpegts.h>
static void
advertise_service (GstElement * mux)
{
  GstMpegtsSDTService *service;
  GstMpegtsSDT *sdt;
  GstMpegtsDescriptor *desc;
  GstMpegtsSection *section;

    NXGLOGI();
  sdt = gst_mpegts_sdt_new ();

  sdt->actual_ts = TRUE;
  sdt->transport_stream_id = 42;

  service = gst_mpegts_sdt_service_new ();
  service->service_id = 42;
  service->running_status = GST_MPEGTS_RUNNING_STATUS_RUNNING + service->service_id;
      //GstMpegtsRunningStatus(GST_MPEGTS_RUNNING_STATUS_RUNNING + service->service_id);
  service->EIT_schedule_flag = FALSE;
  service->EIT_present_following_flag = FALSE;
  service->free_CA_mode = FALSE;

  desc = gst_mpegts_descriptor_from_dvb_service
      (GST_DVB_SERVICE_DIGITAL_TELEVISION, "some-service", NULL);

  g_ptr_array_add (service->descriptors, desc);
  g_ptr_array_add (sdt->services, service);

  section = gst_mpegts_section_from_sdt (sdt);
  NXGLOGI("pid(%04x) section_type(%d)", section->pid, section->section_type);
  gst_mpegts_section_send_event (section, mux);
  gst_mpegts_section_unref (section);
}


static void demuxer_notify_pat_info (GObject *obj, GParamSpec *pspec, gpointer data)
{
    NXGLOGI("############################");

    GValueArray *patinfo = NULL;
    GValue * value = NULL;
    GObject *entry = NULL;
    guint program, pid;
    gint i;
    //TypeFindSt *ty_handle = data;

    g_object_get (obj, "pat-info", &patinfo, NULL);

    NXGLOGI("program total number = %d", patinfo->n_values);

    g_print ("PAT: entries: %d\n", patinfo->n_values);  
    for (i = 0; i < (gint)patinfo->n_values; i++) {
        value = g_value_array_get_nth (patinfo, (guint)i);
        entry = (GObject*) g_value_get_object (value);
        g_object_get (entry, "program-number", &program, NULL);
        g_object_get (entry, "pid", &pid, NULL);
        g_print ("    program: %04x pid: %04x\n", program, pid);
        //ty_handle->TymediaInfo->program_no[i] = program;
    }
}

gint find_avcodec_num_ps(const char* filePath)
{
    GMainLoop *loop;
    GstElement *pipeline, *filesrc, *demux, *decodebin, *typefind, *fakesink;
    GstBus *bus;

    // init GStreamer
    if(!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }
    loop = g_main_loop_new (NULL, FALSE);

    NXGLOGI("create a new pipeline to hold the elements");
    pipeline = gst_pipeline_new ("pipe");

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, my_bus_callback, loop);
    gst_object_unref (bus);

    NXGLOGI("create file source and typefind element\n");
    filesrc = gst_element_factory_make ("filesrc", "source");
    g_object_set (G_OBJECT (filesrc), "location", filePath, NULL);

    demux = gst_element_factory_make ("tsdemux", "tsdemux");
    advertise_service(demux);
    g_signal_connect (demux, "pad-added", G_CALLBACK(on_pad_added_demux), NULL);
    g_signal_connect (G_OBJECT(demux), "notify::pat-info", (GCallback)demuxer_notify_pat_info, NULL);
//    typefind = gst_element_factory_make ("typefind", "typefinder");
//    g_signal_connect (typefind, "have-type", G_CALLBACK (cb_typefound), loop);

    fakesink = gst_element_factory_make ("fakesink", "sink");

    // Add elements
    gst_bin_add_many (GST_BIN (pipeline), filesrc, demux, fakesink, NULL);
    gst_element_link_many (filesrc, demux, fakesink, NULL);
    gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);
    g_main_loop_run (loop);

    // unset
    gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (pipeline));

    return 0;
}