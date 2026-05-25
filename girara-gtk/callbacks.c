/* SPDX-License-Identifier: Zlib */

#include "callbacks.h"

#include "commands.h"
#include "internal.h"
#include "session.h"
#include "shortcuts.h"

#include <girara/input-history.h>
#include <girara/datastructures.h>
#include <girara/log.h>
#include <girara/utils.h>
#include <glib/gi18n-lib.h>

static const guint ALL_ACCELS_MASK = GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK;
static const guint MOUSE_MASK      = GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK | GDK_BUTTON1_MASK |
                                     GDK_BUTTON2_MASK | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK;

static bool clean_mask(GtkEventControllerKey* controller, guint hardware_keycode, GdkModifierType state, guint* clean,
                       guint* keyval) {
  guint group = gtk_event_controller_key_get_group(controller);

  GtkWidget* widget        = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
  GdkDisplay* display      = gtk_widget_get_display(widget);
  GdkModifierType consumed = 0;

  if ((gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(display), hardware_keycode, state, group, keyval,
                                           NULL, NULL, &consumed)) == FALSE) {
    return false;
  }

  if (clean != NULL) {
    *clean = state & ~consumed & ALL_ACCELS_MASK;
  }

  /* numpad numbers */
  switch (*keyval) {
  case GDK_KEY_KP_0:
    *keyval = GDK_KEY_0;
    break;
  case GDK_KEY_KP_1:
    *keyval = GDK_KEY_1;
    break;
  case GDK_KEY_KP_2:
    *keyval = GDK_KEY_2;
    break;
  case GDK_KEY_KP_3:
    *keyval = GDK_KEY_3;
    break;
  case GDK_KEY_KP_4:
    *keyval = GDK_KEY_4;
    break;
  case GDK_KEY_KP_5:
    *keyval = GDK_KEY_5;
    break;
  case GDK_KEY_KP_6:
    *keyval = GDK_KEY_6;
    break;
  case GDK_KEY_KP_7:
    *keyval = GDK_KEY_7;
    break;
  case GDK_KEY_KP_8:
    *keyval = GDK_KEY_8;
    break;
  case GDK_KEY_KP_9:
    *keyval = GDK_KEY_9;
    break;
  }

  return true;
}

