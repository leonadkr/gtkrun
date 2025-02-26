#include "grwindow.h"

#include "config.h"
#include "grapplication.h"
#include "grentry.h"
#include "grlist.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct _GrWindow
{
	GtkApplicationWindow parent_instance;

	GrEntry *entry;
	GrList *list;
	gboolean is_entry_visible;

	GrApplication *app;
};
typedef struct _GrWindow GrWindow;

G_DEFINE_TYPE( GrWindow, gr_window, GTK_TYPE_APPLICATION_WINDOW )

static void
gr_window_close(
	GrWindow *self )
{
	if( gr_application_get_conceal( self->app ) )
	{
		gtk_widget_set_visible( GTK_WIDGET( self ), FALSE );
		return;
	}

	gtk_window_destroy( GTK_WINDOW( self ) );
}

static void
gr_window_switch_widgets(
	GrWindow *self )
{
	/* gchar *text; */

	g_print( "window_switch_widgets()\n" );

	if( self->is_entry_visible )
	{
		/* text = gr_entry_get_text( self->entry ); */
		/* gr_list_set_model_by_text( self->list, text ); */
		/* g_free( text ); */

		gtk_widget_set_visible( GTK_WIDGET( self->entry ), FALSE );
		gtk_widget_set_visible( GTK_WIDGET( self->list ), TRUE );
		gtk_widget_grab_focus( GTK_WIDGET( self->list ) );
	}
	else
	{
		/* text = gr_list_get_selected_text( self->list ); */
		/* gr_entry_set_text( self->entry, text ); */
		/* g_free( text ); */

		gtk_widget_set_visible( GTK_WIDGET( self->entry ), TRUE );
		gtk_widget_set_visible( GTK_WIDGET( self->list ), FALSE );
		gtk_widget_grab_focus( GTK_WIDGET( self->entry ) );
	}
	self->is_entry_visible = !self->is_entry_visible;
}

static gboolean
on_event_key_pressed(
	GtkEventControllerKey *self,
	guint keyval,
	guint keycode,
	GdkModifierType state,
	gpointer user_data )
{
	GrWindow *window = GR_WINDOW( user_data );

	if( ( keyval == GDK_KEY_q && ( state & GDK_CONTROL_MASK ) ) ||
			keyval == GDK_KEY_Escape )
	{
		gr_window_close( window );
		return GDK_EVENT_STOP;
	}

	if( keyval == GDK_KEY_Tab )
	{
		gr_window_switch_widgets( window );
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void
on_entry_activate(
	GrEntry *self,
	gpointer user_data )
{
	/* GrWindow *window = GR_WINDOW( user_data ); */

	g_print( "on_entry_activate()\n" );
}

static void
on_list_activate(
	GrEntry *self,
	gpointer user_data )
{
	/* GrWindow *window = GR_WINDOW( user_data ); */

	g_print( "on_list_activate()\n" );
}

static void
gr_window_init(
	GrWindow *self )
{
	GtkEventControllerKey *event_key;
	GtkBox *box;

	/* setup window */
	gtk_window_set_title( GTK_WINDOW( self ), PROGRAM_NAME );
	gtk_window_set_resizable( GTK_WINDOW( self ), FALSE );
	gtk_window_set_decorated( GTK_WINDOW( self ), FALSE );

	/* add event controller */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( self ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), self );

	/* create widgets */
	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 1 ) );

	self->entry = gr_entry_new();
	g_signal_connect( G_OBJECT( self->entry ), "activate", G_CALLBACK( on_entry_activate ), self );

	self->list = gr_list_new();
	g_signal_connect( G_OBJECT( self->list ), "activate", G_CALLBACK( on_list_activate ), self );

	/* layout widgets */
	gtk_window_set_child( GTK_WINDOW( self ), GTK_WIDGET( box ) );
	gtk_box_append( box, GTK_WIDGET( self->entry ) );
	gtk_box_append( box, GTK_WIDGET( self->list ) );

	/* configure widgets */
	gtk_widget_set_visible( GTK_WIDGET( self->entry ), TRUE );
	gtk_widget_set_visible( GTK_WIDGET( self->list ), FALSE );
	gtk_widget_grab_focus( GTK_WIDGET( self->entry ) );
	self->is_entry_visible = TRUE;
}

static void
gr_window_class_init(
	GrWindowClass *klass )
{
}

GrWindow*
gr_window_new(
	GrApplication *app )
{
	GrWindow *self;

	g_return_val_if_fail( GR_IS_APPLICATION( app ), NULL );

	/* create window */
	self =  GR_WINDOW( g_object_new( GR_TYPE_WINDOW, "application", app, NULL ) );
	self->app = app;

	/* setup window with options from the application */
	gtk_window_set_default_size( GTK_WINDOW( self ), gr_application_get_width( self->app ), -1 );

	/* setup widgets with options from the application */
	if( gr_application_get_max_height_set( self->app ) )
		gr_list_set_max_content_height( self->list, gr_application_get_max_height( self->app ) );
	else
	{
		gr_list_set_min_content_height( self->list, gr_application_get_height( self->app ) );
		gr_list_set_max_content_height( self->list, gr_application_get_height( self->app ) );
	}

	return self;
}

