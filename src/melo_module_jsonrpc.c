/*
 * melo_module_jsonrpc.c: Module base JSON-RPC interface
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

#include "melo_module.h"
#include "melo_jsonrpc.h"

/* Method callbacks */
static void
melo_module_jsonrpc_get_list (const gchar *method,
                              JsonArray *s_params, JsonNode *params,
                              JsonNode **result, JsonNode **error,
                              gpointer user_data)
{
  JsonArray *array;
  const gchar *id;
  GList *list;
  GList *l;

  /* Get module list */
  list = melo_module_get_module_list ();

  /* Generate list */
  array = json_array_new ();
  for (l = list; l != NULL; l = l->next) {
    id = melo_module_get_id ((MeloModule *) l->data);
    json_array_add_string_element (array, id);
  }

  /* Free module list */
  g_list_free_full (list, g_object_unref);

  /* Return result */
  *result = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (*result, array);
}

/* List of methods */
static MeloJSONRPCMethod melo_module_jsonrpc_methods[] = {
  {
    .method = "get_list",
    .params = "[]",
    .result = "{\"type\":\"array\"}",
    .callback = melo_module_jsonrpc_get_list,
    .user_data = NULL,
  },
};

/* Register / Unregister methods */
void
melo_module_register_methods (void)
{
  melo_jsonrpc_register_methods ("module", melo_module_jsonrpc_methods,
                                 G_N_ELEMENTS (melo_module_jsonrpc_methods));
}

void
melo_module_unregister_methods (void)
{
  melo_jsonrpc_unregister_methods ("module", melo_module_jsonrpc_methods,
                                   G_N_ELEMENTS (melo_module_jsonrpc_methods));
}
