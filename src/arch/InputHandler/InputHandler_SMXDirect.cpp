#include "global.h"
#include "InputHandler_SMXDirect.h"
#include "RageLog.h"

REGISTER_INPUT_HANDLER_CLASS2( SMXDirect, SMXDirect );

InputHandler_SMXDirect::InputHandler_SMXDirect() {
    m_bInitialized = false;
    for (int i = 0; i < SMX_PAD_COUNT; i++) {
        m_playerInputStates[i] = 0;
    }
    
    // Initialize the SDK with a lambda callback that captures 'this'
    m_bInitialized = LowLatencyDanceGameSDK::getInstance().initialize(
        [this](LowLatencyDanceGameSDK::Player player, uint16_t button_state) {
            ProcessInputEvent(player, button_state);
        }
    );
    
    if (m_bInitialized) {
        LOG->Info("SMXDirect: LowLatencyDanceGameSDK initialized successfully");
    } else {
        LOG->Warn("SMXDirect: LowLatencyDanceGameSDK initialization failed");
    }
}

InputHandler_SMXDirect::~InputHandler_SMXDirect() {
    if (m_bInitialized) {
        LOG->Info("SMXDirect InputHandler shutting down");
        LowLatencyDanceGameSDK::getInstance().shutdown();
        m_bInitialized = false;
    }
}


void InputHandler_SMXDirect::GetDevicesAndDescriptions(std::vector<InputDeviceInfo>& vDevicesOut)
{
    if (!m_bInitialized) {
        return;
    }

    // Check which players are connected and add their devices
    if (LowLatencyDanceGameSDK::getInstance().isPlayerConnected(LowLatencyDanceGameSDK::Player::P1)) {
        vDevicesOut.push_back(InputDeviceInfo(InputDevice(DEVICE_SMXD1), "SMXD1"));
    }
    if (LowLatencyDanceGameSDK::getInstance().isPlayerConnected(LowLatencyDanceGameSDK::Player::P2)) {
        vDevicesOut.push_back(InputDeviceInfo(InputDevice(DEVICE_SMXD2), "SMXD2"));
    }
}

RString InputHandler_SMXDirect::GetDeviceSpecificInputString(const DeviceInput &di)
{
    int pad = di.device == DEVICE_SMXD2 ? 2 : 1;
    int button = di.button - JOY_BUTTON_1;

    static const char* buttonStrings[SMX_PANEL_COUNT] =
    {
        "UpLeft", "Up", "UpRight", "Left", "Center",
        "Right", "DownLeft", "Down", "DownRight", "Start", "Select"
    };

    const char* buttonString = (button >= 0 && button < SMX_PANEL_COUNT) ? buttonStrings[button] : "Unknown";

    return ssprintf("SMX P%d %s", pad, buttonString);
}

void InputHandler_SMXDirect::ProcessInputEvent(LowLatencyDanceGameSDK::Player player, uint16_t button_state) {
    // Convert player enum to pad index
    int padIndex = static_cast<int>(player);
    if (padIndex < 0 || padIndex >= SMX_PAD_COUNT) {
        return;
    }

    // Determine which device this corresponds to
    InputDevice device = (player == LowLatencyDanceGameSDK::Player::P2) ? DEVICE_SMXD2 : DEVICE_SMXD1;

    // Get the previous state and calculate changes
    uint16_t previousState = m_playerInputStates[padIndex];
    uint16_t changedInputs = button_state ^ previousState;

    // Process each button that changed
    for (int i = 0; i < SMX_PANEL_COUNT; i++) {
        bool didButtonStateChange = (changedInputs & (1 << i)) != 0;
        if (didButtonStateChange) {
            bool pressed = button_state & (1 << i);
            DeviceInput di(device, enum_add2(JOY_BUTTON_1, i), pressed);
            di.ts.Touch();
            ButtonPressed(di);
        }
    }

    // Save the new state and update timer
    m_playerInputStates[padIndex] = button_state;
    InputHandler::UpdateTimer();
}

bool InputHandler_SMXDirect::IsDeviceHandledByLLDG(uint16_t vendor_id, uint16_t product_id) {
    return LowLatencyDanceGameSDK::isPadCompatible(vendor_id, product_id);
}
