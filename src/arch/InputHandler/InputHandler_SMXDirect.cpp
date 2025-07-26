#include "global.h"
#include "InputHandler_SMXDirect.h"
#include "RageLog.h"

REGISTER_INPUT_HANDLER_CLASS2( SMXDirect, SMXDirect );

constexpr short SMX_VENDOR_ID  = 0x2341;
constexpr short SMX_PRODUCT_ID = 0x8037;

InputHandler_SMXDirect::InputHandler_SMXDirect() {
    if (!m_isLibusbInitialized) {
        if (libusb_init(&m_ctx) < 0) {
            LOG->Warn("SMXDirect InputHandler failed to initialize libusb.");
            return;
        }
        else {
            m_isLibusbInitialized = true;
        }
    }

    m_bShutdown = false;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        m_padDeviceStates[i].is_initialized = false;
    }

    // check for smx devices:

    // Open device
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(m_ctx, SMX_VENDOR_ID, SMX_PRODUCT_ID);
    if (!handle)
    {
        LOG->Info("SMXDirect InputHandler: Device not found.");
        libusb_exit(m_ctx);
        return;
    }

    // Find the correct HID interface
    struct libusb_config_descriptor *config;
    libusb_get_active_config_descriptor(libusb_get_device(handle), &config);

    int hid_interface = -1;
    int hid_interface_index = -1;
    for (int i = 0; i < config->bNumInterfaces; i++) {
        const struct libusb_interface_descriptor *intf = &config->interface[i].altsetting[0];
        if (intf->bInterfaceClass == 3) { // HID class = 3
            hid_interface = intf->bInterfaceNumber;  // Use the actual interface number
            hid_interface_index = i;  // Keep track of the array index for endpoint discovery
            LOG->Info("SMXDirect InputHandler: Found HID interface: %d (index=%d, class=%d, subclass=%d, protocol=%d)", 
                hid_interface, 
                i, 
                (int)intf->bInterfaceClass, 
                (int)intf->bInterfaceSubClass, 
                (int)intf->bInterfaceProtocol);
            break;
        }
    }

    if (hid_interface == -1) {
        LOG->Warn("SMXDirect InputHandler: No HID interface found!");
        libusb_free_config_descriptor(config);
        libusb_close(handle);
        libusb_exit(m_ctx);
        return;
    }

    // Detach kernel driver if active
    if (libusb_kernel_driver_active(handle, hid_interface) == 1)
    {
        LOG->Info("SMXDirect InputHandler: Kernel driver is active on interface %d, detaching...", hid_interface);
        int ret = libusb_detach_kernel_driver(handle, hid_interface);
        if (ret != 0)
        {
            LOG->Warn("SMXDirect InputHandler: Failed to detach kernel driver: %s", libusb_error_name(ret));
            return;
        }
    }

    // Claim the HID interface
    if (libusb_claim_interface(handle, hid_interface) < 0)
    {
        LOG->Warn("SMXDirect InputHandler: Failed to claim HID interface %d", hid_interface);
        libusb_free_config_descriptor(config);
        libusb_close(handle);
        libusb_exit(m_ctx);
        return;
    }

    LOG->Info("SMXDirect InputHandler: Connected! Discovering endpoints...");

    // Discover endpoints on the HID interface
    uint8_t interrupt_in_endpoint = 0;
    uint8_t interrupt_out_endpoint = 0;

    for (int i = 0; i < config->interface[hid_interface_index].altsetting[0].bNumEndpoints; i++)
    {
        const struct libusb_endpoint_descriptor *ep = &config->interface[hid_interface_index].altsetting[0].endpoint[i];
        
        bool is_interrupt = (ep->bmAttributes & 0x03) == 0x03;
        bool is_input = (ep->bEndpointAddress & 0x80) != 0;
        bool is_output = (ep->bEndpointAddress & 0x80) == 0;
        
        LOG->Info("SMXDirect InputHandler: Endpoint 0x%02x: %s, %s, max packet size: %d",
               ep->bEndpointAddress,
               is_input ? "IN" : "OUT",
               is_interrupt ? "Interrupt" : "Other",
               ep->wMaxPacketSize);
        
        if (is_interrupt && is_input && interrupt_in_endpoint == 0) {
            interrupt_in_endpoint = ep->bEndpointAddress;
            LOG->Info("Using IN endpoint: 0x%02x", (int)interrupt_in_endpoint);
        }

        if (is_interrupt && is_output && interrupt_out_endpoint == 0) {
            interrupt_out_endpoint = ep->bEndpointAddress;
            LOG->Info("Found OUT endpoint: 0x%02x", (int)interrupt_out_endpoint);
        }
    }

    libusb_free_config_descriptor(config);

    if (interrupt_in_endpoint == 0) {
        LOG->Warn("SMXDirect InputHandler: No interrupt IN endpoint found!");
        libusb_release_interface(handle, hid_interface);
        libusb_close(handle);
        libusb_exit(m_ctx);
        return;
    }

    // Start poll loop

    // Select pad
    int pad = 0; //TODO: placeholder: need to handle two pads

    // Configure metadata
    m_padDeviceStates[pad].device_handle = handle;
    m_padDeviceStates[pad].is_initialized = true;
    m_padDeviceStates[pad].is_p2 = false; //TODO: placeholder: need to handle two pads
    m_padDeviceStates[pad].hid_interface = hid_interface;
    m_padDeviceStates[pad].interrupt_in_endpoint = interrupt_in_endpoint;

    // Configure thread
    m_padDeviceStates[pad].device_input_thread.SetName( ssprintf("SMX Device Thread %d", pad) );
    m_padDeviceStates[pad].device_input_thread.Create( pad == 0 ? DeviceThreadP1_Start : DeviceThreadP2_Start, this );
}

InputHandler_SMXDirect::~InputHandler_SMXDirect() {
    m_bShutdown = true;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        if (m_padDeviceStates[i].is_initialized) {
            LOG->Info("SMXDirect InputHandler is closing pad %d", i);

            if (m_padDeviceStates[i].device_input_thread.IsCreated()) {
                m_padDeviceStates[i].device_input_thread.Wait();
            }

            libusb_device_handle *handle = m_padDeviceStates[i].device_handle;
            uint8_t hid_interface = m_padDeviceStates[i].hid_interface;
            libusb_release_interface(handle, hid_interface);
            libusb_close(handle);
            m_padDeviceStates[i].is_initialized = false;
        }
    }

    libusb_exit(m_ctx);
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
    libusb_device_handle *handle = m_padDeviceStates[pad].device_handle;
    uint8_t interrupt_in_endpoint = m_padDeviceStates[pad].interrupt_in_endpoint;
    uint16_t last_state = 0;
    int bytes_read = 0;
    int result = 0;

    while (!m_bShutdown) {
        bytes_read = 0;
        result = libusb_interrupt_transfer(
            handle,
            interrupt_in_endpoint,
            buf,
            sizeof(buf),
            &bytes_read,
            0); // never time out

        // An input state is at least 3 bytes
        //TODO: constantize? do more validation? gracefully shut down device / loop upon being unplugged?
        if (bytes_read < 3) {
            if (result < 0) {
                const char *error_string = libusb_error_name(result);
                LOG->Warn("SMXDirect InputHandler (libusb error): %s", error_string);
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