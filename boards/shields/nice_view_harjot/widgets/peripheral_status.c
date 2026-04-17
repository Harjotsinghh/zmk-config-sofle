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

// Draw the battery + BT status header on the "top" canvas (physical TOP of display)
static void draw_top(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 0);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);

    // Fill background
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Battery indicator
    draw_battery(canvas, state);

    // BT connection icon
    canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc,
                     ((struct peripheral_status_state *)state)->connected ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_CLOSE);

    // Starfield for this canvas region
    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    lv_draw_rect_dsc_t star_dsc;
    init_rect_dsc(&star_dsc, LVGL_FOREGROUND);
    int star_positions[][2] = {{55, 20}, {60, 45}, {50, 60}};
    for (int i = 0; i < 3; i++) {
        if ((tick + i) % 4 != 0) {
            int sz = ((tick + i) % 7 == 0) ? 2 : 1;
            canvas_draw_rect(canvas, star_positions[i][0], star_positions[i][1], sz, sz, &star_dsc);
        }
    }

    rotate_canvas(canvas, widget->cbuf);
}

// Draw H, A, R, J on the "middle" canvas (physical MIDDLE of display)
static void draw_middle(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 1);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);

    // Fill background
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int float_y = (tick % 4 == 0 || tick % 4 == 2) ? 1 : ((tick % 4 == 1) ? 2 : 0);

    // Letters H, A, R, J spaced within the 68px canvas
    canvas_draw_text(canvas, 0, 2 + float_y, 68, &label_dsc, "H");
    canvas_draw_text(canvas, 0, 18 + float_y, 68, &label_dsc, "A");
    canvas_draw_text(canvas, 0, 34 + float_y, 68, &label_dsc, "R");
    canvas_draw_text(canvas, 0, 50 + float_y, 68, &label_dsc, "J");

    // Starfield
    lv_draw_rect_dsc_t star_dsc;
    init_rect_dsc(&star_dsc, LVGL_FOREGROUND);
    int star_positions[][2] = {{10, 10}, {55, 30}, {15, 55}, {60, 60}};
    for (int i = 0; i < 4; i++) {
        if ((tick + i + 3) % 4 != 0) {
            int sz = ((tick + i) % 7 == 0) ? 2 : 1;
            canvas_draw_rect(canvas, star_positions[i][0], star_positions[i][1], sz, sz, &star_dsc);
        }
    }

    rotate_canvas(canvas, widget->cbuf2);
}

// Draw O, T, line, cursor on the "bottom" canvas (physical BOTTOM of display)
static void draw_bottom(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 2);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    // Fill background
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int float_y = (tick % 4 == 0 || tick % 4 == 2) ? 1 : ((tick % 4 == 1) ? 2 : 0);

    // Letters O, T
    canvas_draw_text(canvas, 0, 2 + float_y, 68, &label_dsc, "O");
    canvas_draw_text(canvas, 0, 18 + float_y, 68, &label_dsc, "T");

    // Decorative line
    lv_point_t line_pts[] = {{10, 40 + float_y}, {58, 40 + float_y}};
    canvas_draw_line(canvas, line_pts, 2, &line_dsc);

    // Blinking cursor
    if (tick % 2 == 0) {
        canvas_draw_text(canvas, 0, 44 + float_y, 68, &label_dsc, "_");
    }

    // Starfield
    lv_draw_rect_dsc_t star_dsc;
    init_rect_dsc(&star_dsc, LVGL_FOREGROUND);
    int star_positions[][2] = {{10, 55}, {45, 60}, {55, 50}};
    for (int i = 0; i < 3; i++) {
        if ((tick + i + 7) % 4 != 0) {
            int sz = ((tick + i) % 7 == 0) ? 2 : 1;
            canvas_draw_rect(canvas, star_positions[i][0], star_positions[i][1], sz, sz, &star_dsc);
        }
    }

    rotate_canvas(canvas, widget->cbuf3);
}

static void update_all_canvases(struct zmk_widget_status *widget) {
    draw_top(widget, &widget->state);
    draw_middle(widget, &widget->state);
    draw_bottom(widget, &widget->state);
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
