/* SPDX-License-Identifier: Zlib */

#include "document-widget.h"
#include "girara/log.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "document.h"
#include "page.h"
#include "utils.h"
#include "zathura.h"
#include "zathura/adjustment.h"

typedef struct {
  unsigned int pos;
  unsigned int size;
} document_widget_line_s;

typedef struct zathura_document_widget_private_s {
  zathura_t* zathura;

  /* Layout */
  gboolean pages_right_to_left;
  unsigned int nrow;
  unsigned int ncol;
  document_widget_line_s* row_heights;
  document_widget_line_s* col_widths;

  /* Scrolling */
  GtkAdjustment* hadjustment;
  GtkAdjustment* vadjustment;
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;
} ZathuraDocumentPrivate;

G_DEFINE_TYPE_WITH_CODE(ZathuraDocument, zathura_document_widget, GTK_TYPE_CONTAINER,
                        G_ADD_PRIVATE(ZathuraDocument) G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL))

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec);
static void zathura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec);
static void zathura_document_widget_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static void zathura_document_widget_container_add(GtkContainer* container, GtkWidget* widget);
static void zathura_document_widget_container_remove(GtkContainer* container, GtkWidget* widget);
static void zathura_document_widget_container_forall(GtkContainer* container, gboolean include_internals, GtkCallback callback, gpointer user_data);

enum properties_e {
  PROP_0,
  PROP_ZATHURA,
  PROP_PAGES_RIGHT_TO_LEFT,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
};

