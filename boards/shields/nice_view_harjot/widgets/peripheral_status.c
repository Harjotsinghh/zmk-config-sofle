/*
 * Copyright (c) 2024 Harjot Singh
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include "peripheral_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct peripheral_status_state {
    bool connected;
    uint8_t tick;
};

static void draw_stars(lv_obj_t *canvas, uint8_t tick, int y_offset) {
    lv_draw_rect_dsc_t star_dsc;
    init_rect_dsc(&star_dsc, LVGL_FOREGROUND);
    
    // Absolute layout of stars, perfectly spread across the 160px vertical height
    int star_coords[12][2] = {
        {10, 10}, {45, 25}, {25, 50}, {55, 65}, {15, 80}, {40, 95},
        {10, 115}, {55, 125}, {30, 145}, {15, 155}, {60, 10}, {50, 110}
    };

    for (int i = 0; i < 12; i++) {
        // Simple twinkle effect
        if ((tick + i) % 4 != 0) {
            int size = ((tick + i) % 7 == 0) ? 2 : 1;
            // Draw relative to the current canvas view
            canvas_draw_rect(canvas, star_coords[i][0], star_coords[i][1] + y_offset, size, size, &star_dsc);
        }
    }
}

static void draw_harjot_screen(lv_obj_t *canvas, const struct status_state *state, int y_offset) {
    // Fill current canvas background
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    
    uint8_t tick = ((struct peripheral_status_state *)state)->tick;

    // Draw background stars
    draw_stars(canvas, tick, y_offset);

    // 1. Header (Battery & Connection)
    draw_battery_offset(canvas, state, 0, y_offset); 
    lv_draw_label_dsc_t label_dsc_bt;
    init_label_dsc(&label_dsc_bt, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    canvas_draw_text(canvas, 14, 0 + y_offset, 54, &label_dsc_bt,
                     ((struct peripheral_status_state *)state)->connected ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_CLOSE);

    // 2. HARJOT Name
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);

    int float_y = (tick % 4 == 0 || tick % 4 == 2) ? 1 : ((tick % 4 == 1) ? 2 : 0);

    // Distributed exactly across the middle 140 pixels
    canvas_draw_text(canvas, 0, 20 + y_offset + float_y, 68, &label_dsc, "H");
    canvas_draw_text(canvas, 0, 38 + y_offset + float_y, 68, &label_dsc, "A");
    canvas_draw_text(canvas, 0, 56 + y_offset + float_y, 68, &label_dsc, "R");
    canvas_draw_text(canvas, 0, 74 + y_offset + float_y, 68, &label_dsc, "J");
    canvas_draw_text(canvas, 0, 92 + y_offset + float_y, 68, &label_dsc, "O");
    canvas_draw_text(canvas, 0, 110 + y_offset + float_y, 68, &label_dsc, "T");

    // 3. Bottom Framing
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);
    lv_point_t line_bottom[] = {{10, 132 + y_offset + float_y}, {58, 132 + y_offset + float_y}};
    canvas_draw_line(canvas, line_bottom, 2, &line_dsc);

    // Blinking cursor
    if (tick % 2 == 0) {
        // Draw standard underscore for cursor
        canvas_draw_text(canvas, 0, 136 + y_offset + float_y, 68, &label_dsc, "_");
    }
}

static void update_all_canvases(struct zmk_widget_status *widget) {
    lv_obj_t *top = lv_obj_get_child(widget->obj, 0);
    lv_obj_t *middle = lv_obj_get_child(widget->obj, 1);
    lv_obj_t *bottom = lv_obj_get_child(widget->obj, 2);
    
    // After 90° CW rotation, source_y maps to parent_X:
    //   parent_X = canvas_start_X + source_y
    // So offset = -canvas_start_X to convert absolute → source_y.
    
    // "bottom" canvas (start_X = -44) covers physical TOP (parent_X 0-23)
    draw_harjot_screen(bottom, &widget->state, 44);
    rotate_canvas(bottom, widget->cbuf3);
    
    // "middle" canvas (start_X = 24) covers physical MIDDLE (parent_X 24-91)
    draw_harjot_screen(middle, &widget->state, -24);
    rotate_canvas(middle, widget->cbuf2);
    
    // "top" canvas (start_X = 92) covers physical BOTTOM (parent_X 92-159)
    draw_harjot_screen(top, &widget->state, -92);
    rotate_canvas(top, widget->cbuf);
}

static void set_battery_status(struct zmk_widget_status *widget, struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    update_all_canvases(widget);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    return (struct battery_status_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_harjot_battery_status, struct battery_status_state, battery_status_update_cb, battery_status_get_state)
ZMK_SUBSCRIPTION(widget_harjot_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_harjot_battery_status, zmk_usb_conn_state_changed);
#endif

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected(), .tick = 0};
}

static void set_connection_status(struct zmk_widget_status *widget, struct peripheral_status_state state) {
    ((struct peripheral_status_state *)&widget->state)->connected = state.connected;
    update_all_canvases(widget);
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_harjot_peripheral_status, struct peripheral_status_state, output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_harjot_peripheral_status, zmk_split_peripheral_status_changed);

// Timer for animation
static void anim_timer_cb(lv_timer_t *timer) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        ((struct peripheral_status_state *)&widget->state)->tick++;
        update_all_canvases(widget);
    }
}

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    // Use native upstream alignment coordinates covering the exact 160px width
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0); 
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    lv_obj_t *middle = lv_canvas_create(widget->obj);
    lv_obj_align(middle, LV_ALIGN_TOP_LEFT, 24, 0);
    lv_canvas_set_buffer(middle, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, -44, 0);
    lv_canvas_set_buffer(bottom, widget->cbuf3, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    sys_slist_append(&widgets, &widget->node);
    
    // Initial draw
    ((struct peripheral_status_state *)&widget->state)->tick = 0;
    
    widget_harjot_battery_status_init();
    widget_harjot_peripheral_status_init();
    
    update_all_canvases(widget);

    // Create a timer to run every 500ms for blink and float animation
    lv_timer_create(anim_timer_cb, 500, NULL);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
