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

// Draw battery + BT icon + H + A on the "top" canvas (physical TOP, 68px)
static void draw_top(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 0);

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_dsc_bt;
    init_label_dsc(&label_dsc_bt, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_RIGHT);
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_20, LV_TEXT_ALIGN_CENTER);
    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);

    // Fill background
    canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &rect_black_dsc);

    // Battery indicator
    draw_battery(canvas, state);

    // BT connection icon
    canvas_draw_text(canvas, 0, 0, CANVAS_SIZE, &label_dsc_bt,
                     ((struct peripheral_status_state *)state)->connected ? LV_SYMBOL_BLUETOOTH : LV_SYMBOL_CLOSE);

    uint8_t tick = ((struct peripheral_status_state *)state)->tick;
    int wave[4] = {0, 1, 2, 1};

    // HUD Top Separator
    canvas_draw_rect(canvas, 18, 18, 32, 1, &rect_dsc);

    // H (y=22), A (y=44)
    canvas_draw_text(canvas, 0, 22 + wave[tick % 4], 68, &label_dsc, "H");
    canvas_draw_text(canvas, 0, 44 + wave[(tick+1) % 4], 68, &label_dsc, "A");

    // Dynamic Data Bars (Living HUD effect)
    int w_H = 2 + ((tick * 3 + 1) % 6);
    canvas_draw_rect(canvas, 0, 28 + wave[tick % 4], w_H, 3, &rect_dsc);
    canvas_draw_rect(canvas, 68 - w_H, 28 + wave[tick % 4], w_H, 3, &rect_dsc);

    int w_A = 2 + ((tick * 5 + 4) % 6);
    canvas_draw_rect(canvas, 0, 50 + wave[(tick+1) % 4], w_A, 3, &rect_dsc);
    canvas_draw_rect(canvas, 68 - w_A, 50 + wave[(tick+1) % 4], w_A, 3, &rect_dsc);

    // Cyber dust
    int star_positions[][2] = {{15, 20}, {55, 40}};
    for (int i = 0; i < 2; i++) {
        if ((tick + i) % 3 != 0) {
            canvas_draw_rect(canvas, star_positions[i][0], star_positions[i][1], 1, 1, &rect_dsc);
        }
    }

    rotate_canvas(canvas, widget->cbuf);
}

// Draw R, J, O on the "middle" canvas (physical MIDDLE, 68px)
static void draw_middle(struct zmk_widget_status *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 1);

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

    // R (y=0), J (y=22), O (y=44)
    canvas_draw_text(canvas, 0, 0 + wave[(tick+2) % 4], 68, &label_dsc, "R");
    canvas_draw_text(canvas, 0, 22 + wave[(tick+3) % 4], 68, &label_dsc, "J");
    canvas_draw_text(canvas, 0, 44 + wave[(tick+4) % 4], 68, &label_dsc, "O");

    // Dynamic Data Bars (Living HUD effect)
    int w_R = 2 + ((tick * 4 + 7) % 6);
    canvas_draw_rect(canvas, 0, 6 + wave[(tick+2) % 4], w_R, 3, &rect_dsc);
    canvas_draw_rect(canvas, 68 - w_R, 6 + wave[(tick+2) % 4], w_R, 3, &rect_dsc);

    int w_J = 2 + ((tick * 6 + 2) % 6);
    canvas_draw_rect(canvas, 0, 28 + wave[(tick+3) % 4], w_J, 3, &rect_dsc);
    canvas_draw_rect(canvas, 68 - w_J, 28 + wave[(tick+3) % 4], w_J, 3, &rect_dsc);

    int w_O = 2 + ((tick * 2 + 5) % 6);
    canvas_draw_rect(canvas, 0, 50 + wave[(tick+4) % 4], w_O, 3, &rect_dsc);
    canvas_draw_rect(canvas, 68 - w_O, 50 + wave[(tick+4) % 4], w_O, 3, &rect_dsc);

    // Cyber dust
    int star_positions[][2] = {{12, 15}, {50, 35}, {10, 55}};
    for (int i = 0; i < 3; i++) {
        if ((tick + i + 2) % 3 != 0) {
            canvas_draw_rect(canvas, star_positions[i][0], star_positions[i][1], 1, 1, &rect_dsc);
        }
    }

    rotate_canvas(canvas, widget->cbuf2);
}

// Draw T, block cursor on the "bottom" canvas (physical BOTTOM, only 24px visible)
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

    // T (y=0)
    canvas_draw_text(canvas, 0, 0 + wave[(tick+5) % 4], 68, &label_dsc, "T");

    // Dynamic Data Bars
    int w_T = 2 + ((tick * 5 + 3) % 6);
    canvas_draw_rect(canvas, 0, 6 + wave[(tick+5) % 4], w_T, 3, &rect_dsc);
    canvas_draw_rect(canvas, 68 - w_T, 6 + wave[(tick+5) % 4], w_T, 3, &rect_dsc);

    // Blinking Block Cursor
    if (tick % 2 == 0) {
        // Positioned perfectly at physical line 156 (Y=20)
        canvas_draw_rect(canvas, 26, 20 + wave[(tick+5) % 4], 16, 2, &rect_dsc);
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
