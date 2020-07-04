/*
 * Copyright (C) 2016-2020 Alexandre Dilly <dillya@sparod.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#ifndef _MELO_MDNS_H_
#define _MELO_MDNS_H_

#include <stdbool.h>

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * MeloMdns:
 *
 * The opaque #MeloMdns data structure.
 */

typedef struct _MeloMdnsService MeloMdnsService;

#define MELO_TYPE_MDNS (melo_mdns_get_type ())
G_DECLARE_FINAL_TYPE (MeloMdns, melo_mdns, MELO, MDNS, GObject)

/**
 * MeloMdnsService:
 * @name: the service name
 * @type: the type of service (protocol like "_http._tcp")
 * @port: the service port number
 * @txt: the service TXT record (opaque structure)
 * @is_ipv6: the service uses IPv6 protocol
 * @ip4: the service IPv4 address
 * @ip6: the service IPv6 address
 * @iface: the network interface on which service is listening
 *
 * A #MeloMdnsService describe a Zeroconf / mDNS service registered on the
 * network.
 */
struct _MeloMdnsService {
  char *name;
  char *type;
  int port;
  void *txt;
  bool is_ipv6;
  union {
    unsigned char ipv4[4];
    unsigned char ipv6[16];
  };
  int iface;
};

GType melo_mdns_get_type (void);

MeloMdns *melo_mdns_new (void);

/* Service publisher */
const MeloMdnsService *melo_mdns_add_service (
    MeloMdns *mdns, const char *name, const char *type, int port, ...);
bool melo_mdns_update_service (MeloMdns *mdns, const MeloMdnsService *service,
    const char *name, const char *type, int port, bool update_txt, ...);
void melo_mdns_remove_service (MeloMdns *mdns, const MeloMdnsService *service);

/* Service browser */
bool melo_mdns_add_browser (MeloMdns *mdns, const char *type);
GList *melo_mdns_list_services (MeloMdns *mdns);
void melo_mdns_remove_browser (MeloMdns *mdns, const char *type);

char *melo_mdns_service_get_txt (const MeloMdnsService *s, const char *key);
MeloMdnsService *melo_mdns_service_copy (const MeloMdnsService *s);
void melo_mdns_service_free (MeloMdnsService *s);

G_END_DECLS

#endif /* !_MELO_MDNS_H_ */
