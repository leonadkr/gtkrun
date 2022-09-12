#ifndef WINDOW_H
#define WINDOW_H

#include <glib.h>
#include <gtk/gtk.h>
#include "path.h"

GtkWindow* gr_window_new( GApplication *app, GrPath *path );

#endif

