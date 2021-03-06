#include "app/Knob.hpp"
#include "app.hpp"
#include "app/Scene.hpp"
#include "history.hpp"


namespace rack {
namespace app {


static const float KNOB_SENSITIVITY = 0.0015f;


void Knob::onHover(const event::Hover &e) {
	math::Vec c = box.size.div(2);
	float dist = e.pos.minus(c).norm();
	if (dist <= c.x) {
		ParamWidget::onHover(e);
	}
}

void Knob::onButton(const event::Button &e) {
	math::Vec c = box.size.div(2);
	float dist = e.pos.minus(c).norm();
	if (dist <= c.x) {
		ParamWidget::onButton(e);
	}
}

void Knob::onDragStart(const event::DragStart &e) {
	if (paramQuantity) {
		oldValue = paramQuantity->getSmoothValue();
		if (snap) {
			snapValue = paramQuantity->getValue();
		}
	}

	APP->window->cursorLock();
	e.consume(this);
}

void Knob::onDragEnd(const event::DragEnd &e) {
	APP->window->cursorUnlock();

	if (paramQuantity) {
		float newValue = paramQuantity->getSmoothValue();
		if (oldValue != newValue) {
			// Push ParamChange history action
			history::ParamChange *h = new history::ParamChange;
			h->name = "move knob";
			h->moduleId = paramQuantity->module->id;
			h->paramId = paramQuantity->paramId;
			h->oldValue = oldValue;
			h->newValue = newValue;
			APP->history->push(h);
		}
	}
}

void Knob::onDragMove(const event::DragMove &e) {
	if (paramQuantity) {
		float range;
		if (paramQuantity->isBounded()) {
			range = paramQuantity->getRange();
		}
		else {
			// Continuous encoders scale as if their limits are +/-1
			range = 2.f;
		}
		float delta = (horizontal ? e.mouseDelta.x : -e.mouseDelta.y);
		delta *= KNOB_SENSITIVITY;
		delta *= speed;
		delta *= range;

		// Drag slower if mod is held
		int mods = APP->window->getMods();
		if ((mods & WINDOW_MOD_MASK) == WINDOW_MOD_CTRL) {
			delta /= 16.f;
		}
		// Drag even slower if mod+shift is held
		if ((mods & WINDOW_MOD_MASK) == (WINDOW_MOD_CTRL | GLFW_MOD_SHIFT)) {
			delta /= 256.f;
		}

		if (snap) {
			snapValue += delta;
			snapValue = math::clamp(snapValue, paramQuantity->getMinValue(), paramQuantity->getMaxValue());
			paramQuantity->setValue(std::round(snapValue));
		}
		else if (smooth) {
			paramQuantity->setSmoothValue(paramQuantity->getSmoothValue() + delta);
		}
		else {
			paramQuantity->setValue(paramQuantity->getValue() + delta);
		}
	}

	ParamWidget::onDragMove(e);
}


} // namespace app
} // namespace rack
