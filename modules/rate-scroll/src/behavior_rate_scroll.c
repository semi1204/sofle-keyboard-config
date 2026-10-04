/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT zmk_behavior_rate_scroll

#include <errno.h>
#include <limits.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

#include <drivers/behavior.h>
#include <dt-bindings/zmk/pointing.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

#include "rate_scroll_policy.h"

#define RATE_SCROLL_PULSE_SLOTS 8

struct rate_scroll_config {
    const char *scroll_behavior_name;
    uint16_t pulse_ms;
};

struct rate_scroll_pulse {
    bool active;
    uint8_t sensor_index;
    int16_t applied_y;
    int64_t release_at_ms;
    struct zmk_behavior_binding_event event;
};

struct rate_scroll_sensor_state {
    bool has_previous;
    int direction;
    int multiplier;
    int64_t last_ms;
    int32_t active_speed;
};

struct rate_scroll_data {
    const struct device *dev;
    struct k_mutex lock;
    struct k_work_delayable release_work;
    struct rate_scroll_pulse pulses[RATE_SCROLL_PULSE_SLOTS];
    struct rate_scroll_sensor_state sensors[ZMK_KEYMAP_SENSORS_LEN];
};

static struct zmk_behavior_binding make_scroll_binding(const struct rate_scroll_config *config,
                                                        int16_t y) {
    const struct zmk_behavior_binding binding = {
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_LOCAL_IDS_IN_BINDINGS)
        .local_id = zmk_behavior_get_local_id(config->scroll_behavior_name),
#endif
        .behavior_dev = config->scroll_behavior_name,
        .param1 = MOVE_Y(y),
        .param2 = 0,
    };
    return binding;
}

static int invoke_scroll(const struct rate_scroll_config *config,
                         struct zmk_behavior_binding_event event, int16_t y, bool pressed) {
    const struct zmk_behavior_binding scroll_binding = make_scroll_binding(config, y);
    return zmk_behavior_invoke_binding(&scroll_binding, event, pressed);
}

static void reschedule_release(struct rate_scroll_data *data, int64_t now_ms) {
    int64_t earliest_ms = INT64_MAX;

    for (size_t i = 0; i < ARRAY_SIZE(data->pulses); i++) {
        if (data->pulses[i].active && data->pulses[i].release_at_ms < earliest_ms) {
            earliest_ms = data->pulses[i].release_at_ms;
        }
    }

    if (earliest_ms == INT64_MAX) {
        return;
    }

    const int64_t delay_ms = MAX(earliest_ms - now_ms, 0);
    k_work_reschedule(&data->release_work, K_MSEC(delay_ms));
}

static void release_due_pulses(struct rate_scroll_data *data, int64_t now_ms) {
    const struct rate_scroll_config *config = data->dev->config;
    int32_t released_by_sensor[ZMK_KEYMAP_SENSORS_LEN] = {0};
    struct zmk_behavior_binding_event event_by_sensor[ZMK_KEYMAP_SENSORS_LEN] = {0};

    for (size_t i = 0; i < ARRAY_SIZE(data->pulses); i++) {
        struct rate_scroll_pulse *pulse = &data->pulses[i];
        if (pulse->active && pulse->release_at_ms <= now_ms) {
            released_by_sensor[pulse->sensor_index] += pulse->applied_y;
            event_by_sensor[pulse->sensor_index] = pulse->event;
            data->sensors[pulse->sensor_index].active_speed -= pulse->applied_y;
            pulse->active = false;
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(released_by_sensor); i++) {
        if (released_by_sensor[i] != 0) {
            invoke_scroll(config, event_by_sensor[i], (int16_t)released_by_sensor[i], false);
        }
    }

    reschedule_release(data, now_ms);
}

static void release_work_handler(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct rate_scroll_data *data = CONTAINER_OF(delayable, struct rate_scroll_data, release_work);

    k_mutex_lock(&data->lock, K_FOREVER);
    release_due_pulses(data, k_uptime_get());
    k_mutex_unlock(&data->lock);
}

static void release_sensor_pulses(struct rate_scroll_data *data, uint8_t sensor_index,
                                  struct zmk_behavior_binding_event event) {
    const struct rate_scroll_config *config = data->dev->config;
    int32_t release_y = 0;

    for (size_t i = 0; i < ARRAY_SIZE(data->pulses); i++) {
        struct rate_scroll_pulse *pulse = &data->pulses[i];
        if (pulse->active && pulse->sensor_index == sensor_index) {
            release_y += pulse->applied_y;
            data->sensors[sensor_index].active_speed -= pulse->applied_y;
            pulse->active = false;
        }
    }

    if (release_y != 0) {
        invoke_scroll(config, event, (int16_t)release_y, false);
    }
}

static struct rate_scroll_pulse *find_free_pulse(struct rate_scroll_data *data) {
    for (size_t i = 0; i < ARRAY_SIZE(data->pulses); i++) {
        if (!data->pulses[i].active) {
            return &data->pulses[i];
        }
    }
    return NULL;
}

static struct rate_scroll_pulse *find_sensor_pulse(struct rate_scroll_data *data,
                                                    uint8_t sensor_index) {
    struct rate_scroll_pulse *oldest = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(data->pulses); i++) {
        struct rate_scroll_pulse *pulse = &data->pulses[i];
        if (pulse->active && pulse->sensor_index == sensor_index &&
            (!oldest || pulse->release_at_ms < oldest->release_at_ms)) {
            oldest = pulse;
        }
    }
    return oldest;
}

