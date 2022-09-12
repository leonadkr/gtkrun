#include <glib.h>
#include <gtk/gtk.h>
#include "config.h"
#include "window.h"
#include "path.h"
#include "entry.h"
#include "treeview.h"


struct _GrWindowPrivate
{
	GtkEntry *entry;
	GtkTreeView *tree_view;
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
	priv->tree_view = NULL;
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
			gr_tree_view_set_model_by_text( priv->tree_view, text );
			g_free( text );
			gtk_widget_hide( GTK_WIDGET( priv->entry ) );
			gtk_widget_show( GTK_WIDGET( priv->tree_view ) );
		}
		else
		{
			text = gr_tree_view_get_selected_text( priv->tree_view );
			gr_entry_set_text( priv->entry, text );
			g_free( text );
			gtk_widget_hide( GTK_WIDGET( priv->tree_view ) );
			gtk_widget_show( GTK_WIDGET( priv->entry ) );
		}
		priv->is_entry_visible = !priv->is_entry_visible;
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}


GtkWindow*
gr_window_new(
	GApplication *app,
	GrPath *path )
{
	GrWindowPrivate *priv;
	GtkWindow *window;
	GtkBox *box;
	GtkEntry *entry;
	GtkTreeView *tree_view;
	GtkEventControllerKey *event_key;

	g_return_val_if_fail( G_IS_APPLICATION( app ), NULL );
	g_return_val_if_fail( GR_IS_PATH( path ), NULL );

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
	box = GTK_BOX( gtk_box_new( GTK_ORIENTATION_VERTICAL, 1 ) );
	entry = gr_entry_new( window, path );
	tree_view = gr_tree_view_new( window, path );
	gtk_widget_hide( GTK_WIDGET( tree_view ) );

	/* layout widgets */
	gtk_window_set_child( window, GTK_WIDGET( box ) );
	gtk_box_append( box, GTK_WIDGET( entry ) );
	gtk_box_append( box, GTK_WIDGET( tree_view ) );

	/* collect private */
	priv = gr_window_private_new( window );
	priv->entry = entry;
	priv->tree_view = tree_view;
	priv->is_entry_visible = TRUE;

	return window;
}

