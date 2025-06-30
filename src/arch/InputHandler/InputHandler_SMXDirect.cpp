#include "global.h"
#include "InputHandler_SMXDirect.h"

REGISTER_INPUT_HANDLER_CLASS2( SMXDirect, SMXDirect );

static bool _is_hidapi_initialized = false;

constexpr short SMX_VENDOR_ID  = 0x2341;
constexpr short SMX_PRODUCT_ID = 0x8037;

inline void exit_hidapi() {
    if (_is_hidapi_initialized) {
        hid_exit();
        _is_hidapi_initialized = false;
    }
}

InputHandler_SMXDirect::InputHandler_SMXDirect() {
    if (!_is_hidapi_initialized) {
        hid_init();
        _is_hidapi_initialized = true;
    }

    m_bShutdown = false;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        m_padDeviceStates[i].is_initialized = false;
    }

    // check for smx devices:

    // get all devices
    struct hid_device_info *devices_info = hid_enumerate(SMX_VENDOR_ID, SMX_PRODUCT_ID);
    if (devices_info == NULL) {
        exit_hidapi();
        return;
    }

    // count the number of pads initialized
    int pad_counter = 0;

    // open all of them
    while (pad_counter < SMX_PAD_COUNT && devices_info != NULL) {
        // Open device
        hid_device *handle = hid_open(SMX_VENDOR_ID, SMX_PRODUCT_ID, devices_info->serial_number);
        if (handle == NULL) {
            // Device could not be opened; move on to next
            devices_info = devices_info->next;
            continue;
        }

        // Select pad
        bool is_p2 = IsDeviceP2(handle);
        int pad = is_p2 ? 1 : 0;
        while (m_padDeviceStates[pad].is_initialized) {
            // This loop shouldn't be triggered if pads are configured correctly
            //TODO: Warn if multiple P1 or P2 pads?
            pad = (pad + 1) % SMX_PAD_COUNT;
        }

        // Configure metadata
        m_padDeviceStates[pad].device_handle = handle;
        m_padDeviceStates[pad].is_initialized = true;
        m_padDeviceStates[pad].is_p2 = is_p2;
        m_padDeviceStates[pad].last_state = 0; // Default: No buttons pressed upon initialization

        // Configure thread
        m_padDeviceStates[pad].device_input_thread.SetName( ssprintf("SMX Device Thread %d", pad) );
        m_padDeviceStates[pad].device_input_thread.Create( pad == 0 ? DeviceThreadP1_Start : DeviceThreadP2_Start, this );
        
        pad_counter += 1;
        devices_info = devices_info->next;
    }
}

InputHandler_SMXDirect::~InputHandler_SMXDirect() {
    m_bShutdown = true;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        if (m_padDeviceStates[i].is_initialized) {
            if (m_padDeviceStates[i].device_input_thread.IsCreated()) {
                m_padDeviceStates[i].device_input_thread.Wait();
            }

            hid_close(m_padDeviceStates[i].device_handle);
            m_padDeviceStates[i].is_initialized = false;
        }
    }

    exit_hidapi();
}

void InputHandler_SMXDirect::GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut)
{
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
    hid_device *handle = m_padDeviceStates[pad].device_handle;

    while (!m_bShutdown) {
        int bytes_read = hid_read(handle, buf, 65);

        // An input state is at least 3 bytes
        //TODO: constantize? do more validation?
        if (bytes_read < 3) {
            //TODO: Handle error better than simply requesting more data
            continue;
        }

        // Read input state from buffer
        uint16_t new_state = ((buf[2] & 0xFF) << 8) |
                ((buf[1] & 0xFF) << 0);

        // Update inputs
        uint16_t changed_inputs = new_state ^ m_padDeviceStates[pad].last_state;

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
        m_padDeviceStates[pad].last_state = new_state;
        InputHandler::UpdateTimer();
    }
}

bool InputHandler_SMXDirect::IsDeviceP2(hid_device *handle) {
    // Request device info
    const unsigned char data[] = { 5, 0x80, 0 };
    hid_write(handle, data, sizeof(data));

    // Read next HID report to get response
    // Pad data is contained in byte index 3, so 
    unsigned char buf[65];
    int bytes_read = hid_read(handle, buf, sizeof(buf));
    if (bytes_read < 4) {
        //TODO: Error handle better?
        return false;
    }

    // Pad is char '1' if P2, or char '0' if P1.
    return (char)buf[3] == '1';
}