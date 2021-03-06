#include "plugin.hpp"
#include "audio.hpp"
#include "app.hpp"
#include <mutex>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>


static const int AUDIO_OUTPUTS = 8;
static const int AUDIO_INPUTS = 8;


using namespace rack;


struct AudioInterfacePort : audio::Port {
	std::mutex engineMutex;
	std::condition_variable engineCv;
	std::mutex audioMutex;
	std::condition_variable audioCv;
	// Audio thread produces, engine thread consumes
	dsp::DoubleRingBuffer<dsp::Frame<AUDIO_INPUTS>, (1<<15)> inputBuffer;
	// Audio thread consumes, engine thread produces
	dsp::DoubleRingBuffer<dsp::Frame<AUDIO_OUTPUTS>, (1<<15)> outputBuffer;
	bool active = false;

	~AudioInterfacePort() {
		// Close stream here before destructing AudioInterfacePort, so the mutexes are still valid when waiting to close.
		setDevice(-1, 0);
	}

	void processStream(const float *input, float *output, int frames) override {
		// Reactivate idle stream
		if (!active) {
			active = true;
			inputBuffer.clear();
			outputBuffer.clear();
		}

		if (numInputs > 0) {
			// TODO Do we need to wait on the input to be consumed here? Experimentally, it works fine if we don't.
			for (int i = 0; i < frames; i++) {
				if (inputBuffer.full())
					break;
				dsp::Frame<AUDIO_INPUTS> inputFrame;
				std::memset(&inputFrame, 0, sizeof(inputFrame));
				std::memcpy(&inputFrame, &input[numInputs * i], numInputs * sizeof(float));
				inputBuffer.push(inputFrame);
			}
		}

		if (numOutputs > 0) {
			std::unique_lock<std::mutex> lock(audioMutex);
			auto cond = [&] {
				return (outputBuffer.size() >= (size_t) frames);
			};
			auto timeout = std::chrono::milliseconds(100);
			if (audioCv.wait_for(lock, timeout, cond)) {
				// Consume audio block
				for (int i = 0; i < frames; i++) {
					dsp::Frame<AUDIO_OUTPUTS> f = outputBuffer.shift();
					for (int j = 0; j < numOutputs; j++) {
						output[numOutputs*i + j] = clamp(f.samples[j], -1.f, 1.f);
					}
				}
			}
			else {
				// Timed out, fill output with zeros
				std::memset(output, 0, frames * numOutputs * sizeof(float));
				// DEBUG("Audio Interface Port underflow");
			}
		}

		// Notify engine when finished processing
		engineCv.notify_one();
	}

	void onCloseStream() override {
		inputBuffer.clear();
		outputBuffer.clear();
	}

	void onChannelsChange() override {
	}
};


struct AudioInterface : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(AUDIO_INPUT, AUDIO_INPUTS),
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(AUDIO_OUTPUT, AUDIO_OUTPUTS),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(INPUT_LIGHT, AUDIO_INPUTS / 2),
		ENUMS(OUTPUT_LIGHT, AUDIO_OUTPUTS / 2),
		NUM_LIGHTS
	};

	AudioInterfacePort port;
	int lastSampleRate = 0;
	int lastNumOutputs = -1;
	int lastNumInputs = -1;

	dsp::SampleRateConverter<AUDIO_INPUTS> inputSrc;
	dsp::SampleRateConverter<AUDIO_OUTPUTS> outputSrc;

	// in rack's sample rate
	dsp::DoubleRingBuffer<dsp::Frame<AUDIO_INPUTS>, 16> inputBuffer;
	dsp::DoubleRingBuffer<dsp::Frame<AUDIO_OUTPUTS>, 16> outputBuffer;

	AudioInterface() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		onSampleRateChange();
	}

	void process(const ProcessArgs &args) override {
		// Update SRC states
		inputSrc.setRates(port.sampleRate, args.sampleRate);
		outputSrc.setRates(args.sampleRate, port.sampleRate);

		inputSrc.setChannels(port.numInputs);
		outputSrc.setChannels(port.numOutputs);

		// Inputs: audio engine -> rack engine
		if (port.active && port.numInputs > 0) {
			// Wait until inputs are present
			// Give up after a timeout in case the audio device is being unresponsive.
			std::unique_lock<std::mutex> lock(port.engineMutex);
			auto cond = [&] {
				return (!port.inputBuffer.empty());
			};
			auto timeout = std::chrono::milliseconds(200);
			if (port.engineCv.wait_for(lock, timeout, cond)) {
				// Convert inputs
				int inLen = port.inputBuffer.size();
				int outLen = inputBuffer.capacity();
				inputSrc.process(port.inputBuffer.startData(), &inLen, inputBuffer.endData(), &outLen);
				port.inputBuffer.startIncr(inLen);
				inputBuffer.endIncr(outLen);
			}
			else {
				// Give up on pulling input
				port.active = false;
				// DEBUG("Audio Interface underflow");
			}
		}

		// Take input from buffer
		dsp::Frame<AUDIO_INPUTS> inputFrame;
		if (!inputBuffer.empty()) {
			inputFrame = inputBuffer.shift();
		}
		else {
			std::memset(&inputFrame, 0, sizeof(inputFrame));
		}
		for (int i = 0; i < port.numInputs; i++) {
			outputs[AUDIO_OUTPUT + i].setVoltage(10.f * inputFrame.samples[i]);
		}
		for (int i = port.numInputs; i < AUDIO_INPUTS; i++) {
			outputs[AUDIO_OUTPUT + i].setVoltage(0.f);
		}

		// Outputs: rack engine -> audio engine
		if (port.active && port.numOutputs > 0) {
			// Get and push output SRC frame
			if (!outputBuffer.full()) {
				dsp::Frame<AUDIO_OUTPUTS> outputFrame;
				for (int i = 0; i < AUDIO_OUTPUTS; i++) {
					outputFrame.samples[i] = inputs[AUDIO_INPUT + i].getVoltage() / 10.f;
				}
				outputBuffer.push(outputFrame);
			}

			if (outputBuffer.full()) {
				// Wait until enough outputs are consumed
				// Give up after a timeout in case the audio device is being unresponsive.
				std::unique_lock<std::mutex> lock(port.engineMutex);
				auto cond = [&] {
					return (port.outputBuffer.size() < (size_t) port.blockSize);
				};
				auto timeout = std::chrono::milliseconds(200);
				if (port.engineCv.wait_for(lock, timeout, cond)) {
					// Push converted output
					int inLen = outputBuffer.size();
					int outLen = port.outputBuffer.capacity();
					outputSrc.process(outputBuffer.startData(), &inLen, port.outputBuffer.endData(), &outLen);
					outputBuffer.startIncr(inLen);
					port.outputBuffer.endIncr(outLen);
				}
				else {
					// Give up on pushing output
					port.active = false;
					outputBuffer.clear();
					// DEBUG("Audio Interface underflow");
				}
			}

			// Notify audio thread that an output is potentially ready
			port.audioCv.notify_one();
		}

		// Turn on light if at least one port is enabled in the nearby pair
		for (int i = 0; i < AUDIO_INPUTS / 2; i++)
			lights[INPUT_LIGHT + i].setBrightness(port.active && port.numOutputs >= 2*i+1);
		for (int i = 0; i < AUDIO_OUTPUTS / 2; i++)
			lights[OUTPUT_LIGHT + i].setBrightness(port.active && port.numInputs >= 2*i+1);
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		json_object_set_new(rootJ, "audio", port.toJson());
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *audioJ = json_object_get(rootJ, "audio");
		port.fromJson(audioJ);
	}

	void onReset() override {
		port.setDevice(-1, 0);
	}
};


