#ifndef ENTRY_H
#define ENTRY_H

#include <glib.h>
#include <gtk/gtk.h>
#include "shared.h"

GtkEntry* gr_entry_new( GtkWindow *window, GrShared *shared );
gchar* gr_entry_get_text_before_cursor( GtkEntry *self );
void gr_entry_set_text( GtkEntry *self, const gchar* text );

#endif
