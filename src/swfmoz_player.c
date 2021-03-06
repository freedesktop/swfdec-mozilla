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

#include <math.h>
#include "plugin.h"
#include "swfmoz_player.h"
#include "swfmoz_loader.h"

#include "swfmoz_dialog.h"

/*** menu ***/

static void
swfmoz_player_menu_playing_toggled (GtkCheckMenuItem *item, SwfmozPlayer* player)
{
  gboolean menu_item_active = gtk_check_menu_item_get_active (item);

  if (swfdec_gtk_player_get_playing (SWFDEC_GTK_PLAYER (player)) != menu_item_active) {
      swfdec_gtk_player_set_playing (SWFDEC_GTK_PLAYER (player), menu_item_active);
      swfmoz_config_set_autoplay (player->config, swfdec_player_get_url (SWFDEC_PLAYER (player)), menu_item_active);
  }
}

static void
swfmoz_player_menu_notify_playing (SwfdecGtkPlayer *player, GParamSpec *pspec,
    GtkCheckMenuItem *item)
{
  if (swfdec_gtk_player_get_playing (player) != gtk_check_menu_item_get_active (item))
    gtk_check_menu_item_set_active (item, swfdec_gtk_player_get_playing (player));
}

static void
swfmoz_player_menu_audio_toggled (GtkCheckMenuItem *item, SwfdecGtkPlayer* player)
{
  if (swfdec_gtk_player_get_audio_enabled (player) != gtk_check_menu_item_get_active (item))
    swfdec_gtk_player_set_audio_enabled (player, gtk_check_menu_item_get_active (item));
}

static void
swfmoz_player_menu_notify_audio (SwfdecGtkPlayer *player, GParamSpec *pspec,
    GtkCheckMenuItem *item)
{
  if (swfdec_gtk_player_get_audio_enabled (player) != gtk_check_menu_item_get_active (item))
    gtk_check_menu_item_set_active (item, swfdec_gtk_player_get_audio_enabled (player));
}

static void
swfmoz_player_menu_autoplay_always_toggled (GtkCheckMenuItem *item,
    SwfmozPlayer* player)
{
  gboolean item_active = gtk_check_menu_item_get_active (item);
  swfmoz_config_set_global_autoplay (player->config, item_active);
}

static void
swfmoz_player_menu_autoplay_remember_last_toggled (GtkCheckMenuItem *item,
    SwfmozPlayer* player)
{
  gboolean item_active = gtk_check_menu_item_get_active (item);
  if (item_active && swfmoz_config_has_global_key (player->config))
    swfmoz_config_remove_global_autoplay (player->config);
}

static void
swfmoz_player_menu_autoplay_never_toggled (GtkCheckMenuItem *item,
    SwfmozPlayer* player)
{
  gboolean item_active = gtk_check_menu_item_get_active (item);
  swfmoz_config_set_global_autoplay (player->config, !item_active);
}

static void
swfmoz_player_menu_about (GtkMenuItem *item, SwfmozPlayer *player)
{
  static const char *authors[] = {
    "Benjamin Otte <otte@gnome.org>",
    "David Schleef <ds@schleef.org>",
    "Eric Anholt <eric@anholt.net>",
    "Pekka Lampila <pekka.lampila@iki.fi>",
    NULL,
  };
  static const char *artists[] = {
    "Andreas Nilsson <andreas@andreasn.se>",
    "Cristian Grada <krigenator@gmail.com>",
    NULL
  };

  gtk_show_about_dialog (NULL, "program-name", "Swfdec",
      "logo-icon-name", "swfdec-mozilla",
      "authors", authors,
      "artists", artists,
      "comments", "Play Flash content in your browser",
      "name", "Swfdec Mozilla Plugin",
      "version", VERSION,
      "website", "http://swfdec.freedesktop.org/",
      NULL);
}