static void zathura_document_widget_class_init(ZathuraDocumentClass* class) {
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS (class);
  widget_class->size_allocate = zathura_document_widget_size_allocate;

  GObjectClass* object_class = G_OBJECT_CLASS (class);
  object_class->set_property = zathura_document_widget_set_property;
  object_class->get_property = zathura_document_widget_get_property;

  GtkContainerClass* container_class = GTK_CONTAINER_CLASS (class);
  container_class->add               = zathura_document_widget_container_add;
  container_class->remove            = zathura_document_widget_container_remove;
  container_class->forall            = zathura_document_widget_container_forall;

  g_object_class_install_property(
      object_class, PROP_ZATHURA,
      g_param_spec_pointer("zathura", "zathura", "the zathura instance",
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property(
      object_class, PROP_PAGES_RIGHT_TO_LEFT,
      g_param_spec_boolean("pages-right-to-left", "pages-right-to-left", "layout pages left to right", false, 
                           G_PARAM_WRITABLE | G_PARAM_READABLE));

  g_object_class_override_property (object_class, PROP_HADJUSTMENT, "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT, "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY, "vscroll-policy");
}

static void zathura_document_widget_init(ZathuraDocument* widget) {
  gtk_widget_set_has_window(GTK_WIDGET(widget), false);

  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(widget);

  priv->zathura = NULL;

  priv->nrow = 0;
  priv->ncol = 0;
  priv->row_heights = NULL;
  priv->col_widths = NULL;
}

GtkWidget* zathura_document_widget_new(zathura_t* zathura) {
  GObject* ret = g_object_new(ZATHURA_TYPE_DOCUMENT, "zathura", zathura, NULL);
  if (ret == NULL) {
    return NULL;
  }

  ZathuraDocument* widget = ZATHURA_DOCUMENT(ret);
  GtkWidget* gtk_widget   = GTK_WIDGET(widget);

  return gtk_widget;
}

static void zathura_document_widget_set_scroll_adjustment (ZathuraDocument* widget, 
                                                           GtkOrientation orientation, 
                                                           GtkAdjustment *adjustment) {
  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(widget);
  GtkAdjustment **to_set;
  const gchar *prop_name;

  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    to_set = &priv->hadjustment;
    prop_name = "hadjustment";
  } else {
    to_set = &priv->vadjustment;
    prop_name = "vadjustment";
  }

  if (adjustment && adjustment == *to_set)
    return;

  if (*to_set) {
    g_signal_handlers_disconnect_by_data (*to_set, widget);
    g_object_unref (*to_set);
  }

  if (!adjustment)
    adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

  g_signal_connect_swapped(adjustment, "value-changed",
                           G_CALLBACK (gtk_widget_queue_allocate),
                           widget);

  *to_set = g_object_ref_sink (adjustment);

  g_object_notify(G_OBJECT(widget), prop_name);
}

static void zathura_document_widget_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
  ZathuraDocument* document = ZATHURA_DOCUMENT(object);
  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(document);

  switch (prop_id) {
  case PROP_ZATHURA:
    priv->zathura = g_value_get_pointer(value);
    break;
  case PROP_PAGES_RIGHT_TO_LEFT:
    priv->pages_right_to_left = g_value_get_boolean(value);
    break;
  case PROP_HADJUSTMENT:
    zathura_document_widget_set_scroll_adjustment(document, GTK_ORIENTATION_HORIZONTAL, g_value_get_object(value));
    break;
  case PROP_VADJUSTMENT:
    zathura_document_widget_set_scroll_adjustment(document, GTK_ORIENTATION_VERTICAL, g_value_get_object(value));
    break;
  case PROP_HSCROLL_POLICY:
    priv->hscroll_policy = g_value_get_enum (value);
    gtk_widget_queue_resize (GTK_WIDGET (document));
    break;
  case PROP_VSCROLL_POLICY:
    priv->vscroll_policy = g_value_get_enum (value);
    gtk_widget_queue_resize (GTK_WIDGET (document));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void zathura_document_widget_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
  ZathuraDocument* document = ZATHURA_DOCUMENT(object);
  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(document);

  switch (prop_id) {
  case PROP_HADJUSTMENT:
    g_value_set_object (value, priv->hadjustment);
    break;
  case PROP_PAGES_RIGHT_TO_LEFT:
    g_value_set_boolean (value, priv->pages_right_to_left);
    break;
  case PROP_VADJUSTMENT:
    g_value_set_object (value, priv->vadjustment);
    break;
  case PROP_HSCROLL_POLICY:
    g_value_set_enum (value, priv->hscroll_policy);
    break;
  case PROP_VSCROLL_POLICY:
    g_value_set_enum (value, priv->vscroll_policy);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

/* drawing */
static void zathura_document_widget_line_prefix_sum(document_widget_line_s *array, unsigned int n, unsigned int pad) {
  array[0].pos = 0;

  for (unsigned int i = 1; i < n; i++) {
    array[i].pos = array[i - 1].pos + array[i - 1].size + pad;
  }
}

static void zathura_document_widget_arrange_grid(ZathuraDocument* widget) {
  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(widget);
  zathura_document_t* z_document = zathura_get_document(priv->zathura);

  const unsigned int c0   = zathura_document_get_first_page_column(z_document);
  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  const unsigned int ncol = zathura_document_get_pages_per_row(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  const unsigned int page_v_padding = zathura_document_get_page_v_padding(z_document);
  const unsigned int page_h_padding = zathura_document_get_page_h_padding(z_document);

  memset(priv->row_heights, 0, nrow * sizeof( document_widget_line_s ));
  memset(priv->col_widths, 0, ncol * sizeof( document_widget_line_s ));

  unsigned int col = c0 - 1;
  unsigned int row = 0;

  // calculate the max width and height required for each column and row
  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page = zathura_document_get_page(z_document, i);

    unsigned int x = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
    unsigned int y = row;

    unsigned int width = zathura_page_get_width(page);
    unsigned int height = zathura_page_get_height(page);

    unsigned int page_width, page_height;
    page_calc_height_width(z_document, height, width, &page_height, &page_width, true);

    priv->row_heights[y].size = MAX(page_height, priv->row_heights[y].size);
    priv->col_widths[x].size  = MAX(page_width, priv->col_widths[x].size);

    // increment row and column
    row += (col + 1) / ncol;
    col = (col + 1) % ncol;
  }

  zathura_document_widget_line_prefix_sum(priv->col_widths, ncol, page_h_padding);
  zathura_document_widget_line_prefix_sum(priv->row_heights, nrow, page_v_padding);
}

static void zathura_document_widget_get_adjustment(ZathuraDocument* document, int height, int width, int* adj_v, int* adj_h) {
  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document = zathura_get_document(priv->zathura);

  const unsigned int value_v = gtk_adjustment_get_value(priv->vadjustment);
  const unsigned int value_h = gtk_adjustment_get_value(priv->hadjustment);

  unsigned int doc_height, doc_width;
  zathura_document_get_document_size(z_document, &doc_height, &doc_width);

  const int center_v = (height - doc_height) / 2;
  const int center_h = (width - doc_width) / 2;

  // if document is smaller than allocation, center the document
  *adj_v = ((int)doc_height < height) ? -center_v : (int)value_v;
  *adj_h = ((int)doc_width < width) ? -center_h : (int)value_h;
}

static void zathura_document_widget_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
  ZathuraDocument* document = ZATHURA_DOCUMENT(widget);
  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document = zathura_get_document(priv->zathura);

  unsigned int height, width;
  zathura_document_get_document_size(z_document, &height, &width);

  gtk_adjustment_set_upper(priv->hadjustment, width);
  gtk_adjustment_set_upper(priv->vadjustment, height);

  gtk_adjustment_set_page_size(priv->hadjustment, allocation->width);
  gtk_adjustment_set_page_size(priv->vadjustment, allocation->height);

  zathura_document_widget_arrange_grid(document);

  const unsigned int c0   = zathura_document_get_first_page_column(z_document);
  const unsigned int ncol = zathura_document_get_pages_per_row(z_document);
  const unsigned int npag = zathura_document_get_number_of_pages(z_document);

  int adj_v, adj_h;
  zathura_document_widget_get_adjustment(document, allocation->height, allocation->width, &adj_v, &adj_h);

  unsigned int x, y;
  unsigned int col = c0 - 1;
  unsigned int row = 0;

  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);

    x = priv->pages_right_to_left ? priv->ncol - 1 - col : col;
    y = row;

    document_widget_line_s col_line = priv->col_widths[x];
    document_widget_line_s row_line = priv->row_heights[y];

    GtkAllocation page_alloc = {
      .x = col_line.pos - adj_h, 
      .y = row_line.pos - adj_v, 
      .width = col_line.size, 
      .height = row_line.size
    };

    gtk_widget_set_child_visible(page_widget, true);
    gtk_widget_size_allocate(page_widget, &page_alloc);

    // increment row and column
    row += (col + 1) / ncol;
    col = (col + 1) % ncol;
  }

  GTK_WIDGET_CLASS(zathura_document_widget_parent_class)->size_allocate(widget, allocation);
}

