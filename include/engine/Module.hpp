#pragma once
#include "common.hpp"
#include "string.hpp"
#include "engine/Param.hpp"
#include "engine/Port.hpp"
#include "engine/Light.hpp"
#include <vector>
#include <jansson.h>


namespace rack {
namespace engine {


struct Module {
	/** Automatically generated by the engine. */
	int id = -1;
	std::vector<Param> params;
	std::vector<Output> outputs;
	std::vector<Input> inputs;
	std::vector<Light> lights;
	/** For CPU meter. */
	float cpuTime = 0.f;
	bool bypass = false;

	/** Constructs a Module with no params, inputs, outputs, and lights. */
	Module();
	/** Use config() instead. */
	DEPRECATED Module(int numParams, int numInputs, int numOutputs, int numLights = 0) : Module() {
		config(numParams, numInputs, numOutputs, numLights);
	}
	virtual ~Module() {}

	/** Configures the number of Params, Outputs, Inputs, and Lights. */
	void config(int numParams, int numInputs, int numOutputs, int numLights = 0);
	json_t *toJson();
	void fromJson(json_t *rootJ);
	void reset();
	void randomize();

	struct ProcessArgs {
		float sampleRate;
		float sampleTime;
	};

	/** Advances the module by one audio sample.
	Override this method to read Inputs and Params and to write Outputs and Lights.
	*/
	virtual void process(const ProcessArgs &args) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		step();
#pragma GCC diagnostic pop
	}
	/** Override process(const ProcessArgs &args) instead. */
	DEPRECATED virtual void step() {}

	/** Called when the engine sample rate is changed. */
	virtual void onSampleRateChange() {}
	/** Called when user clicks Initialize in the module context menu. */
	virtual void onReset() {}
	/** Called when user clicks Randomize in the module context menu. */
	virtual void onRandomize() {}
	/** Called when the Module is added to the Engine */
	virtual void onAdd() {}
	/** Called when the Module is removed from the Engine */
	virtual void onRemove() {}

	/** Override to store extra internal data in the "data" property of the module's JSON object. */
	virtual json_t *dataToJson() { return NULL; }
	virtual void dataFromJson(json_t *root) {}
};


} // namespace engine
} // namespace rack
