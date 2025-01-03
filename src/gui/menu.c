/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021- sfwbar maintainers
 */

#include "menu.h"
#include "window.h"
#include "taskbarpopup.h"
#include "scaleimage.h"
#include "popup.h"
#include "util/string.h"
#include "vm/vm.h"

static GHashTable *menus, *menu_items;

GtkWidget *menu_from_name ( gchar *name )
{
  if(!menus || !name)
    return NULL;
  return g_hash_table_lookup(menus, name);
}

void menu_remove ( gchar *name )
{
  GtkWidget *menu;
  GList *items, *iter;

  if(!menus || !name)
    return;
  menu = menu_from_name(name);
  if(!menu)
    return;
  items = gtk_container_get_children(GTK_CONTAINER(menu));
  for(iter=items; iter; iter=g_list_next(iter))
    if(gtk_menu_item_get_submenu(iter->data))
      gtk_menu_item_set_submenu(iter->data, NULL);
  g_list_free(items);

  g_hash_table_remove(menus, name);
}

void menu_item_remove ( gchar *id )
{
  GtkWidget *item;

  if(!menu_items || !(item = g_hash_table_lookup(menu_items, id)) )
    return;

  if(gtk_menu_item_get_submenu(GTK_MENU_ITEM(item)))
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), NULL);

  g_hash_table_remove(menu_items, id);
}

void menu_clamp_size ( GtkMenu *menu )
{
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkWindow *gdk_win;
  GtkWindow *toplevel;
  GdkRectangle workarea;
  gint w,h;

  toplevel = GTK_WINDOW(gtk_widget_get_ancestor(GTK_WIDGET(menu),
        GTK_TYPE_WINDOW));
  gdk_win = gtk_widget_get_window(GTK_WIDGET(toplevel));
  w = gdk_window_get_width(gdk_win);
  h = gdk_window_get_height(gdk_win);

  display = gdk_window_get_display(gdk_win);
  monitor = gdk_display_get_monitor_at_window(display, gdk_win);
  gdk_monitor_get_workarea(monitor, &workarea);

  gdk_window_resize(gdk_win, MIN(w, workarea.width), MIN(h, workarea.height));
}

GtkWidget *menu_new ( gchar *name )
{
  GtkWidget *menu;

  if(name)
  {
    menu = menu_from_name(name);
    if(menu)
      return menu;
  }

  menu = gtk_menu_new();
  gtk_widget_set_name(menu, name);
  g_signal_connect(menu, "popped-up", G_CALLBACK(menu_clamp_size), NULL);
  gtk_menu_set_reserve_toggle_size(GTK_MENU(menu), FALSE);

  if(name)
  {
    g_object_ref_sink(G_OBJECT(menu));
    if(!menus)
      menus = g_hash_table_new_full((GHashFunc)str_nhash,
          (GEqualFunc)str_nequal, g_free,g_object_unref);
    g_hash_table_insert(menus, g_strdup(name), menu);
  }
  return menu;
}

static void menu_popdown_cb ( GtkWidget *widget, GtkWidget *menu )
{
  gtk_menu_popdown(GTK_MENU(menu));
}

static void menu_set_names ( GtkWidget *menu );
static void menu_item_set_name ( GtkWidget *item, void *d )
{
  expr_cache_t *expr;
  GtkWidget *sub;
  gchar *text, *icon;

  if(!GTK_IS_MENU_ITEM(item))
    return;

  if( (expr = g_object_get_data(G_OBJECT(item), "label")) )
  {
    if(expr_cache_eval(expr))
    {
      if( (icon = strchr(expr->cache, '%')) )
        text = g_strndup(expr->cache, icon-expr->cache);
      else
        text = g_strdup(expr->cache);
      menu_item_update(item, text, icon?icon+1:NULL);
      g_free(text);
    }
  }

  if( (sub=gtk_menu_item_get_submenu(GTK_MENU_ITEM(item))) )
    menu_set_names(sub);
}

static void menu_set_names ( GtkWidget *menu )
{
  gtk_container_foreach(GTK_CONTAINER(menu), menu_item_set_name, NULL);
}

