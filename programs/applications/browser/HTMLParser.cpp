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

#include <string>
#include <vector>
#include <cassert>

#include "HTMLParser.h"

bool isWhitespace(char c)
{
	return c == ' ';
}

HTMLElement *HTMLParser(std::string input)
{
	HTMLElement *root = new HTMLElement();

	State state = STATE_INIT;
	HTMLElement *lastParent = root;
	std::string tagName = "";

	for (auto c : input) {
		if (c == '<') {
			state = STATE_START_TAG;
		} else if (state == STATE_START_TAG) {
			if (c == '/') {
				state = STATE_BEGIN_CLOSING_TAG;
			} else if (!isWhitespace(c)) {
				state = STATE_READING_TAG;
				tagName = c;
			}
		} else if (state == STATE_READING_TAG) {
			if (isWhitespace(c)) {
				state = STATE_READING_ATTRIBUTES;
			} else if(c == '>') {
				state = STATE_END_TAG;

				auto parent = new HTMLElement();
				parent->tagName = tagName;
				parent->parentElement = lastParent;

				lastParent->children.push_back(parent);
				lastParent = parent;
			} else {
				tagName += c;
			}
		} else if(state == STATE_READING_ATTRIBUTES) {
			if (c == '>') {
				state = STATE_END_TAG;

				auto parent = new HTMLElement();
				parent->tagName = tagName;
				parent->parentElement = lastParent;

				lastParent->children.push_back(parent);
				lastParent = parent;
			}
		} else if (state == STATE_END_TAG) {
			lastParent->textContent += c;
		} else if (state == STATE_BEGIN_CLOSING_TAG) {
			if (c == '>') {
				lastParent = lastParent->parentElement;
			}
		}
	}

	return root;
}
