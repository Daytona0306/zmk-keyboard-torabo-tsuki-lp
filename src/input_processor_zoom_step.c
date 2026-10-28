// SPDX-License-Identifier: MIT

/* Ctrl / Cmd + ホイールのズーム用に、ホイールを1ノッチ単位へまとめ直す。
 * 背景は dts/bindings/zmk,input-processor-zoom-step.yaml を見ること。
 *
 * チェーン上は scroll-inertia より前に置く。修飾キー押下中のホイールは
 * ここで STOP するので、慣性側はそもそもイベントを見ない
 * (= ズーム中に弾いても慣性が arm されない)。 */

#define DT_DRV_COMPAT zmk_input_processor_zoom_step

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
#include <zmk/pointing/resolution_multipliers.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct zoom_step_config {
    uint8_t mods;
    int32_t counts_per_step;
    int32_t idle_timeout_ms;
};

struct zoom_step_axis {
    int32_t accum;
    int64_t last_time;
};

struct zoom_step_data {
    struct zoom_step_axis wheel;
    struct zoom_step_axis hwheel;
};

/* ホストから見た「1ノッチ」のカウント数。
 * Resolution Multiplier の値は 0..15 で、倍率は値 + 1
 * (ZMK の既定は 15 = 16倍。app/src/pointing/resolution_multipliers.c)。 */
static int32_t notch_counts(uint16_t code) {
#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
    struct zmk_pointing_resolution_multipliers m =
        zmk_pointing_resolution_multipliers_get_current_profile();
    return (code == INPUT_REL_HWHEEL ? m.hor_wheel : m.wheel) + 1;
#else
    ARG_UNUSED(code);
    return 1;
#endif
}

static void send_notches(uint16_t code, int32_t notches, int32_t notch) {
    int32_t value = CLAMP(notches * notch, -INT16_MAX, INT16_MAX);

    zmk_hid_mouse_movement_set(0, 0);
    if (code == INPUT_REL_HWHEEL) {
        zmk_hid_mouse_scroll_set(value, 0);
    } else {
        zmk_hid_mouse_scroll_set(0, value);
    }
    zmk_endpoint_send_mouse_report();
    zmk_hid_mouse_scroll_set(0, 0);
}

static int zoom_step_handle_event(const struct device *dev, struct input_event *event,
                                  uint32_t param1, uint32_t param2,
                                  struct zmk_input_processor_state *state) {
    const struct zoom_step_config *cfg = dev->config;
    struct zoom_step_data *data = dev->data;

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_WHEEL && event->code != INPUT_REL_HWHEEL)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    struct zoom_step_axis *axis =
        event->code == INPUT_REL_HWHEEL ? &data->hwheel : &data->wheel;

    if ((zmk_hid_get_explicit_mods() & cfg->mods) == 0) {
        /* 修飾キーを離したら端数は持ち越さない。 */
        axis->accum = 0;
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int64_t now = k_uptime_get();
    if (now - axis->last_time > cfg->idle_timeout_ms ||
        (axis->accum > 0 && event->value < 0) || (axis->accum < 0 && event->value > 0)) {
        /* 間が空いた / 向きが変わったら貯め直す。 */
        axis->accum = 0;
    }
    axis->last_time = now;
    axis->accum += event->value;

    int32_t notch = notch_counts(event->code);
    int32_t step = cfg->counts_per_step > 0 ? cfg->counts_per_step : notch;
    int32_t steps = axis->accum / step;

    if (steps != 0) {
        axis->accum -= steps * step;
        send_notches(event->code, steps, notch);
    }

    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api zoom_step_driver_api = {
    .handle_event = zoom_step_handle_event,
};

#define ZOOM_STEP_INST(n)                                                                          \
    static struct zoom_step_data zoom_step_data_##n;                                               \
    static const struct zoom_step_config zoom_step_config_##n = {                                  \
        .mods = DT_INST_PROP(n, mods),                                                             \
        .counts_per_step = DT_INST_PROP(n, counts_per_step),                                       \
        .idle_timeout_ms = DT_INST_PROP(n, idle_timeout_ms),                                       \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &zoom_step_data_##n, &zoom_step_config_##n, POST_KERNEL,  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &zoom_step_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ZOOM_STEP_INST)
