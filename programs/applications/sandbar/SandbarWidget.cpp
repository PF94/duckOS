/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2023 Byteduck */
/* Copyright © 2023 Chaziz */

#include "SandbarWidget.h"
#include "Sandbar.h"
#include "modules/TimeModule.h"
#include "modules/CPUModule.h"
#include "modules/MemoryModule.h"
#include <libui/widget/Cell.h>
#include <libui/libui.h>

using namespace UI;
using namespace Duck;

SandbarWidget::SandbarWidget():
	m_layout(FlexLayout::make(FlexLayout::HORIZONTAL))
{
	add_child(Cell::make(m_layout));

	m_duck_button = UI::Button::make(UI::icon("/duck"));
	m_duck_button->set_sizing_mode(UI::PREFERRED);
	m_duck_button->set_style(ButtonStyle::INSET);
	m_duck_button->on_pressed = [&] {
		// FIXME: no matching function for call to 'SandbarWidget::open_menu(Duck::Ptr<UI::Menu>, Gfx::Point)'
		// (despite the fact there's a way of opening menus using a Gfx::Point)
		open_menu(create_menu());
	};

	m_layout->add_child(m_duck_button);
	m_layout->add_child(UI::Cell::make());

	auto add_module = [&](Duck::Ptr<Module> module) {
		m_layout->add_child(module);
		m_modules.push_back(module);
	};

	add_module(MemoryModule::make());
	add_module(CPUModule::make());
	add_module(TimeModule::make());

	m_module_timer = UI::set_interval([&]() {
		for(auto& module : m_modules)
			module->update();
	}, 1000);
}

void SandbarWidget::do_repaint(const DrawContext& ctx) {
	ctx.fill(ctx.rect(), UI::Theme::bg());
}

Gfx::Dimensions SandbarWidget::preferred_size() {
	return {100, Sandbar::HEIGHT};
}

Duck::Ptr<Menu> SandbarWidget::create_menu() {
	auto apps = App::get_all_apps();

	std::vector<Duck::Ptr<UI::MenuItem>> items;

	for(auto app : apps) {
		if(app.hidden())
			continue;
		items.push_back(UI::MenuItem::make(app.name(), [app] {
			app.run();
		}));
	}

	return UI::Menu::make(items);
}
