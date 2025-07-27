#include "global.h"
#include "InputHandler_SMXDirect.h"
#include "RageLog.h"

REGISTER_INPUT_HANDLER_CLASS2( SMXDirect, SMXDirect );

constexpr short SMX_VENDOR_ID  = 0x2341;
constexpr short SMX_PRODUCT_ID = 0x8037;

InputHandler_SMXDirect::InputHandler_SMXDirect() {
    m_instance = nullptr;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        m_padDeviceStates[i].is_initialized = false;
    }
}

InputHandler_SMXDirect::~InputHandler_SMXDirect() {
    m_bShutdown = true;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        if (m_padDeviceStates[i].is_initialized) {
            LOG->Info("SMXDirect InputHandler is closing pad %d", i);

            if (m_padDeviceStates[i].device_input_thread.IsCreated()) {
                m_padDeviceStates[i].device_input_thread.Wait();
            }

            //TODO: per-pad cleanup
        }
    }

    delete m_instance;
}

bool InputHandler_SMXDirect::InitializePads() {
    if (m_instance != nullptr) {
        return true;
    }

    m_instance = new LowLatencyDanceGameSDK(SMX_VENDOR_ID, SMX_PRODUCT_ID);
    if (!m_instance->is_valid()) {
        return false;
    }
    
    // Start poll loop

    // Select pad
    int pad = 0; //TODO: placeholder: need to handle two pads

    // Configure metadata
    m_padDeviceStates[pad].is_initialized = true;
    m_padDeviceStates[pad].is_p2 = false; //TODO: placeholder: need to handle two pads

    // Configure thread
    m_padDeviceStates[pad].device_input_thread.SetName( ssprintf("SMX Device Thread %d", pad) );
    m_padDeviceStates[pad].device_input_thread.Create( pad == 0 ? DeviceThreadP1_Start : DeviceThreadP2_Start, this );

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

    while (!m_bShutdown) {
        bytes_read = m_instance->read_data(buf, sizeof(buf));

        // An input state is at least 3 bytes
        //TODO: constantize? do more validation? gracefully shut down device / loop upon being unplugged?
        if (bytes_read < 3) {
            if (bytes_read < 0) {
                LOG->Warn("SMXDirect InputHandler failed to read data");
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

bool InputHandler_SMXDirect::IsDeviceP2(SMXDevice *device) {
    //TODO: placeholder: need to handle two pads
    /* // Request device info
    const unsigned char data[] = { 5, 0x80, 0 };
    hid_write(handle, data, sizeof(data));

    // Read next HID report to get response
    // Pad data is contained in byte index 3, so 
    unsigned char buf[65];
    int bytes_read = hid_read(handle, buf, sizeof(buf));
    if (bytes_read < 4) {
        if (bytes_read == -1) {
            const wchar_t *error_string = hid_read_error(handle);
            LOG->Warn("SMXDirect InputHandler (Pad Metadata USB Read Error): %ls", error_string);
        }
        return false;
    }

    // Pad is char '1' if P2, or char '0' if P1.
    return (char)buf[3] == '1'; */

    return false;
}