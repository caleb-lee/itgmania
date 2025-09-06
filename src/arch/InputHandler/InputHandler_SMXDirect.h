#ifndef INPUT_HANDLER_SMXDIRECT_H
#define INPUT_HANDLER_SMXDIRECT_H

#include "InputHandler.h"
#include <lowlatencydancegamesdk.h>

#include <cstdint>
#include <vector>

constexpr int SMX_PAD_COUNT = 2;
constexpr int SMX_PANEL_COUNT = 11;

class InputHandler_SMXDirect: public InputHandler
{
public:
	InputHandler_SMXDirect();
	~InputHandler_SMXDirect();

	void GetDevicesAndDescriptions( std::vector<InputDeviceInfo>& vDevicesOut );
    RString GetDeviceSpecificInputString(const DeviceInput &di);

    static bool IsDeviceHandledByLLDG(uint16_t vendor_id, uint16_t product_id);
    
private:
    static void InputCallback(LowLatencyDanceGameSDK::Player player, uint16_t button_state, void* user_data);
    void ProcessInputEvent(LowLatencyDanceGameSDK::Player player, uint16_t button_state);
    
    bool m_bInitialized;
    uint16_t m_playerInputStates[SMX_PAD_COUNT];
};

#endif