#ifndef PATH_H
#define PATH_H

GList* gr_path_get_compared_list( GList *filename_list, const gchar *text );
GList* gr_path_get_filename_list_from_env( const gchar *pathenv );
GList* gr_path_get_filename_list_from_cache( gchar *cache_filepath );
void gr_path_store_command_to_cache( gchar *cache_filepath, const gchar *command );

#endif

