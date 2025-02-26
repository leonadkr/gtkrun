#include "grentry.h"

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

struct _GrEntry
{
	GtkWidget parent_instance;

	gulong insert_text_handler_id;
	gulong delete_text_handler_id;
	guint insert_text_signal_id;
	guint delete_text_signal_id;

	GtkEntry *entry;
	GtkEditable *editable;
};
typedef struct _GrEntry GrEntry;

enum _GrEntrySignalID
{
	SIGNAL_ACTIVATE,

	N_SIGNALS
};
typedef enum _GrEntrySignalID GrEntrySignalID;

static guint gr_entry_signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE( GrEntry, gr_entry, GTK_TYPE_WIDGET )

static void
on_editable_insert_text(
	GtkEditable *self,
	gchar *text,
	gint length,
	gint *position,
	gpointer user_data )
{
	GrEntry *entry = GR_ENTRY( user_data );

	g_signal_handler_block( G_OBJECT( self ), entry->insert_text_handler_id );
	g_signal_handler_block( G_OBJECT( self ), entry->delete_text_handler_id );

	/* gr_editable_set_compared_text( self, *position ); */
	g_print( "on_editable_insert_text()\n" );

	g_signal_handler_unblock( G_OBJECT( self ), entry->insert_text_handler_id );
	g_signal_handler_unblock( G_OBJECT( self ), entry->delete_text_handler_id );

	g_signal_stop_emission( G_OBJECT( self ), entry->insert_text_signal_id, 0 );
}

static void
on_editable_delete_text(
	GtkEditable *self,
	gint start_pos,
	gint end_pos,
	gpointer user_data )
{
	GrEntry *entry = GR_ENTRY( user_data );

	g_signal_handler_block( G_OBJECT( self ), entry->insert_text_handler_id );
	g_signal_handler_block( G_OBJECT( self ), entry->delete_text_handler_id );

	/* gr_editable_set_compared_text( self, start_pos ); */
	g_print( "on_editable_delete_text()\n" );

	g_signal_handler_unblock( G_OBJECT( self ), entry->insert_text_handler_id );
	g_signal_handler_unblock( G_OBJECT( self ), entry->delete_text_handler_id );

	g_signal_stop_emission( G_OBJECT( self ), entry->delete_text_signal_id, 0 );
}

static gboolean
on_event_key_pressed(
	GtkEventControllerKey *self,
	guint keyval,
	guint keycode,
	GdkModifierType state,
	gpointer user_data )
{
	GrEntry *entry = GR_ENTRY( user_data );

	if( keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter )
	{
		g_signal_emit( entry, gr_entry_signals[SIGNAL_ACTIVATE], 0 );
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void
gr_entry_init(
	GrEntry *self )
{
	GtkEventControllerKey *event_key;

	self->entry = GTK_ENTRY( gtk_entry_new() );

	/* setup editable */
	self->editable = gtk_editable_get_delegate( GTK_EDITABLE( self->entry ) );
	self->insert_text_handler_id = g_signal_connect_after( G_OBJECT( self->editable ), "insert-text", G_CALLBACK( on_editable_insert_text ), self );
	self->delete_text_handler_id = g_signal_connect_after( G_OBJECT( self->editable ), "delete-text", G_CALLBACK( on_editable_delete_text ), self );
	self->insert_text_signal_id = g_signal_lookup( "insert-text", GTK_TYPE_EDITABLE );
	self->delete_text_signal_id = g_signal_lookup( "delete-text", GTK_TYPE_EDITABLE );

	/* setup event handler */
	event_key = GTK_EVENT_CONTROLLER_KEY( gtk_event_controller_key_new() );
	gtk_widget_add_controller( GTK_WIDGET( self->editable ), GTK_EVENT_CONTROLLER( event_key ) );
	g_signal_connect( G_OBJECT( event_key ), "key-pressed", G_CALLBACK( on_event_key_pressed ), self );

	/* layout widgets */
	gtk_widget_set_parent( GTK_WIDGET( self->entry ), GTK_WIDGET( self ) );
}

static void
gr_entry_dispose(
	GObject *object )
{
	GrEntry *self = GR_ENTRY( object );

	gtk_widget_unparent( GTK_WIDGET( self->entry ) );
}

static void
gr_entry_class_init(
	GrEntryClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( klass );

	object_class->dispose = gr_entry_dispose;

	gr_entry_signals[SIGNAL_ACTIVATE] = g_signal_new(
		"activate",
		G_TYPE_FROM_CLASS( klass ),
		G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		0,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		0 );

	gtk_widget_class_set_layout_manager_type( widget_class, GTK_TYPE_BIN_LAYOUT );
}

GrEntry*
gr_entry_new(
	void )
{
	return GR_ENTRY( g_object_new( GR_TYPE_ENTRY, NULL ) );
}

void
gr_entry_set_text(
	GrEntry *self,
	const gchar* text )
{
	g_return_if_fail( GR_IS_ENTRY( self ) );

	if( text == NULL )
		return;

	gtk_editable_set_text( self->editable, text );
}

gchar*
gr_entry_get_text(
	GrEntry *self )
{
	g_return_val_if_fail( GR_IS_ENTRY( self ), NULL );

	return g_strdup( gtk_editable_get_text( self->editable ) );
}

