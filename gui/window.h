#ifndef WINDOW_H
#define WINDOW_H

#include <glib.h>
#include <gtk/gtk.h>
#include "shared.h"

GtkWindow* gr_window_new( GrShared *shared );
void gr_window_close( GtkWindow *self, GrShared *shared );
void gr_window_reset( GtkWindow *self, GrShared *shared );

#endif

