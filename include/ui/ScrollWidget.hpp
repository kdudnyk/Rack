#pragma once
#include "ui/common.hpp"
#include "widget/Widget.hpp"
#include "ui/ScrollBar.hpp"


namespace rack {
namespace ui {


/** Handles a container with ScrollBar */
struct ScrollWidget : widget::Widget {
	widget::Widget *container;
	ScrollBar *horizontalScrollBar;
	ScrollBar *verticalScrollBar;
	math::Vec offset;

	ScrollWidget();
	void scrollTo(math::Rect r);
	void draw(const DrawArgs &args) override;
	void step() override;
	void onHover(const event::Hover &e) override;
	void onHoverScroll(const event::HoverScroll &e) override;
};


} // namespace ui
} // namespace rack
