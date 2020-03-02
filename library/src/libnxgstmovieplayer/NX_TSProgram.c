/* GStreamer
 *
 * ts-parser.c: sample application to display mpeg-ts info from any pipeline
 * Copyright (C) 2013
 *           Edward Hervey <bilboed@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DUMP_DESCRIPTORS 0

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>
#include "NX_TypeFind.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NX_TSProgram]"

#define MPEGTIME_TO_GSTTIME(t) ((t) * (guint64)100000 / 9)
// gst-launch-1.0 -v filesrc location=.ts ! 
// tsdemux name=demux demux. ! queue !  decodebin ! typefind ! fakesink
typedef struct MpegTsSt {
    GMainLoop   *loop;
    GstBus      *bus;
    GstElement  *pipeline;
    GstElement  *demuxer;
    GstElement  *filesrc;
    GstElement  *video_queue;
    GstElement  *decodebin;
    GstElement  *typefind;
    GstElement  *fakesink;
    struct GST_MEDIA_INFO *media_info;
} MpegTsSt;

static gboolean
idle_exit_loop (gpointer data)
{
    g_main_loop_quit ((GMainLoop *) data);

    /* once */
    return FALSE;
}

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
gst_info_dump_mem_line (gchar * linebuf, gsize linebuf_size,
    const guint8 * mem, gsize mem_offset, gsize mem_size)
{
  gchar hexstr[50], ascstr[18], digitstr[4];

  if (mem_size > 16)
    mem_size = 16;

  hexstr[0] = '\0';
  ascstr[0] = '\0';

  if (mem != NULL) {
    guint i = 0;

    mem += mem_offset;
    while (i < mem_size) {
      ascstr[i] = (g_ascii_isprint (mem[i])) ? mem[i] : '.';
      g_snprintf (digitstr, sizeof (digitstr), "%02x ", mem[i]);
      g_strlcat (hexstr, digitstr, sizeof (hexstr));
      ++i;
    }
    ascstr[i] = '\0';
  }

  g_snprintf (linebuf, linebuf_size, "%08x: %-48.48s %-16.16s",
      (guint) mem_offset, hexstr, ascstr);
}

static void
dump_memory_bytes (guint8 * data, guint len, guint spacing)
{
  gsize off = 0;

  while (off < len) {
    gchar buf[128];

    /* gst_info_dump_mem_line will process 16 bytes at most */
    gst_info_dump_mem_line (buf, sizeof (buf), data, off, len - off);
    NXGLOGI("%*s   %s", spacing, "", buf);
    off += 16;
  }
}

#define dump_memory_content(desc, spacing) dump_memory_bytes((desc)->data + 2, (desc)->length, spacing)

static const gchar *
descriptor_name (gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with DVB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with ATSC enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with ISB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    /* Else try with misc enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE)), val);
  if (en == NULL)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static const gchar *
table_id_name (gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (GST_TYPE_MPEGTS_SECTION_TABLE_ID)), val);
  if (en == NULL)
    /* Else try with DVB enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID)), val);
  if (en == NULL)
    /* Else try with ATSC enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID)), val);
  if (en == NULL)
    /* Else try with SCTE enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID)), val);
  if (en == NULL)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static const gchar *
stream_type_name (gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
          (GST_TYPE_MPEGTS_STREAM_TYPE)), val);
  if (en == NULL)
    /* Else try with SCTE enum types */
    en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
            (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE)), val);
  if (en == NULL)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static const gchar *
enum_name (GType instance_type, gint val)
{
  GEnumValue *en;

  en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek (instance_type)), val);

  if (!en)
    return "UNKNOWN/PRIVATE";
  return en->value_nick;
}

static void
dump_cable_delivery_descriptor (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsCableDeliverySystemDescriptor res;

  if (gst_mpegts_descriptor_parse_cable_delivery_system (desc, &res)) {
    NXGLOGI("%*s Cable Delivery Descriptor", spacing, "");
    NXGLOGI("%*s   Frequency   : %d Hz", spacing, "", res.frequency);
    NXGLOGI("%*s   Outer FEC   : %d (%s)", spacing, "", res.outer_fec,
        enum_name (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME, res.outer_fec));
    NXGLOGI("%*s   modulation  : %d (%s)", spacing, "", res.modulation,
        enum_name (GST_TYPE_MPEGTS_MODULATION_TYPE, res.modulation));
    NXGLOGI("%*s   Symbol rate : %d sym/s", spacing, "", res.symbol_rate);
    NXGLOGI("%*s   Inner FEC   : %d (%s)", spacing, "", res.fec_inner,
        enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE, res.fec_inner));
  }
}

static void
dump_terrestrial_delivery (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsTerrestrialDeliverySystemDescriptor res;

  if (gst_mpegts_descriptor_parse_terrestrial_delivery_system (desc, &res)) {
    NXGLOGI("%*s Terrestrial Delivery Descriptor", spacing, "");
    NXGLOGI("%*s   Frequency         : %d Hz", spacing, "", res.frequency);
    NXGLOGI("%*s   Bandwidth         : %d Hz", spacing, "", res.bandwidth);
    NXGLOGI("%*s   Priority          : %s", spacing, "",
        res.priority ? "TRUE" : "FALSE");
    NXGLOGI("%*s   Time slicing      : %s", spacing, "",
        res.time_slicing ? "TRUE" : "FALSE");
    NXGLOGI("%*s   MPE FEC           : %s", spacing, "",
        res.mpe_fec ? "TRUE" : "FALSE");
    NXGLOGI("%*s   Constellation     : %d (%s)", spacing, "",
        res.constellation, enum_name (GST_TYPE_MPEGTS_MODULATION_TYPE,
            res.constellation));
    NXGLOGI("%*s   Hierarchy         : %d (%s)", spacing, "",
        res.hierarchy, enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY,
            res.hierarchy));
    NXGLOGI("%*s   Code Rate HP      : %d (%s)", spacing, "",
        res.code_rate_hp, enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE,
            res.code_rate_hp));
    NXGLOGI("%*s   Code Rate LP      : %d (%s)", spacing, "",
        res.code_rate_lp, enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE,
            res.code_rate_lp));
    NXGLOGI("%*s   Guard Interval    : %d (%s)", spacing, "",
        res.guard_interval,
        enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL,
            res.guard_interval));
    NXGLOGI("%*s   Transmission Mode : %d (%s)", spacing, "",
        res.transmission_mode,
        enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE,
            res.transmission_mode));
    NXGLOGI("%*s   Other Frequency   : %s", spacing, "",
        res.other_frequency ? "TRUE" : "FALSE");
  }
}

static void
dump_dvb_service_list (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *res;

  if (gst_mpegts_descriptor_parse_dvb_service_list (desc, &res)) {
    guint i;
    NXGLOGI("%*s DVB Service List Descriptor", spacing, "");
    for (i = 0; i < res->len; i++) {
      GstMpegtsDVBServiceListItem *item = g_ptr_array_index (res, i);
      NXGLOGI("%*s   Service #%d, id:0x%04x, type:0x%x (%s)",
          spacing, "", i, item->service_id, item->type,
          enum_name (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE, item->type));
    }
    g_ptr_array_unref (res);
  }
}

static void
dump_logical_channel_descriptor (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsLogicalChannelDescriptor res;
  guint i;

  if (gst_mpegts_descriptor_parse_logical_channel (desc, &res)) {
    NXGLOGI("%*s Logical Channel Descriptor (%d channels)", spacing, "",
        res.nb_channels);
    for (i = 0; i < res.nb_channels; i++) {
      GstMpegtsLogicalChannel *chann = &res.channels[i];
      NXGLOGI("%*s   service_id: 0x%04x, logical channel number:%4d",
          spacing, "", chann->service_id, chann->logical_channel_number);
    }
  }
}

