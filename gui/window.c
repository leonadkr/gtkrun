#include <glib.h>
#include <gtk/gtk.h>
#include "config.h"
#include "window.h"
#include "shared.h"
#include "entry.h"
#include "listview.h"


struct _GrWindowPrivate
{
	GtkEntry *entry;
	GtkListView *list_view;
	GtkScrolledWindow *scrolled_window;
	gboolean is_entry_visible;
};
typedef struct _GrWindowPrivate GrWindowPrivate;


static G_DEFINE_QUARK( gr-window-private, gr_window_private )

static void
gr_window_private_free(
	GrWindowPrivate *priv )
{
	g_return_if_fail( priv != NULL );

	g_free( priv );
}

static GrWindowPrivate*
gr_window_private_new(
	GtkWindow *self )
{
	GrWindowPrivate *priv;

	g_return_val_if_fail( GTK_IS_WINDOW( self ), NULL );

	priv = g_new( GrWindowPrivate, 1 );
	priv->entry = NULL;
	priv->list_view = NULL;
	priv->scrolled_window = NULL;
	priv->is_entry_visible = TRUE;

	g_object_set_qdata_full( G_OBJECT( self ), gr_window_private_quark(), priv, (GDestroyNotify)gr_window_private_free );

	return priv;
}

static GrWindowPrivate*
gr_window_get_private(
	GtkWindow *self )
{
	g_return_val_if_fail( GTK_IS_WINDOW( self ), NULL );

	return (GrWindowPrivate*)g_object_get_qdata( G_OBJECT( self ), gr_window_private_quark() );
}

static gboolean
on_event_key_pressed(
	GtkEventControllerKey *self,
	guint keyval,
	guint keycode,
	GdkModifierType state,
	gpointer user_data )
{
	GtkWindow *window = GTK_WINDOW( user_data );
	GrWindowPrivate *priv = gr_window_get_private( window );
	gchar *text;

	if( ( keyval == GDK_KEY_q && ( state & GDK_CONTROL_MASK ) ) ||
			keyval == GDK_KEY_Escape )
	{
		gtk_window_destroy( window );
		return GDK_EVENT_STOP;
	}

	if( keyval == GDK_KEY_Tab )
	{
		if( priv->is_entry_visible )
		{
			text = gr_entry_get_text_before_cursor( priv->entry );
			gr_list_view_set_model_by_text( priv->list_view, text );
			g_free( text );

			gtk_widget_hide( GTK_WIDGET( priv->entry ) );
			gtk_widget_show( GTK_WIDGET( priv->scrolled_window ) );
			gtk_widget_grab_focus( GTK_WIDGET( priv->list_view ) );
		}
		else
		{
			text = gr_list_view_get_selected_text( priv->list_view );
			gr_entry_set_text( priv->entry, text );
			g_free( text );

			gtk_widget_hide( GTK_WIDGET( priv->scrolled_window ) );
			gtk_widget_show( GTK_WIDGET( priv->entry ) );
			gtk_widget_grab_focus( GTK_WIDGET( priv->entry ) );
		}
		priv->is_entry_visible = !priv->is_entry_visible;
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static GtkScrolledWindow*
gr_scrolled_window_new(
	GtkWindow *window,
	GrShared *shared )
{
	GtkScrolledWindow *scrolled_window;

	g_return_val_if_fail( GTK_IS_WINDOW( window ), NULL );
	g_return_val_if_fail( shared != NULL, NULL );

	scrolled_window = GTK_SCROLLED_WINDOW( gtk_scrolled_window_new() );
	gtk_scrolled_window_set_policy( scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
	gtk_scrolled_window_set_has_frame( scrolled_window, FALSE );
	gtk_scrolled_window_set_kinetic_scrolling( scrolled_window, FALSE );
	gtk_scrolled_window_set_overlay_scrolling( scrolled_window, FALSE );
	gtk_scrolled_window_set_propagate_natural_height( scrolled_window, TRUE );

	/* if max-height is set, height is ignored */
	if( shared->max_height_set )
		gtk_scrolled_window_set_max_content_height( scrolled_window, shared->max_height );
	else
	{
		gtk_scrolled_window_set_min_content_height( scrolled_window, shared->height );
		gtk_scrolled_window_set_max_content_height( scrolled_window, shared->height );
	}

	return scrolled_window;
}

GtkWindow*
gr_window_new(
	GApplication *app,
	GrShared *shared )
{
	GrWindowPrivate *priv;
	GtkWindow *window;
	GtkBox *box;
	GtkEntry *entry;
	GtkScrolledWindow *scrolled_window;
	GtkListView *list_view;
	GtkEventControllerKey *event_key;

	g_return_val_if_fail( G_IS_APPLICATION( app ), NULL );
	g_return_val_if_fail( shared != NULL, NULL );

	/* create window */
	window = GTK_WINDOW( gtk_application_window_new( GTK_APPLICATION( app ) ) );
	gtk_window_set_title( window, PROGRAM_NAME );
	gtk_window_set_resizable( window, FALSE );
	gtk_window_set_decorated( window, FALSE );
	gtk_window_set_default_size( window, shared->width, -1 );

	/* bind event callback */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( window ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), window );

	/* create widgets */
	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 1 ) );
	entry = gr_entry_new( window, shared );
	scrolled_window = gr_scrolled_window_new( window, shared );
	list_view = gr_list_view_new( window, shared );

	/* layout widgets */
	gtk_scrolled_window_set_child( scrolled_window, GTK_WIDGET( list_view ) );
	gtk_window_set_child( window, GTK_WIDGET( box ) );
	gtk_box_append( box, GTK_WIDGET( entry ) );
	gtk_box_append( box, GTK_WIDGET( scrolled_window ) );
	gtk_widget_hide( GTK_WIDGET( scrolled_window ) );

	/* collect private */
	priv = gr_window_private_new( window );
	priv->entry = entry;
	priv->list_view = list_view;
	priv->scrolled_window = scrolled_window;
	priv->is_entry_visible = TRUE;

	return window;
}