struct AudioInterfaceWidget : ModuleWidget {
	AudioInterfaceWidget(AudioInterface *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::system("res/Core/AudioInterface.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInput<PJ301MPort>(mm2px(Vec(3.7069211, 55.530807)), module, AudioInterface::AUDIO_INPUT + 0));
		addInput(createInput<PJ301MPort>(mm2px(Vec(15.307249, 55.530807)), module, AudioInterface::AUDIO_INPUT + 1));
		addInput(createInput<PJ301MPort>(mm2px(Vec(26.906193, 55.530807)), module, AudioInterface::AUDIO_INPUT + 2));
		addInput(createInput<PJ301MPort>(mm2px(Vec(38.506519, 55.530807)), module, AudioInterface::AUDIO_INPUT + 3));
		addInput(createInput<PJ301MPort>(mm2px(Vec(3.7069209, 70.144905)), module, AudioInterface::AUDIO_INPUT + 4));
		addInput(createInput<PJ301MPort>(mm2px(Vec(15.307249, 70.144905)), module, AudioInterface::AUDIO_INPUT + 5));
		addInput(createInput<PJ301MPort>(mm2px(Vec(26.906193, 70.144905)), module, AudioInterface::AUDIO_INPUT + 6));
		addInput(createInput<PJ301MPort>(mm2px(Vec(38.506519, 70.144905)), module, AudioInterface::AUDIO_INPUT + 7));

		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.7069209, 92.143906)), module, AudioInterface::AUDIO_OUTPUT + 0));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(15.307249, 92.143906)), module, AudioInterface::AUDIO_OUTPUT + 1));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(26.906193, 92.143906)), module, AudioInterface::AUDIO_OUTPUT + 2));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(38.506519, 92.143906)), module, AudioInterface::AUDIO_OUTPUT + 3));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(3.7069209, 108.1443)), module, AudioInterface::AUDIO_OUTPUT + 4));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(15.307249, 108.1443)), module, AudioInterface::AUDIO_OUTPUT + 5));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(26.906193, 108.1443)), module, AudioInterface::AUDIO_OUTPUT + 6));
		addOutput(createOutput<PJ301MPort>(mm2px(Vec(38.506523, 108.1443)), module, AudioInterface::AUDIO_OUTPUT + 7));

		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(12.524985, 54.577202)), module, AudioInterface::INPUT_LIGHT + 0));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(35.725647, 54.577202)), module, AudioInterface::INPUT_LIGHT + 1));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(12.524985, 69.158226)), module, AudioInterface::INPUT_LIGHT + 2));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(35.725647, 69.158226)), module, AudioInterface::INPUT_LIGHT + 3));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(12.524985, 91.147583)), module, AudioInterface::OUTPUT_LIGHT + 0));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(35.725647, 91.147583)), module, AudioInterface::OUTPUT_LIGHT + 1));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(12.524985, 107.17003)), module, AudioInterface::OUTPUT_LIGHT + 2));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(35.725647, 107.17003)), module, AudioInterface::OUTPUT_LIGHT + 3));

		AudioWidget *audioWidget = createWidget<AudioWidget>(mm2px(Vec(3.2122073, 14.837339)));
		audioWidget->box.size = mm2px(Vec(44, 28));
		audioWidget->setAudioPort(module ? &module->port : NULL);
		addChild(audioWidget);
	}
};


Model *modelAudioInterface = createModel<AudioInterface, AudioInterfaceWidget>("AudioInterface");