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

#pragma once

class HTMLElement
{
public:
	std::string tagName;
	std::vector<struct HTMLElement *> children;
	struct HTMLElement *parentElement;
	std::string textContent;
};

enum State
{
	STATE_INIT,
	STATE_START_TAG,
	STATE_READING_TAG,
	STATE_READING_ATTRIBUTES,
	STATE_END_TAG,
	STATE_BEGIN_CLOSING_TAG
};

HTMLElement *HTMLParser(std::string input);