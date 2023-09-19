/*
	This file is part of Sangria.

	Sangria is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Sangria is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Sangria.  If not, see <https://www.gnu.org/licenses/>.

	Copyright (c) Chaziz 2023. All rights reserved.
*/

#include <sys/wait.h>
#include <libui/libui.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/MenuBar.h>
#include <libui/widget/Label.h>
#include <libui/widget/Cell.h>
#include <libui/widget/ContainerView.h>
#include <libui/widget/Button.h>
#include <libui/widget/Image.h>

using namespace Duck;

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);

	auto container = UI::ContainerView::make();
	auto app_list = UI::BoxLayout::make(UI::BoxLayout::VERTICAL, 0);

	std::vector<Duck::Ptr<UI::MenuItem>> menulist = {
			UI::MenuItem::make("Desktop"),
			UI::MenuItem::make("Help"),
	};
	auto menu = UI::Menu::make(menulist);
	auto menubar = UI::MenuBar::make(menu);

	auto apps = App::get_all_apps();
	for(auto app : apps) {
		auto btn_layout = UI::BoxLayout::make(UI::BoxLayout::HORIZONTAL, 5);

		auto image = UI::Image::make(app.icon());
		image->set_preferred_size({32, 32});

		btn_layout->add_child(image);

		auto title_label = UI::Label::make(app.name());
		title_label->set_font(UI::pond_context->get_font("gohu-14"));
		title_label->set_sizing_mode(UI::PREFERRED);

		btn_layout->add_child(title_label);

		auto btn = UI::Button::make(btn_layout);
		btn->add_child(btn_layout);
		btn->set_style(UI::ButtonStyle::FLAT);
		btn->on_pressed = [app]{
			app.run();
		};

		app_list->add_child(UI::Cell::make(btn));
	}

	container->set_contents(UI::Cell::make(app_list));

	auto window = UI::Window::make();
	window->set_titlebar_accessory(menubar);
	window->set_contents(container);
	window->set_title("Sangria Desktop");
	window->resize({333, 300});
	window->show();

	UI::run();
}