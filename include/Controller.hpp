#pragma once

#include <SDL3/SDL.h>
#include <Windows.h>
#include <Xinput.h>

namespace ControllerHelper
{
	// ==========================================================
	// Types and Enums
	// ==========================================================

	enum class GamepadStyle
	{
		Xbox,
		PlayStation,
		Nintendo,
		Unknown
	};

	struct GamepadCapabilities
	{
		GamepadStyle style = GamepadStyle::Unknown;
		bool hasTouchpad = false;
		const char* name = nullptr;
		Uint16 vendorId = 0;
		Uint16 productId = 0;
	};

	struct TouchpadConfig
	{
		bool isEnabled = true;
		int currentWidth = 0;
		int currentHeight = 0;
	};

	struct TouchpadState
	{
		bool wasDown = false;
		float lastX = 0.0f;
		float lastY = 0.0f;
	};

	// ==========================================================
	// Static State
	// ==========================================================

	inline SDL_Gamepad* s_pGamepad = nullptr;
	inline GamepadCapabilities s_capabilities;

	inline TouchpadConfig s_touchpadConfig;
	inline TouchpadState s_touchpadFinger[2];
	inline bool s_wasTouchpadPressed[2] = { false, false };

	// ==========================================================
	// Controller Database
	// ==========================================================

	inline static bool LoadGamepadMappings()
	{
		const char* paths[] =
		{
			"gamecontrollerdb.txt",
			"../gamecontrollerdb.txt",
		};

		for (const char* path : paths)
		{
			int result = SDL_AddGamepadMappingsFromFile(path);
			if (result >= 0)
			{
				return true;
			}
		}

		return false;
	}

	// ==========================================================
	// Capability Detection
	// ==========================================================

	inline static GamepadStyle DetectGamepadStyle(SDL_Gamepad* pGamepad)
	{
		if (!pGamepad)
			return GamepadStyle::Unknown;

		SDL_GamepadType type = SDL_GetGamepadType(pGamepad);

		switch (type)
		{
			case SDL_GAMEPAD_TYPE_XBOX360:
			case SDL_GAMEPAD_TYPE_XBOXONE:
				return GamepadStyle::Xbox;

			case SDL_GAMEPAD_TYPE_PS3:
			case SDL_GAMEPAD_TYPE_PS4:
			case SDL_GAMEPAD_TYPE_PS5:
				return GamepadStyle::PlayStation;

			case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
			case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
				return GamepadStyle::Nintendo;

			default:
				return GamepadStyle::Xbox;
		}
	}

	inline static void LoadGamepadCapabilities(SDL_Gamepad* pGamepad)
	{
		s_capabilities = GamepadCapabilities();

		if (!pGamepad)
			return;

		s_capabilities.style = DetectGamepadStyle(pGamepad);
		s_capabilities.hasTouchpad = SDL_GetNumGamepadTouchpads(pGamepad) > 0;
		s_capabilities.name = SDL_GetGamepadName(pGamepad);
		s_capabilities.vendorId = SDL_GetGamepadVendor(pGamepad);
		s_capabilities.productId = SDL_GetGamepadProduct(pGamepad);
	}

	// ==========================================================
	// Initialization / Shutdown
	// ==========================================================

	inline bool InitializeSDLGamepad()
	{
		if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
		{
			SDL_Quit();
			return false;
		}

		LoadGamepadMappings();

		return true;
	}

	inline void ShutdownSDLGamepad()
	{
		if (s_pGamepad)
		{
			SDL_CloseGamepad(s_pGamepad);
			s_pGamepad = nullptr;
		}

		s_capabilities = GamepadCapabilities();
		s_touchpadFinger[0] = TouchpadState();
		s_touchpadFinger[1] = TouchpadState();
		s_wasTouchpadPressed[0] = false;
		s_wasTouchpadPressed[1] = false;

		SDL_Quit();
	}

	// ==========================================================
	// Accessors - Controller
	// ==========================================================

	inline SDL_Gamepad* GetGamepad()
	{
		return s_pGamepad;
	}

	inline const GamepadCapabilities& GetCapabilities()
	{
		return s_capabilities;
	}

	inline GamepadStyle GetGamepadStyle()
	{
		return s_capabilities.style;
	}

	inline bool IsConnected()
	{
		return s_pGamepad && SDL_GamepadConnected(s_pGamepad);
	}

