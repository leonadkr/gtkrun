#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <glib.h>
#include <gtk/gtk.h>
#include "shared.h"

GtkListView* gr_list_view_new( GtkWindow *window, GrShared *shared );
void gr_list_view_set_model_by_text( GtkListView *self, const gchar* text );
gchar* gr_list_view_get_selected_text( GtkListView *self );

#endif
