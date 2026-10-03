/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT zmk_behavior_profile_modifier

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/endpoints.h>

struct profile_modifier_config {
    struct zmk_behavior_binding bindings[2];
    int windows_profile;
};

struct profile_modifier_data {
    uint8_t pressed_index;
    uint32_t pressed_position;
    bool pressed;
};

static int selected_index(const struct profile_modifier_config *config) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    const struct zmk_endpoint_instance endpoint = zmk_endpoints_selected();
    return endpoint.transport == ZMK_TRANSPORT_BLE &&
                   endpoint.ble.profile_index == config->windows_profile
               ? 1
               : 0;
#else
    (void)config;
    return 0;
#endif
}

static int on_binding_pressed(struct zmk_behavior_binding *binding,
                              struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct profile_modifier_config *config = dev->config;
    struct profile_modifier_data *data = dev->data;

    if (data->pressed) {
        return -ENOTSUP;
    }

    data->pressed_index = selected_index(config);
    data->pressed_position = event.position;
    data->pressed = true;
    return zmk_behavior_invoke_binding(&config->bindings[data->pressed_index], event, true);
}

static int on_binding_released(struct zmk_behavior_binding *binding,
                               struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct profile_modifier_config *config = dev->config;
    struct profile_modifier_data *data = dev->data;

    if (!data->pressed || data->pressed_position != event.position) {
        return -ENOTSUP;
    }

    const uint8_t index = data->pressed_index;
    data->pressed = false;
    return zmk_behavior_invoke_binding(&config->bindings[index], event, false);
}

static const struct behavior_driver_api profile_modifier_driver_api = {
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
    .binding_pressed = on_binding_pressed,
    .binding_released = on_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define PROFILE_BINDING(node, idx)                                                                 \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(node, bindings, idx)),               \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(node, bindings, idx, param1), (0),       \
                              (DT_INST_PHA_BY_IDX(node, bindings, idx, param1))),                 \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(node, bindings, idx, param2), (0),       \
                              (DT_INST_PHA_BY_IDX(node, bindings, idx, param2))),                 \
    }

#define PROFILE_MODIFIER_INST(n)                                                                   \
    static const struct profile_modifier_config profile_modifier_config_##n = {                   \
        .bindings = {PROFILE_BINDING(n, 0), PROFILE_BINDING(n, 1)},                                \
        .windows_profile = DT_INST_PROP(n, windows_profile),                                       \
    };                                                                                             \
    static struct profile_modifier_data profile_modifier_data_##n;                                \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &profile_modifier_data_##n,                             \
                            &profile_modifier_config_##n, POST_KERNEL,                            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &profile_modifier_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PROFILE_MODIFIER_INST)
