/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "flowgrid.h"
#include "taskbarpager.h"
#include "taskbarshell.h"
#include "taskbaritem.h"
#include "pager.h"
#include "scaleimage.h"
#include "action.h"
#include "bar.h"
#include "wintree.h"
#include "menu.h"
#include "popup.h"
#include <gtk-layer-shell.h>

G_DEFINE_TYPE_WITH_CODE (TaskbarPager, taskbar_pager, FLOW_ITEM_TYPE,
    G_ADD_PRIVATE(TaskbarPager))

GtkWidget *taskbar_pager_get_taskbar ( GtkWidget *shell, window_t *win,
    gboolean create )
{
  TaskbarPagerPrivate *priv;
  workspace_t *ws;
  GtkWidget *holder;

  if( !(ws = workspace_from_id(win->workspace)) )
    return NULL;

  if( !(holder = flow_grid_find_child(shell, ws)) )
  {
    if(!create)
      return NULL;
    holder = taskbar_pager_new(ws, shell);
  }

  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(holder));
  return priv->taskbar;
}

static gpointer taskbar_pager_get_ws ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self),NULL);
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  return priv->ws;
}

static void taskbar_pager_decorate ( GtkWidget *self, gboolean labels,
    gboolean icons )
{
  TaskbarPagerPrivate *priv;

  g_return_if_fail(IS_TASKBAR_PAGER(self));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  if(labels == !!priv->button)
    return;

  if(!labels && priv->button)
    gtk_widget_destroy(priv->button);
  else
  {
    g_object_ref(priv->taskbar);
    gtk_container_remove(GTK_CONTAINER(priv->grid), priv->taskbar);
    priv->button = gtk_button_new_with_label("");
    gtk_container_add(GTK_CONTAINER(priv->grid), priv->button);
    gtk_container_add(GTK_CONTAINER(priv->grid), priv->taskbar);
    g_object_unref(priv->taskbar);
  }
}

static void taskbar_pager_update ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;
  gchar *title;

  g_return_if_fail(IS_TASKBAR_PAGER(self));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  if(!priv->invalid)
    return;

  title = priv->ws? priv->ws->name: NULL;
  if(priv->button &&
      g_strcmp0(gtk_button_get_label(GTK_BUTTON(priv->button)), title))
    gtk_button_set_label(GTK_BUTTON(priv->button), title);

  if (flow_grid_find_child(priv->taskbar, wintree_from_id(wintree_get_focus())))
    gtk_widget_set_name(base_widget_get_child(self), "taskbar_pager_active");
  else
    gtk_widget_set_name(base_widget_get_child(self), "taskbar_pager_normal");

  gtk_widget_unset_state_flags(base_widget_get_child(self),
      GTK_STATE_FLAG_PRELIGHT);

  flow_grid_update(priv->taskbar);
  flow_item_set_active(self, flow_grid_n_children(priv->taskbar)>0 );

  priv->invalid = FALSE;
}

static gint taskbar_pager_compare ( GtkWidget *a, GtkWidget *b,
    GtkWidget *parent )
{
  TaskbarPagerPrivate *p1,*p2;

  g_return_val_if_fail(IS_TASKBAR_PAGER(a), 0);
  g_return_val_if_fail(IS_TASKBAR_PAGER(b), 0);

  p1 = taskbar_pager_get_instance_private(TASKBAR_PAGER(a));
  p2 = taskbar_pager_get_instance_private(TASKBAR_PAGER(b));

  if(g_object_get_data(G_OBJECT(parent), "sort_numeric"))
    return strtoll(p1->ws->name, NULL, 10) - strtoll(p2->ws->name, NULL, 10);
  else
    return g_strcmp0(p1->ws->name, p2->ws->name);
}

static gboolean taskbar_pager_action_exec ( GtkWidget *self, gint slot,
    GdkEvent *ev )
{
  TaskbarPagerPrivate *priv;
  GdkModifierType mods;

  g_return_val_if_fail(IS_TASKBAR_PAGER(self),FALSE);
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));
  
  if(slot != 1)
    return FALSE;

  mods = base_widget_get_modifiers(self);

  if(!mods && slot==1)
  {
    workspace_activate(priv->ws);
    return TRUE;
  }

  return FALSE;
}

static void taskbar_pager_invalidate ( GtkWidget *self )
{
  TaskbarPagerPrivate *priv;

  if(!self)
    return;

  g_return_if_fail(IS_TASKBAR_PAGER(self));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  flow_grid_invalidate(priv->shell);
  priv->invalid = TRUE;
}

static void taskbar_pager_dnd_dest ( GtkWidget *dest, GtkWidget *src,
    gint x, gint y )
{
  workspace_t *ws;
  window_t *win;

  g_return_if_fail(IS_TASKBAR_PAGER(dest));
  ws = flow_item_get_source(dest);
  win = flow_item_get_source(src);
  if(win && ws)
    wintree_move_to(win->uid, ws->id);
}

static void taskbar_pager_class_init ( TaskbarPagerClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->action_exec = taskbar_pager_action_exec;
  FLOW_ITEM_CLASS(kclass)->decorate = taskbar_pager_decorate;
  FLOW_ITEM_CLASS(kclass)->update = taskbar_pager_update;
  FLOW_ITEM_CLASS(kclass)->invalidate = taskbar_pager_invalidate;
  FLOW_ITEM_CLASS(kclass)->get_source = taskbar_pager_get_ws;
  FLOW_ITEM_CLASS(kclass)->compare = taskbar_pager_compare;
  FLOW_ITEM_CLASS(kclass)->dnd_dest = taskbar_pager_dnd_dest;
}

static void taskbar_pager_init ( TaskbarPager *self )
{
}

GtkWidget *taskbar_pager_new( workspace_t *ws, GtkWidget *shell )
{
  GtkWidget *self;
  TaskbarPagerPrivate *priv;

  g_return_val_if_fail(IS_TASKBAR_SHELL(shell), NULL);

  self = GTK_WIDGET(g_object_new(taskbar_pager_get_type(), NULL));
  priv = taskbar_pager_get_instance_private(TASKBAR_PAGER(self));

  priv->shell = shell;
  priv->taskbar = taskbar_new(self);
  flow_grid_set_dnd_target(priv->taskbar, flow_grid_get_dnd_target(shell));
  flow_grid_child_dnd_enable(shell, self, NULL);
  priv->ws = ws;
  priv->grid = gtk_grid_new();
  gtk_widget_set_name(GTK_WIDGET(priv->grid), "taskbar_pager");
  gtk_container_add(GTK_CONTAINER(self), priv->grid);
  gtk_container_add(GTK_CONTAINER(priv->grid), priv->taskbar);
  gtk_widget_show_all(self);

  base_widget_copy_actions(priv->taskbar, shell);

  g_object_ref_sink(G_OBJECT(self));
  flow_grid_add_child(shell, self);
  flow_item_invalidate(self);
  return self;
}