static void
swfmoz_player_popup_menu (SwfmozPlayer *player)
{
  if (player->menu == NULL) {
    GtkWidget *item, *submenu;

    player->menu = GTK_MENU (gtk_menu_new ());
    g_object_ref_sink (player->menu);
    
    item = gtk_check_menu_item_new_with_mnemonic ("Playing");
    g_signal_connect (item, "toggled",
	G_CALLBACK (swfmoz_player_menu_playing_toggled), player);
    g_signal_connect (player, "notify::playing",
	G_CALLBACK (swfmoz_player_menu_notify_playing), item);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
	swfdec_gtk_player_get_playing (SWFDEC_GTK_PLAYER (player)));
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (player->menu), item);

    item = gtk_check_menu_item_new_with_mnemonic ("Enable Audio");
    g_signal_connect (item, "toggled",
	G_CALLBACK (swfmoz_player_menu_audio_toggled), player);
    g_signal_connect (player, "notify::audio-enabled",
	G_CALLBACK (swfmoz_player_menu_notify_audio), item);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
	swfdec_gtk_player_get_audio_enabled (SWFDEC_GTK_PLAYER (player)));
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (player->menu), item);

    submenu = gtk_menu_new();
    item = gtk_radio_menu_item_new_with_mnemonic (NULL, "Always");
    g_signal_connect (item, "toggled",
        G_CALLBACK (swfmoz_player_menu_autoplay_always_toggled), player);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
	swfmoz_config_read_autoplay (player->config, "global", FALSE));
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

    item = gtk_radio_menu_item_new_with_mnemonic_from_widget (GTK_RADIO_MENU_ITEM (item),
        "Remember last choice");
    g_signal_connect (item, "toggled",
	G_CALLBACK (swfmoz_player_menu_autoplay_remember_last_toggled), player);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
        !swfmoz_config_has_global_key (player->config));
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

    item = gtk_radio_menu_item_new_with_mnemonic_from_widget (GTK_RADIO_MENU_ITEM (item), "Never");
    g_signal_connect (item, "toggled",
        G_CALLBACK (swfmoz_player_menu_autoplay_never_toggled), player);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
	!swfmoz_config_read_autoplay (player->config, "global", TRUE));
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);

    item = gtk_menu_item_new_with_label ("Autoplay");
    gtk_widget_show (item);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (player->menu), item);

    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (player->menu), item);

    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
    g_signal_connect_swapped (item, "activate", 
	G_CALLBACK (swfmoz_dialog_show), player);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (player->menu), item);

    item = gtk_separator_menu_item_new ();
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (player->menu), item);

    item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
    g_signal_connect (item, "activate", 
	G_CALLBACK (swfmoz_player_menu_about), player);
    gtk_widget_show (item);
    gtk_menu_shell_append (GTK_MENU_SHELL (player->menu), item);
  }
  gtk_menu_popup (player->menu, NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

/*** SWFMOZ_PLAYER ***/

G_DEFINE_TYPE (SwfmozPlayer, swfmoz_player, SWFDEC_TYPE_GTK_PLAYER)

static gboolean
swfmoz_player_idle_redraw (gpointer playerp)
{
  SwfmozPlayer *player = playerp;
  GdkRegion *region;

  region = player->repaint;
  player->repaint = NULL;
  g_source_unref (player->repaint_source);
  player->repaint_source = NULL;
  if (player->windowless) {
    NPRect rect;
    GdkRectangle *rectangles;
    int i, n_rectangles;

    g_assert (player->repaint == NULL);
    
    gdk_region_get_rectangles (region, &rectangles, &n_rectangles);

    for (i = 0; i < n_rectangles; i++) {
      rect.left = rectangles[i].x;
      rect.top = rectangles[i].y;
      rect.right = rectangles[i].x + rectangles[i].width;
      rect.bottom = rectangles[i].y + rectangles[i].height;
      plugin_invalidate_rect (player->instance, &rect);
    }
  } else {
    swfmoz_player_render (player, NULL, region);
  }
  gdk_region_destroy (region);

  return FALSE;
}

static void
swfmoz_player_redraw (SwfmozPlayer *player,
    const SwfdecRectangle *rects, guint n_rects, gpointer unused)
{
  GdkRegion *region;
  guint i;

  if (player->target == NULL)
    return;

  if (player->repaint)
    region = player->repaint;
  else
    region = gdk_region_new ();

  for (i = 0; i < n_rects; i++) {
    gdk_region_union_with_rect (region, (GdkRectangle *) &rects[i]);
  }

  if (player->repaint_source) {
    g_assert (player->repaint);
  } else {
    GSource *source = g_idle_source_new ();
    player->repaint_source = source;
    g_source_set_priority (source, SWFDEC_GTK_PRIORITY_REDRAW);
    g_source_set_callback (source, swfmoz_player_idle_redraw, player, NULL);
    g_source_attach (source, player->context);
    player->repaint = region;
  }
}

static SwfdecBuffer *
swfmoz_player_format_headers (guint header_count, const char **header_names,
    const char **header_values, gsize content_length)
{
  GString *string;
  guint i;
  gsize len;

  g_return_val_if_fail (header_count == 0 || header_names != NULL, NULL);
  g_return_val_if_fail (header_count == 0 || header_values != NULL, NULL);

  string = g_string_new ("Content-Length: ");
  g_string_append_printf (string, "%"G_GSIZE_FORMAT, content_length);
  g_string_append (string, "\n");
  for (i = 0; i < header_count; i++) {
    g_string_append (string, header_names[i]);
    g_string_append (string, ": ");
    g_string_append (string, header_values[i]);
    g_string_append (string, "\n");
  }
  g_string_append (string, "\n");

  len = string->len;
  return swfdec_buffer_new_for_data (
      (unsigned char *)g_string_free (string, FALSE), len);
}

/* Returns a new buffer with the headers prepended, suitable for passing to
 * plugin_post_url_notify */
SwfdecBuffer *
swfmoz_player_add_headers (SwfdecBuffer *data, guint header_count,
    const char **header_names, const char **header_values)
{
  SwfdecBufferQueue *queue;
  SwfdecBuffer *buffer;

  g_return_val_if_fail (data != NULL, NULL);

  queue = swfdec_buffer_queue_new ();
  swfdec_buffer_queue_push (queue, swfmoz_player_format_headers (
	header_count, header_names, header_values, data->length));
  swfdec_buffer_queue_push (queue, swfdec_buffer_ref (data));

  buffer = swfdec_buffer_queue_pull (queue,
      swfdec_buffer_queue_get_depth (queue));
  swfdec_buffer_queue_unref (queue);

  return buffer;
}

static void
swfmoz_player_launch (SwfmozPlayer *player, const char *url,
    const char *target, SwfdecBuffer *data, guint header_count,
    const char **header_names, const char **header_values,  gpointer unused)
{
  if (data) {
  // use the notify version of the function so headers can be sent
  // we don't actually care about the notify
    plugin_post_url_notify (player->instance, url, target,
	(const char *)data->data, data->length, NULL);
  } else {
    // FIXME: no way to send headers here?
    plugin_get_url (player->instance, url, target);
  }
}

static void
swfmoz_player_invalidate (SwfmozPlayer *player)
{
  SwfdecRectangle rect = { 0, 0, player->target_rect.width, player->target_rect.height };
  swfmoz_player_redraw (player, &rect, 1, NULL);
}

/* function stolen from SwfdecGtkWidget */
static void
swfmoz_player_update_cursor (SwfmozPlayer *player)
{
  GdkWindow *window = player->target;
  GdkDisplay *display;
  SwfdecMouseCursor swfcursor;
  GdkCursor *cursor;

  /* FIXME: make this work for windowless mode */
  if (window == NULL || player->windowless)
    return;
  display = gdk_drawable_get_display (window);

  if (!swfdec_gtk_player_get_playing (SWFDEC_GTK_PLAYER (player))) {
    swfcursor = SWFDEC_MOUSE_CURSOR_CLICK;
  } else {
    g_object_get (player, "mouse-cursor", &swfcursor, NULL);
  }

  switch (swfcursor) {
    case SWFDEC_MOUSE_CURSOR_NONE:
      {
	GdkBitmap *bitmap;
	GdkColor color = { 0, 0, 0, 0 };
	char data = 0;

	bitmap = gdk_bitmap_create_from_data (window, &data, 1, 1);
	if (bitmap == NULL)
	  return;
	cursor = gdk_cursor_new_from_pixmap (bitmap, bitmap, &color, &color, 0, 0);
	gdk_window_set_cursor (window, cursor);
	gdk_cursor_unref (cursor);
	g_object_unref (bitmap);
	break;
      }
    case SWFDEC_MOUSE_CURSOR_TEXT:
      cursor = gdk_cursor_new_for_display (display, GDK_XTERM);
      gdk_window_set_cursor (window, cursor);
      gdk_cursor_unref (cursor);
      break;
    case SWFDEC_MOUSE_CURSOR_CLICK:
      cursor = gdk_cursor_new_for_display (display, GDK_HAND2);
      gdk_window_set_cursor (window, cursor);
      gdk_cursor_unref (cursor);
      break;
    case SWFDEC_MOUSE_CURSOR_NORMAL:
      cursor = gdk_cursor_new_for_display (display, GDK_LEFT_PTR);
      gdk_window_set_cursor (window, cursor);
      gdk_cursor_unref (cursor);
      break;
    default:
      g_warning ("invalid cursor %d", (int) swfcursor);
      gdk_window_set_cursor (window, NULL);
      break;
  }
}

static void
swfmoz_player_update_background (SwfmozPlayer *player)
{
  GdkWindow *window = player->target;
  GdkColor bgcolor;
  guint bg;

  if (player->windowless || window == NULL)
    return;

  bg = swfdec_player_get_background_color (SWFDEC_PLAYER (player));
  bgcolor.red = 0x101 * ((bg >> 16) & 0xFF);
  bgcolor.green = 0x101 * ((bg >> 8) & 0xFF);
  bgcolor.blue = 0x101 * (bg & 0xFF);
  gdk_rgb_find_color (gdk_drawable_get_colormap (window), &bgcolor);
  gdk_window_set_background (window, &bgcolor);
}

static void
swfmoz_player_fullscreen_destroyed (GtkWidget *widget, SwfmozPlayer *player)
{
  player->fullscreen = NULL;
  if (swfdec_player_get_fullscreen (SWFDEC_PLAYER (player))) {
    swfdec_player_set_focus (SWFDEC_PLAYER (player), TRUE);
    swfdec_player_key_press (SWFDEC_PLAYER (player), SWFDEC_KEY_ESCAPE, 0);
  }
  swfmoz_player_invalidate (player);
}

static void
swfmoz_player_notify (SwfmozPlayer *player, GParamSpec *pspec, gpointer unused)
{
  if (g_str_equal (pspec->name, "playing")) {
    swfmoz_player_update_cursor (player);
    swfmoz_player_invalidate (player);
  } else if (g_str_equal (pspec->name, "mouse-cursor")) {
    swfmoz_player_update_cursor (player);
  } else if (g_str_equal (pspec->name, "fullscreen")) {
    gboolean fullscreen = swfdec_player_get_fullscreen (SWFDEC_PLAYER (player));
    if (fullscreen && player->fullscreen == NULL) {
      GtkWidget *window, *child;
      
      player->fullscreen = window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      child = swfdec_gtk_widget_new_fullscreen (SWFDEC_PLAYER (player));
      gtk_container_add (GTK_CONTAINER (window), child);
      gtk_widget_show_all (window);
      gtk_window_fullscreen (GTK_WINDOW (window));
      g_signal_connect (window, "destroy", 
	  G_CALLBACK (swfmoz_player_fullscreen_destroyed), player);
      gtk_widget_grab_focus (child);
    } else if (!fullscreen && player->fullscreen != NULL) {
      gtk_widget_destroy (player->fullscreen);
      g_assert (player->fullscreen == NULL);
    }
  } else if (g_str_equal (pspec->name, "background-color")) {
    swfmoz_player_update_background (player);
  }
}

static gboolean
swfmoz_player_query_size (SwfmozPlayer *player, gboolean fullscreen, 
    int *width, int *height)
{
  if (fullscreen) {
    GdkWindow *window = plugin_get_window (player->instance);
    GdkScreen *screen;
    GdkRectangle rect;
    int monitor;

    if (window == NULL)
      return FALSE;
    screen = gdk_drawable_get_screen (GDK_DRAWABLE (window));
    monitor = gdk_screen_get_monitor_at_window (screen, window);
    gdk_screen_get_monitor_geometry (screen, monitor, &rect);
    *width = rect.width;
    *height = rect.height;
  } else if (player->target == NULL) {
    *width = -1;
    *height = -1;
  } else {
    *width = player->target_rect.width;
    *height = player->target_rect.height;
  }
  return TRUE;
}

static void
swfmoz_player_get_property (GObject *object, guint param_id, GValue *value, 
    GParamSpec * pspec)
{
  //SwfmozPlayer *player = SWFMOZ_PLAYER (object);
  
  switch (param_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfmoz_player_set_property (GObject *object, guint param_id, const GValue *value,
    GParamSpec *pspec)
{
  //SwfmozPlayer *player = SWFMOZ_PLAYER (object);

  switch (param_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
swfmoz_player_clear_repaints (SwfmozPlayer *player)
{
  if (player->repaint_source) {
    g_source_destroy (player->repaint_source);
    g_source_unref (player->repaint_source);
    player->repaint_source = NULL;
    gdk_region_destroy (player->repaint);
    player->repaint = NULL;
  }
}

static void
swfmoz_player_dispose (GObject *object)
{
  SwfmozPlayer *player = SWFMOZ_PLAYER (object);

  /* do this first or we'll get unhappy */
  if (player->fullscreen != NULL) {
    gtk_widget_destroy (player->fullscreen);
    g_assert (player->fullscreen == NULL);
  }

  if (player->menu) {
    g_signal_handlers_disconnect_matched (player, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, 
	swfmoz_player_menu_notify_playing, NULL);
    g_signal_handlers_disconnect_matched (player, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, 
	swfmoz_player_menu_notify_audio, NULL);
    gtk_widget_destroy (GTK_WIDGET (player->menu));
    player->menu = NULL;
  }
  /* the player might be refed elsewhere */
  g_signal_handlers_disconnect_by_func (player, swfmoz_player_redraw, NULL);
  g_signal_handlers_disconnect_by_func (player, swfmoz_player_launch, NULL);
  g_signal_handlers_disconnect_by_func (player, swfmoz_player_notify, NULL);
  g_signal_handlers_disconnect_by_func (player, swfmoz_player_query_size, NULL);
  if (player->target) {
    g_object_unref (player->target);
    player->target = NULL;
  }
  swfmoz_player_clear_repaints (player);
  if (player->loaders) {
    g_object_unref (player->loaders);
    player->loaders = NULL;
  }

  g_object_unref (player->config);
  player->config = NULL;

  G_OBJECT_CLASS (swfmoz_player_parent_class)->dispose (object);
}

static void
swfmoz_player_class_init (SwfmozPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = swfmoz_player_dispose;
  object_class->get_property = swfmoz_player_get_property;
  object_class->set_property = swfmoz_player_set_property;
}

static void
swfmoz_player_init (SwfmozPlayer *player)
{
  g_signal_connect (player, "invalidate", G_CALLBACK (swfmoz_player_redraw), NULL);
  g_signal_connect (player, "launch", G_CALLBACK (swfmoz_player_launch), NULL);
  g_signal_connect (player, "notify", G_CALLBACK (swfmoz_player_notify), NULL);
  g_signal_connect (player, "query-size", G_CALLBACK (swfmoz_player_query_size), NULL);
  player->context = g_main_context_default ();

  player->loaders = GTK_TREE_MODEL (gtk_list_store_new (SWFMOZ_LOADER_N_COLUMNS,
      SWFMOZ_TYPE_LOADER, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING));
}

SwfdecPlayer *
swfmoz_player_new (NPP instance, gboolean windowless, gboolean opaque)
{
  SwfmozPlayer *ret;
  SwfdecSystem *system;

  system = swfdec_gtk_system_new (NULL);
  g_object_set (system, "player-type", "PlugIn", NULL);
  ret = g_object_new (SWFMOZ_TYPE_PLAYER,
      "loader-type", SWFMOZ_TYPE_LOADER, "socket-type", SWFDEC_TYPE_SOCKET, //SWFDEC_TYPE_GTK_SOCKET,
      "system", system,
      NULL);
  ret->instance = instance;
  ret->windowless = windowless;
  ret->opaque = opaque;
  ret->config = swfmoz_config_new ();

  return SWFDEC_PLAYER (ret);
}

static void
swfmoz_player_loaders_update (GtkListStore *store, GtkTreeIter *iter, SwfdecLoader *loader)
{
  goffset loaded, size;
  gboolean error;
  const SwfdecURL *url;
  SwfdecPlayer *player;
  const char *url_string;
  char *str_loaded, *str_size;
  gchar *status, *s = NULL;

  loaded = swfdec_loader_get_loaded (loader);
  size = swfdec_loader_get_size (loader);

  /* FIXME: swfdec needs a function for this */
  g_object_get (G_OBJECT (loader), "error", &error, NULL);

  if (error == TRUE) {
    status = g_strdup("error");
  } else {
    str_loaded = g_format_size_for_display(loaded);
    str_size = g_format_size_for_display(size);

    if (swfdec_stream_is_complete (SWFDEC_STREAM (loader))) {
      status = g_strdup_printf("%s", str_loaded);
    } else if (size < 0 || (size < loaded)) {
      status = g_strdup_printf("at %s", str_loaded);
    } else {
      status = g_strdup_printf("%s of %s", str_loaded, str_size);
    }

    g_free (str_loaded);
    g_free (str_size);
  }

  url = swfdec_loader_get_url (loader);
  player = (*SWFMOZ_LOADER (loader)->instance)->pdata;
  if (url && SWFMOZ_LOADER (loader)->initial && swfdec_player_get_variables (player)) {
    /* This auto-appends the FlashVars to the reported URL. You should be able
     * to copy/paste that URL easily without breakage that way 
     * (minus cookies and referer) */
    s = g_strconcat (swfdec_url_get_url (url),
	swfdec_url_get_query (url) ? "&" : "?",
	swfdec_player_get_variables (player), NULL);
    url_string = s;
  } else if (url) {
    url_string = swfdec_url_get_url (url);
  } else if (SWFMOZ_LOADER (loader)->stream) {
    url_string = SWFMOZ_LOADER (loader)->stream->url;
  } else {
    /* we don't store URLs of requests */
    url_string = "";
  }

  gtk_list_store_set (store, iter,
    SWFMOZ_LOADER_COLUMN_LOADER, loader,
    SWFMOZ_LOADER_COLUMN_URL, url_string,
    SWFMOZ_LOADER_COLUMN_TYPE, swfmoz_loader_get_data_type_string (loader),
    SWFMOZ_LOADER_COLUMN_STATUS, status,
    -1);

  g_free (status);
  g_free (s);
}

static gboolean
swfmoz_player_find_loader (GtkListStore *store, SwfdecLoader *loader, GtkTreeIter *iter)
{
  GtkTreeModel *model = GTK_TREE_MODEL (store);

  if (!gtk_tree_model_get_iter_first (model, iter))
    return FALSE;
  do {
    SwfdecLoader *comp;
    gtk_tree_model_get (model, iter, SWFMOZ_LOADER_COLUMN_LOADER, &comp, -1);
    g_object_unref (comp);
    if (comp == loader)
      return TRUE;
  } while (gtk_tree_model_iter_next (model, iter));
  return FALSE;
}

static void
swfmoz_player_loader_notify_cb (SwfdecLoader *loader, GParamSpec *pspec, GtkListStore *store)
{
  GtkTreeIter iter;

  if (!swfmoz_player_find_loader (store, loader, &iter)) {
    g_assert_not_reached ();
  }
  swfmoz_player_loaders_update (store, &iter, loader);
}

gboolean
swfmoz_player_set_initial_stream (SwfmozPlayer *player, NPStream *stream)
{
  SwfdecURL *url;

  g_return_val_if_fail (SWFMOZ_IS_PLAYER (player), FALSE);
  g_return_val_if_fail (stream != NULL, FALSE);

  if (player->initial || swfdec_player_get_url (SWFDEC_PLAYER (player)))
    return FALSE;
  player->initial = stream;
  url = swfdec_url_new (stream->url);
  if (url == NULL) {
    g_printerr ("%s is either a broken URL or Swfdec can't parse it\n",
	stream->url);
    return FALSE;
  }
  swfdec_player_set_url (SWFDEC_PLAYER (player), url);
  if (swfmoz_config_should_autoplay (player->config, url)) {
    swfdec_gtk_player_set_playing (SWFDEC_GTK_PLAYER (player), TRUE);
  }
  swfdec_url_free (url);
  swfmoz_player_invalidate (player);

  return TRUE;
}

void
swfmoz_player_add_loader (SwfmozPlayer *player, SwfmozLoader *loader)
{
  GtkTreeIter iter;
  
  g_return_if_fail (SWFMOZ_IS_PLAYER (player));
  g_return_if_fail (SWFMOZ_IS_LOADER (loader));

  /* add loader to the list of loaders */
  g_signal_connect (loader, "notify", G_CALLBACK (swfmoz_player_loader_notify_cb),
      GTK_LIST_STORE (player->loaders));
  gtk_list_store_append (GTK_LIST_STORE (player->loaders), &iter);
  swfmoz_player_loaders_update (GTK_LIST_STORE (player->loaders), &iter, 
      SWFDEC_LOADER (loader));
}

void
swfmoz_player_set_target (SwfmozPlayer *player, GdkWindow *target, 
    int x, int y, int width, int height, Visual *visual)
{
  g_return_if_fail (SWFMOZ_IS_PLAYER (player));
  g_return_if_fail (target == NULL || GDK_IS_WINDOW (target));

  if (target != player->target) {
    if (player->target) {
      g_object_unref (player->target);
    }
    player->target = target;
    swfmoz_player_clear_repaints (player);
    if (target) {
      cairo_t *cr;
      SwfdecRenderer *renderer;

      g_object_ref (target);
      cr = gdk_cairo_create (target);
      renderer = swfdec_renderer_new_for_player (cairo_get_target (cr), 
	  SWFDEC_PLAYER (player));
      swfdec_player_set_renderer (SWFDEC_PLAYER (player), renderer);
      g_object_unref (renderer);
      cairo_destroy (cr);

      swfdec_gtk_player_set_missing_plugins_window (SWFDEC_GTK_PLAYER (player), 
	  gdk_window_get_toplevel (target));
      swfmoz_player_update_cursor (player);
      swfmoz_player_update_background (player);
    } else {
      swfdec_gtk_player_set_missing_plugins_window (SWFDEC_GTK_PLAYER (player), 
	  NULL);
    }
  }
  player->target_rect.x = x;
  player->target_rect.y = y;
  player->target_rect.width = width;
  player->target_rect.height = height;
  player->target_visual = visual;
  swfdec_player_set_size (SWFDEC_PLAYER (player), width, height);
}

static void
swfdec_gtk_player_draw_pause (cairo_t *cr)
{
  cairo_pattern_t *pattern;
#if 0
#define ALPHA 0.447059
#else
#define ALPHA 0.6
#endif

  cairo_set_line_width (cr, 0.959352);
  cairo_set_miter_limit (cr, 4);
  cairo_set_line_cap (cr, 0);
  cairo_set_line_join (cr, 0);
  cairo_move_to (cr, 16.0003, 0.567675);
  cairo_curve_to (cr, 7.44305, 0.567675, 0.505822, 7.53349, 0.505822, 16.1256);
  cairo_curve_to (cr, 0.505822, 24.7178, 7.44305, 31.6836, 16.0003, 31.6836);
  cairo_curve_to (cr, 24.5576, 31.6836, 31.4948, 24.7178, 31.4948, 16.1256);
  cairo_curve_to (cr, 31.4948, 7.53349, 24.5576, 0.567675, 16.0003, 0.567675);
  cairo_close_path (cr);
  cairo_move_to (cr, 16.0003, 0.567675);
  cairo_move_to (cr, 16.0607, 3.36325);
  cairo_curve_to (cr, 22.984, 3.36325, 28.5953, 8.99503, 28.5953, 15.9433);
  cairo_curve_to (cr, 28.5953, 22.8916, 22.984, 28.5234, 16.0607, 28.5234);
  cairo_curve_to (cr, 9.13743, 28.5234, 3.49599, 22.8916, 3.49599, 15.9433);
  cairo_curve_to (cr, 3.49599, 8.99503, 9.13743, 3.36325, 16.0607, 3.36325);
  cairo_close_path (cr);
  cairo_move_to (cr, 16.0607, 3.36325);
  cairo_set_fill_rule (cr, 0);
  cairo_set_source_rgba (cr, 0.827451, 0.843137, 0.811765, ALPHA);
  cairo_fill_preserve (cr);
  pattern = cairo_pattern_create_linear (24.906, 26.4817, 3.6134, 5.18912);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 0, 0, 0, ALPHA);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 1, 1, 1, ALPHA);
  cairo_set_source (cr, pattern);
  cairo_stroke (cr);
  cairo_set_line_width (cr, 1);
  cairo_set_miter_limit (cr, 4);
  cairo_move_to (cr, 11.4927, 8.57249);
  cairo_line_to (cr, 23.5787, 16.0226);
  cairo_line_to (cr, 11.6273, 23.4948);
  cairo_line_to (cr, 11.4927, 8.57249);
  cairo_close_path (cr);
  cairo_move_to (cr, 11.4927, 8.57249);
  cairo_set_fill_rule (cr, 1);
  cairo_set_source_rgba (cr, 0.827451, 0.843137, 0.811765, ALPHA);
  cairo_fill_preserve (cr);
  cairo_set_source (cr, pattern);
  cairo_stroke (cr);
#if 0
  cairo_scale(cr, 1.18644, 1.20339);
  cairo_translate (cr, -5.3063, -2.93549);
  cairo_set_line_width (cr, 0.8369);
  cairo_set_miter_limit (cr, 4);
  cairo_move_to (cr, 28.461, 15.7365);
  cairo_curve_to (cr, 28.4623, 21.4977, 23.7923, 26.1687, 18.0312, 26.1687);
  cairo_curve_to (cr, 12.2701, 26.1687, 7.60014, 21.4977, 7.6014, 15.7365);
  cairo_curve_to (cr, 7.60014, 9.97542, 12.2701, 5.30444, 18.0312, 5.30444);
  cairo_curve_to (cr, 23.7923, 5.30444, 28.4623, 9.97542, 28.461, 15.7365);
  cairo_close_path (cr);
  cairo_move_to (cr, 28.461, 15.7365);
  cairo_set_source (cr, pattern);
  cairo_stroke (cr);
#endif
  cairo_pattern_destroy (pattern);
}

void
swfmoz_player_render (SwfmozPlayer *player, cairo_t *cr, GdkRegion *region)
{
  GdkRectangle rect;
  gboolean has_cr = cr != NULL;

  g_return_if_fail (SWFMOZ_IS_PLAYER (player));
  g_return_if_fail (!gdk_region_empty (region));

  /* first, remove the repainted stuff from the stuff that needs a redraw */
  if (player->repaint) {
    g_assert (player->repaint_source);
    gdk_region_subtract (player->repaint, region);
    if (gdk_region_empty (player->repaint)) {
      g_source_destroy (player->repaint_source);
      g_source_unref (player->repaint_source);
      player->repaint_source = NULL;
      gdk_region_destroy (player->repaint);
      player->repaint = NULL;
    }
  }

  /* second, check if we have anything to draw */
  if (player->target == NULL || player->fullscreen)
    return;

  if (!has_cr) {
    gdk_window_begin_paint_region (player->target, region);
    cr = gdk_cairo_create (player->target);
  } else {
    cairo_save (cr);
  }
  gdk_cairo_region (cr, region);
  cairo_clip (cr);
  /* paint it */
  if (player->opaque) {
    guint bgcolor = swfdec_player_get_background_color (SWFDEC_PLAYER (player));
    cairo_set_source_rgb (cr, 
	((bgcolor >> 16) & 0xFF) / 255.0,
	((bgcolor >> 8) & 0xFF) / 255.0,
	(bgcolor & 0xFF) / 255.0);
    cairo_paint (cr);
  }
  cairo_translate (cr, player->target_rect.x, player->target_rect.y);
  gdk_region_get_clipbox (region, &rect);
  swfdec_player_render (SWFDEC_PLAYER (player), cr);
  /* paint optional pause sign */
  if (!swfdec_gtk_player_get_playing (SWFDEC_GTK_PLAYER (player)) &&
      swfdec_player_get_url (SWFDEC_PLAYER (player)) != NULL) {
    int w = player->target_rect.width;
    int h = player->target_rect.height;
    int len = MIN (w, h) * 4 / 5;
    cairo_rectangle (cr, 0, 0, w, h);
    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.4);
    cairo_fill (cr);
    cairo_translate (cr, (w - len) / 2.0, (h - len) / 2.0);
    cairo_scale (cr, len / 32.0, len / 32.0);
    swfdec_gtk_player_draw_pause (cr);
  }
  if (!has_cr) {
    cairo_destroy (cr);
    gdk_window_end_paint (player->target);
  } else {
    cairo_restore (cr);
  }
}

void
swfmoz_player_set_allow_popups (SwfmozPlayer *player, gboolean allow)
{
  g_return_if_fail (SWFMOZ_IS_PLAYER (player));

  swfdec_player_set_allow_fullscreen (SWFDEC_PLAYER (player), allow);
  if (allow)
    plugin_push_allow_popups (player->instance, TRUE);
  else
    plugin_pop_allow_popups (player->instance);
}

gboolean
swfmoz_player_mouse_press (SwfmozPlayer *player, int x, int y, guint button)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (SWFMOZ_IS_PLAYER (player), FALSE);

  if (player->menu && GTK_WIDGET_VISIBLE (player->menu)) {
    gtk_menu_popdown (GTK_MENU (player->menu));
    player->no_release = button;
    return TRUE;
  }

  if (button > 32)
    return FALSE;

  if (swfdec_gtk_player_get_playing (SWFDEC_GTK_PLAYER (player))) {
    swfmoz_player_set_allow_popups (player, TRUE);
    ret = swfdec_player_mouse_press (SWFDEC_PLAYER (player), x, y, button);
    swfmoz_player_set_allow_popups (player, FALSE);
  }
  return ret;
}

gboolean
swfmoz_player_mouse_release (SwfmozPlayer *player, int x, int y, guint button)
{
  gboolean ret;

  g_return_val_if_fail (SWFMOZ_IS_PLAYER (player), FALSE);

  if (button == player->no_release) {
    player->no_release = 0;
    return TRUE;
  }

  if (button > 32)
    return FALSE;

  if (swfdec_gtk_player_get_playing (SWFDEC_GTK_PLAYER (player))) {
    swfmoz_player_set_allow_popups (player, TRUE);
    ret = swfdec_player_mouse_release (SWFDEC_PLAYER (player), x, y, button);
    swfmoz_player_set_allow_popups (player, FALSE);
  } else {
    if (button == 1) {
      swfdec_gtk_player_set_playing (SWFDEC_GTK_PLAYER (player), TRUE);
      swfmoz_config_set_autoplay (player->config, swfdec_player_get_url (SWFDEC_PLAYER (player)), TRUE);
      ret = TRUE;
    } else {
      ret = FALSE;
    }
  }

  if (button == 3) {
    swfmoz_player_popup_menu (player);
    ret = TRUE;
  }
  return ret;
}

gboolean
swfmoz_player_mouse_move (SwfmozPlayer *player, int x, int y)
{
  g_return_val_if_fail (SWFMOZ_IS_PLAYER (player), FALSE);

  if (swfdec_gtk_player_get_playing (SWFDEC_GTK_PLAYER (player))) {
    swfdec_player_mouse_move (SWFDEC_PLAYER (player), x, y);
  }
  return FALSE;
}

char *
swfmoz_player_get_filename (SwfmozPlayer *player)
{
  const SwfdecURL *url;

  g_return_val_if_fail (SWFMOZ_IS_PLAYER (player), NULL);

  url = swfdec_player_get_url (SWFDEC_PLAYER (player));
  if (url == NULL)
    return g_strdup ("unknown.swf");

  return swfdec_url_format_for_display (url);
}

void
swfmoz_player_remove (SwfmozPlayer *player)
{
  /* This function is called when the player is removed from screen.
   * It may still be necessary to keep it around if dialogs are open.
   */
  g_return_if_fail (SWFMOZ_IS_PLAYER (player));

  swfdec_gtk_player_set_playing (SWFDEC_GTK_PLAYER (player), FALSE);
  swfdec_gtk_player_set_audio_enabled (SWFDEC_GTK_PLAYER (player), FALSE);
  swfmoz_dialog_remove (player);
  player->instance = NULL;
  g_object_unref (player);
}

