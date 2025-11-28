/* SPDX-License-Identifier: Zlib */

#ifndef DOCUMENT_WIDGET_H
#define DOCUMENT_WIDGET_H

#include <gtk/gtk.h>
#include "types.h"

/**
 * The document view widget. 
 */
struct zathura_document_widget_s {
  GtkContainer parent;
};

struct zathura_document_widget_class_s {
  GtkContainerClass parent_class;
};

#define ZATHURA_TYPE_DOCUMENT (zathura_document_widget_get_type())
#define ZATHURA_DOCUMENT(obj)                                                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocument))
#define ZATHURA_DOCUMENT_CLASS(obj)                                                                             \
  (G_TYPE_CHECK_CLASS_CAST((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocumentClass))
#define ZATHURA_IS_DOCUMENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZATHURA_TYPE_DOCUMENT))
#define ZATHURA_IS_DOCUMENT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), ZATHURA_TYPE_DOCUMENT))
#define ZATHURA_DOCUMENT_GET_CLASS(obj)                                                                         \
  (G_TYPE_INSTANCE_GET_CLASS((obj), ZATHURA_TYPE_DOCUMENT, ZathuraDocumentClass))

/**
 * Returns the type of the document view widget.
 *
 * @return the type
 */
GType zathura_document_widget_get_type(void) G_GNUC_CONST;

/**
 * Create a document view widget.
 *
 * @param zathura the zathura instance
 * @return a document view widget
 */
GtkWidget* zathura_document_widget_new(zathura_t* zathura);

#endif // DOCUMENT_WIDGET_H