static void
dump_multiligual_network_name (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_network_name (desc, &items)) {
    guint i;
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualNetworkNameItem *item =
          g_ptr_array_index (items, i);
      NXGLOGI("%*s item : %u", spacing, "", i);
      NXGLOGI("%*s   language_code : %s", spacing, "", item->language_code);
      NXGLOGI("%*s   network_name  : %s", spacing, "", item->network_name);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_multiligual_bouquet_name (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_bouquet_name (desc, &items)) {
    guint i;
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualBouquetNameItem *item =
          g_ptr_array_index (items, i);
      NXGLOGI("%*s item : %u", spacing, "", i);
      NXGLOGI("%*s   language_code : %s", spacing, "", item->language_code);
      NXGLOGI("%*s   bouguet_name  : %s", spacing, "", item->bouquet_name);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_multiligual_service_name (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_service_name (desc, &items)) {
    guint i;
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualServiceNameItem *item =
          g_ptr_array_index (items, i);
      NXGLOGI("%*s item : %u", spacing, "", i);
      NXGLOGI("%*s   language_code : %s", spacing, "", item->language_code);
      NXGLOGI("%*s   service_name  : %s", spacing, "", item->service_name);
      NXGLOGI("%*s   provider_name : %s", spacing, "", item->provider_name);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_multiligual_component (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *items;
  guint8 tag;
  if (gst_mpegts_descriptor_parse_dvb_multilingual_component (desc, &tag,
          &items)) {
    guint8 i;
    NXGLOGI("%*s component_tag : 0x%02x", spacing, "", tag);
    for (i = 0; i < items->len; i++) {
      GstMpegtsDvbMultilingualComponentItem *item =
          g_ptr_array_index (items, i);
      NXGLOGI("%*s   item : %u", spacing, "", i);
      NXGLOGI("%*s     language_code : %s", spacing, "",
          item->language_code);
      NXGLOGI("%*s     description   : %s", spacing, "", item->description);
    }
    g_ptr_array_unref (items);
  }
}

static void
dump_linkage (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsDVBLinkageDescriptor *res;

  if (gst_mpegts_descriptor_parse_dvb_linkage (desc, &res)) {
    NXGLOGI("%*s Linkage Descriptor : 0x%02x (%s)", spacing, "",
        res->linkage_type, enum_name (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE,
            res->linkage_type));

    NXGLOGI("%*s   Transport Stream ID : 0x%04x", spacing, "",
        res->transport_stream_id);
    NXGLOGI("%*s   Original Network ID : 0x%04x", spacing, "",
        res->original_network_id);
    NXGLOGI("%*s   Service ID          : 0x%04x", spacing, "",
        res->service_id);

    switch (res->linkage_type) {
      case GST_MPEGTS_DVB_LINKAGE_MOBILE_HAND_OVER:
      {
        const GstMpegtsDVBLinkageMobileHandOver *linkage =
            gst_mpegts_dvb_linkage_descriptor_get_mobile_hand_over (res);
        NXGLOGI("%*s   hand_over_type    : 0x%02x (%s)", spacing,
            "", linkage->hand_over_type,
            enum_name (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE,
                linkage->hand_over_type));
        NXGLOGI("%*s   origin_type       : %s", spacing, "",
            linkage->origin_type ? "SDT" : "NIT");
        NXGLOGI("%*s   network_id        : 0x%04x", spacing, "",
            linkage->network_id);
        NXGLOGI("%*s   initial_service_id: 0x%04x", spacing, "",
            linkage->initial_service_id);
        break;
      }
      case GST_MPEGTS_DVB_LINKAGE_EVENT:
      {
        const GstMpegtsDVBLinkageEvent *linkage =
            gst_mpegts_dvb_linkage_descriptor_get_event (res);
        NXGLOGI("%*s   target_event_id   : 0x%04x", spacing, "",
            linkage->target_event_id);
        NXGLOGI("%*s   target_listed     : %s", spacing, "",
            linkage->target_listed ? "TRUE" : "FALSE");
        NXGLOGI("%*s   event_simulcast   : %s", spacing, "",
            linkage->event_simulcast ? "TRUE" : "FALSE");
        break;
      }
      case GST_MPEGTS_DVB_LINKAGE_EXTENDED_EVENT:
      {
        guint i;
        const GPtrArray *items =
            gst_mpegts_dvb_linkage_descriptor_get_extended_event (res);

        for (i = 0; i < items->len; i++) {
          GstMpegtsDVBLinkageExtendedEvent *linkage =
              g_ptr_array_index (items, i);
          NXGLOGI("%*s   target_event_id   : 0x%04x", spacing, "",
              linkage->target_event_id);
          NXGLOGI("%*s   target_listed     : %s", spacing, "",
              linkage->target_listed ? "TRUE" : "FALSE");
          NXGLOGI("%*s   event_simulcast   : %s", spacing, "",
              linkage->event_simulcast ? "TRUE" : "FALSE");
          NXGLOGI("%*s   link_type         : 0x%01x", spacing, "",
              linkage->link_type);
          NXGLOGI("%*s   target_id_type    : 0x%01x", spacing, "",
              linkage->target_id_type);
          NXGLOGI("%*s   original_network_id_flag : %s", spacing, "",
              linkage->original_network_id_flag ? "TRUE" : "FALSE");
          NXGLOGI("%*s   service_id_flag   : %s", spacing, "",
              linkage->service_id_flag ? "TRUE" : "FALSE");
          if (linkage->target_id_type == 3) {
            NXGLOGI("%*s   user_defined_id   : 0x%02x", spacing, "",
                linkage->user_defined_id);
          } else {
            if (linkage->target_id_type == 1)
              NXGLOGI("%*s   target_transport_stream_id : 0x%04x",
                  spacing, "", linkage->target_transport_stream_id);
            if (linkage->original_network_id_flag)
              NXGLOGI("%*s   target_original_network_id : 0x%04x",
                  spacing, "", linkage->target_original_network_id);
            if (linkage->service_id_flag)
              NXGLOGI("%*s   target_service_id          : 0x%04x",
                  spacing, "", linkage->target_service_id);
          }
        }
        break;
      }
      default:
        break;
    }
    if (res->private_data_length > 0) {
      dump_memory_bytes (res->private_data_bytes, res->private_data_length,
          spacing + 2);
    }
    gst_mpegts_dvb_linkage_descriptor_free (res);
  }
}

static void
dump_component (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsComponentDescriptor *res;

  if (gst_mpegts_descriptor_parse_dvb_component (desc, &res)) {
    NXGLOGI("%*s stream_content : 0x%02x (%s)", spacing, "",
        res->stream_content,
        enum_name (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT,
            res->stream_content));
    NXGLOGI("%*s component_type : 0x%02x", spacing, "",
        res->component_type);
    NXGLOGI("%*s component_tag  : 0x%02x", spacing, "", res->component_tag);
    NXGLOGI("%*s language_code  : %s", spacing, "", res->language_code);
    NXGLOGI("%*s text           : %s", spacing, "",
        res->text ? res->text : "NULL");
    gst_mpegts_dvb_component_descriptor_free (res);
  }
}

static void
dump_content (GstMpegtsDescriptor * desc, guint spacing)
{
  GPtrArray *contents;
  guint i;

  if (gst_mpegts_descriptor_parse_dvb_content (desc, &contents)) {
    for (i = 0; i < contents->len; i++) {
      GstMpegtsContent *item = g_ptr_array_index (contents, i);
      NXGLOGI("%*s content nibble 1 : 0x%01x (%s)", spacing, "",
          item->content_nibble_1,
          enum_name (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI,
              item->content_nibble_1));
      NXGLOGI("%*s content nibble 2 : 0x%01x", spacing, "",
          item->content_nibble_2);
      NXGLOGI("%*s user_byte        : 0x%02x", spacing, "",
          item->user_byte);
    }
    g_ptr_array_unref (contents);
  }
}

static void
dump_iso_639_language (GstMpegtsDescriptor * desc, guint spacing)
{
  guint i;
  GstMpegtsISO639LanguageDescriptor *res;

  if (gst_mpegts_descriptor_parse_iso_639_language (desc, &res)) {
    for (i = 0; i < res->nb_language; i++) {
      NXGLOGI
          ("%*s ISO 639 Language Descriptor %s , audio_type:0x%x (%s)",
          spacing, "", res->language[i], res->audio_type[i],
          enum_name (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE, res->audio_type[i]));
    }
    gst_mpegts_iso_639_language_descriptor_free (res);
  }
}

static void
dump_dvb_extended_event (GstMpegtsDescriptor * desc, guint spacing)
{
  GstMpegtsExtendedEventDescriptor *res;

  if (gst_mpegts_descriptor_parse_dvb_extended_event (desc, &res)) {
    guint i;
    NXGLOGI("%*s DVB Extended Event", spacing, "");
    NXGLOGI("%*s   descriptor_number:%d, last_descriptor_number:%d",
        spacing, "", res->descriptor_number, res->last_descriptor_number);
    NXGLOGI("%*s   language_code:%s", spacing, "", res->language_code);
    NXGLOGI("%*s   text : %s", spacing, "", res->text);
    for (i = 0; i < res->items->len; i++) {
      GstMpegtsExtendedEventItem *item = g_ptr_array_index (res->items, i);
      NXGLOGI("%*s     #%d [description:item]  %s : %s",
          spacing, "", i, item->item_description, item->item);
    }
    gst_mpegts_extended_event_descriptor_free (res);
  }
}

static void
dump_descriptors (GPtrArray * descriptors, guint spacing)
{
  guint i;

  for (i = 0; i < descriptors->len; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    NXGLOGI("%*s [descriptor 0x%02x (%s) length:%d]", spacing, "",
        desc->tag, descriptor_name (desc->tag), desc->length);
    if (DUMP_DESCRIPTORS)
      dump_memory_content (desc, spacing + 2);
    switch (desc->tag) {
      case GST_MTS_DESC_REGISTRATION:
      {
        const guint8 *data = desc->data + 2;
#define SAFE_CHAR(a) (g_ascii_isprint(a) ? a : '.')
        NXGLOGI("%*s   Registration : %c%c%c%c [%02x%02x%02x%02x]", spacing,
            "", SAFE_CHAR (data[0]), SAFE_CHAR (data[1]), SAFE_CHAR (data[2]),
            SAFE_CHAR (data[3]), data[0], data[1], data[2], data[3]);

        break;
      }
      case GST_MTS_DESC_CA:
      {
        guint16 ca_pid, ca_system_id;
        const guint8 *private_data;
        gsize private_data_size;
        if (gst_mpegts_descriptor_parse_ca (desc, &ca_system_id, &ca_pid,
                &private_data, &private_data_size)) {
          NXGLOGI("%*s   CA system id : 0x%04x", spacing, "", ca_system_id);
          NXGLOGI("%*s   CA PID       : 0x%04x", spacing, "", ca_pid);
          if (private_data_size) {
            NXGLOGI("%*s   Private Data :", spacing, "");
            dump_memory_bytes ((guint8 *) private_data, private_data_size,
                spacing + 2);
          }
        }
        break;
      }
      case GST_MTS_DESC_DVB_NETWORK_NAME:
      {
        gchar *network_name;
        if (gst_mpegts_descriptor_parse_dvb_network_name (desc, &network_name)) {
          NXGLOGI("%*s   Network Name : %s", spacing, "", network_name);
          g_free (network_name);
        }
        break;
      }
      case GST_MTS_DESC_DVB_SERVICE_LIST:
      {
        dump_dvb_service_list (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_CABLE_DELIVERY_SYSTEM:
        dump_cable_delivery_descriptor (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM:
        dump_terrestrial_delivery (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_BOUQUET_NAME:
      {
        gchar *bouquet_name;
        if (gst_mpegts_descriptor_parse_dvb_bouquet_name (desc, &bouquet_name)) {
          NXGLOGI("%*s   Bouquet Name Descriptor, bouquet_name:%s", spacing,
              "", bouquet_name);
          g_free (bouquet_name);
        }
        break;
      }
      case GST_MTS_DESC_DTG_LOGICAL_CHANNEL:
        dump_logical_channel_descriptor (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_SERVICE:
      {
        gchar *service_name, *provider_name;
        GstMpegtsDVBServiceType service_type;
        if (gst_mpegts_descriptor_parse_dvb_service (desc, &service_type,
                &service_name, &provider_name)) {
          NXGLOGI("%*s   Service Descriptor, type:0x%02x (%s)", spacing, "",
              service_type, enum_name (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE,
                  service_type));
          NXGLOGI("%*s      service_name  : %s", spacing, "", service_name);
          NXGLOGI("%*s      provider_name : %s", spacing, "",
              provider_name);
          g_free (service_name);
          g_free (provider_name);

        }
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_BOUQUET_NAME:
      {
        dump_multiligual_bouquet_name (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_NETWORK_NAME:
      {
        dump_multiligual_network_name (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_SERVICE_NAME:
      {
        dump_multiligual_service_name (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_MULTILINGUAL_COMPONENT:
      {
        dump_multiligual_component (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_PRIVATE_DATA_SPECIFIER:
      {
        guint32 specifier;
        guint8 len = 0, *data = NULL;

        if (gst_mpegts_descriptor_parse_dvb_private_data_specifier (desc,
                &specifier, &data, &len)) {
          NXGLOGI("%*s   private_data_specifier : 0x%08x", spacing, "",
              specifier);
          if (len > 0) {
            dump_memory_bytes (data, len, spacing + 2);
            g_free (data);
          }
        }
        break;
      }
      case GST_MTS_DESC_DVB_FREQUENCY_LIST:
      {
        gboolean offset;
        GArray *list;
        if (gst_mpegts_descriptor_parse_dvb_frequency_list (desc, &offset,
                &list)) {
          guint j;
          for (j = 0; j < list->len; j++) {
            guint32 freq = g_array_index (list, guint32, j);
            NXGLOGI("%*s   Frequency : %u %s", spacing, "", freq,
                offset ? "kHz" : "Hz");
          }
          g_array_unref (list);
        }
        break;
      }
      case GST_MTS_DESC_DVB_LINKAGE:
        dump_linkage (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_COMPONENT:
        dump_component (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_STREAM_IDENTIFIER:
      {
        guint8 tag;
        if (gst_mpegts_descriptor_parse_dvb_stream_identifier (desc, &tag)) {
          NXGLOGI("%*s   Component Tag : 0x%02x", spacing, "", tag);
        }
        break;
      }
      case GST_MTS_DESC_DVB_CA_IDENTIFIER:
      {
        GArray *list;
        guint j;
        guint16 ca_id;
        if (gst_mpegts_descriptor_parse_dvb_ca_identifier (desc, &list)) {
          for (j = 0; j < list->len; j++) {
            ca_id = g_array_index (list, guint16, j);
            NXGLOGI("%*s   CA Identifier : 0x%04x", spacing, "", ca_id);
          }
          g_array_unref (list);
        }
        break;
      }
      case GST_MTS_DESC_DVB_CONTENT:
        dump_content (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_PARENTAL_RATING:
      {
        GPtrArray *ratings;
        guint j;

        if (gst_mpegts_descriptor_parse_dvb_parental_rating (desc, &ratings)) {
          for (j = 0; j < ratings->len; j++) {
            GstMpegtsDVBParentalRatingItem *item =
                g_ptr_array_index (ratings, j);
            NXGLOGI("%*s   country_code : %s", spacing, "",
                item->country_code);
            NXGLOGI("%*s   rating age   : %d", spacing, "", item->rating);
          }
          g_ptr_array_unref (ratings);
        }
        break;
      }
      case GST_MTS_DESC_DVB_DATA_BROADCAST:
      {
        GstMpegtsDataBroadcastDescriptor *res;

        if (gst_mpegts_descriptor_parse_dvb_data_broadcast (desc, &res)) {
          NXGLOGI("%*s   data_broadcast_id : 0x%04x", spacing, "",
              res->data_broadcast_id);
          NXGLOGI("%*s   component_tag     : 0x%02x", spacing, "",
              res->component_tag);
          if (res->length > 0) {
            NXGLOGI("%*s   selector_bytes:", spacing, "");
            dump_memory_bytes (res->selector_bytes, res->length, spacing + 2);
          }
          NXGLOGI("%*s   text              : %s", spacing, "",
              res->text ? res->text : "NULL");
          gst_mpegts_dvb_data_broadcast_descriptor_free (res);
        }
        break;
      }
      case GST_MTS_DESC_ISO_639_LANGUAGE:
        dump_iso_639_language (desc, spacing + 2);
        break;
      case GST_MTS_DESC_DVB_SHORT_EVENT:
      {
        gchar *language_code, *event_name, *text;
        if (gst_mpegts_descriptor_parse_dvb_short_event (desc, &language_code,
                &event_name, &text)) {
          NXGLOGI("%*s   Short Event, language_code:%s", spacing, "",
              language_code);
          NXGLOGI("%*s     event_name : %s", spacing, "", event_name);
          NXGLOGI("%*s     text       : %s", spacing, "", text);
          g_free (language_code);
          g_free (event_name);
          g_free (text);
        }
      }
        break;
      case GST_MTS_DESC_DVB_EXTENDED_EVENT:
      {
        dump_dvb_extended_event (desc, spacing + 2);
        break;
      }
      case GST_MTS_DESC_DVB_SUBTITLING:
      {
        gchar *lang;
        guint8 type;
        guint16 composition;
        guint16 ancillary;
        guint j;

        for (j = 0;
            gst_mpegts_descriptor_parse_dvb_subtitling_idx (desc, j, &lang,
                &type, &composition, &ancillary); j++) {
          NXGLOGI("%*s   Subtitling, language_code:%s", spacing, "", lang);
          NXGLOGI("%*s      type                : %u", spacing, "", type);
          NXGLOGI("%*s      composition page id : %u", spacing, "",
              composition);
          NXGLOGI("%*s      ancillary page id   : %u", spacing, "",
              ancillary);
          g_free (lang);
        }
      }
        break;
      case GST_MTS_DESC_DVB_TELETEXT:
      {
        GstMpegtsDVBTeletextType type;
        gchar *lang;
        guint8 magazine, page_number;
        guint j;

        for (j = 0;
            gst_mpegts_descriptor_parse_dvb_teletext_idx (desc, j, &lang, &type,
                &magazine, &page_number); j++) {
          NXGLOGI("%*s   Teletext, type:0x%02x (%s)", spacing, "", type,
              enum_name (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE, type));
          NXGLOGI("%*s      language    : %s", spacing, "", lang);
          NXGLOGI("%*s      magazine    : %u", spacing, "", magazine);
          NXGLOGI("%*s      page number : %u", spacing, "", page_number);
          g_free (lang);
        }
      }
        break;
      default:
        break;
    }
  }
}

static void
dump_pat (GstMpegtsSection * section, MpegTsSt *handle)
{
  GPtrArray *pat = gst_mpegts_section_get_pat (section);
  guint i, len;

  // n_program
  len = pat->len;
  handle->media_info->n_program = pat->len;
  NXGLOGI("   %d program(s):", len);

  for (i = 0; i < len; i++) {
    GstMpegtsPatProgram *patp = g_ptr_array_index (pat, i);
    handle->media_info->program_number[i] = patp->program_number;
    NXGLOGI("## n_program(%d), handle->media_info->program_number[%d] = %d",
            handle->media_info->n_program, i, handle->media_info->program_number[i]);
    // program_number
    NXGLOGI
        ("     program_number:%6d (0x%04x), network_or_program_map_PID:0x%04x",
        patp->program_number, patp->program_number,
        patp->network_or_program_map_PID);
  }

  g_ptr_array_unref (pat);
}

static void
dump_pmt (GstMpegtsSection * section, MpegTsSt *handle)
{
  const GstMpegtsPMT *pmt = gst_mpegts_section_get_pmt (section);
  guint i, len;

  NXGLOGI("     program_number : 0x%04x", section->subtable_extension);
  NXGLOGI("     pcr_pid        : 0x%04x", pmt->pcr_pid);
  dump_descriptors (pmt->descriptors, 7);
  len = pmt->streams->len;
  NXGLOGI("     %d Streams:", len);
  for (i = 0; i < len; i++) {
    GstMpegtsPMTStream *stream = g_ptr_array_index (pmt->streams, i);
    NXGLOGI("       pid:0x%04x , stream_type:0x%02x (%s)", stream->pid,
        stream->stream_type, stream_type_name (stream->stream_type));
    dump_descriptors (stream->descriptors, 9);
  }
}

static void
dump_eit (GstMpegtsSection * section)
{
  const GstMpegtsEIT *eit = gst_mpegts_section_get_eit (section);
  guint i, len;

  g_assert (eit);

  NXGLOGI("     service_id          : 0x%04x", section->subtable_extension);
  NXGLOGI("     transport_stream_id : 0x%04x", eit->transport_stream_id);
  NXGLOGI("     original_network_id : 0x%04x", eit->original_network_id);
  NXGLOGI("     segment_last_section_number:0x%02x, last_table_id:0x%02x",
      eit->segment_last_section_number, eit->last_table_id);
  NXGLOGI("     actual_stream : %s, present_following : %s",
      eit->actual_stream ? "TRUE" : "FALSE",
      eit->present_following ? "TRUE" : "FALSE");

  len = eit->events->len;
  NXGLOGI("     %d Event(s):", len);
  for (i = 0; i < len; i++) {
    gchar *tmp = (gchar *) "<NO TIME>";
    GstMpegtsEITEvent *event = g_ptr_array_index (eit->events, i);

    if (event->start_time)
      tmp = gst_date_time_to_iso8601_string (event->start_time);
    NXGLOGI("       event_id:0x%04x, start_time:%s, duration:%"
        GST_TIME_FORMAT "", event->event_id, tmp,
        GST_TIME_ARGS (event->duration * GST_SECOND));
    NXGLOGI("       running_status:0x%02x (%s), free_CA_mode:%d (%s)",
        event->running_status, enum_name (GST_TYPE_MPEGTS_RUNNING_STATUS,
            event->running_status), event->free_CA_mode,
        event->free_CA_mode ? "MAYBE SCRAMBLED" : "NOT SCRAMBLED");
    if (event->start_time)
      g_free (tmp);
    dump_descriptors (event->descriptors, 9);
  }
}

static void
dump_atsc_mult_string (GPtrArray * mstrings, guint spacing)
{
  guint i;

  for (i = 0; i < mstrings->len; i++) {
    GstMpegtsAtscMultString *mstring = g_ptr_array_index (mstrings, i);
    gint j, n;

    n = mstring->segments->len;

    NXGLOGI("%*s [multstring entry (%d) iso_639 langcode: %s]", spacing, "",
        i, mstring->iso_639_langcode);
    NXGLOGI("%*s   segments:%d", spacing, "", n);
    for (j = 0; j < n; j++) {
      GstMpegtsAtscStringSegment *segment =
          g_ptr_array_index (mstring->segments, j);

      NXGLOGI("%*s    Compression:0x%x", spacing, "",
          segment->compression_type);
      NXGLOGI("%*s    Mode:0x%x", spacing, "", segment->mode);
      NXGLOGI("%*s    Len:%u", spacing, "", segment->compressed_data_size);
      NXGLOGI("%*s    %s", spacing, "",
          gst_mpegts_atsc_string_segment_get_string (segment));
    }
  }
}

static void
dump_atsc_eit (GstMpegtsSection * section)
{
  const GstMpegtsAtscEIT *eit = gst_mpegts_section_get_atsc_eit (section);
  guint i, len;

  g_assert (eit);

  NXGLOGI("     event_id            : 0x%04x", eit->source_id);
  NXGLOGI("     protocol_version    : %u", eit->protocol_version);

  len = eit->events->len;
  NXGLOGI("     %d Event(s):", len);
  for (i = 0; i < len; i++) {
    GstMpegtsAtscEITEvent *event = g_ptr_array_index (eit->events, i);

    NXGLOGI("     %d)", i);
    NXGLOGI("       event_id: 0x%04x", event->event_id);
    NXGLOGI("       start_time: %u", event->start_time);
    NXGLOGI("       etm_location: 0x%x", event->etm_location);
    NXGLOGI("       length_in_seconds: %u", event->length_in_seconds);
    NXGLOGI("       Title(s):");
    dump_atsc_mult_string (event->titles, 9);
    dump_descriptors (event->descriptors, 9);
  }
}

static void
dump_ett (GstMpegtsSection * section)
{
  const GstMpegtsAtscETT *ett = gst_mpegts_section_get_atsc_ett (section);
  guint len;

  g_assert (ett);

  NXGLOGI("     ett_table_id_ext    : 0x%04x", ett->ett_table_id_extension);
  NXGLOGI("     protocol_version    : 0x%04x", ett->protocol_version);
  NXGLOGI("     etm_id              : 0x%04x", ett->etm_id);

  len = ett->messages->len;
  NXGLOGI("     %d Messages(s):", len);
  dump_atsc_mult_string (ett->messages, 9);
}

static void
dump_stt (GstMpegtsSection * section)
{
  const GstMpegtsAtscSTT *stt = gst_mpegts_section_get_atsc_stt (section);
  GstDateTime *dt;
  gchar *dt_str = NULL;

  g_assert (stt);

  dt = gst_mpegts_atsc_stt_get_datetime_utc ((GstMpegtsAtscSTT *) stt);
  if (dt)
    dt_str = gst_date_time_to_iso8601_string (dt);

  NXGLOGI("     protocol_version    : 0x%04x", stt->protocol_version);
  NXGLOGI("     system_time         : 0x%08x", stt->system_time);
  NXGLOGI("     gps_utc_offset      : %d", stt->gps_utc_offset);
  NXGLOGI("     daylight saving     : %d day:%d hour:%d", stt->ds_status,
      stt->ds_dayofmonth, stt->ds_hour);
  NXGLOGI("     utc datetime        : %s", dt_str);

  g_free (dt_str);
  gst_date_time_unref (dt);
}

static void
dump_nit (GstMpegtsSection * section)
{
  const GstMpegtsNIT *nit = gst_mpegts_section_get_nit (section);
  guint i, len;

  g_assert (nit);

  NXGLOGI("     network_id     : 0x%04x", section->subtable_extension);
  NXGLOGI("     actual_network : %s",
      nit->actual_network ? "TRUE" : "FALSE");
  dump_descriptors (nit->descriptors, 7);
  len = nit->streams->len;
  NXGLOGI("     %d Streams:", len);
  for (i = 0; i < len; i++) {
    GstMpegtsNITStream *stream = g_ptr_array_index (nit->streams, i);
    NXGLOGI
        ("       transport_stream_id:0x%04x , original_network_id:0x%02x",
        stream->transport_stream_id, stream->original_network_id);
    dump_descriptors (stream->descriptors, 9);
  }
}

static void
dump_bat (GstMpegtsSection * section)
{
  const GstMpegtsBAT *bat = gst_mpegts_section_get_bat (section);
  guint i, len;

  g_assert (bat);

  NXGLOGI("     bouquet_id     : 0x%04x", section->subtable_extension);
  dump_descriptors (bat->descriptors, 7);
  len = bat->streams->len;
  NXGLOGI("     %d Streams:", len);
  for (i = 0; i < len; i++) {
    GstMpegtsBATStream *stream = g_ptr_array_index (bat->streams, i);
    NXGLOGI
        ("       transport_stream_id:0x%04x , original_network_id:0x%02x",
        stream->transport_stream_id, stream->original_network_id);
    dump_descriptors (stream->descriptors, 9);
  }
}

static void
dump_sdt (GstMpegtsSection * section)
{
  const GstMpegtsSDT *sdt = gst_mpegts_section_get_sdt (section);
  guint i, len;

  g_assert (sdt);

  NXGLOGI("     original_network_id : 0x%04x", sdt->original_network_id);
  NXGLOGI("     actual_ts           : %s",
      sdt->actual_ts ? "TRUE" : "FALSE");
  len = sdt->services->len;
  NXGLOGI("     %d Services:", len);
  for (i = 0; i < len; i++) {
    GstMpegtsSDTService *service = g_ptr_array_index (sdt->services, i);
    NXGLOGI
        ("       service_id:0x%04x, EIT_schedule_flag:%d, EIT_present_following_flag:%d",
        service->service_id, service->EIT_schedule_flag,
        service->EIT_present_following_flag);
    NXGLOGI
        ("       running_status:0x%02x (%s), free_CA_mode:%d (%s)",
        service->running_status,
        enum_name (GST_TYPE_MPEGTS_RUNNING_STATUS, service->running_status),
        service->free_CA_mode,
        service->free_CA_mode ? "MAYBE SCRAMBLED" : "NOT SCRAMBLED");
    dump_descriptors (service->descriptors, 9);
  }
}

static void
dump_tdt (GstMpegtsSection * section)
{
  GstDateTime *date = gst_mpegts_section_get_tdt (section);

  if (date) {
    gchar *str = gst_date_time_to_iso8601_string (date);
    NXGLOGI("     utc_time : %s", str);
    g_free (str);
    gst_date_time_unref (date);
  } else {
    NXGLOGI("     No utc_time present");
  }
}

static void
dump_tot (GstMpegtsSection * section)
{
  const GstMpegtsTOT *tot = gst_mpegts_section_get_tot (section);
  gchar *str = gst_date_time_to_iso8601_string (tot->utc_time);

  NXGLOGI("     utc_time : %s", str);
  dump_descriptors (tot->descriptors, 7);
  g_free (str);
}

static void
dump_mgt (GstMpegtsSection * section)
{
  const GstMpegtsAtscMGT *mgt = gst_mpegts_section_get_atsc_mgt (section);
  gint i;

  NXGLOGI("     protocol_version    : %u", mgt->protocol_version);
  NXGLOGI("     tables number       : %d", mgt->tables->len);
  for (i = 0; i < mgt->tables->len; i++) {
    GstMpegtsAtscMGTTable *table = g_ptr_array_index (mgt->tables, i);
    NXGLOGI("     table %d)", i);
    NXGLOGI("       table_type    : %u", table->table_type);
    NXGLOGI("       pid           : 0x%x", table->pid);
    NXGLOGI("       version_number: %u", table->version_number);
    NXGLOGI("       number_bytes  : %u", table->number_bytes);
    dump_descriptors (table->descriptors, 9);
  }
  dump_descriptors (mgt->descriptors, 7);
}

static void
dump_vct (GstMpegtsSection * section)
{
  const GstMpegtsAtscVCT *vct;
  gint i;

  if (GST_MPEGTS_SECTION_TYPE (section) == GST_MPEGTS_SECTION_ATSC_CVCT) {
    vct = gst_mpegts_section_get_atsc_cvct (section);
  } else {
    /* GST_MPEGTS_SECTION_ATSC_TVCT */
    vct = gst_mpegts_section_get_atsc_tvct (section);
  }

  g_assert (vct);

  NXGLOGI("     transport_stream_id : 0x%04x", vct->transport_stream_id);
  NXGLOGI("     protocol_version    : %u", vct->protocol_version);
  NXGLOGI("     %d Sources:", vct->sources->len);
  for (i = 0; i < vct->sources->len; i++) {
    GstMpegtsAtscVCTSource *source = g_ptr_array_index (vct->sources, i);
    NXGLOGI("       short_name: %s", source->short_name);
    NXGLOGI("       major_channel_number: %u, minor_channel_number: %u",
        source->major_channel_number, source->minor_channel_number);
    NXGLOGI("       modulation_mode: %u", source->modulation_mode);
    NXGLOGI("       carrier_frequency: %u", source->carrier_frequency);
    NXGLOGI("       channel_tsid: %u", source->channel_TSID);
    NXGLOGI("       program_number: %u", source->program_number);
    NXGLOGI("       ETM_location: %u", source->ETM_location);
    NXGLOGI("       access_controlled: %u", source->access_controlled);
    NXGLOGI("       hidden: %u", source->hidden);
    if (section->table_id == GST_MPEGTS_SECTION_ATSC_CVCT) {
      NXGLOGI("       path_select: %u", source->path_select);
      NXGLOGI("       out_of_band: %u", source->out_of_band);
    }
    NXGLOGI("       hide_guide: %u", source->hide_guide);
    NXGLOGI("       service_type: %u", source->service_type);
    NXGLOGI("       source_id: %u", source->source_id);

    dump_descriptors (source->descriptors, 9);
  }
  dump_descriptors (vct->descriptors, 7);
}

static void
dump_cat (GstMpegtsSection * section)
{
  GPtrArray *descriptors;

  descriptors = gst_mpegts_section_get_cat (section);
  g_assert (descriptors);
  dump_descriptors (descriptors, 7);
  g_ptr_array_unref (descriptors);
}

static const gchar *
scte_descriptor_name (guint8 tag)
{
  switch (tag) {
    case 0x00:
      return "avail";
    case 0x01:
      return "DTMF";
    case 0x02:
      return "segmentation";
    case 0x03:
      return "time";
    case 0x04:
      return "audio";
    default:
      return "UNKNOWN";
  }
}

static void
dump_scte_descriptors (GPtrArray * descriptors, guint spacing)
{
  guint i;

  for (i = 0; i < descriptors->len; i++) {
    GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
    NXGLOGI("%*s [scte descriptor 0x%02x (%s) length:%d]", spacing, "",
        desc->tag, scte_descriptor_name (desc->tag), desc->length);
    if (DUMP_DESCRIPTORS)
      dump_memory_content (desc, spacing + 2);
    /* FIXME : Add parsing of SCTE descriptors */
  }
}

#if 0
static void
dump_scte_sit (GstMpegtsSection * section)
{
  const GstMpegtsSCTESIT *sit = gst_mpegts_section_get_scte_sit (section);
  guint i, len;

  g_assert (sit);

  NXGLOGI("     encrypted_packet    : %d", sit->encrypted_packet);
  if (sit->encrypted_packet) {
    NXGLOGI("     encryption_algorithm: %d", sit->encryption_algorithm);
    NXGLOGI("     cw_index            : %d", sit->cw_index);
    NXGLOGI("     tier                : %d", sit->tier);
  }
  NXGLOGI("     pts_adjustment      : %" G_GUINT64_FORMAT " (%"
      GST_TIME_FORMAT ")", sit->pts_adjustment,
      GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (sit->pts_adjustment)));
  NXGLOGI("     command_type        : %d", sit->splice_command_type);

  if ((len = sit->splices->len)) {
    NXGLOGI("     %d splice(s):", len);
    for (i = 0; i < len; i++) {
      GstMpegtsSCTESpliceEvent *event = g_ptr_array_index (sit->splices, i);
      NXGLOGI("     event_id:%d event_cancel_indicator:%d",
          event->splice_event_id, event->splice_event_cancel_indicator);
      if (!event->splice_event_cancel_indicator) {
        NXGLOGI("       out_of_network_indicator:%d",
            event->out_of_network_indicator);
        if (event->program_splice_flag) {
          if (event->program_splice_time_specified)
            NXGLOGI("       program_splice_time:%" G_GUINT64_FORMAT " (%"
                GST_TIME_FORMAT ")", event->program_splice_time,
                GST_TIME_ARGS (MPEGTIME_TO_GSTTIME
                    (event->program_splice_time)));
          else
            NXGLOGI("       program_splice_time not specified");
        }
        if (event->duration_flag) {
          NXGLOGI("       break_duration_auto_return:%d",
              event->break_duration_auto_return);
          NXGLOGI("       break_duration:%" G_GUINT64_FORMAT " (%"
              GST_TIME_FORMAT ")", event->break_duration,
              GST_TIME_ARGS (MPEGTIME_TO_GSTTIME (event->break_duration)));

        }
        NXGLOGI("       unique_program_id  : %d", event->unique_program_id);
        NXGLOGI("       avail num/expected : %d/%d",
            event->avail_num, event->avails_expected);
      }
    }
  }

  dump_scte_descriptors (sit->descriptors, 4);
}
#endif

static void
dump_section (GstMpegtsSection * section, MpegTsSt *handle)
{
    NXGLOGI();
  switch (GST_MPEGTS_SECTION_TYPE (section)) {
    case GST_MPEGTS_SECTION_PAT:
      dump_pat (section, handle);
      break;
    case GST_MPEGTS_SECTION_PMT:
      dump_pmt (section, handle);
      break;
    case GST_MPEGTS_SECTION_CAT:
      dump_cat (section);
      break;
    case GST_MPEGTS_SECTION_TDT:
      dump_tdt (section);
      break;
    case GST_MPEGTS_SECTION_TOT:
      dump_tot (section);
      break;
    case GST_MPEGTS_SECTION_SDT:
      dump_sdt (section);
      break;
    case GST_MPEGTS_SECTION_NIT:
      dump_nit (section);
      break;
    case GST_MPEGTS_SECTION_BAT:
      dump_bat (section);
      break;
    case GST_MPEGTS_SECTION_EIT:
      dump_eit (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_MGT:
      dump_mgt (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_CVCT:
    case GST_MPEGTS_SECTION_ATSC_TVCT:
      dump_vct (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_EIT:
      dump_atsc_eit (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_ETT:
      dump_ett (section);
      break;
    case GST_MPEGTS_SECTION_ATSC_STT:
      dump_stt (section);
      break;
#if 0
    case GST_MPEGTS_SECTION_SCTE_SIT:
      dump_scte_sit (section);
      break;
#endif
    default:
      NXGLOGI("     Unknown section type");
      break;
  }
}

static void
dump_collection (GstStreamCollection * collection, MpegTsSt *handle)
{
  guint i;
  GstTagList *tags;
  GstCaps *caps;
  GstStructure *structure;

  for (i = 0; i < gst_stream_collection_get_size (collection); i++) {
    GstStream *stream = gst_stream_collection_get_stream (collection, i);
    GstStreamType stype = gst_stream_get_stream_type (stream);
    NXGLOGI (" Stream %u type %s flags 0x%x", i,
        gst_stream_type_get_name (stype),
        gst_stream_get_stream_flags (stream));
    const char* stream_id = gst_stream_get_stream_id (stream);
    NXGLOGI ("  ID: %s", stream_id);
#if 0
    gchar **split_str = NULL;
    gint len_split_str = 0;

    split_str = g_strsplit_set(stream_id, ":/", -1);
    len_split_str = g_strv_length(split_str);
    if (3 == len_split_str)
    {
      handle->media_info->current_program = atoi(split_str[1]);
    }
    handle->media_info->current_program = atoi(split_str[1]);
    NXGLOGI("len_split_str = %d, split_str[1]=%s, "
            "handle->media_info->current_program=%d",
            len_split_str, split_str[1], handle->media_info->current_program);
    g_strfreev(split_str);
#endif
/*    for (int i=0; i<handle->media_info->n_program; i++)
    {
        if (handle->media_info->program_number[i] == handle->media_info->current_program) {
            handle->media_info->current_program_idx = i;
            NXGLOGI("Found matched program number! idx:%d", i);
        }
    }*/

    caps = gst_stream_get_caps (stream);
    if (caps) {
      gchar *caps_str = gst_caps_to_string (caps);
      NXGLOGI ("  caps: %s", caps_str);
      g_free (caps_str);
      gst_caps_unref (caps);
    }

    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *mime_type = gst_structure_get_name(structure);

    tags = gst_stream_get_tags (stream);
    if (tags) {
      NXGLOGI ("  tags:");
      gst_tag_list_foreach (tags, print_tag_foreach, GUINT_TO_POINTER (3));
      gst_tag_list_unref (tags);
    }
#if 0
    int cur_pro_idx = handle->media_info->current_program_idx;
    // n_video, n_audio, n_subtitle
    if (stype & GST_STREAM_TYPE_AUDIO)
    {
		gint audio_mpegversion, channels, samplerate;
		AUDIO_TYPE audio_type = get_audio_codec_type(mime_type);
		int32_t a_idx = handle->media_info->ProgramInfo[cur_pro_idx].n_audio;
		handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = audio_type;
		if (gst_structure_get_int (structure, "mpegversion", &audio_mpegversion))
		{
			if (audio_mpegversion == 1) {
				handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V1;
			} else if (audio_mpegversion == 2) {
				handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V2;
			} 

		}
		if (gst_structure_get_int(structure, "channels", &channels))
			handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].n_channels = channels;
		if (gst_structure_get_int(structure, "rate", &samplerate))
			handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].samplerate = samplerate;

		// tag - bitrate
		NXGLOGI("n_channels(%d), samplerate(%d), type(%d)",
				channels, samplerate,
				handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type);
    }
    else if (stype & GST_STREAM_TYPE_VIDEO)
    {
        gint video_mpegversion, width, height, num, den = 0;
        VIDEO_TYPE video_type = get_video_codec_type(mime_type);
        const gchar *stream_id = gst_stream_get_stream_id (stream);
		int32_t v_idx = handle->media_info->ProgramInfo[cur_pro_idx].n_video;

        handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = video_type;
        if ((structure != NULL) && (video_type == VIDEO_TYPE_MPEG_V4))
        {
            gst_structure_get_int (structure, "mpegversion", &video_mpegversion);
            if (video_mpegversion == 1) {
                handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V1;
            } else if (video_mpegversion == 2) {
                handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V2;
            }
        }

        handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].stream_id = stream_id;
		handle->media_info->ProgramInfo[cur_pro_idx].n_video++;

        NXGLOGI("video_mpegversion(%d) type(%d) stream_id(%s)",
                video_mpegversion, handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[0].type,
                stream_id);
    }
	else if (stype & GST_STREAM_TYPE_TEXT)
	{
		SUBTITLE_TYPE sub_type = get_video_codec_type(mime_type);
		const gchar *stream_id = gst_stream_get_stream_id (stream);
		int32_t sub_idx = handle->media_info->ProgramInfo[cur_pro_idx].n_subtitle;

        handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].type = sub_type;
        handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].stream_id = stream_id;
        handle->media_info->ProgramInfo[cur_pro_idx].n_subtitle++;

        NXGLOGI("n_subtitle(%d), stream_id(%s), subtitle_type(%d)",
                handle->media_info->ProgramInfo[cur_pro_idx].n_subtitle,
                handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].stream_id,
                handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].type);
    }
#endif
  }
}

static void
_on_bus_message (GstBus * bus, GstMessage * message, MpegTsSt *handle)
{
  NXGLOGI("Got message %s", GST_MESSAGE_TYPE_NAME (message));

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_EOS:
      g_main_loop_quit (handle->loop);
      break;
    case GST_MESSAGE_ELEMENT:
    {
      GstMpegtsSection *section;
      if ((section = gst_message_parse_mpegts_section(message))) {
        const gchar *table_name;

        table_name = table_id_name(section->table_id);
        NXGLOGI
            ("Got section: PID:0x%04x type:%s (table_id 0x%02x (%s)) at offset %"
            G_GUINT64_FORMAT "", section->pid,
            enum_name (GST_TYPE_MPEGTS_SECTION_TYPE, section->section_type),
            section->table_id, table_name, section->offset);
        if (!section->short_section)
        {
          NXGLOGI
              ("   subtable_extension:0x%04x, version_number:0x%02x",
              section->subtable_extension, section->version_number);
          NXGLOGI
              ("   section_number:0x%02x last_section_number:0x%02x crc:0x%08x",
              section->section_number, section->last_section_number,
              section->crc);
        }

        //dump_section(section, handle);
        GstMpegtsSectionType section_type = GST_MPEGTS_SECTION_TYPE(section);
        if (GST_MPEGTS_SECTION_PAT == section_type) {
            dump_pat(section, handle);
            gst_mpegts_section_unref(section);
        } else if (GST_MPEGTS_SECTION_PMT == section_type) {
            dump_pmt(section, handle);
            gst_mpegts_section_unref(section);
            g_idle_add (idle_exit_loop, handle->loop);
        }/* else if (GST_MPEGTS_SECTION_SDT == section_type) {
            dump_sdt(section);
            gst_mpegts_section_unref (section);
            g_idle_add (idle_exit_loop, handle->loop);
        }*/
      }
      break;
    }
    /*case GST_MESSAGE_STREAM_COLLECTION:
    {
        GstStreamCollection *collection = NULL;
        GstObject *src = GST_MESSAGE_SRC(message);

        gst_message_parse_stream_collection(message, &collection);
        if (collection)
        {
            NXGLOGI("Got a collection from %s:",
                    src ? GST_OBJECT_NAME (src) : "Unknown");
            dump_collection(collection, handle);
        }
        break;
    }*/
    default:
      break;
  }
}

static void
demux_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
    gchar *name;
    GstCaps *caps;
    GstStructure *structure;
    GstElement *target_sink;
    GstPad *target_sink_pad;

    MpegTsSt * handle = (MpegTsSt *)data;
    name = gst_pad_get_name(pad);
    NXGLOGI("A new pad %s was created for %s\n", name, gst_element_get_name(element));
    g_free(name);

    caps = gst_pad_get_current_caps(pad);
    if (caps == NULL) {
        NXGLOGE("Failed to get current caps");
        return;
    }

    structure = gst_caps_get_structure(caps, 0);
    if (structure == NULL) {
        NXGLOGE("Failed to get current caps");
        gst_caps_unref (caps);
        return;
    }

    const char *mime_type = gst_structure_get_name(structure);
    if (g_str_has_prefix(mime_type, "video/"))
    {
        target_sink = handle->video_queue;
        NXGLOGI("element %s will be linked to %s\n",
                gst_element_get_name(element),
                gst_element_get_name(target_sink));

        target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
        NXGLOGI("%s to link %s:%s to %s:%s",
                (ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
                GST_DEBUG_PAD_NAME(pad),
                GST_DEBUG_PAD_NAME(target_sink_pad));
        gst_object_unref (target_sink_pad);
    }
    gst_caps_unref (caps);
}

static void
decodebin_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
    gchar *name;
    GstCaps *caps;
    GstStructure *structure;
    GstElement *target_sink;
    GstPad *target_sink_pad;
    MpegTsSt * handle = (MpegTsSt *)data;

    name = gst_pad_get_name(pad);
    NXGLOGI("A new pad %s was created for %s\n", name, gst_element_get_name(element));
    g_free(name);

    caps = gst_pad_get_current_caps(pad);
    if (caps == NULL) {
        NXGLOGE("Failed to get current caps");
        return;
    }

    structure = gst_caps_get_structure(caps, 0);
    if (structure == NULL) {
        NXGLOGE("Failed to get current caps");
        gst_caps_unref (caps);
        return;
    }

    //int current_program_idx = handle->media_info->current_program_idx;
    const char *mime_type = gst_structure_get_name(structure);
    NXGLOGI("MIME-type:%s", mime_type);
    if (g_str_has_prefix(mime_type, "video/"))
    {
        gint video_mpegversion, width, height, num, den;

		//int32_t v_idx = handle->media_info->ProgramInfo[cur_pro_idx].n_video;
        /*if (gst_structure_get_int(structure, "width", &width) &&
            gst_structure_get_int(structure, "height", &height))
        {
//            NXGLOGI("width(%d), height(%d)", width, height);
            handle->media_info->ProgramInfo[current_program_idx].StreamInfo->VideoInfo[0].width = width;
            handle->media_info->ProgramInfo[current_program_idx].StreamInfo->VideoInfo[0].height = height;
        }

        if (gst_structure_get_fraction(structure, "framerate", &num, &den))
        {
            NXGLOGI("framerate(%d/%d)", num, den);
            handle->media_info->ProgramInfo[current_program_idx].StreamInfo->VideoInfo[0].framerate_num = num;
            handle->media_info->ProgramInfo[current_program_idx].StreamInfo->VideoInfo[0].framerate_denom = den;
        }*/
        NXGLOGI("width(%d), height(%d), framerate(%d/%d)", width, height, num, den);

        target_sink = handle->typefind;
        NXGLOGV("element %s will be linked to %s\n",
                gst_element_get_name(element),
                gst_element_get_name(target_sink));

        target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
        NXGLOGI("%s to link %s:%s to %s:%s",
                (ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
                GST_DEBUG_PAD_NAME(pad),
                GST_DEBUG_PAD_NAME(target_sink_pad));
        gst_object_unref (target_sink_pad);
    }
    else if (g_str_has_prefix(mime_type, "audio/"))
    {
        /*gint audio_mpegversion, channels, samplerate;
		AUDIO_TYPE audio_type = get_audio_codec_type(mime_type);
		handle->media_info->ProgramInfo[current_program_idx].StreamInfo->AudioInfo[0].type = audio_type;
        if (gst_structure_get_int (structure, "mpegversion", &audio_mpegversion))
        {
			NXGLOGI("audio_mpegversion(%d)", audio_mpegversion);
            if (audio_mpegversion == 1) {
                handle->media_info->ProgramInfo[current_program_idx].StreamInfo->AudioInfo[0].type = AUDIO_TYPE_MPEG_V1;
            } else if (audio_mpegversion == 2) {
                handle->media_info->ProgramInfo[current_program_idx].StreamInfo->AudioInfo[0].type = AUDIO_TYPE_MPEG_V2;
            }
        }
        if (gst_structure_get_int(structure, "channels", &channels))
        {
			NXGLOGI("n_channels(%d)", channels);
            handle->media_info->ProgramInfo[current_program_idx].StreamInfo->AudioInfo[0].n_channels = channels;
        }
        if (gst_structure_get_int(structure, "rate", &samplerate))
        {
			NXGLOGI("samplerate(%d)", samplerate);
            handle->media_info->ProgramInfo[current_program_idx].StreamInfo->AudioInfo[0].samplerate = samplerate;
        }
        // tag - bitrate
        NXGLOGI("n_channels(%d), samplerate(%d), type(%d)",
                channels, samplerate,
                handle->media_info->ProgramInfo[current_program_idx].StreamInfo->AudioInfo[0].type);*/

        target_sink = handle->typefind;
        NXGLOGV("element %s will be linked to %s\n",
                gst_element_get_name(element),
                gst_element_get_name(target_sink));

        target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
        GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
        NXGLOGI("%s to link %s:%s to %s:%s",
                (ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
                GST_DEBUG_PAD_NAME(pad),
                GST_DEBUG_PAD_NAME(target_sink_pad));
        gst_object_unref (target_sink_pad);
    }
    gst_caps_unref (caps);
}

gint
get_program_info(const char* filePath, struct GST_MEDIA_INFO *media_info)
{
    // Initialize struct 'MpegTsSt'
    MpegTsSt handle;
    memset(&handle, 0, sizeof(MpegTsSt));
    handle.media_info = media_info;

    GError *error = NULL;

    NXGLOGI();

    // init GStreamer
    if(!gst_is_initialized()) {
        gst_init(NULL, NULL);
    }

    // init mpegts library
    gst_mpegts_initialize();

    handle.loop = g_main_loop_new (NULL, FALSE);

    //  gst-launch-1.0 -v filesrc location=/tmp/media/sda1/VIDEO_MPEG2/bbc010906.ts ! tsdemux name=demux demux. ! queue !  decodebin ! typefind ! fakesink
    // Create file source and typefind element
    handle.pipeline = gst_pipeline_new("pipe");
    handle.filesrc = gst_element_factory_make ("filesrc", "source");
    g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);
    handle.demuxer = gst_element_factory_make ("tsdemux", "tsdemux");
    g_signal_connect(handle.demuxer, "pad-added",
                    G_CALLBACK(demux_pad_added), &handle);
    handle.video_queue = gst_element_factory_make ("queue2", "queue2");
    handle.decodebin = gst_element_factory_make ("decodebin", "decodebin");
    handle.typefind = gst_element_factory_make ("typefind", "typefind");
    g_signal_connect(handle.decodebin, "pad-added",
                    G_CALLBACK(decodebin_pad_added), &handle);
    handle.fakesink = gst_element_factory_make ("fakesink", "sink");

    // Add & link elements
    gst_bin_add_many (GST_BIN (handle.pipeline), handle.filesrc,
                        handle.demuxer, handle.video_queue,
                        handle.decodebin, handle.typefind, handle.fakesink, NULL);

    if (!gst_element_link_many (handle.filesrc, handle.demuxer, NULL))
        NXGLOGE("Failed to link filesrc<-->demuxer");
    if (!gst_element_link_many (handle.video_queue, handle.decodebin, NULL))
        NXGLOGE("Failed to link video_parser<-->decodebin");
    if (!gst_element_link_many (handle.typefind, handle.fakesink, NULL))
        NXGLOGE("Failed to link typefind<-->fakesink");

    /* Put a bus handler */
    handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
    gst_bus_add_signal_watch (handle.bus);
    g_signal_connect (handle.bus, "message", (GCallback) _on_bus_message, &handle);

    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_RUNNING_STATUS);
    g_type_class_ref (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_STREAM_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);
    g_type_class_ref (GST_TYPE_MPEGTS_MODULATION_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_CODE_RATE);
    g_type_class_ref (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME);
    g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE);
    g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL);
    g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT);
    g_type_class_ref (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI);
    g_type_class_ref (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE);
    g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);

    // Set the state to PLAYING
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_PLAYING);
    g_main_loop_run (handle.loop);

    // unset
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (handle.pipeline));
    gst_object_unref (GST_OBJECT (handle.bus));

    return 0;
}