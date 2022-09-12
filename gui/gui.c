#include <glib.h>
#include <gtk/gtk.h>
#include "config.h"
#include "gui.h"
#include "path.h"
#include "window.h"


static void
on_app_startup(
	GApplication *self,
	gpointer user_data )
{
	g_return_if_fail( G_IS_APPLICATION( self ) );

	g_application_set_resource_base_path( self, NULL );
}

static void
on_app_activate(
	GApplication *self,
	gpointer user_data )
{
	GrPath *path = GR_PATH( user_data );
	GtkWindow *window;

	g_return_if_fail( G_IS_APPLICATION( self ) );

	window = gr_window_new( self, path );
	gtk_widget_show( GTK_WIDGET( window ) );
}


gint
gui_start(
	gint argc,
	gchar *argv[],
	GrPath *path )
{
	GApplication *app;
	gint ret = EXIT_FAILURE;

	g_return_val_if_fail( argc > 0, EXIT_FAILURE );
	g_return_val_if_fail( argv != NULL, EXIT_FAILURE );
	g_return_val_if_fail( GR_IS_PATH( path ), EXIT_FAILURE );
	g_return_val_if_fail( g_application_id_is_valid( APP_ID ), EXIT_FAILURE );

	app = G_APPLICATION( gtk_application_new( APP_ID, G_APPLICATION_FLAGS_NONE ) );
	if( app != NULL )
	{
		g_signal_connect( G_OBJECT( app ), "startup", G_CALLBACK( on_app_startup ), NULL );
		g_signal_connect( G_OBJECT( app ), "activate", G_CALLBACK( on_app_activate ), path );
		ret = g_application_run( app, argc, argv );
		g_object_unref( G_OBJECT( app ) );
	}

	return ret;
}

