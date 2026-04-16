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

static void draw_stars(lv_obj_t *canvas, uint8_t tick, uint8_t seed) {
    lv_draw_rect_dsc_t star_dsc;
    init_rect_dsc(&star_dsc, LVGL_FOREGROUND);
    
    // Simple pseudo-random stars based on seed and tick
    int star_coords[5][2] = {
        {(15 + seed) % 60, (10 + seed * 2) % 60},
        {(45 + seed * 3) % 64, (20 + seed) % 64},
        {(10 + seed * 4) % 55, (45 + seed * 2) % 60},
        {(55 + seed) % 68, (55 + seed * 3) % 68},
        {(30 + seed * 2) % 60, (35 + seed) % 50}
    };

    for (int i = 0; i < 5; i++) {
        // Only draw if the "twinkle" condition is met for this star
        if ((tick + i + seed) % 4 != 0) {
            int size = ((tick + i) % 7 == 0) ? 2 : 1;
            canvas_draw_rect(canvas, star_coords[i][0], star_coords[i][1], size, size, &star_dsc);
        }
    }
}

    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 0);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);
    draw_stars(canvas, ((struct peripheral_status_state *)state)->tick, 10);
    // Draw battery and connection at the top
    draw_battery_offset(canvas, state, 0, 0); 
    canvas_draw_text(canvas, 14, 0, 54, &label_dsc,
                     ((struct peripheral_status_state *)state)->connected ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_CLOSE);
    rotate_canvas(canvas, widget->cbuf);
}

static void draw_body(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 1);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);

    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;

    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);
    draw_stars(canvas, tick, 42);

    int offset_y = (tick % 4 == 0 || tick % 4 == 2) ? 1 : ((tick % 4 == 1) ? 2 : 0);

    // Draw H A R J O T spread out across the 130px space
    canvas_draw_text(canvas, 0, 2 + offset_y, 68, &label_dsc, "H");
    canvas_draw_text(canvas, 0, 18 + offset_y, 68, &label_dsc, "A");
    canvas_draw_text(canvas, 0, 34 + offset_y, 68, &label_dsc, "R");
    canvas_draw_text(canvas, 0, 50 + offset_y, 68, &label_dsc, "J");
    canvas_draw_text(canvas, 0, 66 + offset_y, 68, &label_dsc, "O");
    canvas_draw_text(canvas, 0, 82 + offset_y, 68, &label_dsc, "T");

    // Adjusted line position
    lv_point_t line_bottom[] = {{10, 105 + offset_y}, {58, 105 + offset_y}};
    canvas_draw_line(canvas, line_bottom, 2, &line_dsc);

    // Blinking cursor
    if (tick % 2 == 0) {
        canvas_draw_text(canvas, 0, 109 + offset_y, 68, &label_dsc, "_");
    }

    rotate_canvas(canvas, widget->cbuf2);
}

static void draw_bottom(struct zmk_widget_status *widget, const struct status_state *state) {
    // This function is now superseded by draw_body to prevent overlaps
}

static void set_battery_status(struct zmk_widget_status *widget, struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_top(widget, &widget->state);
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
    draw_top(widget, &widget->state);
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
        draw_body(widget, &widget->state);
    }
}

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    // Two canvases: Header (Battery/Status) and Body (HARJOT animation)
    // Top canvas at the very top (X=0)
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_LEFT, 0, 0); 
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    // Body canvas starts right after Top segment (at 30px)
    lv_obj_t *body = lv_canvas_create(widget->obj);
    lv_obj_align(body, LV_ALIGN_TOP_LEFT, 30, 0);
    lv_canvas_set_buffer(body, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    sys_slist_append(&widgets, &widget->node);
    
    // Initial draw
    ((struct peripheral_status_state *)&widget->state)->tick = 0;
    
    widget_harjot_battery_status_init();
    widget_harjot_peripheral_status_init();
    draw_top(widget, &widget->state);
    draw_body(widget, &widget->state);

    // Create a timer to run every 500ms for blink and float animation
    lv_timer_create(anim_timer_cb, 500, NULL);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