/* container class funcs */

static void zathura_document_widget_container_add(GtkContainer* container, GtkWidget* widget) {
  GtkWidget* parent = gtk_widget_get_parent(widget);
  if (parent != NULL) {
    girara_warning("page widget is already added to a document widget");
    return;
  }

  gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

static void zathura_document_widget_container_remove(GtkContainer* UNUSED(container), GtkWidget* widget) {
  gtk_widget_unparent(widget);
}

static void zathura_document_widget_container_forall(GtkContainer* container, gboolean UNUSED(include_internals), 
                                                     GtkCallback callback, gpointer user_data) {
  ZathuraDocument* document = ZATHURA_DOCUMENT(container);
  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(document);

  zathura_document_t* z_document = zathura_get_document(priv->zathura);
  unsigned int npag = zathura_document_get_number_of_pages(z_document);

  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);

    callback(page_widget, user_data);
  }
}

void zathura_document_widget_refresh_layout(ZathuraDocument* document) {
  g_return_if_fail(document != NULL);

  ZathuraDocumentPrivate* priv = zathura_document_widget_get_instance_private(document);
  zathura_document_t* z_document = zathura_get_document(priv->zathura);

  const unsigned int c0   = zathura_document_get_first_page_column(z_document);
  const unsigned int npag = zathura_document_get_number_of_pages(z_document);
  const unsigned int ncol = zathura_document_get_pages_per_row(z_document);
  const unsigned int nrow = (npag + c0 - 1 + ncol - 1) / ncol;

  g_free(priv->col_widths);
  g_free(priv->row_heights);

  priv->col_widths = g_try_malloc_n(ncol, sizeof( document_widget_line_s ));
  priv->row_heights = g_try_malloc_n(nrow, sizeof( document_widget_line_s ));

  priv->ncol = ncol;
  priv->nrow = nrow;

  // parent all page widgets to the document widget
  for (unsigned int i = 0; i < npag; i++) {
    zathura_page_t* page = zathura_document_get_page(z_document, i);
    GtkWidget* page_widget = zathura_page_get_widget(priv->zathura, page);

    gtk_widget_unparent(page_widget);
    gtk_container_add(GTK_CONTAINER(document), page_widget);
  }
}
