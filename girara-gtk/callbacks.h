/* SPDX-License-Identifier: Zlib */

#ifndef GIRARA_CALLBACKS_H
#define GIRARA_CALLBACKS_H

#include "gdk/gdk.h"
#include "types.h"
#include <gtk/gtk.h>

/**
 * Callback definition for an inputbar key press event handler
 *
 * @param widget The widget
 * @param event Event
 * @param data Custom data
 * @return true if no error occurred
 */
typedef gboolean (*girara_callback_inputbar_key_press_event_t)(GtkEventControllerKey* widget, guint keycode,
                                                               guint keyval, GdkModifierType state, void* data);

/**
 * Callback definition for an inputbar key press event handler
 *
 * @param entry The inputbar
 * @param data Custom data
 * @return true if no error occurred
 */
typedef gboolean (*girara_callback_inputbar_activate_t)(GtkEntry* entry, void* data);

/**
 * Default callback for key press events in the view area
 *
 * @param widget event controller
 * @param keycode key press keycode
 * @param keyval key press keyval
 * @param state key press state
 * @param session The used girara session
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
gboolean girara_callback_view_key_press_event(GtkEventControllerKey* widget, guint keycode, guint keyval,
                                              GdkModifierType state, girara_session_t* session);

/**
 * Default callback when a button (typically a mouse button) has been pressed
 *
 * @param widget The event controller
 * @param n_press The number of presses
 * @param x The x position
 * @param y The y position
 * @param session The used girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_callback_view_button_press_event(GtkGestureMultiPress* self, gint n_press, gdouble x, gdouble y,
                                                 girara_session_t* session);

/**
 * Default callback when a button (typically a mouse button) has been released
 *
 * @param widget The event controller
 * @param n_press The number of presses
 * @param x The x position
 * @param y The y position
 * @param session The used girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_callback_view_button_release_event(GtkGestureMultiPress* self, gint n_press, gdouble x, gdouble y,
                                                   girara_session_t* session);

/**
 * Default callback when the pointer moves over the widget
 *
 * @param widget The event controller
 * @param x The x position
 * @param y The y position
 * @param session The used girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_callback_view_button_motion_notify_event(GtkEventControllerMotion* self, gdouble x, gdouble y,
                                                         girara_session_t* session);

/**
 * Default callback then a scroll event is triggered by the view
 *
 * @param widget The widget
 * @param dx The x delta
 * @param dy The y delta
 * @param session The girara session
 * @return true to stop other handlers from being invoked for the event.
 * @return false to propagate the event further.
 */
gboolean girara_callback_view_scroll_event(GtkEventControllerScroll* widget, gdouble dx, gdouble dy,
                                           girara_session_t* session);

/**
 * Default callback if the inputbar gets activated
 *
 * @param entry The inputbar entry
 * @param session The used girara session
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
gboolean girara_callback_inputbar_activate(GtkEntry* entry, girara_session_t* session);

/**
 * Default callback if an key in the input bar gets pressed
 *
 * @param widget The used widget
 * @param event The occurred event
 * @param session The used girara session
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
gboolean girara_callback_inputbar_key_press_event(GtkEventControllerKey* self, guint hardware_keyval,
                                                  guint hardware_keycode, GdkModifierType state,
                                                  girara_session_t* session);

/**
 * Default callback if the text of the input bar has changed
 *
 * @param widget The used widget
 * @param session The used girara session
 * @return TRUE No error occurred
 * @return FALSE An error occurred
 */
gboolean girara_callback_inputbar_changed_event(GtkEditable* widget, girara_session_t* session);

#endif
