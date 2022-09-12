#ifndef TREEVIEW_H
#define TREEVIEW_H

#include <glib.h>
#include <gtk/gtk.h>
#include "path.h"

GtkTreeView* gr_tree_view_new( GtkWindow *window, GrPath *path );
void gr_tree_view_set_model_by_text( GtkTreeView *self, const gchar* text );
gchar* gr_tree_view_get_selected_text( GtkTreeView *self );

#endif