/* callback implementation */
gboolean girara_callback_view_key_press_event(GtkEventControllerKey* widget, guint UNUSED(hardware_keyval),
                                              guint hardware_keycode, GdkModifierType state,
                                              girara_session_t* session) {
  g_return_val_if_fail(session != NULL, FALSE);

  guint clean  = 0;
  guint keyval = 0;

  if (clean_mask(widget, hardware_keycode, state, &clean, &keyval) == false) {
    return false;
  }

  girara_session_private_t* session_private = session->private_data;

  /* prepare event */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.shortcuts); ++idx) {
    girara_shortcut_t* shortcut = girara_list_nth(session->bindings.shortcuts, idx);
    if (session_private->buffer.command != NULL) {
      break;
    }

    if (keyval == shortcut->key &&
        (clean == shortcut->mask || (shortcut->key >= 0x21 && shortcut->key <= 0x7E && clean == GDK_SHIFT_MASK)) &&
        (session->modes.current_mode == shortcut->mode || shortcut->mode == 0) && shortcut->function != NULL) {
      const int t = (session_private->buffer.n > 0) ? session_private->buffer.n : 1;
      for (int i = 0; i < t; i++) {
        if (shortcut->function(session, &(shortcut->argument), NULL, session_private->buffer.n) == false) {
          break;
        }
      }

      if (session->global.buffer != NULL) {
        g_string_free(session->global.buffer, TRUE);
        session->global.buffer = NULL;
      }

      session_private->buffer.n = 0;

      if (session->events.buffer_changed != NULL) {
        session->events.buffer_changed(session);
      }

      return TRUE;
    }
  }

  gunichar codepoint = gdk_keyval_to_unicode(keyval);

  /* update buffer */
  /* 0xff00 was chosen because every "special" keyval seems to be above it */
  if (keyval >= 0x21 && keyval < 0xff00 && codepoint) {
    /* overall buffer */
    if (session->global.buffer == NULL) {
      session->global.buffer = g_string_new("");
    }

    session->global.buffer = g_string_append_unichar(session->global.buffer, codepoint);

    if (session_private->buffer.command == NULL && keyval >= 0x30 && keyval <= 0x39) {
      if (((session_private->buffer.n * 10) + (keyval - '0')) < INT_MAX) {
        session_private->buffer.n = (session_private->buffer.n * 10) + (keyval - '0');
      }
    } else {
      if (session_private->buffer.command == NULL) {
        session_private->buffer.command = g_string_new("");
      }

      session_private->buffer.command = g_string_append_unichar(session_private->buffer.command, codepoint);
    }

    if (session->events.buffer_changed != NULL) {
      session->events.buffer_changed(session);
    }
  } else if (keyval == GDK_KEY_Escape) {
    if (session_private->buffer.command != NULL) {
      g_string_free(session_private->buffer.command, TRUE);
      session_private->buffer.command = NULL;
    }
    if (session->global.buffer != NULL) {
      g_string_free(session->global.buffer, TRUE);
      session->global.buffer = NULL;
    }
    if (session->events.buffer_changed != NULL) {
      session->events.buffer_changed(session);
    }
  }

  /* check for buffer command */
  if (session_private->buffer.command != NULL) {
    bool matching_command = false;

    for (size_t idx = 0; idx != girara_list_size(session->bindings.shortcuts); ++idx) {
      girara_shortcut_t* shortcut = girara_list_nth(session->bindings.shortcuts, idx);
      if (shortcut->buffered_command != NULL) {
        /* buffer could match a command */
        if (!strncmp(session_private->buffer.command->str, shortcut->buffered_command,
                     session_private->buffer.command->len)) {
          /* command matches buffer exactly */
          if (!g_strcmp0(session_private->buffer.command->str, shortcut->buffered_command) &&
              (session->modes.current_mode == shortcut->mode || shortcut->mode == 0)) {
            g_string_free(session_private->buffer.command, TRUE);
            g_string_free(session->global.buffer, TRUE);
            session_private->buffer.command = NULL;
            session->global.buffer          = NULL;

            if (session->events.buffer_changed != NULL) {
              session->events.buffer_changed(session);
            }

            int t = (session_private->buffer.n > 0) ? session_private->buffer.n : 1;
            for (int i = 0; i < t; i++) {
              if (shortcut->function(session, &(shortcut->argument), NULL, session_private->buffer.n) == false) {
                break;
              }
            }

            session_private->buffer.n = 0;
            return TRUE;
          }

          matching_command = true;
        }
      }
    }

    /* free buffer if buffer will never match a command */
    if (matching_command == false) {
      g_string_free(session_private->buffer.command, TRUE);
      g_string_free(session->global.buffer, TRUE);
      session_private->buffer.command = NULL;
      session->global.buffer          = NULL;
      session_private->buffer.n       = 0;

      if (session->events.buffer_changed != NULL) {
        session->events.buffer_changed(session);
      }
    }
  }

  return FALSE;
}

gboolean girara_callback_view_button_press_event(GtkGestureMultiPress* widget, gint UNUSED(n_press), gdouble x, gdouble y,
                                                 girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  // GTK4: changes to gtk_event_controller_get_current_event(self)
  GdkEvent* gtk_event = gtk_get_current_event();

  // GTK4: changes to gdk_event_get_modifier_state(gtk_event)
  GdkModifierType gtk_state;
  gdk_event_get_state(gtk_event, &gtk_state);

  GdkEventType type = gdk_event_get_event_type(gtk_event);
  guint state       = gtk_state & MOUSE_MASK;
  guint button      = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(widget));

  /* prepare girara event */
  girara_event_t event = {.x = x, .y = y};

  switch (type) {
  case GDK_BUTTON_PRESS:
    event.type = GIRARA_EVENT_BUTTON_PRESS;
    break;
  case GDK_2BUTTON_PRESS:
    event.type = GIRARA_EVENT_2BUTTON_PRESS;
    break;
  case GDK_3BUTTON_PRESS:
    event.type = GIRARA_EVENT_3BUTTON_PRESS;
    break;
  default: /* do not handle unknown events */
    event.type = GIRARA_EVENT_OTHER;
    break;
  }

  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && button == mouse_event->button && state == mouse_event->mask &&
        mouse_event->event_type == event.type &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &mouse_event->argument, &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

