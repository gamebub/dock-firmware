#include "bluetooth/bluetooth.h"

#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "priorities.h"
#include "task.h"
#include "uni.h"

namespace {
constexpr size_t kStackSize = 16 * 1024;
}

static void platform_init(int argc, const char** argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    logi("bluetooth: init()\n");
}

static void platform_on_init_complete(void) {
    logi("bluetooth: on_init_complete()\n");

    // Start scanning and autoconnect to supported controllers.
    uni_bt_start_scanning_and_autoconnect_unsafe();

    uni_bt_del_keys_unsafe();
    // TODO: `uni_bt_list_keys_unsafe`?

    uni_property_dump_all();
}

static uni_error_t platform_on_device_discovered(bd_addr_t addr, const char* name, uint16_t device_class,
                                                 uint8_t rssi) {
    (void)addr;
    (void)name;
    (void)device_class;
    (void)rssi;
    // Can ignore connection by returning an error.
    return UNI_ERROR_SUCCESS;
}

static void platform_on_device_connected(uni_hid_device_t* d) {
    logi("bluetooth: device connected: %p\n", d);
}

static void platform_on_device_disconnected(uni_hid_device_t* d) {
    logi("bluetooth: device disconnected: %p\n", d);
}

static uni_error_t platform_on_device_ready(uni_hid_device_t* d) {
    logi("my_platform: device ready: %p\n", d);

    // Can reject the connection by returning an error.
    return UNI_ERROR_SUCCESS;
}

static void platform_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) {
    static uint8_t enabled = true;

    // Debug log if the state has changed.
    static uni_controller_t prev = {};
    if (memcmp(&prev, ctl, sizeof(*ctl)) != 0) {
        logi("(%p) id=%d ", d, uni_hid_device_get_idx_for_instance(d));
        uni_controller_dump(ctl);
    }
    prev = *ctl;

    if (ctl->klass == UNI_CONTROLLER_CLASS_GAMEPAD) {
        uni_gamepad_t* gp = &ctl->gamepad;

        // Debugging
        // Axis ry: control rumble
        if ((gp->buttons & BUTTON_A) && d->report_parser.play_dual_rumble != NULL) {
            d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 250 /* duration ms */,
                                              128 /* weak magnitude */, 0 /* strong magnitude */);
        }

        if ((gp->buttons & BUTTON_B) && d->report_parser.play_dual_rumble != NULL) {
            d->report_parser.play_dual_rumble(d, 0 /* delayed start ms */, 250 /* duration ms */,
                                              0 /* weak magnitude */, 128 /* strong magnitude */);
        }

        // Toggle Bluetooth connections
        if ((gp->buttons & BUTTON_SHOULDER_L) && enabled) {
            logi("*** Disabling Bluetooth connections\n");
            uni_bt_stop_scanning_safe();
            enabled = false;
        }
        if ((gp->buttons & BUTTON_SHOULDER_R) && !enabled) {
            logi("*** Enabling Bluetooth connections\n");
            uni_bt_start_scanning_and_autoconnect_safe();
            enabled = true;
        }
    }
}

static const uni_property_t* platform_get_property(uni_property_idx_t idx) {
    ARG_UNUSED(idx);
    return NULL;
}

static void platform_on_oob_event(uni_platform_oob_event_t event, void* data) {
    switch (event) {
        case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON: {
            auto device = (uni_hid_device_t*)data;
            logi("platform: system button: %p\n", device);
            break;
        }

        case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED: {
            // When the "bt scanning" is on / off. Could be triggered by different events
            // Useful to notify the user
            logi("my_platform_on_oob_event: Bluetooth enabled: %d\n", (bool)(data));
            break;
        }

        default: {
            logi("my_platform_on_oob_event: unsupported event: 0x%04x\n", event);
        }
    }
}

void bluetooth_task(void*) {
    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43\n");
        return;
    }

    static struct uni_platform platform{
        .name = "Game Bub Dock",
        .init = platform_init,
        .on_init_complete = platform_on_init_complete,
        .on_device_discovered = platform_on_device_discovered,
        .on_device_connected = platform_on_device_connected,
        .on_device_disconnected = platform_on_device_disconnected,
        .on_device_ready = platform_on_device_ready,
        .on_gamepad_data = nullptr,
        .on_controller_data = platform_on_controller_data,
        .get_property = platform_get_property,
        .on_oob_event = platform_on_oob_event,
        .device_dump = nullptr,
        .register_console_cmds = nullptr,
    };

    uni_platform_set_custom(&platform);
    uni_init(0, NULL);
    btstack_run_loop_execute();
}

void InitBluetooth() {
    // Create USB task, pin it to core 0.
    TaskHandle_t task_handle;
    xTaskCreate(bluetooth_task, "bt", kStackSize, NULL, static_cast<UBaseType_t>(TaskPriority::kBluetooth),
                &task_handle);
    vTaskCoreAffinitySet(task_handle, 1 << 0);
}
