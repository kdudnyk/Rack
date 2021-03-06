#include "keyboard.hpp"
#include "midi.hpp"
#include "window.hpp"
#include <map>


namespace rack {
namespace keyboard {


struct Driver;


static const int DRIVER = -11;
static Driver *driver = NULL;


struct InputDevice : midi::InputDevice {
	int octave = 5;
	std::map<int, int> pressedNotes;

	void onKeyPress(int key) {
		int note = -1;
		switch (key) {
			case GLFW_KEY_Z: note = 0; break;
			case GLFW_KEY_S: note = 1; break;
			case GLFW_KEY_X: note = 2; break;
			case GLFW_KEY_D: note = 3; break;
			case GLFW_KEY_C: note = 4; break;
			case GLFW_KEY_V: note = 5; break;
			case GLFW_KEY_G: note = 6; break;
			case GLFW_KEY_B: note = 7; break;
			case GLFW_KEY_H: note = 8; break;
			case GLFW_KEY_N: note = 9; break;
			case GLFW_KEY_J: note = 10; break;
			case GLFW_KEY_M: note = 11; break;
			case GLFW_KEY_COMMA: note = 12; break;
			case GLFW_KEY_L: note = 13; break;
			case GLFW_KEY_PERIOD: note = 14; break;
			case GLFW_KEY_SEMICOLON: note = 15; break;
			case GLFW_KEY_SLASH: note = 16; break;

			case GLFW_KEY_Q: note = 12; break;
			case GLFW_KEY_2: note = 13; break;
			case GLFW_KEY_W: note = 14; break;
			case GLFW_KEY_3: note = 15; break;
			case GLFW_KEY_E: note = 16; break;
			case GLFW_KEY_R: note = 17; break;
			case GLFW_KEY_5: note = 18; break;
			case GLFW_KEY_T: note = 19; break;
			case GLFW_KEY_6: note = 20; break;
			case GLFW_KEY_Y: note = 21; break;
			case GLFW_KEY_7: note = 22; break;
			case GLFW_KEY_U: note = 23; break;
			case GLFW_KEY_I: note = 24; break;
			case GLFW_KEY_9: note = 25; break;
			case GLFW_KEY_O: note = 26; break;
			case GLFW_KEY_0: note = 27; break;
			case GLFW_KEY_P: note = 28; break;
			case GLFW_KEY_LEFT_BRACKET: note = 29; break;
			case GLFW_KEY_EQUAL: note = 30; break;
			case GLFW_KEY_RIGHT_BRACKET: note = 31; break;

			case GLFW_KEY_GRAVE_ACCENT: {
				octave--;
			} break;
			case GLFW_KEY_1: {
				octave++;
			} break;
			default: break;
		}

		octave = math::clamp(octave, 0, 9);
		if (note < 0)
			return;

		note += 12 * octave;
		if (note > 127)
			return;

		midi::Message msg;
		msg.cmd = 0x9 << 4;
		msg.data1 = note;
		msg.data2 = 127;
		onMessage(msg);

		pressedNotes[key] = note;
	}

	void onKeyRelease(int key) {
		auto it = pressedNotes.find(key);
		if (it != pressedNotes.end()) {
			int note = it->second;
			midi::Message msg;
			msg.cmd = 0x8 << 4;
			msg.data1 = note;
			msg.data2 = 127;
			onMessage(msg);

			pressedNotes.erase(it);
		}
	}
};


struct Driver : midi::Driver {
	InputDevice device;
	std::string getName() override {return "Computer keyboard";}

	std::vector<int> getInputDeviceIds() override {
		return {0};
	}

	std::string getInputDeviceName(int deviceId) override {
		if (deviceId == 0)
			return "QWERTY keyboard (US)";
		return "";
	}

	midi::InputDevice *subscribeInput(int deviceId, midi::Input *input) override {
		if (deviceId != 0)
			return NULL;

		device.subscribe(input);
		return &device;
	}

	void unsubscribeInput(int deviceId, midi::Input *input) override {
		if (deviceId != 0)
			return;

		device.unsubscribe(input);
	}
};


void init() {
	driver = new Driver;
	midi::addDriver(DRIVER, driver);
}

void press(int key) {
	if (!driver)
		return;
	driver->device.onKeyPress(key);
}

void release(int key) {
	if (!driver)
		return;
	driver->device.onKeyRelease(key);
}


} // namespace keyboard
} // namespace rack
