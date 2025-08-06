#include "global.h"
#include "InputHandler_SMXDirect.h"
#include "RageLog.h"

REGISTER_INPUT_HANDLER_CLASS2( SMXDirect, SMXDirect );

constexpr short SMX_VENDOR_ID  = 0x2341;
constexpr short SMX_PRODUCT_ID = 0x8037;

InputHandler_SMXDirect::InputHandler_SMXDirect() {
    m_bShutdown = false;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        m_padDeviceStates[i].is_initialized = false;
        m_padDeviceStates[i].device_instance = nullptr;
    }
}

InputHandler_SMXDirect::~InputHandler_SMXDirect() {
    m_bShutdown = true;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        if (m_padDeviceStates[i].is_initialized) {
            LOG->Info("SMXDirect InputHandler is closing pad %d", i + 1);

            if (m_padDeviceStates[i].device_input_thread.IsCreated()) {
                m_padDeviceStates[i].device_input_thread.Wait();
            }

            // Clean up device instance
            if (m_padDeviceStates[i].device_instance) {
                delete m_padDeviceStates[i].device_instance;
                m_padDeviceStates[i].device_instance = nullptr;
            }
        }
    }

    LowLatencyDanceGameSDK::shutdown();
}

bool InputHandler_SMXDirect::InitializePads() {
    // Check if already initialized
    bool anyInitialized = false;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        if (m_padDeviceStates[i].is_initialized) {
            anyInitialized = true;
            break;
        }
    }
    if (anyInitialized) {
        return true;
    }

    // Initialize the SDK
    if (!LowLatencyDanceGameSDK::initialize()) {
        LOG->Warn("SMXDirect: LowLatencyDanceGameSDK initialization failed");
        return false;
    }

    // Discover devices
    LowLatencyDanceGameSDK** devices = LowLatencyDanceGameSDK::discover_devices();
    if (!devices) {
        LOG->Warn("SMXDirect: Failed to discover devices");
        LowLatencyDanceGameSDK::shutdown();
        return false;
    }

    // Initialize each pad that was discovered
    for (int pad = 0; pad < SMX_PAD_COUNT; pad++) {
        if (devices[pad] && devices[pad]->is_valid()) {
            m_padDeviceStates[pad].device_instance = devices[pad];
            m_padDeviceStates[pad].is_initialized = true;
            m_padDeviceStates[pad].is_p2 = (devices[pad]->get_player_number() == 1);

            // Configure thread
            m_padDeviceStates[pad].device_input_thread.SetName(ssprintf("SMX Device Thread P%d", pad + 1));
            m_padDeviceStates[pad].device_input_thread.Create(pad == 0 ? DeviceThreadP1_Start : DeviceThreadP2_Start, this);
            
            LOG->Info("SMXDirect: Initialized pad %d (P%d)", pad, devices[pad]->get_player_number() + 1);
        } else {
            m_padDeviceStates[pad].device_instance = nullptr;
        }
    }

    // Clean up the devices array (instances are now stored in m_padDeviceStates)
    delete[] devices;

    return true;
}

void InputHandler_SMXDirect::GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut)
{
    if (!InitializePads()) {
        return;
    }

    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        if (m_padDeviceStates[i].is_initialized) {
            vDevicesOut.push_back(InputDeviceInfo(InputDevice(i == 1 ? DEVICE_SMXD2 : DEVICE_SMXD1), ssprintf("SMXD%d", i + 1)));
        }
    }
}

RString InputHandler_SMXDirect::GetDeviceSpecificInputString(const DeviceInput &di)
{
    int pad = di.device == DEVICE_SMXD2 ? 2 : 1;
    int button = di.button - JOY_BUTTON_1;

    static const char* buttonStrings[SMX_PANEL_COUNT] =
    {
        "UpLeft", "Up", "UpRight", "Left", "Center",
        "Right", "DownLeft", "Down", "DownRight"
    };

    const char* buttonString = (button >= 0 && button < SMX_PANEL_COUNT) ? buttonStrings[button] : "unknown";

    return ssprintf("SMX P%d %s", pad, buttonString);
}

int InputHandler_SMXDirect::DeviceThreadP1_Start(void *p) {
    ((InputHandler_SMXDirect *) p) -> DeviceThreadLoop(0);
    return 0;
}

int InputHandler_SMXDirect::DeviceThreadP2_Start(void *p) {
    ((InputHandler_SMXDirect *) p) -> DeviceThreadLoop(1);
    return 0;
}

void InputHandler_SMXDirect::DeviceThreadLoop(int pad) {
    InputDevice device = pad == 1 ? DEVICE_SMXD2 : DEVICE_SMXD1;
    unsigned char buf[65];
    uint16_t last_state = 0;
    int bytes_read = 0;

    LowLatencyDanceGameSDK* deviceInstance = m_padDeviceStates[pad].device_instance;
    if (!deviceInstance) {
        LOG->Warn("SMXDirect: No device instance for pad %d", pad);
        return;
    }

    while (!m_bShutdown) {
        bytes_read = deviceInstance->read_data(buf, sizeof(buf));

        // An input state is at least 3 bytes
        //TODO: constantize? do more validation? gracefully shut down device / loop upon being unplugged?
        if (bytes_read < 3) {
            if (bytes_read < 0) {
                LOG->Warn("SMXDirect InputHandler failed to read data (error %d)", bytes_read);
                break; // Break on error
            }
            continue;
        }

        // Read input state from buffer
        uint16_t new_state = ((buf[2] & 0xFF) << 8) |
                ((buf[1] & 0xFF) << 0);

        // Update inputs
        uint16_t changed_inputs = new_state ^ last_state;

        // If inputs weren't updated, report the end of an empty poll and return
        if (changed_inputs == 0) {
            InputHandler::UpdateTimer();
            continue;
        }

	    // Process inputs
        for (int i = 0; i < SMX_PANEL_COUNT; i++) {
		    bool didButtonStateChange = (changed_inputs & (1 << i)) != 0;
            if (didButtonStateChange) {
                bool pressed = new_state & (1 << i); // Get pressed state
                DeviceInput di(device, enum_add2(JOY_BUTTON_1, i), pressed); // Make input event with pressed state for this button
			    di.ts.Touch(); // Touch the timestamp timer to indicate the input happened *now*
                ButtonPressed(di); // Report the input event
            }
        }

	    // Save the new input state for later, and report the end of a poll.
        last_state = new_state;
        InputHandler::UpdateTimer();
    }
}