static int rate_scroll_init(const struct device *dev) {
    struct rate_scroll_data *data = dev->data;
    data->dev = dev;
    k_mutex_init(&data->lock);
    k_work_init_delayable(&data->release_work, release_work_handler);
    return 0;
}

static int on_binding_pressed(struct zmk_behavior_binding *binding,
                              struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct rate_scroll_config *config = dev->config;
    struct rate_scroll_data *data = dev->data;
    const int sensor_index = ZMK_SENSOR_POSITION_FROM_VIRTUAL_KEY_POSITION(event.position);
    const int16_t base_y = MOVE_Y_DECODE(binding->param1);
    const int direction = (base_y > 0) - (base_y < 0);

    if (sensor_index < 0 || sensor_index >= ZMK_KEYMAP_SENSORS_LEN || direction == 0 ||
        MOVE_X_DECODE(binding->param1) != 0) {
        return -EINVAL;
    }
    const uint8_t sensor_id = (uint8_t)sensor_index;

    const int64_t now_ms = event.timestamp > 0 ? event.timestamp : k_uptime_get();

    k_mutex_lock(&data->lock, K_FOREVER);
    struct rate_scroll_sensor_state *sensor = &data->sensors[sensor_id];
    const bool reversed = sensor->has_previous && sensor->direction != direction;
    const int64_t interval_ms = sensor->has_previous ? now_ms - sensor->last_ms : 0;
    const int multiplier = reversed
                               ? 1
                               : rate_scroll_multiplier(interval_ms, sensor->direction, direction,
                                                        sensor->has_previous, sensor->multiplier);

    if (reversed) {
        release_sensor_pulses(data, sensor_id, event);
    }

    sensor->has_previous = true;
    sensor->direction = direction;
    sensor->multiplier = multiplier;
    sensor->last_ms = now_ms;

    const int32_t requested = (int32_t)base_y * multiplier;
    const int32_t applied = rate_scroll_clamp_increment(sensor->active_speed, requested);
    if (applied == 0) {
        struct rate_scroll_pulse *pulse = find_sensor_pulse(data, sensor_id);
        if (pulse) {
            pulse->release_at_ms = now_ms + config->pulse_ms;
            pulse->event = event;
            reschedule_release(data, now_ms);
        }
        k_mutex_unlock(&data->lock);
        return 0;
    }

    struct rate_scroll_pulse *pulse = find_free_pulse(data);
    if (!pulse) {
        pulse = find_sensor_pulse(data, sensor_id);
        if (!pulse) {
            k_mutex_unlock(&data->lock);
            return 0;
        }
        pulse->applied_y += (int16_t)applied;
    } else {
        pulse->active = true;
        pulse->sensor_index = sensor_id;
        pulse->applied_y = (int16_t)applied;
    }
    pulse->release_at_ms = now_ms + config->pulse_ms;
    pulse->event = event;
    sensor->active_speed += applied;

    const int ret = invoke_scroll(config, event, (int16_t)applied, true);
    reschedule_release(data, now_ms);
    k_mutex_unlock(&data->lock);
    return ret;
}

static int on_binding_released(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata param_values[] = {{
    .display_name = "X Y",
    .type = BEHAVIOR_PARAMETER_VALUE_TYPE_RANGE,
    .range = {.min = 0, .max = UINT32_MAX},
}};

static const struct behavior_parameter_metadata_set param_metadata_set[] = {{
    .param1_values = param_values,
    .param1_values_len = ARRAY_SIZE(param_values),
}};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = ARRAY_SIZE(param_metadata_set),
    .sets = param_metadata_set,
};
#endif

static const struct behavior_driver_api rate_scroll_driver_api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = on_binding_pressed,
    .binding_released = on_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif
};

#define RATE_SCROLL_INST(n)                                                                        \
    static const struct rate_scroll_config rate_scroll_config_##n = {                              \
        .scroll_behavior_name = DEVICE_DT_NAME(DT_INST_PHANDLE(n, scroll_behavior)),                \
        .pulse_ms = DT_INST_PROP(n, pulse_ms),                                                      \
    };                                                                                             \
    static struct rate_scroll_data rate_scroll_data_##n;                                           \
    BEHAVIOR_DT_INST_DEFINE(n, rate_scroll_init, NULL, &rate_scroll_data_##n,                       \
                            &rate_scroll_config_##n, POST_KERNEL,                                  \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &rate_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RATE_SCROLL_INST)