	// ==========================================================
	// Accessors - Touchpad
	// ==========================================================

	inline bool HasTouchpad()
	{
		return s_capabilities.hasTouchpad;
	}

	// ==========================================================
	// Configuration - Touchpad
	// ==========================================================

	inline void SetTouchpadEnabled(bool enabled)
	{
		s_touchpadConfig.isEnabled = enabled;
	}

	inline void SetTouchpadDimensions(int width, int height)
	{
		s_touchpadConfig.currentWidth = width;
		s_touchpadConfig.currentHeight = height;
	}

	// ==========================================================
	// Processing - Touchpad (Internal)
	// ==========================================================

	inline static void ProcessTouchpadMouse()
	{
		if (!s_pGamepad || !s_capabilities.hasTouchpad || !s_touchpadConfig.isEnabled)
			return;

		for (int touchpadIndex = 0; touchpadIndex < 2; touchpadIndex++)
		{
			bool fingerDown = false;
			float x = 0.0f, y = 0.0f, pressure = 0.0f;

			if (SDL_GetGamepadTouchpadFinger(s_pGamepad, touchpadIndex, 0, &fingerDown, &x, &y, &pressure))
			{
				auto& finger = s_touchpadFinger[touchpadIndex];

				if (fingerDown)
				{
					if (finger.wasDown)
					{
						float deltaX = (x - finger.lastX) * s_touchpadConfig.currentWidth;
						float deltaY = (y - finger.lastY) * s_touchpadConfig.currentHeight;

						if (deltaX != 0.0f || deltaY != 0.0f)
						{
							INPUT input = {};
							input.type = INPUT_MOUSE;
							input.mi.dwFlags = MOUSEEVENTF_MOVE;
							input.mi.dx = static_cast<LONG>(deltaX);
							input.mi.dy = static_cast<LONG>(deltaY);
							SendInput(1, &input, sizeof(INPUT));
						}
					}

					finger.lastX = x;
					finger.lastY = y;
					finger.wasDown = true;
				}
				else
				{
					finger.wasDown = false;
				}
			}
		}
	}

