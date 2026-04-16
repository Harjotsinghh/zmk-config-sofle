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

static void draw_top(lv_obj_t *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);
    draw_battery(canvas, state);
    canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc,
                     ((struct peripheral_status_state *)state)->connected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE);
    rotate_canvas(canvas);
}

static void draw_middle(lv_obj_t *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);

    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Floating offset bounces between 0 and 2
    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int offset_y = (tick % 4 == 0 || tick % 4 == 2) ? 1 : ((tick % 4 == 1) ? 2 : 0);

    // Draw top framing line
    lv_point_t line_top[] = {{10, 5 + offset_y}, {58, 5 + offset_y}};
    canvas_draw_line(canvas, line_top, 2, &line_dsc);

    // Draw H A R
    canvas_draw_text(canvas, 0, 15 + offset_y, 68, &label_dsc, "H");
    canvas_draw_text(canvas, 0, 31 + offset_y, 68, &label_dsc, "A");
    canvas_draw_text(canvas, 0, 47 + offset_y, 68, &label_dsc, "R");

    rotate_canvas(canvas);
}

static void draw_bottom(lv_obj_t *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);

    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int offset_y = -3 + ((tick % 4 == 0 || tick % 4 == 2) ? 1 : ((tick % 4 == 1) ? 2 : 0));

    // Draw J O T
    canvas_draw_text(canvas, 0, 0 + offset_y, 68, &label_dsc, "J");
    canvas_draw_text(canvas, 0, 16 + offset_y, 68, &label_dsc, "O");
    canvas_draw_text(canvas, 0, 32 + offset_y, 68, &label_dsc, "T");

    // Draw bottom framing line
    lv_point_t line_bottom[] = {{10, 54 + offset_y}, {58, 54 + offset_y}};
    canvas_draw_line(canvas, line_bottom, 2, &line_dsc);

    // Blinking cursor
    if (tick % 2 == 0) {
        canvas_draw_text(canvas, 0, 58 + offset_y, 68, &label_dsc, "_");
    }

    rotate_canvas(canvas);
}

static void set_battery_status(struct zmk_widget_status *widget, struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_top(widget->obj, &widget->state);
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

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state, battery_status_update_cb, battery_status_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected(), .tick = 0};
}

static void set_connection_status(struct zmk_widget_status *widget, struct peripheral_status_state state) {
    ((struct peripheral_status_state *)&widget->state)->connected = state.connected;
    draw_top(widget->obj, &widget->state);
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state, output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

// Timer for animation
static void anim_timer_cb(lv_timer_t *timer) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        ((struct peripheral_status_state *)&widget->state)->tick++;
        draw_middle(widget->obj, &widget->state);
        draw_bottom(widget->obj, &widget->state);
    }
}

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    // Three canvases spanning the 160x68 sideways display
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
    
    widget_battery_status_init();
    widget_peripheral_status_init();
    draw_middle(widget->obj, &widget->state);
    draw_bottom(widget->obj, &widget->state);

    // Create a timer to run every 500ms for blink and float animation
    lv_timer_create(anim_timer_cb, 500, NULL);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
