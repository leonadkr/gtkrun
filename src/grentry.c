#include "grentry.h"

#include "grcommandlist.h"

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

	GrCommandList *com_list;
};
typedef struct _GrEntry GrEntry;

enum _GrEntryPropertyID
{
	PROP_0, /* 0 is reserved for GObject */

	PROP_COMMAND_LIST,

	N_PROPS
};
typedef enum _GrEntryPropertyID GrEntryPropertyID;

static GParamSpec *object_props[N_PROPS] = { NULL, };

enum _GrEntrySignalID
{
	SIGNAL_ACTIVATE,

	N_SIGNALS
};
typedef enum _GrEntrySignalID GrEntrySignalID;

static guint gr_entry_signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE( GrEntry, gr_entry, GTK_TYPE_WIDGET )

static void
gr_entry_set_compared_text(
	GrEntry *self,
	gint pos )
{
	gchar *text, *s;

	g_return_if_fail( GR_IS_ENTRY( self ) );

	g_print( "gr_entry_set_compared_text()\n" );

	/* no list, do noting */
	if( self->com_list == NULL )
		return;
	
	text = gtk_editable_get_chars( self->editable, 0, pos );
	s = gr_command_list_get_compared_string( self->com_list, text );

	gtk_editable_set_text( self->editable, s == NULL ? text : s );
	gtk_editable_set_position( self->editable, pos );

	g_free( s );
	g_free( text );
}

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

	g_print( "on_editable_insert_text()\n" );
	gr_entry_set_compared_text( entry, *position );

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

	g_print( "on_editable_delete_text()\n" );
	gr_entry_set_compared_text( entry, start_pos );

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
	gchar *text;

	if( keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter )
	{
		text = gr_entry_get_text( entry );
		g_signal_emit( entry, gr_entry_signals[SIGNAL_ACTIVATE], 0, text );
		g_free( text );
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

	self->com_list = NULL;
}

static void
gr_entry_get_property(
	GObject *object,	
	guint prop_id,	
	GValue *value,	
	GParamSpec *pspec )
{
	GrEntry *self = GR_ENTRY( object );

	switch( (GrEntryPropertyID)prop_id )
	{
		case PROP_COMMAND_LIST:
			g_value_set_object( value, self->com_list );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_entry_set_property(
	GObject *object,
	guint prop_id,
	const GValue *value,
	GParamSpec *pspec )
{
	GrEntry *self = GR_ENTRY( object );

	switch( (GrEntryPropertyID)prop_id )
	{
		case PROP_COMMAND_LIST:
			gr_entry_set_command_list( self, g_value_get_object( value ) );
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID( object, prop_id, pspec );
			break;
	}
}

static void
gr_entry_dispose(
	GObject *object )
{
	GrEntry *self = GR_ENTRY( object );

	gtk_widget_unparent( GTK_WIDGET( self->entry ) );
	g_clear_object( &self->com_list );

	G_OBJECT_CLASS( gr_entry_parent_class )->dispose( object );
}

static void
gr_entry_class_init(
	GrEntryClass *klass )
{
	GObjectClass *object_class = G_OBJECT_CLASS( klass );
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS( klass );

	object_class->get_property = gr_entry_get_property;
	object_class->set_property = gr_entry_set_property;
	object_class->dispose = gr_entry_dispose;

	object_props[PROP_COMMAND_LIST] = g_param_spec_object(
		"command-list",
		"Command list",
		"Object storing the list of commands",
		GR_TYPE_COMMAND_LIST,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS );
	g_object_class_install_properties( object_class, N_PROPS, object_props );

	gr_entry_signals[SIGNAL_ACTIVATE] = g_signal_new(
		"activate",
		G_TYPE_FROM_CLASS( klass ),
		G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		0,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING );

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

gchar*
gr_entry_get_text_befor_cursor(
	GrEntry *self )
{
	gint position;
	gchar *text;

	g_return_val_if_fail( GR_IS_ENTRY( self ), NULL );

	position = gtk_editable_get_position( self->editable );
	text = gtk_editable_get_chars( self->editable, 0, position );

	return text;
}

GrCommandList*
gr_entry_get_command_list(
	GrEntry *self )
{
	g_return_val_if_fail( GR_IS_ENTRY( self ), NULL );

	if( self->com_list == NULL )
		return NULL;

	return GR_COMMAND_LIST( g_object_ref( G_OBJECT( self->com_list ) ) );
}

void
gr_entry_set_command_list(
	GrEntry *self,
	GrCommandList *com_list )
{
	g_return_if_fail( GR_IS_ENTRY( self ) );
	g_return_if_fail( GR_IS_COMMAND_LIST( com_list ) );

	g_object_freeze_notify( G_OBJECT( self ) );

	g_clear_object( &self->com_list );
	self->com_list = GR_COMMAND_LIST( g_object_ref( com_list ) );

	g_object_notify_by_pspec( G_OBJECT( self ), object_props[PROP_COMMAND_LIST] );

	g_object_thaw_notify( G_OBJECT( self ) );
}

