#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include <algorithm>
#include <arpa/inet.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "raygui.h"

#include "GLFW/glfw3.h"

#define CPAD_BOUND          0x5d0
#define CPP_BOUND           0x7f

#define TOUCH_SCREEN_WIDTH  320
#define TOUCH_SCREEN_HEIGHT 240

#define DEADZONE_MIN 	    0.05f

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

double lx = 0.0, ly = 0.0;
double rx = 0.0, ry = 0.0;
u32 interfaceButtons = 0;
float yAxisMultiplier = 1.0f;
bool abInverse = false;
bool xyInverse = false;
bool touchScreenPressed = false;
Vector2 touchScreenPosition = {0, 0};
char ipAddress[32] = "192.168.1.185";
int gamepadIndex = 0;

void sendFrame(const char* ipAddress, u32 hidPad, u32 touchScreenState, u32 circlePadState, u32 cppState, u32 interfaceButtons) {
    unsigned char ba[20] = {};
    memcpy(ba, &hidPad, 4);
    memcpy(ba + 4, &touchScreenState, 4);
    memcpy(ba + 8, &circlePadState, 4);
    memcpy(ba + 12, &cppState, 4);
    memcpy(ba + 16, &interfaceButtons, 4);

	const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4950);
    inet_pton(AF_INET, ipAddress, &addr.sin_addr);
    sendto(sock, ba, 20, 0, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
    close(sock);
}

GamepadButton hidButtonsAB[] = {
	GAMEPAD_BUTTON_RIGHT_FACE_DOWN, // A
	GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, // B
};

GamepadButton hidButtonsMiddle[] = {
	GAMEPAD_BUTTON_MIDDLE_LEFT,      // Select
	GAMEPAD_BUTTON_MIDDLE_RIGHT,     // Start

	GAMEPAD_BUTTON_LEFT_FACE_RIGHT,  // D-Pad Right
	GAMEPAD_BUTTON_LEFT_FACE_LEFT,   // D-Pad Left
	GAMEPAD_BUTTON_LEFT_FACE_UP,     // D-Pad Up
	GAMEPAD_BUTTON_LEFT_FACE_DOWN,   // D-Pad Down

	GAMEPAD_BUTTON_RIGHT_TRIGGER_1,  // 3DS R
	GAMEPAD_BUTTON_LEFT_TRIGGER_1,   // 3DS L
};

GamepadButton hidButtonsXY[] = {
	GAMEPAD_BUTTON_RIGHT_FACE_UP,    // Y
	GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  // X
};