gboolean girara_callback_view_button_release_event(GtkGestureMultiPress* widget, gint UNUSED(n_press), gdouble x,
                                                   gdouble y, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  // GTK4: changes to gtk_event_controller_get_current_event(self)
  GdkEvent* gtk_event = gtk_get_current_event();

  // GTK4: changes to gdk_event_get_modifier_state(gtk_event)
  GdkModifierType gtk_state;
  gdk_event_get_state(gtk_event, &gtk_state);

  guint state  = gtk_state & MOUSE_MASK;
  guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(widget));

  /* prepare girara event */
  girara_event_t event                      = {.type = GIRARA_EVENT_BUTTON_RELEASE, .x = x, .y = y};
  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && button == mouse_event->button && state == mouse_event->mask &&
        mouse_event->event_type == GIRARA_EVENT_BUTTON_RELEASE &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &(mouse_event->argument), &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

gboolean girara_callback_view_button_motion_notify_event(GtkEventControllerMotion* UNUSED(widget), gdouble x, gdouble y,
                                                         girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  /* prepare girara event */
  girara_event_t event = {.type = GIRARA_EVENT_MOTION_NOTIFY, .x = x, .y = y};

  // GTK4: changes to gtk_event_controller_get_current_event(self)
  GdkEvent* gtk_event = gtk_get_current_event();

  // GTK4: changes to gdk_event_get_modifier_state(gtk_event)
  GdkModifierType gtk_state;
  gdk_event_get_state(gtk_event, &gtk_state);

  const guint state                         = gtk_state & MOUSE_MASK;
  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && state == mouse_event->mask && mouse_event->event_type == event.type &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &(mouse_event->argument), &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

gboolean girara_callback_view_scroll_event(GtkEventControllerScroll* UNUSED(widget), gdouble dx, gdouble dy, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  // GTK4: changes to gtk_event_controller_get_current_event(self)
  GdkEvent* gtk_event = gtk_get_current_event();

  // GTK4: changes to gdk_event_get_modifier_state(gtk_event)
  GdkModifierType gtk_state;
  gdk_event_get_state(gtk_event, &gtk_state);

  // GTK4: changes to gdk_event_get_position(gtk_event, &event.x, &event.y)
  /* prepare girara event */
  girara_event_t event = {.x = gtk_event->scroll.x, gtk_event->scroll.y};

  // GTK4: changes to gdk_scroll_event_get_direction(gtk_event)
  switch (gtk_event->scroll.direction) {
  case GDK_SCROLL_UP:
    event.type = GIRARA_EVENT_SCROLL_UP;
    break;
  case GDK_SCROLL_DOWN:
    event.type = GIRARA_EVENT_SCROLL_DOWN;
    break;
  case GDK_SCROLL_LEFT:
    event.type = GIRARA_EVENT_SCROLL_LEFT;
    break;
  case GDK_SCROLL_RIGHT:
    event.type = GIRARA_EVENT_SCROLL_RIGHT;
    break;
  case GDK_SCROLL_SMOOTH:
    event.type = GIRARA_EVENT_SCROLL_BIDIRECTIONAL;
    /* We abuse x and y here. We really need more fields in girara_event_t. */
    event.x = dx;
    event.y = dy;
#ifdef __APPLE__
    /* Apple has much higher deltas */
    event.x /= 50;
    event.y /= 50;
#endif
    break;
  default:
    return false;
  }

  const guint state                         = gtk_state & MOUSE_MASK;
  girara_session_private_t* session_private = session->private_data;

  /* search registered mouse events */
  /* TODO: Filter correct event */
  for (size_t idx = 0; idx != girara_list_size(session->bindings.mouse_events); ++idx) {
    girara_mouse_event_t* mouse_event = girara_list_nth(session->bindings.mouse_events, idx);
    if (mouse_event->function != NULL && state == mouse_event->mask && mouse_event->event_type == event.type &&
        (session->modes.current_mode == mouse_event->mode || mouse_event->mode == 0)) {
      mouse_event->function(session, &(mouse_event->argument), &event, session_private->buffer.n);
      return true;
    }
  }

  return false;
}

gboolean girara_callback_inputbar_activate(GtkEntry* entry, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, FALSE);

  /* a custom handler has been installed (e.g. by girara_dialog) */
  if (session->signals.inputbar_custom_activate != NULL) {
    gboolean return_value = session->signals.inputbar_custom_activate(entry, session->signals.inputbar_custom_data);

    /* disconnect custom handler */
    session->signals.inputbar_custom_activate        = NULL;
    session->signals.inputbar_custom_key_press_event = NULL;
    session->signals.inputbar_custom_data            = NULL;

    if (session->gtk.inputbar_dialog != NULL && session->gtk.inputbar_entry != NULL) {
      gtk_label_set_markup(session->gtk.inputbar_dialog, "");
      gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar_dialog));
      if (session->global.autohide_inputbar == true) {
        gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
      }
      gtk_entry_set_visibility(session->gtk.inputbar_entry, TRUE);
      girara_isc_abort(session, NULL, NULL, 0);
      return true;
    }

    return return_value;
  }

  g_autofree gchar* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
  if (input == NULL) {
    girara_isc_abort(session, NULL, NULL, 0);
    return false;
  }

  if (strlen(input) == 0) {
    girara_isc_abort(session, NULL, NULL, 0);
    return false;
  }

  /* append to command history */
  const char* command = gtk_entry_get_text(entry);
  girara_input_history_append(session->command_history, command);

  /* special commands */
  g_autofree char* identifier_s = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, 1);
  if (identifier_s == NULL) {
    return false;
  }

  const char identifier = identifier_s[0];
  girara_debug("Processing special command with identifier '%c'.", identifier);
  for (size_t idx = 0; idx != girara_list_size(session->bindings.special_commands); ++idx) {
    girara_special_command_t* special_command = girara_list_nth(session->bindings.special_commands, idx);
    if (special_command->identifier == identifier) {
      girara_debug("Found special command.");
      if (special_command->always != true) {
        special_command->function(session, input, &(special_command->argument));
      }

      girara_isc_abort(session, NULL, NULL, 0);
      return true;
    }
  }

  return girara_command_run(session, input);
}

