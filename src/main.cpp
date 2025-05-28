#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define CPAD_BOUND          0x5d0
#define CPP_BOUND           0x7f
#define TOUCH_SCREEN_WIDTH  320
#define TOUCH_SCREEN_HEIGHT 240

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
	GAMEPAD_BUTTON_LEFT_FACE_UP,    // D-Pad Right
	GAMEPAD_BUTTON_LEFT_FACE_LEFT,  // D-Pad Left
	GAMEPAD_BUTTON_LEFT_FACE_UP,     // D-Pad Up
	GAMEPAD_BUTTON_LEFT_FACE_DOWN,   // D-Pad Down
	GAMEPAD_BUTTON_RIGHT_TRIGGER_1,  // R1
	GAMEPAD_BUTTON_LEFT_TRIGGER_1,   // L1
};

GamepadButton hidButtonsXY[] = {
	GAMEPAD_BUTTON_RIGHT_FACE_LEFT,  // X
	GAMEPAD_BUTTON_RIGHT_FACE_UP,    // Y
};

int main() {
	InitWindow(TOUCH_SCREEN_WIDTH, TOUCH_SCREEN_HEIGHT, "InputRedirectionClient-Raylib");
    SetTargetFPS(45);

	bool hideUI = false;

	bool ipEditMode = false;
    Rectangle ipBox = { 10, 10, 200, 30 };

    while (!WindowShouldClose()) {
    	if (IsKeyPressed(KEY_LEFT) && gamepadIndex > 0 && !ipEditMode) gamepadIndex--;
    	if (IsKeyPressed(KEY_RIGHT) && !ipEditMode) gamepadIndex++;

    	if (IsKeyPressed(KEY_H) && !ipEditMode) hideUI = !hideUI;

        u32 hidPad = 0xfff;
    	u32 circlePadState = 0x7ff7ff;
    	u32 cppState = 0x80800081;

    	if (IsGamepadAvailable(gamepadIndex)) {
	        lx = GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_LEFT_X);
        	ly = -GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_LEFT_Y) * yAxisMultiplier;
        	rx = GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_RIGHT_X);
        	ry = -GetGamepadAxisMovement(gamepadIndex, GAMEPAD_AXIS_RIGHT_Y) * yAxisMultiplier;

        	// A and B buttons
        	for(u32 i = 0; i < 2; i++) {
        		if(IsGamepadButtonDown(gamepadIndex, hidButtonsAB[i])) {
        			hidPad &= ~(1 << i);
        		}
        	}

        	// Middle buttons (Select, Start, D-Pad, R1, L1)
        	for(u32 i = 2; i < 10; i++)
        	{
        		if (IsGamepadButtonDown(gamepadIndex, hidButtonsMiddle[i - 2]))
        			hidPad &= ~(1 << i);
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
    		u32 x = static_cast<u32>(0xfff *
		  std::min(std::max(0.0f, touchScreenPosition.x), static_cast<float>(TOUCH_SCREEN_WIDTH))) / TOUCH_SCREEN_WIDTH;
    		u32 y = static_cast<u32>(0xfff *
		  std::min(std::max(0.0f, touchScreenPosition.y), static_cast<float>(TOUCH_SCREEN_HEIGHT))) / TOUCH_SCREEN_HEIGHT;
    		touchScreenState = 1 << 24 | y << 12 | x;
    	}

    	sendFrame(ipAddress, hidPad, touchScreenState, circlePadState, cppState, interfaceButtons);

        BeginDrawing();
	        ClearBackground({.r = 32, .g = 29, .b = 29, .a = 255});

    		DrawText(TextFormat("%d FPS", GetFPS()), 5, 5, 10, {.r = 0, .g = 255, .b = 0, .a = 100});
    		if (!hideUI) {
    			if (GuiTextBox(ipBox, ipAddress, 32, ipEditMode)) ipEditMode = !ipEditMode;
    			DrawText("Enter your 3DS' IP address above!", 10, 45, 10, WHITE);
    			DrawText("Press H to toggle UI visibility!", 10, 45 + 10 * 1 + 5, 10, PURPLE);


    			DrawText(TextFormat("Gamepad #%d: %s", gamepadIndex, GetGamepadName(gamepadIndex) != nullptr ? GetGamepadName(gamepadIndex) : "[none]"), 10, 45 + 10 * 2 + 10, 10, GRAY);
    			DrawText(TextFormat("Buttons: %d", hidPad), 10, 45 + 10 * 3 + 10, 10, GRAY);

    			DrawText(TextFormat("Left joystick (circle pad): %d", circlePadState), 10, 45 + 10 * 4 + 15, 10, GRAY);
    			DrawText(TextFormat("LX, LY: %f, %f", lx, ly), 10, 45 + 10 * 5 + 15, 10, GRAY);

    		}
    	EndDrawing();
    }

    CloseWindow();
    return 0;
}
