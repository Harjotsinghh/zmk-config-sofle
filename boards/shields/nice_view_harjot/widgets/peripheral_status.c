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

// --- Next-Level Cyber HUD UI Helpers ---

static void draw_hud_framework(lv_obj_t *canvas, int start_y, int height) {
    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
    
    // Outer Thick Rails
    canvas_draw_rect(canvas, 0, start_y, 2, height, &rect_dsc);
    canvas_draw_rect(canvas, 66, start_y, 2, height, &rect_dsc);

    // Inner Dotted Guide Lines
    for (int y = start_y; y < start_y + height; y += 4) {
        canvas_draw_rect(canvas, 14, y, 1, 2, &rect_dsc);
        canvas_draw_rect(canvas, 53, y, 1, 2, &rect_dsc);
    }
}

static void draw_targeting_reticle(lv_obj_t *canvas, int x, int y, int w, int h) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);
    
    lv_point_t tl[] = {{x+4, y}, {x, y}, {x, y+4}};
    canvas_draw_line(canvas, tl, 3, &line_dsc);
    
    lv_point_t tr[] = {{x+w-5, y}, {x+w-1, y}, {x+w-1, y+4}};
    canvas_draw_line(canvas, tr, 3, &line_dsc);
    
    lv_point_t bl[] = {{x+4, y+h-1}, {x, y+h-1}, {x, y+h-5}};
    canvas_draw_line(canvas, bl, 3, &line_dsc);
    
    lv_point_t br[] = {{x+w-5, y+h-1}, {x+w-1, y+h-1}, {x+w-1, y+h-5}};
    canvas_draw_line(canvas, br, 3, &line_dsc);
}

static void draw_letter_data(lv_obj_t *canvas, int tick, int global_index, int base_y, int wave_offset) {
    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
    
    int act_y = base_y + wave_offset;

    // Side pulsing data blocks (Living Machine effect)
    int w_left = 2 + ((tick * (global_index + 2) + 1) % 8);
    canvas_draw_rect(canvas, 4, act_y + 4, w_left, 10, &rect_dsc);
    
    int w_right = 2 + ((tick * (global_index + 3) + 4) % 8);
    canvas_draw_rect(canvas, 64 - w_right, act_y + 4, w_right, 10, &rect_dsc);

    // Active Target Scanner framing logic (Sweeps down)
    if (tick % 6 == global_index) {
        draw_targeting_reticle(canvas, 18, act_y + 1, 32, 22);
        
        // Solid accent marker to show "Lock On"
        canvas_draw_rect(canvas, 16, act_y + 8, 2, 6, &rect_dsc);
        canvas_draw_rect(canvas, 50, act_y + 8, 2, 6, &rect_dsc);
    }
}

// --- Frame Draw Functions ---

static void draw_top(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 0);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc_bt;
    init_label_dsc(&label_dsc_bt, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);

    // Fill background
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Battery indicator
    draw_battery(canvas, state);

    // BT connection icon
    canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc_bt,
                     ((struct peripheral_status_state *)state)->connected ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_CLOSE);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int wave[4] = {0, 1, 2, 1};

    // HUD Framework (starts under battery)
    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
    canvas_draw_rect(canvas, 0, 16, 68, 2, &rect_dsc); // Horizontal separator roof
    draw_hud_framework(canvas, 18, 50); // Vertical frames

    int float_H = wave[tick % 4];
    int float_A = wave[(tick + 1) % 4];

    // Build H
    canvas_draw_text(canvas, 0, 22 + float_H, 68, &label_dsc, "H");
    draw_letter_data(canvas, tick, 0, 22, float_H);

    // Build A
    canvas_draw_text(canvas, 0, 44 + float_A, 68, &label_dsc, "A");
    draw_letter_data(canvas, tick, 1, 44, float_A);

    rotate_canvas(canvas, widget->cbuf);
}

static void draw_middle(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 1);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);

    // Fill background
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int wave[4] = {0, 1, 2, 1};

    // Continuous HUD Frame
    draw_hud_framework(canvas, 0, 68);

    // Build R
    int float_R = wave[(tick + 2) % 4];
    canvas_draw_text(canvas, 0, 0 + float_R, 68, &label_dsc, "R");
    draw_letter_data(canvas, tick, 2, 0, float_R);

    // Build J
    int float_J = wave[(tick + 3) % 4];
    canvas_draw_text(canvas, 0, 22 + float_J, 68, &label_dsc, "J");
    draw_letter_data(canvas, tick, 3, 22, float_J);

    // Build O
    int float_O = wave[(tick + 4) % 4];
    canvas_draw_text(canvas, 0, 44 + float_O, 68, &label_dsc, "O");
    draw_letter_data(canvas, tick, 4, 44, float_O);

    rotate_canvas(canvas, widget->cbuf2);
}

static void draw_bottom(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 2);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);
    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);

    // Fill background
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int wave[4] = {0, 1, 2, 1};

    // Continuous HUD Frame (only for visible chunk 0-24)
    draw_hud_framework(canvas, 0, 24);

    // HUD Floor Line
    canvas_draw_rect(canvas, 0, 22, 68, 2, &rect_dsc); 

    // Build T
    int float_T = wave[(tick + 5) % 4];
    canvas_draw_text(canvas, 0, 0 + float_T, 68, &label_dsc, "T");
    draw_letter_data(canvas, tick, 5, 0, float_T);

    // Blinking Block Cursor
    if (tick % 2 == 0) {
        canvas_draw_rect(canvas, 26, 18 + float_T, 16, 2, &rect_dsc);
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
