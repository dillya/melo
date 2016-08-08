/*
 * gstraopclientclock.c: clock that synchronizes with RAOP NTP clock
 *
 * Copyright (C) 2016 Alexandre Dilly <dillya@sparod.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <gio/gio.h>
#include <gst/net/gstnet.h>

#include <string.h>

#include "gstraopclientclock.h"

#define RAOP_PACKET_SIZE        32

#define LOCAL_ADDRESS           "127.0.0.1"
#define LOCAL_PORT              7000
#define DEFAULT_ADDRESS         "127.0.0.1"
#define DEFAULT_PORT            6002
#define DEFAULT_ROUNDTRIP_LIMIT GST_SECOND
#define DEFAULT_MINIMUM_UPDATE_INTERVAL (GST_SECOND / 20)
#define DEFAULT_BASE_TIME       0
enum
{
  PROP_0,
  PROP_PORT,
  PROP_ADDRESS,
  PROP_ROUNDTRIP_LIMIT,
  PROP_MINIMUM_UPDATE_INTERVAL,
  PROP_BUS,
  PROP_BASE_TIME,
  PROP_INTERNAL_CLOCK
};

#define GST_RAOP_CLIENT_CLOCK_GET_PRIVATE(obj)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_RAOP_CLIENT_CLOCK, GstRaopClientClockPrivate))

struct _GstRaopClientClockPrivate {
  /* Net clock client */
  GstClockTime base_time;
  GstClock *clock;

  /* Net -> RAOP NTP proxy */
  gchar *address;
  int port;
  GThread *thread;
  GSocket *socket;
  GCancellable *cancel;
  gboolean made_cancel_fd;
};

G_DEFINE_TYPE (GstRaopClientClock, gst_raop_client_clock, GST_TYPE_SYSTEM_CLOCK);

static void gst_raop_client_clock_finalize (GObject * object);
static void gst_raop_client_clock_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_raop_client_clock_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_raop_client_clock_constructed (GObject * object);

static GstClockTime gst_raop_client_clock_get_internal_time (GstClock * clock);

static gboolean gst_raop_client_clock_start (GstRaopClientClock * self,
    const gchar *address, guint16 *port);
static void gst_raop_client_clock_stop (GstRaopClientClock * self);

