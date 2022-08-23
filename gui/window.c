#include <gtk/gtk.h>
#include "config.h"
#include "entry.h"

/*
	private
*/
static gboolean
on_event_key_pressed(
	GtkEventControllerKey *self,
	guint keyval,
	guint keycode,
	GdkModifierType state,
	gpointer user_data )
{
	GtkWindow *window = GTK_WINDOW( user_data );

	if( ( keyval == GDK_KEY_q && ( state & GDK_CONTROL_MASK ) ) ||
			keyval == GDK_KEY_Escape )
	{
		gtk_window_destroy( window );
		return FALSE;
	}

	return FALSE;
}


/*
	public
*/
GtkWindow*
gr_window_new(
	GApplication *app )
{
	GtkWindow *window;
	GtkEntry *entry;
	GtkEventControllerKey *event_key;

	g_return_val_if_fail( G_IS_APPLICATION( app ), NULL );

	/* create window */
	window = GTK_WINDOW( gtk_application_window_new( GTK_APPLICATION( app ) ) );
	gtk_window_set_title( window, PROGRAM_NAME );
	gtk_window_set_resizable( window, FALSE );
	gtk_window_set_decorated( window, FALSE );
	gtk_window_set_default_size( window, 400, -1 );

	/* bind event callback */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( window ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), window );

	/* create widgets */
	entry = gr_entry_new( window );

	/* layout widgets */
	gtk_window_set_child( window, GTK_WIDGET( entry ) );

	return window;
}

