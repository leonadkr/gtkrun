#ifndef PATH_H
#define PATH_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define GR_TYPE_PATH ( gr_path_get_type() )
G_DECLARE_FINAL_TYPE( GrPath, gr_path, GR, PATH, GObject )

GrPath* gr_path_new( const gchar *cache_filepath, const gchar *pathenv );
GList* gr_path_get_compared_list( GrPath *self, const gchar *text );
void gr_path_store_command_to_cache( GrPath *self, const gchar *command );
void gr_path_system_call( GrPath *self, const gchar* command );

G_END_DECLS

#endif

