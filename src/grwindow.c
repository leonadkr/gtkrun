#include "grwindow.h"

#include "config.h"
#include "grcommandlist.h"
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
on_window_notify_application(
	GrWindow *self,
	GParamSpec *pspec,
	gpointer user_data )
{
	GrCommandList *com_list;

	g_print( "on_window_notify_application()\n" );

	self->app = GR_APPLICATION( gtk_window_get_application( GTK_WINDOW( self ) ) );

	/*setup window and widgets with the options from the application */
	gtk_window_set_default_size( GTK_WINDOW( self ), gr_application_get_width( self->app ), -1 );

	if( gr_application_get_max_height_set( self->app ) )
		gr_list_set_max_content_height( self->list, gr_application_get_max_height( self->app ) );
	else
	{
		gr_list_set_min_content_height( self->list, gr_application_get_height( self->app ) );
		gr_list_set_max_content_height( self->list, gr_application_get_height( self->app ) );
	}

	com_list = gr_application_get_command_list( self->app );
	gr_entry_set_command_list( self->entry, com_list );
	g_object_unref( G_OBJECT( com_list ) );
}

static void
gr_window_switch_widgets(
	GrWindow *self )
{
	GrCommandList *com_list;
	gchar *text;
	GStrv arr;

	g_print( "window_switch_widgets()\n" );

	if( self->is_entry_visible )
	{
		text = gr_entry_get_text_befor_cursor( self->entry );

		com_list = gr_application_get_command_list( self->app );
		arr = gr_command_list_get_compared_array( com_list, text );
		g_free( text );
		g_object_unref( G_OBJECT( com_list ) );

		gr_list_set_array( self->list, arr );
		g_strfreev( arr );

		gtk_widget_set_visible( GTK_WIDGET( self->entry ), FALSE );
		gtk_widget_set_visible( GTK_WIDGET( self->list ), TRUE );
		gtk_widget_grab_focus( GTK_WIDGET( self->list ) );
	}
	else
	{
		text = gr_list_get_selected_text( self->list );
		gr_entry_set_text( self->entry, text );
		g_free( text );

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
		gtk_window_destroy( GTK_WINDOW( window ) );
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
on_widget_activate(
	GtkWidget *widget,
	const gchar *text,
	gpointer user_data )
{
	GrWindow *window = GR_WINDOW( user_data );
	gchar *command, *command_locale;
	GSubprocessFlags flags;
	GSubprocess *subproc;
	GrCommandList *com_list;
	GError *error = NULL;

	g_print( "on_widget_activate() with %s\n", text );

	/* nothing to do */
	if( text == NULL || *text == '\0' )
		return;

	/* strip text to nice command string */
	command = g_strdup( text );
	g_strstrip( command );

	/* nothing to call */
	if( strlen( command ) == 0 )
	{
		g_free(command );
		return;
	}

	if( gr_application_get_silent( window->app ) )
		flags = G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE;
	else
		flags = G_SUBPROCESS_FLAGS_NONE;

	/* do system call */
	command_locale = g_filename_from_utf8( command, -1, NULL, NULL, NULL );
	subproc = g_subprocess_new( flags, &error, command_locale, NULL );
	g_free(command_locale );
	if( error != NULL )
	{
		g_log_structured( G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
			"MESSAGE", error->message,
			NULL );
		g_clear_error( &error );
		g_free(command );
		return;
	}
	g_object_unref( G_OBJECT( subproc ) );

	/* store new command */
	com_list = gr_application_get_command_list( window->app );
	gr_command_list_push( com_list, command );
	g_free(command );
	g_object_unref( G_OBJECT( com_list ) );

	gtk_window_destroy( GTK_WINDOW( window ) );
}

static void
gr_window_init(
	GrWindow *self )
{
	GtkEventControllerKey *event_key;
	GtkBox *box;

	g_print( "gr_window_init()\n" );

	/* setup window */
	gtk_window_set_title( GTK_WINDOW( self ), PROGRAM_NAME );
	gtk_window_set_resizable( GTK_WINDOW( self ), FALSE );
	gtk_window_set_decorated( GTK_WINDOW( self ), FALSE );
	g_signal_connect( G_OBJECT( self ), "notify::application", G_CALLBACK( on_window_notify_application ), self );

	/* add event controller */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( self ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), self );

	/* create widgets */
	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 1 ) );

	self->entry = gr_entry_new();
	g_signal_connect( G_OBJECT( self->entry ), "activate", G_CALLBACK( on_widget_activate ), self );

	self->list = gr_list_new();
	g_signal_connect( G_OBJECT( self->list ), "activate", G_CALLBACK( on_widget_activate ), self );

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
	g_return_val_if_fail( GR_IS_APPLICATION( app ), NULL );

	g_print( "gr_window_new()\n" );

	return GR_WINDOW( g_object_new( GR_TYPE_WINDOW, "application", app, NULL ) );
}

