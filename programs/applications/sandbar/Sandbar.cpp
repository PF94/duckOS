/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */

#include "Sandbar.h"
#include <libui/libui.h>


Sandbar::Sandbar() {
	// Make sandbar window
	auto window = UI::Window::make();
	window->set_title("Sandbar");
	window->set_decorated(false);
	window->set_resizable(false);
	window->pond_window()->set_draggable(false);

	// Get display dimensions
	auto dims = UI::pond_context->get_display_dimensions();

	// Make sandbar widget
	m_widget = SandbarWidget::make();
	window->set_contents(m_widget);

	// Position window at top of screen and show
	window->set_position({0, 0});
	window->resize({dims.width, window->dimensions().height});
	window->show();
}