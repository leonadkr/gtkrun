#ifndef GRCOMMANDLIST_H
#define GRCOMMANDLIST_H

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GR_TYPE_COMMAND_LIST ( gr_command_list_get_type() )
G_DECLARE_FINAL_TYPE( GrCommandList, gr_command_list, GR, COMMAND_LIST, GObject )

GrCommandList* gr_command_list_new( const gchar *com_list_path );
gchar* gr_command_list_get_history_file_path( GrCommandList *self );
void gr_command_list_set_history_file_path( GrCommandList *self, const gchar *path );
gchar* gr_command_list_get_compared_string( GrCommandList *self, const gchar *str );
GStrv gr_command_list_get_compared_array( GrCommandList *self, const gchar *str );
void gr_command_list_push( GrCommandList *self, const gchar *text );

G_END_DECLS

#endif