int main() {
	InitWindow(TOUCH_SCREEN_WIDTH, TOUCH_SCREEN_HEIGHT, "InputRedirectionClient-Raylib");
    SetTargetFPS(45);

	bool hideUI = false;
	bool ipEditMode = false;

    while (!WindowShouldClose()) {
    	if (IsKeyPressed(KEY_LEFT) && gamepadIndex > 0 && !ipEditMode) gamepadIndex--;
    	if (IsKeyPressed(KEY_RIGHT) && gamepadIndex < GLFW_JOYSTICK_LAST && !ipEditMode) gamepadIndex++;

    	if (IsKeyPressed(KEY_H) && !ipEditMode) hideUI = !hideUI;

    	GLFWgamepadstate state;
        u32 hidPad = 0xfff;
    	u32 circlePadState = 0x7ff7ff;
    	u32 cppState = 0x80800081;

    	if (IsGamepadAvailable(gamepadIndex)) {
			if (!glfwGetGamepadState(gamepadIndex, &state) && glfwGetError(nullptr) == GLFW_NO_ERROR) {
				TraceLog(LOG_WARNING, "Gamepad %d (%s) (%s) does not have a mapping.", gamepadIndex, glfwGetJoystickName(gamepadIndex), glfwGetJoystickGUID(gamepadIndex));
			}

    		lx = GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_LEFT_X);
    		ly = -GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_LEFT_Y) * yAxisMultiplier;
    		if (lx < DEADZONE_MIN && lx > -DEADZONE_MIN) lx = 0.0;
    		if (ly < DEADZONE_MIN && ly > -DEADZONE_MIN) ly = 0.0;

    		rx = GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_RIGHT_X);
    		ry = -GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_RIGHT_Y) * yAxisMultiplier;
    		if (rx < DEADZONE_MIN && rx > -DEADZONE_MIN) rx = 0.0;
    		if (ry < DEADZONE_MIN && ry > -DEADZONE_MIN) ry = 0.0;

        	// A and B buttons
        	for(u32 i = 0; i < 2; i++) {
        		if(IsGamepadButtonDown(gamepadIndex, hidButtonsAB[i])) {
        			hidPad &= ~(1 << i);
        		}
        	}

        	// Middle buttons (Select, Start, D-Pad, R, L)
        	for(u32 i = 2; i < 10; i++)
        	{
        		if (IsGamepadButtonDown(gamepadIndex, hidButtonsMiddle[i - 2]))
        			hidPad &= ~(1 << i);
        	}

    		// Make triggers act as R and L buttons
    		if (IsGamepadButtonDown(gamepadIndex, GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) {
				hidPad &= ~(1 << 8); // R
			}
    		if (IsGamepadButtonDown(gamepadIndex, GAMEPAD_BUTTON_LEFT_TRIGGER_2)) {
    			hidPad &= ~(1 << 9); // L
    		}

        	// X and Y buttons
        	for(u32 i = 10; i < 12; i++)
        	{
        		if (IsGamepadButtonDown(gamepadIndex, hidButtonsXY[1-(i-10)]))
        			hidPad &= ~(1 << i);
        	}

    		// Circle pad, C-Stick
    		if (lx != 0.0 || ly != 0.0) {
    			u32 x = static_cast<u32>(lx * CPAD_BOUND + 0x800);
    			u32 y = static_cast<u32>(ly * CPAD_BOUND + 0x800);
    			x = x >= 0xfff ? (lx < 0.0 ? 0x000 : 0xfff) : x;
    			y = y >= 0xfff ? (ly < 0.0 ? 0x000 : 0xfff) : y;
    			circlePadState = y << 12 | x;
    		}

    		if (rx != 0.0 || ry != 0.0) {
    			u32 x = static_cast<u32>(M_SQRT1_2 * (rx + ry) * CPP_BOUND + 0x80);
    			u32 y = static_cast<u32>(M_SQRT1_2 * (ry - rx) * CPP_BOUND + 0x80);
    			x = x >= 0xff ? (rx < 0.0 ? 0x00 : 0xff) : x;
    			y = y >= 0xff ? (ry < 0.0 ? 0x00 : 0xff) : y;
    			cppState = y << 24 | x << 16 | 0 << 8 | 0x81;
    		}
        }

    	// Touch input (simulate touch screen)
    	u32 touchScreenState = 0x2000000;

    	touchScreenPosition = GetMousePosition();
    	touchScreenPressed = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    	if (touchScreenPressed) {
    		const u32 x = static_cast<u32>(0xfff * std::min(std::max(0.0f, touchScreenPosition.x), static_cast<float>(TOUCH_SCREEN_WIDTH))) / TOUCH_SCREEN_WIDTH;
    		const u32 y = static_cast<u32>(0xfff * std::min(std::max(0.0f, touchScreenPosition.y), static_cast<float>(TOUCH_SCREEN_HEIGHT))) / TOUCH_SCREEN_HEIGHT;
    		touchScreenState = 1 << 24 | y << 12 | x;
    	}

    	sendFrame(ipAddress, hidPad, touchScreenState, circlePadState, cppState, interfaceButtons);

        BeginDrawing();
	        ClearBackground({.r = 32, .g = 29, .b = 29, .a = 255});

    		if (!hideUI) {
    			DrawText(TextFormat("%d FPS", GetFPS()), 5, 5, 10, {.r = 0, .g = 255, .b = 0, .a = 100});

    			if (constexpr Rectangle ipBox = {10, 10, 200, 30}; GuiTextBox(ipBox, ipAddress, 32, ipEditMode)) ipEditMode = !ipEditMode;
    			DrawText("Enter your console's IP address above!", 10, 45, 10, WHITE);
    			DrawText("Press H to toggle UI visibility!", 10, 45 + 10 * 1 + 5, 10, PURPLE);


    			DrawText(TextFormat("Gamepad #%d: %s", gamepadIndex, glfwGetJoystickName(gamepadIndex) != nullptr ? glfwGetJoystickName(gamepadIndex) : "[none]"), 10, 45 + 10 * 2 + 10, 10, GRAY);

    			DrawText(TextFormat("Buttons: %d", hidPad), 10, 45 + 10 * 3 + 10, 10, GRAY);

    			DrawText(TextFormat("Left joystick (circle pad): %d", circlePadState), 10, 45 + 10 * 4 + 15, 10, GRAY);
    			DrawText(TextFormat("LX, LY: %f, %f", lx, ly), 10, 45 + 10 * 5 + 15, 10, GRAY);

    			DrawText("This window acts as your console's touch screen.\nInteract using your cursor!", 5, GetScreenHeight()-25, 10, GRAY);

    		}
    	EndDrawing();
    }

    CloseWindow();
    return 0;
}
