#ifndef GRAPPLICATION_H
#define GRAPPLICATION_H

#include "grcommandlist.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GR_TYPE_APPLICATION ( gr_application_get_type() )
G_DECLARE_FINAL_TYPE( GrApplication, gr_application, GR, APPLICATION, GtkApplication )

GrApplication* gr_application_new( const gchar *application_id );
gboolean gr_application_get_silent( GrApplication *self );
gint gr_application_get_width( GrApplication *self );
gint gr_application_get_height( GrApplication *self );
gint gr_application_get_max_height( GrApplication *self );
gboolean gr_application_get_max_height_set( GrApplication *self );
gchar* gr_application_get_cache_dir( GrApplication *self );
gboolean gr_application_get_no_cache( GrApplication *self );
gchar* gr_application_get_config_path( GrApplication *self );
gboolean gr_application_get_no_config( GrApplication *self );
GrCommandList* gr_application_get_command_list( GrApplication *self );

G_END_DECLS

#endif
