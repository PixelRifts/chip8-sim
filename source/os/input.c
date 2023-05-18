#include "defines.h"
#include "base/base.h"

#include "input.h"

typedef struct OS_InputState {
    u8 key_frame_states[350];
    u8 key_current_states[350];
    u8 button_frame_states[8];
    u8 button_current_states[8];
    
    f32 mouse_x;
    f32 mouse_y;
    f32 mouse_scrollx;
    f32 mouse_scrolly;
    f32 mouse_absscrollx;
    f32 mouse_absscrolly;
    f32 mouse_recordedx;
    f32 mouse_recordedy;
} OS_InputState;
static OS_InputState _state;

void __OS_InputKeyCallback(u8 key, i32 action) {
    //if (key < 0 || key >= 350) {
	//LogError("Unknown Key: %u", key);
	//return;
	//}
    
    switch (action) {
        case Input_Press: {
			_state.key_frame_states[key] = 0b00000001;
			_state.key_current_states[key] = 1;
		} break;
		
		case Input_Release: {
			_state.key_frame_states[key] = 0b00000010;
			_state.key_current_states[key] = 0;
		} break;
		
		case Input_Repeat: {
			_state.key_frame_states[key] = 0b00000100;
		} break;
	}
}

b8 __OS_InputKeyCallbackCheckRepeat(u8 key, i32 action) {
	//if (key < 0 || key >= 350) {
	//LogError("Unknown Key: %u", key);
	//return false;
	//}
    
	b8 did_repeat = false;
	if (action == Input_Press) {
		if (_state.key_current_states[key] == 1) {
			action = Input_Repeat;
			did_repeat = true;
		}
	}
	
    switch (action) {
        case Input_Press: {
			_state.key_frame_states[key] = 0b00000001;
			_state.key_current_states[key] = 1;
		} break;
		
		case Input_Release: {
			_state.key_frame_states[key] = 0b00000010;
			_state.key_current_states[key] = 0;
		} break;
		
		case Input_Repeat: {
			_state.key_frame_states[key] = 0b00000100;
		} break;
	}
	
	return did_repeat;
}

void __OS_InputButtonCallback(u8 button, int action) {
    if (button < 0 || button >= 8) return;
    switch (action) {
        case Input_Press: {
            _state.button_frame_states[button] = 0b00000001;
            _state.button_current_states[button] = 1;
            _state.mouse_recordedx = _state.mouse_x;
            _state.mouse_recordedy = _state.mouse_y;
        } break;
        case Input_Release: {
            _state.button_frame_states[button] = 0b00000010;
			_state.button_current_states[button] = 0;
            _state.mouse_recordedx = _state.mouse_x;
            _state.mouse_recordedy = _state.mouse_y;
        } break;
    }
}

void __OS_InputCursorPosCallback(f32 xpos, f32 ypos) {
    _state.mouse_x = xpos;
    _state.mouse_y = ypos;
}

void __OS_InputScrollCallback(f32 xscroll, f32 yscroll) {
    _state.mouse_scrollx = xscroll;
    _state.mouse_scrolly = yscroll;
    _state.mouse_absscrollx += xscroll;
    _state.mouse_absscrolly += yscroll;
}

void __OS_InputReset(void) {
    memset(_state.key_frame_states, 0, 350 * sizeof(u8));
    memset(_state.button_frame_states, 0, 8 * sizeof(u8));
    _state.mouse_scrollx = 0;
    _state.mouse_scrolly = 0;
}

b32 OS_InputKey(u8 key) { return _state.key_current_states[key]; }
b32 OS_InputKeyPressed(u8 key) { return (_state.key_frame_states[key] == 0b00000001); }
b32 OS_InputKeyReleased(u8 key) { return (_state.key_frame_states[key] == 0b00000010); }
b32 OS_InputKeyHeld(u8 key) { return (_state.key_frame_states[key] == 0b00000100); }
b32 OS_InputButton(u8 button) { return _state.button_current_states[button]; }
b32 OS_InputButtonPressed(u8 button)
{ return (_state.button_frame_states[button] == 0b00000001); }
b32 OS_InputButtonReleased(u8 button)
{ return (_state.button_frame_states[button] == 0b00000010); }
f32 OS_InputGetMouseX() { return _state.mouse_x; }
f32 OS_InputGetMouseY() { return _state.mouse_y; }
f32 OS_InputGetMouseScrollX() { return _state.mouse_scrollx; }
f32 OS_InputGetMouseScrollY() { return _state.mouse_scrolly; }
f32 OS_InputGetMouseAbsoluteScrollX() { return _state.mouse_absscrollx; }
f32 OS_InputGetMouseAbsoluteScrollY() { return _state.mouse_absscrolly; }
f32 OS_InputGetMouseDX() { return _state.mouse_x - _state.mouse_recordedx; }
f32 OS_InputGetMouseDY() { return _state.mouse_y - _state.mouse_recordedy; }
f32 OS_InputGetMouseRecordedX() { return _state.mouse_recordedx; }
f32 OS_InputGetMouseRecordedY() { return _state.mouse_recordedy; }
