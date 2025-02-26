#ifndef GRLIST_H
#define GRLIST_H

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GR_TYPE_LIST ( gr_list_get_type() )
G_DECLARE_FINAL_TYPE( GrList, gr_list, GR, LIST, GtkWidget )

GrList* gr_list_new( void );
gint gr_list_get_min_content_height( GrList *self );
void gr_list_set_min_content_height( GrList *self, gint height );
gint gr_list_get_max_content_height( GrList *self );
void gr_list_set_max_content_height( GrList *self, gint height );
void gr_list_set_array( GrList *self, const GStrv array );
gchar* gr_list_get_selected_text( GrList *self );

G_END_DECLS

#endif