gboolean girara_callback_inputbar_key_press_event(GtkEventControllerKey* self, guint hardware_keyval,
                                                  guint hardware_keycode, GdkModifierType state,
                                                  girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  /* a custom handler has been installed (e.g. by girara_dialog) */
  gboolean custom_ret = false;
  if (session->signals.inputbar_custom_key_press_event != NULL) {
    girara_debug("Running custom key press event handler.");
    custom_ret = session->signals.inputbar_custom_key_press_event(self, hardware_keyval, hardware_keycode, state,
                                                                  session->signals.inputbar_custom_data);
    if (custom_ret == true) {
      girara_isc_abort(session, NULL, NULL, 0);

      if (session->global.autohide_inputbar == true) {
        gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar));
      }
      gtk_widget_hide(GTK_WIDGET(session->gtk.inputbar_dialog));
    }
  }

  guint keyval = 0;
  guint clean  = 0;
  if (clean_mask(self, hardware_keycode, state, &clean, &keyval) == false) {
    girara_debug("clean_mask returned false.");
    return false;
  }
  girara_debug("Proccessing key %u with mask %x.", keyval, clean);

  if (custom_ret == false) {
    for (size_t idx = 0; idx != girara_list_size(session->bindings.inputbar_shortcuts); ++idx) {
      girara_inputbar_shortcut_t* inputbar_shortcut = girara_list_nth(session->bindings.inputbar_shortcuts, idx);
      if (inputbar_shortcut->key == keyval && inputbar_shortcut->mask == clean) {
        girara_debug("found shortcut for key %u and mask %x", keyval, clean);
        if (inputbar_shortcut->function != NULL) {
          inputbar_shortcut->function(session, &(inputbar_shortcut->argument), NULL, 0);
        }

        return true;
      }
    }
  }

  if ((session->gtk.results != NULL) && (gtk_widget_get_visible(GTK_WIDGET(session->gtk.results)) == TRUE) &&
      (keyval == GDK_KEY_space)) {
    gtk_widget_hide(GTK_WIDGET(session->gtk.results));
  }

  return custom_ret;
}

gboolean girara_callback_inputbar_changed_event(GtkEditable* entry, girara_session_t* session) {
  g_return_val_if_fail(session != NULL, false);

  /* special commands */
  g_autofree char* identifier_s = gtk_editable_get_chars(entry, 0, 1);
  if (identifier_s == NULL) {
    return false;
  }

  char identifier = identifier_s[0];
  for (size_t idx = 0; idx != girara_list_size(session->bindings.special_commands); ++idx) {
    girara_special_command_t* special_command = girara_list_nth(session->bindings.special_commands, idx);
    if ((special_command->identifier == identifier) && (special_command->always == true)) {
      g_autofree gchar* input = gtk_editable_get_chars(GTK_EDITABLE(entry), 1, -1);
      special_command->function(session, input, &(special_command->argument));
      return true;
    }
  }

  return false;
}
