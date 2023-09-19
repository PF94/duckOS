/*
	This file is part of duckOS.

	duckOS is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	duckOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with duckOS.  If not, see <https://www.gnu.org/licenses/>.

	Copyright (c) Chaziz 2023. All rights reserved.
*/

// Based on https://devtails.xyz/@adam/how-to-build-an-html-parser-in-c++

#include <libui/libui.h>
#include <libui/widget/files/FileGridView.h>
#include <libui/widget/layout/FlexLayout.h>
#include <libui/widget/layout/BoxLayout.h>
#include <libui/widget/files/FileNavigationBar.h>

#include "HTMLParser.h"
#include "Node.h"

using namespace Duck;

// https://stackoverflow.com/a/46711735
constexpr unsigned int hash(const char *s, int off = 0) {
	return !s[off] ? 5381 : (hash(s, off+1)*33) ^ s[off];
}

std::string to_lower(std::string s) {
	for(char &c : s)
		c = tolower(c);
	return s;
}

class Browser: public Duck::Object {
public:
	DUCK_OBJECT_DEF(Browser);

	//void fv_did_navigate(Duck::Path path) override {
	//	header->fv_did_navigate(path);
	//}

	std::vector<Node *> nodes;

protected:
	void initialize() override {
		LoadPage("/usr/share/html/test.html");

		auto main_flex = UI::FlexLayout::make(UI::FlexLayout::VERTICAL);

		auto parsed_html = UI::BoxLayout::make(UI::BoxLayout::VERTICAL, 10);

		for (auto node : nodes) {
			Log::dbgf("[TEXT] {} [TAG] {}", node->Text, node->Tag);

			//auto find = allowed_tags.find(node->Tag);
			auto find = std::find(std::begin(allowed_tags), std::end(allowed_tags), to_lower(node->Tag));

			// This is going to be very stupid.
			if (find != allowed_tags.end()) {
				switch (hash(to_lower(node->Tag).c_str())) {
					case hash("h1"):
					case hash("h2"):
					case hash("h3"):
					case hash("h4"):
					case hash("h5"):
					case hash("h6"): {
						auto header_label = UI::Label::make(node->Text, UI::TextAlignment::BEGINNING);
						header_label->set_font(UI::pond_context->get_font("gohu-14"));
						parsed_html->add_child(header_label);
						break;
					}
					case hash("p"): {
						auto paragraph_label = UI::Label::make(node->Text, UI::TextAlignment::BEGINNING);
						parsed_html->add_child(paragraph_label);
						break;
					}
				}
			}
		}

		main_flex->add_child(parsed_html);

		auto window = UI::Window::make();
		//window->set_titlebar_accessory(header);
		window->set_contents(main_flex);
		window->set_resizable(true);
		window->set_title("Browser");
		window->resize({300, 300});
		window->show();
	}

private:
	//Ptr<UI::FileNavigationBar> header = UI::FileNavigationBar::make();

	std::array<std::string, 7> allowed_tags = {"h1", "h2", "h3", "h4", "h5", "h6", "p"};

	void RecurseHtmlElements(HTMLElement *el) {
		nodes.push_back(new Node(el->tagName, el->textContent));

		for (auto child : el->children)
		{
			RecurseHtmlElements(child);
		}
	}

	void LoadPage(const std::string &url) {
		nodes.clear();
		auto file = Duck::File::open(url, "r");

		if(!file.is_error()) {
			auto& html = file.value();
			// FIXME: This shouldn't be hardcoded.
			HTMLElement *el = HTMLParser(
				"<!DOCTYPE html>\n"
				"<html>\n"
				"<body>\n"
				"<h1>Heading (h1)</h1>\n"
				"<H2>Heading (H2)</H2>\n"
				"<h3>Heading (h3)</h3>\n"
				"<H4>Heading (H4)</H4>\n"
				"<h5>Heading (h5)</h5>\n"
				"<H6>Heading (H6)</H6>\n"
				"<p>This file is<br>hardcoded..</p>\n"
				"</body>\n"
				"</html>"
				);
			assert(el->children.size() == 1);
			RecurseHtmlElements(el);
		}
	}
};

int main(int argc, char** argv, char** envp) {
	UI::init(argv, envp);
	auto fm = Browser::make();

	UI::run();
}