	inline static void ProcessTouchpadClick()
	{
		if (!s_pGamepad || !s_capabilities.hasTouchpad || !s_touchpadConfig.isEnabled)
			return;

		for (int touchpadIndex = 0; touchpadIndex < 2; touchpadIndex++)
		{
			bool isPressed = SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_TOUCHPAD);

			if (isPressed && !s_wasTouchpadPressed[touchpadIndex])
			{
				INPUT input = {};
				input.type = INPUT_MOUSE;
				input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
				SendInput(1, &input, sizeof(INPUT));
			}
			else if (!isPressed && s_wasTouchpadPressed[touchpadIndex])
			{
				INPUT input = {};
				input.type = INPUT_MOUSE;
				input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
				SendInput(1, &input, sizeof(INPUT));
			}

			s_wasTouchpadPressed[touchpadIndex] = isPressed;
		}
	}

	// ==========================================================
	// Event Processing
	// ==========================================================

	inline static void OnGamepadConnected(SDL_JoystickID deviceId)
	{
		if (s_pGamepad)
			return;

		s_pGamepad = SDL_OpenGamepad(deviceId);
		if (s_pGamepad)
		{
			LoadGamepadCapabilities(s_pGamepad);
		}
	}

	inline static void OnGamepadDisconnected(SDL_JoystickID deviceId)
	{
		if (!s_pGamepad)
			return;

		if (deviceId == SDL_GetGamepadID(s_pGamepad))
		{
			SDL_CloseGamepad(s_pGamepad);
			s_pGamepad = nullptr;
			s_capabilities = GamepadCapabilities();
			s_touchpadFinger[0] = TouchpadState();
			s_touchpadFinger[1] = TouchpadState();
			s_wasTouchpadPressed[0] = false;
			s_wasTouchpadPressed[1] = false;

			int count = 0;
			SDL_JoystickID* gamepads = SDL_GetGamepads(&count);
			if (gamepads)
			{
				for (int i = 0; i < count; i++)
				{
					s_pGamepad = SDL_OpenGamepad(gamepads[i]);
					if (s_pGamepad)
					{
						LoadGamepadCapabilities(s_pGamepad);
						break;
					}
				}
				SDL_free(gamepads);
			}
		}
	}

	inline static void ProcessSDLEvents()
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_EVENT_GAMEPAD_ADDED:
					OnGamepadConnected(event.gdevice.which);
					break;

				case SDL_EVENT_GAMEPAD_REMOVED:
					OnGamepadDisconnected(event.gdevice.which);
					break;
			}
		}
	}

	// ==========================================================
	// Main Poll Function
	// ==========================================================

	inline DWORD PollController(XINPUT_STATE* pState)
	{
		ProcessSDLEvents();

		if (!s_pGamepad || !SDL_GamepadConnected(s_pGamepad))
		{
			return ERROR_DEVICE_NOT_CONNECTED;
		}

		ProcessTouchpadMouse();
		ProcessTouchpadClick();

		// ==========================================================
		// Buttons
		// ==========================================================

		WORD buttons = 0;

		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))
			buttons |= XINPUT_GAMEPAD_DPAD_UP;
		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))
			buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))
			buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
			buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;

		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_START))
			buttons |= XINPUT_GAMEPAD_START;
		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_BACK))
			buttons |= XINPUT_GAMEPAD_BACK;

		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK))
			buttons |= XINPUT_GAMEPAD_LEFT_THUMB;
		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK))
			buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;

		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
			buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
		if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
			buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;

		bool swapButtons = (s_capabilities.style == GamepadStyle::Nintendo);

		if (swapButtons)
		{
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_EAST))
				buttons |= XINPUT_GAMEPAD_A;
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_SOUTH))
				buttons |= XINPUT_GAMEPAD_B;
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_NORTH))
				buttons |= XINPUT_GAMEPAD_X;
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_WEST))
				buttons |= XINPUT_GAMEPAD_Y;
		}
		else
		{
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_SOUTH))
				buttons |= XINPUT_GAMEPAD_A;
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_EAST))
				buttons |= XINPUT_GAMEPAD_B;
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_WEST))
				buttons |= XINPUT_GAMEPAD_X;
			if (SDL_GetGamepadButton(s_pGamepad, SDL_GAMEPAD_BUTTON_NORTH))
				buttons |= XINPUT_GAMEPAD_Y;
		}

		pState->Gamepad.wButtons = buttons;

		// ==========================================================
		// Triggers (SDL: 0-32767 -> XInput: 0-255)
		// ==========================================================

		Sint16 leftTrigger = SDL_GetGamepadAxis(s_pGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
		Sint16 rightTrigger = SDL_GetGamepadAxis(s_pGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

		pState->Gamepad.bLeftTrigger = static_cast<BYTE>((leftTrigger * 255) / 32767);
		pState->Gamepad.bRightTrigger = static_cast<BYTE>((rightTrigger * 255) / 32767);

		// ==========================================================
		// Thumbsticks (SDL Y-axis is inverted compared to XInput)
		// ==========================================================

		auto SafeNegate = [](Sint16 value) -> SHORT
		{
			if (value == -32768)
				return 32767;
			return -value;
		};

		pState->Gamepad.sThumbLX = SDL_GetGamepadAxis(s_pGamepad, SDL_GAMEPAD_AXIS_LEFTX);
		pState->Gamepad.sThumbLY = SafeNegate(SDL_GetGamepadAxis(s_pGamepad, SDL_GAMEPAD_AXIS_LEFTY));
		pState->Gamepad.sThumbRX = SDL_GetGamepadAxis(s_pGamepad, SDL_GAMEPAD_AXIS_RIGHTX);
		pState->Gamepad.sThumbRY = SafeNegate(SDL_GetGamepadAxis(s_pGamepad, SDL_GAMEPAD_AXIS_RIGHTY));

		return ERROR_SUCCESS;
	}

	// ==========================================================
	// Vibration
	// ==========================================================

	inline DWORD SetVibration(XINPUT_VIBRATION* pVibration)
	{
		if (!s_pGamepad || !SDL_GamepadConnected(s_pGamepad))
		{
			return ERROR_DEVICE_NOT_CONNECTED;
		}

		if (!pVibration)
		{
			return ERROR_SUCCESS;
		}

		SDL_RumbleGamepad(s_pGamepad, pVibration->wLeftMotorSpeed, pVibration->wRightMotorSpeed, 100);

		return ERROR_SUCCESS;
	}
};