static void
gst_raop_client_clock_class_init (GstRaopClientClockClass * klass)
{
  GObjectClass *gobject_class;
  GstClockClass *clock_class;

  gobject_class = G_OBJECT_CLASS (klass);
  clock_class = GST_CLOCK_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GstRaopClientClockPrivate));

  gobject_class->finalize = gst_raop_client_clock_finalize;
  gobject_class->get_property = gst_raop_client_clock_get_property;
  gobject_class->set_property = gst_raop_client_clock_set_property;
  gobject_class->constructed = gst_raop_client_clock_constructed;

  g_object_class_install_property (gobject_class, PROP_ADDRESS,
      g_param_spec_string ("address", "address",
          "The IP address of the machine providing a time server",
          DEFAULT_ADDRESS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "port",
          "The port on which the remote server is listening", 0, G_MAXUINT16,
          DEFAULT_PORT,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUS,
      g_param_spec_object ("bus", "bus",
          "A GstBus on which to send clock status information", GST_TYPE_BUS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ROUNDTRIP_LIMIT,
      g_param_spec_uint64 ("round-trip-limit", "round-trip limit",
          "Maximum tolerable round-trip interval for packets, in nanoseconds "
          "(0 = no limit)", 0, G_MAXUINT64, DEFAULT_ROUNDTRIP_LIMIT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MINIMUM_UPDATE_INTERVAL,
      g_param_spec_uint64 ("minimum-update-interval", "minimum update interval",
          "Minimum polling interval for packets, in nanoseconds"
          "(0 = no limit)", 0, G_MAXUINT64, DEFAULT_MINIMUM_UPDATE_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BASE_TIME,
      g_param_spec_uint64 ("base-time", "Base Time",
          "Initial time that is reported before synchronization", 0,
          G_MAXUINT64, DEFAULT_BASE_TIME,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTERNAL_CLOCK,
      g_param_spec_object ("internal-clock", "Internal Clock",
          "Internal clock that directly slaved to the remote clock",
          GST_TYPE_CLOCK, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  clock_class->get_internal_time = gst_raop_client_clock_get_internal_time;
}

static void
gst_raop_client_clock_init (GstRaopClientClock * self)
{
  GstRaopClientClockPrivate *priv;
  GstClock *clock;

  self->priv = priv = GST_RAOP_CLIENT_CLOCK_GET_PRIVATE (self);

  GST_OBJECT_FLAG_SET (self, GST_CLOCK_FLAG_CAN_SET_MASTER);
//  GST_OBJECT_FLAG_SET (self, GST_CLOCK_FLAG_NEEDS_STARTUP_SYNC);

  priv->port = DEFAULT_PORT;
  priv->address = g_strdup (DEFAULT_ADDRESS);

  priv->base_time = DEFAULT_BASE_TIME;
}

static void
gst_raop_client_clock_finalize (GObject * object)
{
  GstRaopClientClock *self = GST_RAOP_CLIENT_CLOCK (object);

  /* Free net client clock */
  g_object_unref (self->priv->clock);

  /* Stop proxy server */
  if (self->priv->thread) {
    gst_raop_client_clock_stop (self);
    g_assert (self->priv->thread == NULL);
  }

  g_free (self->priv->address);
  self->priv->address = NULL;

  G_OBJECT_CLASS (gst_raop_client_clock_parent_class)->finalize (object);
}

static void
gst_raop_client_clock_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRaopClientClock *self = GST_RAOP_CLIENT_CLOCK (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      GST_OBJECT_LOCK (self);
      g_free (self->priv->address);
      self->priv->address = g_value_dup_string (value);
      if (self->priv->address == NULL)
        self->priv->address = g_strdup (DEFAULT_ADDRESS);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PORT:
      GST_OBJECT_LOCK (self);
      self->priv->port = g_value_get_int (value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_ROUNDTRIP_LIMIT:
      GST_OBJECT_LOCK (self);
      g_object_set_property (G_OBJECT (self->priv->clock), "round-trip-limit",
           value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_MINIMUM_UPDATE_INTERVAL:
      GST_OBJECT_LOCK (self);
      g_object_set_property (G_OBJECT (self->priv->clock),
          "minimum-update-interval", value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_BUS:
      GST_OBJECT_LOCK (self);
      g_object_set_property (G_OBJECT (self->priv->clock), "bus", value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_BASE_TIME:
      if (!self->priv->clock)
        self->priv->base_time = 
        self->priv->base_time = g_value_get_uint64 (value);
      else
        g_object_set_property (G_OBJECT (self->priv->clock), "base-time",
            value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_raop_client_clock_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRaopClientClock *self = GST_RAOP_CLIENT_CLOCK (object);

  switch (prop_id) {
    case PROP_ADDRESS:
      GST_OBJECT_LOCK (self);
      g_value_set_string (value, self->priv->address);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_PORT:
      g_value_set_int (value, self->priv->port);
      break;
    case PROP_ROUNDTRIP_LIMIT:
      GST_OBJECT_LOCK (self);
      g_object_get_property (G_OBJECT (self->priv->clock), "round-trip-limit",
           value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_MINIMUM_UPDATE_INTERVAL:
      GST_OBJECT_LOCK (self);
      g_object_get_property (G_OBJECT (self->priv->clock),
           "minimum-update-interval", value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_BUS:
      GST_OBJECT_LOCK (self);
      g_object_get_property (G_OBJECT (self->priv->clock), "bus", value);
      GST_OBJECT_UNLOCK (self);
      break;
    case PROP_BASE_TIME:
      if (!self->priv->clock)
        g_value_set_uint64 (value, self->priv->base_time);
      else
        g_object_get_property (G_OBJECT (self->priv->clock), "base-time",
            value);
      break;
    case PROP_INTERNAL_CLOCK:
      g_object_get_property (G_OBJECT (self->priv->clock), "internal-clock",
          value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_raop_client_clock_constructed (GObject * object)
{
  GstRaopClientClock *self = GST_RAOP_CLIENT_CLOCK (object);
  guint16 port = LOCAL_PORT;

  G_OBJECT_CLASS (gst_raop_client_clock_parent_class)->constructed (object);

  /* Create proxy server */
  gst_raop_client_clock_start (self, LOCAL_ADDRESS, &port);

  /* Create new net client clock */
  self->priv->clock = gst_net_client_clock_new ("raop_clock", LOCAL_ADDRESS,
      port, self->priv->base_time);
//  g_object_set (G_OBJECT (self->priv->clock), "minimum-update-interval", 3000000000LL, NULL);
}

static GstClockTime
gst_raop_client_clock_get_internal_time (GstClock * clock)
{
  GstRaopClientClock *self = GST_RAOP_CLIENT_CLOCK (clock);

  return gst_clock_get_internal_time (self->priv->clock);
}

static inline guint32
gst_clock_time_to_ntp_timestamp_seconds (GstClockTime gst)
{
  GstClockTime seconds = gst_util_uint64_scale (gst, 1, GST_SECOND);

  return seconds;
}

static inline guint32
gst_clock_time_to_ntp_timestamp_fraction (GstClockTime gst)
{
  GstClockTime seconds = gst_util_uint64_scale (gst, 1, GST_SECOND);

  return gst_util_uint64_scale (gst - seconds, G_GUINT64_CONSTANT (1) << 32,
      GST_SECOND);
}

static inline GstClockTime
ntp_timestamp_to_gst_clock_time (guint32 seconds, guint32 fraction)
{
  return gst_util_uint64_scale (seconds, GST_SECOND, 1) +
      gst_util_uint64_scale (fraction, GST_SECOND,
      G_GUINT64_CONSTANT (1) << 32);
}


static gpointer
gst_raop_client_clock_thread (gpointer data)
{
  GstRaopClientClock *self = data;
  GCancellable *cancel = self->priv->cancel;
  GSocket *socket = self->priv->socket;
  GSocketAddress *remote_addr;
  GSocketAddress *any_addr;
  GInetAddress *inet_addr;
  GSocketFamily family;
  GSocket *remote_socket;
  GstNetTimePacket *packet;
  GError *err = NULL;
  gssize res;

  /* create remote address */
  inet_addr = g_inet_address_new_from_string (self->priv->address);
  if (!inet_addr) {
    GST_ERROR_OBJECT (self, "cannot connect to %s:%d", self->priv->address,
        self->priv->port);
    goto end;
  }

  GST_DEBUG_OBJECT (self, "will communicate with %s:%d", self->priv->address,
      self->priv->port);

  /* get inet family */
  family = g_inet_address_get_family (inet_addr);

  /* create remote address with port */
  remote_addr = g_inet_socket_address_new (inet_addr, self->priv->port);
  g_object_unref (inet_addr);
  g_assert (remote_addr != NULL);

  /* create remote socket */
  remote_socket = g_socket_new (family, G_SOCKET_TYPE_DATAGRAM,
      G_SOCKET_PROTOCOL_UDP, &err);
  if (!remote_socket) {
    GST_ERROR_OBJECT (self, "failed to create remote socket");
    g_object_unref (inet_addr);
    g_clear_error (&err);
    goto end;
  }

  GST_DEBUG_OBJECT (self, "binding remote socket");

  /* bind socket */
  inet_addr = g_inet_address_new_any (family);
  any_addr = g_inet_socket_address_new (inet_addr, 0);
  g_socket_bind (remote_socket, any_addr, TRUE, &err);
  g_object_unref (any_addr);
  g_object_unref (inet_addr);
  if (err) {
    GST_ERROR_OBJECT (self, "bind failed: %s", err->message);
    g_object_unref (remote_socket);
    g_clear_error (&err);
    goto end;
  }

  GST_INFO_OBJECT (self, "RAOP client clock thread is running");

  while (TRUE) {
    GSocketAddress *sender_addr = NULL;
    guchar raop_packet[RAOP_PACKET_SIZE];

    GST_LOG_OBJECT (self, "waiting on socket");
    if (!g_socket_condition_wait (socket, G_IO_IN, cancel, &err)) {
      GST_INFO_OBJECT (self, "socket error: %s", err->message);

      if (err->code == G_IO_ERROR_CANCELLED)
        break;

      /* try again */
      g_usleep (G_USEC_PER_SEC / 10);
      g_error_free (err);
      err = NULL;
      continue;
    }

    /* got data in */
    packet = gst_net_time_packet_receive (socket, &sender_addr, &err);

    if (err != NULL) {
      GST_DEBUG_OBJECT (self, "receive error: %s", err->message);
      g_usleep (G_USEC_PER_SEC / 10);
      g_error_free (err);
      err = NULL;
      continue;
    }

    /* send RAOP timing request packet */
    memset (raop_packet, 0, RAOP_PACKET_SIZE);
    raop_packet[0] = 0x80;
    raop_packet[1] = 0xd2;
    raop_packet[3] = 0x07;
    *((guint32*) &raop_packet[24]) = g_htonl (gst_clock_time_to_ntp_timestamp_seconds (packet->local_time));
    *((guint32*) &raop_packet[28]) = g_htonl (gst_clock_time_to_ntp_timestamp_fraction (packet->local_time));

    res = g_socket_send_to (remote_socket, G_SOCKET_ADDRESS (remote_addr),
        raop_packet, RAOP_PACKET_SIZE, NULL, &err);
    if (res < 0) {
      GST_DEBUG_OBJECT (self, "RAOP send error: %s", err->message);
      g_error_free (err);
      break;
    }

    /* wait response */
    if (!g_socket_condition_timed_wait (remote_socket, G_IO_IN, G_USEC_PER_SEC,
            cancel, &err)) {
      GST_INFO_OBJECT (self, "remote socket error: %s", err->message);

      if (err->code == G_IO_ERROR_CANCELLED)
        break;
      continue;
    }
    res = g_socket_receive_from (remote_socket,
        NULL, raop_packet, RAOP_PACKET_SIZE, NULL,
        &err);
    if (res < 0 || res < RAOP_PACKET_SIZE)
      break;
{
    guint32 seconds = g_ntohl (*((guint32 *) &raop_packet[24]));
    guint32 fraction = g_ntohl (*((guint32 *) &raop_packet[28]));

    packet->remote_time = ntp_timestamp_to_gst_clock_time (seconds, fraction);
}
    /* add remote time to packet */
    //packet->remote_time = 0;//gst_clock_get_time (clock);

    /* ignore errors */
    gst_net_time_packet_send (packet, socket, sender_addr, NULL);
    g_object_unref (sender_addr);
    g_free (packet);
  }

  /* close remote socket */
  g_object_unref (remote_socket);
  g_object_unref (remote_addr);

  g_error_free (err);

end:
  GST_INFO_OBJECT (self, "RAOP client clock thread is stopping");
  return NULL;
}

static gboolean
gst_raop_client_clock_start (GstRaopClientClock * self, const gchar *address,
    guint16 *port)
{
  GSocketAddress *socket_addr, *bound_addr;
  GInetAddress *inet_addr;
  GPollFD dummy_pollfd;
  GSocket *socket;
  GError *err = NULL;

  /* Create server address */
  inet_addr = g_inet_address_new_from_string (address);
  if (!inet_addr)
    goto invalid_address;

  /* Create socket */
  GST_LOG_OBJECT (self, "creating socket");
  socket = g_socket_new (g_inet_address_get_family (inet_addr),
      G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &err);
  if (!socket)
    goto no_socket;

  /* Bind socket */
  GST_DEBUG_OBJECT (self, "binding on port %d", port);
  socket_addr = g_inet_socket_address_new (inet_addr, *port);
  if (!g_socket_bind (socket, socket_addr, TRUE, &err)) {
    g_object_unref (socket_addr);
    g_object_unref (inet_addr);
    goto bind_error;
  }
  g_object_unref (socket_addr);
  g_object_unref (inet_addr);

#if 0
  bound_addr = g_socket_get_local_address (socket, NULL);
  port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (bound_addr));
  inet_addr =
      g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (bound_addr));
  address = g_inet_address_to_string (inet_addr);

  if (g_strcmp0 (address, self->priv->address)) {
    g_free (self->priv->address);
    self->priv->address = address;
    GST_DEBUG_OBJECT (self, "notifying address %s", address);
    g_object_notify (G_OBJECT (self), "address");
  } else {
    g_free (address);
  }
  if (port != self->priv->port) {
    self->priv->port = port;
    GST_DEBUG_OBJECT (self, "notifying port %d", port);
    g_object_notify (G_OBJECT (self), "port");
  }
  GST_DEBUG_OBJECT (self, "bound on UDP address %s, port %d",
      self->priv->address, port);
  g_object_unref (bound_addr);
#endif

  /* Create poll */
  self->priv->socket = socket;
  self->priv->cancel = g_cancellable_new ();
  self->priv->made_cancel_fd =
      g_cancellable_make_pollfd (self->priv->cancel, &dummy_pollfd);

  /* Create thread */
  self->priv->thread = g_thread_try_new ("GstRaopClockClient",
      gst_raop_client_clock_thread, self, &err);
  if (!self->priv->thread)
    goto no_thread;

  return TRUE;

invalid_address:
  {
    GST_ERROR_OBJECT (self, "invalid address: %s", self->priv->address);
    g_clear_error (&err);
    return FALSE;
  }
no_socket:
  {
    GST_ERROR_OBJECT (self, "could not create socket: %s", err->message);
    g_clear_error (&err);
    g_object_unref (inet_addr);
    return FALSE;
  }
bind_error:
  {
    GST_ERROR_OBJECT (self, "bind failed: %s", err->message);
    g_clear_error (&err);
    g_object_unref (socket);
    return FALSE;
  }
no_thread:
  {
    GST_ERROR_OBJECT (self, "could not create thread: %s", err->message);
    g_clear_error (&err);
    g_object_unref (self->priv->socket);
    self->priv->socket = NULL;
    g_object_unref (self->priv->cancel);
    self->priv->cancel = NULL;
    return FALSE;
  }
}

static void
gst_raop_client_clock_stop (GstRaopClientClock * self)
{
  g_return_if_fail (self->priv->thread != NULL);

  GST_INFO_OBJECT (self, "stopping..");
  g_cancellable_cancel (self->priv->cancel);

  g_thread_join (self->priv->thread);
  self->priv->thread = NULL;

  if (self->priv->made_cancel_fd)
    g_cancellable_release_fd (self->priv->cancel);

  g_object_unref (self->priv->cancel);
  self->priv->cancel = NULL;

  g_object_unref (self->priv->socket);
  self->priv->socket = NULL;

  GST_INFO_OBJECT (self, "stopped");
}

GstClock *
gst_raop_client_clock_new (const gchar * name, const gchar * remote_address,
    gint remote_port, GstClockTime base_time)
{
  GstClock *ret;

  g_return_val_if_fail (remote_address != NULL, NULL);
  g_return_val_if_fail (remote_port > 0, NULL);
  g_return_val_if_fail (remote_port <= G_MAXUINT16, NULL);
  g_return_val_if_fail (base_time != GST_CLOCK_TIME_NONE, NULL);

  ret = g_object_new (GST_TYPE_RAOP_CLIENT_CLOCK, "address", remote_address,
      "port", remote_port, "base-time", base_time, NULL);

  return ret;
}
