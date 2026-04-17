# nice_view_harjot ZMK UI Shield

A heavily customized, vertical-oriented UI shield for the `nice!view` display on the Zephyr/ZMK firmware stack. 

This shield features a unique "Living Cybert HUD" aesthetic designed to run efficiently on low-power split keyboards. It replaces the stock ZMK widgets with a fully synchronized, cascading data array utilizing custom LVGL canvas manipulations.

## Core Architecture & Dimensions

The `nice!view` hardware is physically mounted vertically (portrait mode), but internally recognized by the Zephyr frame buffer as a **horizontal 160x68 landscape canvas**. 

To paint in a vertical orientation, the upstream ZMK architecture splits the display into **three square canvases (68x68)**. The background display driver rotates these chunks via software buffer manipulation before committing them to the screen. 

## The Three-Canvas Slicing Method

To achieve a seamless visual design from Top to Bottom, elements **cannot** be drawn globally. They must be carefully drawn in isolation inside their respective 68x68 chunk using **strictly local coordinates (0 to 67)**.

### 1. Top Canvas (`draw_top`)
* **Local Boundaries:** `X: 0...67`, `Y: 0...67`
* **Maps to Physical:** The absolute top of the vertical display (Y: 0 to 67).
* **Reserved For:** Battery, BT connection, and upper boundary of the display space.

### 2. Middle Canvas (`draw_middle`)
* **Local Boundaries:** `X: 0...67`, `Y: 0...67`
* **Maps to Physical:** The middle of the display (Y: 68 to 135).
* **Reserved For:** The center-mass content.

### 3. Bottom Canvas (`draw_bottom`)
* **Local Boundaries:** `X: 0...67`, `Y: 0...67`
* **Maps to Physical:** The bottom lip of the display (Y: 136 to 159).
* **CRITICAL WARNING:** Because the physical screen is only 160 pixels tall (68+68=136), only the first **24 pixels** of this canvas are actually projected onto the screen! 
* **Safe Zone:** The visible bounds are highly restricted to `Y: 0...23`. Any graphical element drawn at local `Y > 23` will compile correctly but render totally invisible off-screen!

---

## Technical Notes & Proven Best Practices

### The Global Offset Fallacy
When trying to stitch an animation uniformly across the entire display, it is extremely tempting to write a single monolithic `draw_screen(global_y)` function and "slide" elements across the boundaries using negative `lv_obj_align`. 

**Do not attempt this.** Because of deep-level `lv_canvas_get_draw_buf` rotations happening at the driver level, forcing global offsets across rotated canvas children will invert the coordinate axes unexpectedly, resulting in scrambled, flipped, or out-of-order rendering. 

**Solution:** Embrace the hardware fragmentation. Always assign specific elements firmly to either the Top, Middle, or Bottom chunk and calculate their `Y` mathematics constrained safely within their local `0` to `67` boxes.

### Mathematical Centering & Hardware Clipping
When arranging continuous text vertically (like a vertical-spelling name), font bounding boxes must be rigidly calculated against the `68px` chunk boundaries.
* _Example:_ If you are using a font with a height of `20px` (e.g. `montserrat_20`), placing a letter at `Y = 55` locally means the bounding box extends vertically to `75`.
* Because the canvas hardware enforces a strict `68x68` slicing ceiling, that letter will have its bottom neatly sliced flat off by the system buffer mechanism.
* **The Rule:** Keep all font anchor points mathematically distributed beneath `Y = 46` maximum to guarantee no aggressive hardware clipping.

### High-Fidelity Cyber Animations in 1-Bit Color
Because a split-keyboard acts over BLE and runs on extreme low-power MCUs, refreshing the peripheral display at 60 FPS is impossible and catastrophic for battery life. 
Animation timers typically fire at relatively slow paces (`lv_timer` ~500ms sweeps).
* **Visual Density:** Animations should rely heavily on mathematical shifting (`sin`, or array mapped `modulo` math based on the animation `tick`) to toggle geometric boundaries (`canvas_draw_rect`), frame boxes, or binary data lines. 
* **Smoothness Illusion:** The staggered wave array heavily utilized in this architecture (`int wave[4] = {0, 1, 2, 1};`) cascades an offset across components, generating incredibly complex, sweeping visual machine movements across the screen while remaining radically inexpensive at pure integer scale calculations.
