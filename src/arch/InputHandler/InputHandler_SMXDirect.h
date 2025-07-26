#ifndef INPUT_HANDLER_SMXDIRECT_H
#define INPUT_HANDLER_SMXDIRECT_H

#include "InputHandler.h"
#include "RageThreads.h"

#include <libusb.h>
#include <cstdint>
#include <vector>

constexpr int SMX_PAD_COUNT = 2;
constexpr int SMX_PANEL_COUNT = 9;

struct SMXDevice {
    bool is_initialized;
    RageThread device_input_thread;
    libusb_device_handle *device_handle;
    uint8_t interrupt_in_endpoint;
    uint8_t hid_interface;
    bool is_p2;
};

class InputHandler_SMXDirect: public InputHandler
{
public:
	InputHandler_SMXDirect();
	~InputHandler_SMXDirect();

	void GetDevicesAndDescriptions( std::vector<InputDeviceInfo>& vDevicesOut );
    RString GetDeviceSpecificInputString(const DeviceInput &di);
private:
    static int DeviceThreadP1_Start(void *p);
    static int DeviceThreadP2_Start(void *p);
    void DeviceThreadLoop(int pad);
    bool IsDeviceP2(SMXDevice *device);
    struct SMXDevice m_padDeviceStates[SMX_PAD_COUNT];
    bool m_bShutdown;

    libusb_context *m_ctx;
    bool m_isLibusbInitialized;
};

#endif