void menu_popup( GtkWidget *widget, GtkWidget *menu, GdkEvent *event,
    gpointer wid, guint16 *state )
{
  GdkGravity wanchor, manchor;
  GtkWidget *window;

  if(!menu || !widget)
    return;

  menu_set_names(menu);

  if(state)
    g_object_set_data( G_OBJECT(menu), "state", GUINT_TO_POINTER(*state) );
  g_object_set_data( G_OBJECT(menu), "wid", wid );
  g_object_set_data( G_OBJECT(menu), "caller", widget );

  window = gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW);
  g_signal_handlers_disconnect_matched(G_OBJECT(menu),G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, G_CALLBACK(window_unref), NULL);
  if(gtk_window_get_window_type(GTK_WINDOW(window)) == GTK_WINDOW_POPUP)
    g_signal_connect(G_OBJECT(menu), "unmap",
        G_CALLBACK(window_unref), window);

  widget = GTK_IS_BIN(widget)?gtk_bin_get_child(GTK_BIN(widget)):widget;
  gtk_widget_unset_state_flags(widget, GTK_STATE_FLAG_PRELIGHT);
  popup_get_gravity(widget, &wanchor, &manchor);
  gtk_widget_show_all(menu);
  window_ref(window, menu);
  g_signal_handlers_disconnect_by_func(G_OBJECT(widget),
        G_CALLBACK(menu_popdown_cb), menu);
  g_signal_connect(G_OBJECT(widget), "unrealize",
        G_CALLBACK(menu_popdown_cb), menu);
  gtk_menu_popup_at_widget(GTK_MENU(menu), widget, wanchor, manchor, event);
}

gboolean menu_action_cb ( GtkWidget *w, GBytes *action )
{
  GtkWidget *parent;
  gpointer wid;
  guint16 state;
  GtkWidget *widget;

  if( (parent = gtk_widget_get_ancestor(w, GTK_TYPE_MENU)) )
  {
    wid = g_object_get_data (G_OBJECT(parent), "wid");
    state = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(parent), "state"));
    widget = g_object_get_data (G_OBJECT(parent), "caller");
  }
  else
  {
    wid = NULL;
    state = 0;
    widget = NULL;
  }

  if(!wid)
    wid = wintree_get_focus();

  action_exec(widget, action, NULL, wintree_from_id(wid), &state);
  return TRUE;
}

void menu_item_update ( GtkWidget *item, const gchar *label, const gchar *icon )
{
  GtkWidget *box, *wicon, *wlabel;

  g_return_if_fail(GTK_IS_MENU_ITEM(item));

  if( !(box = gtk_bin_get_child(GTK_BIN(item))) )
  {
    box = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(item), box);
  }

  wicon = gtk_grid_get_child_at(GTK_GRID(box), 1, 1);
  if(wicon && !icon)
    g_clear_pointer(&wicon,gtk_widget_destroy);
  else if(!wicon && icon)
  {
    wicon = scale_image_new();
    gtk_grid_attach(GTK_GRID(box), wicon, 1, 1, 1, 1);
  }
  if(wicon && icon)
    scale_image_set_image(wicon, icon, NULL);

  wlabel = gtk_grid_get_child_at(GTK_GRID(box), 2, 1);
  if(wlabel && !label)
    gtk_widget_destroy(wlabel);
  else if(!wlabel && label)
  {
    wlabel = gtk_label_new_with_mnemonic(label);
    gtk_grid_attach(GTK_GRID(box), wlabel, 2, 1, 1, 1);
  }
  else if(wlabel && g_strcmp0(gtk_label_get_text(GTK_LABEL(wlabel)), label))
    gtk_label_set_text_with_mnemonic(GTK_LABEL(wlabel), label);
}

GtkWidget *menu_item_new ( gchar *label, GBytes *action, gchar *id )
{
  GtkWidget *item;
  gchar *text, *icon;

  icon = strchr(label,'%');
  if(icon)
    text = g_strndup(label, icon-label);
  else
    text = g_strdup(label);

  item = gtk_menu_item_new();
  gtk_widget_set_name(item, "menu_item");
  menu_item_update(item, text, icon?icon+1:NULL);
  g_free(text);

  if(action)
  {
    g_signal_connect(G_OBJECT(item), "activate",
        G_CALLBACK(menu_action_cb), action);
    g_object_weak_ref(G_OBJECT(item), (GWeakNotify)g_bytes_unref, action);
  }

  if(id)
  {
    if(!menu_items)
      menu_items = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
          (GDestroyNotify)gtk_widget_destroy);
    if(g_hash_table_lookup(menu_items, id))
      g_message("duplicate menu item id: '%s'", id);
    else
      g_hash_table_insert(menu_items, g_strdup(id), item);
  }

  return item;
}
