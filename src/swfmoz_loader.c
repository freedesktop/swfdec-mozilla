/* Swfdec Mozilla Plugin
 * Copyright (C) 2006 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "swfmoz_loader.h"
#include "plugin.h"

/* Note about refcounting:
 * Refcounts to a SwfdecLoader are held by:
 * - The SwfdecPlayer (until he releases it which can be at any point in time)
 * - The NPStream (released in NPP_DestroyStream)
 * - if the stream was created using NPN_Get/PostURLNotify, the notifyData 
 *   holds a reference. This reference is released by NPP_URLNotify
 * It's necessary to keep this many references since there's a lot of possible
 * orders in which these events happen.
 */

G_DEFINE_TYPE (SwfmozLoader, swfmoz_loader, SWFDEC_TYPE_LOADER)

static void
swfmoz_loader_dispose (GObject *object)
{
  SwfmozLoader *loader = SWFMOZ_LOADER (object);

  g_free (loader->cache_file);

  G_OBJECT_CLASS (swfmoz_loader_parent_class)->dispose (object);
}

static void
swfmoz_loader_load (SwfdecLoader *loader, SwfdecLoader *parent, 
    SwfdecLoaderRequest request, const char *data, gsize data_len)
{
  SwfmozLoader *moz = SWFMOZ_LOADER (loader);
  const char *url;

  moz->instance = SWFMOZ_LOADER (parent)->instance;
  g_object_ref (moz);
  url = swfdec_url_get_url (swfdec_loader_get_url (loader));
  if (request == SWFDEC_LOADER_REQUEST_POST) {
    plugin_post_url_notify (moz->instance, url, NULL, data, data_len, moz);
  } else {
    plugin_get_url_notify (moz->instance, url, NULL, moz);
  }
}

static void
swfmoz_loader_close (SwfdecLoader *loader)
{
  SwfmozLoader *moz = SWFMOZ_LOADER (loader);

  if (moz->stream)
    plugin_destroy_stream (moz->instance, moz->stream);
}

static void
swfmoz_loader_class_init (SwfmozLoaderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  SwfdecLoaderClass *loader_class = SWFDEC_LOADER_CLASS (klass);

  object_class->dispose = swfmoz_loader_dispose;

  loader_class->load = swfmoz_loader_load;
  loader_class->close = swfmoz_loader_close;
}

static void
swfmoz_loader_init (SwfmozLoader *slow_loader)
{
}

void
swfmoz_loader_set_stream (SwfmozLoader *loader, NPP instance, NPStream *stream)
{
  g_return_if_fail (SWFMOZ_IS_LOADER (loader));
  g_return_if_fail (loader->stream == NULL);
  g_return_if_fail (instance != NULL);
  g_return_if_fail (stream != NULL);

  g_printerr ("Loading stream: %s\n", stream->url);
  loader->instance = instance;
  loader->stream = stream;
  if (stream->end)
    swfdec_loader_set_size (SWFDEC_LOADER (loader), stream->end);
}

void
swfmoz_loader_ensure_open (SwfmozLoader *loader)
{
  g_return_if_fail (SWFMOZ_IS_LOADER (loader));

  if (loader->open)
    return;
  swfdec_loader_open (SWFDEC_LOADER (loader), loader->stream->url);
  loader->open = TRUE;
}

SwfdecLoader *
swfmoz_loader_new (NPP instance, NPStream *stream)
{
  SwfmozLoader *ret;
  SwfdecURL *url;

  g_return_val_if_fail (stream != NULL, NULL);

  url = swfdec_url_new (stream->url);
  ret = g_object_new (SWFMOZ_TYPE_LOADER, "url", url, NULL);
  swfdec_url_free (url);
  swfmoz_loader_set_stream (ret, instance, stream);

  return SWFDEC_LOADER (ret);
}

const char *
swfmoz_loader_get_data_type_string (SwfdecLoader *loader)
{
  g_return_val_if_fail (SWFDEC_IS_LOADER (loader), NULL);

  switch (swfdec_loader_get_data_type (loader)) {
    case SWFDEC_LOADER_DATA_UNKNOWN:
      return "Unknown Data";
    case SWFDEC_LOADER_DATA_SWF:
      /* FIXME: what's a useful name for flash movies? */
      return "Flash Movie";
    case SWFDEC_LOADER_DATA_FLV:
      return "Flash Video";
    case SWFDEC_LOADER_DATA_TEXT:
      return "Text";
    case SWFDEC_LOADER_DATA_XML:
      return "XML Data";
    default:
      g_printerr ("unknown data type %u\n", 
	  (guint) swfdec_loader_get_data_type (loader));
      return "You should never read this";
  }
}

