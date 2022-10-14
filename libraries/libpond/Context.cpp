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

	Copyright (c) Byteduck 2016-2021. All rights reserved.
*/

#include "Context.h"
#include "Window.h"
#include <cstdio>
#include <sys/mem.h>
#include <libgraphics/Font.h>
#include <utility>
#include <libriver/river.h>

using namespace Pond;
using namespace River;
using namespace Gfx;
using namespace Duck;

#define MSG_HANDLER(name, pkt_type, event_type) \
auto __msgres_##name = endpoint->set_message_handler<pkt_type>(#name, [&] (pkt_type pkt) { \
	Event evt = {.type = event_type}; \
	handle_##name(pkt, evt); \
	events.push_back(evt); \
}); \
if(__msgres_##name.is_error()) \
	Log::errf("libpond: Couldn't set event handler for event {}: {}", #name, River::error_str(__msgres_##name.code()));

#define GET_FUNC(name, ret_type, data_type, handler) \
auto __funcres_##name = endpoint->get_function<ret_type, data_type>(#name); \
if(__funcres_##name.is_error()) \
	Log::errf("libpond: Couldn't get function {}: {}", #name, River::error_str(__funcres_##name.code())); \
else \
	__river_##handler = __funcres_##name.value();

Context* Context::init() {
	auto conn_res = BusConnection::connect("pond");
	if(conn_res.is_error()) {
		Log::errf("libpond: Couldn't create BusConnection: {}", River::error_str(conn_res.code()));
		return nullptr;
	}
	auto connection = conn_res.value();

	auto endpoint_res = connection->get_endpoint("pond_server");
	if(endpoint_res.is_error()) {
		Log::errf("libpond: Couldn't get endpoint: {}", River::error_str(conn_res.code()));
		return nullptr;
	}


	return new Context(endpoint_res.value());
}

Context::Context(std::shared_ptr<Endpoint> endpt): endpoint(std::move(endpt)) {
	MSG_HANDLER(window_moved, WindowMovePkt, PEVENT_WINDOW_MOVE);
	MSG_HANDLER(window_resized, WindowResizedPkt, PEVENT_WINDOW_RESIZE);
	MSG_HANDLER(window_destroyed, WindowDestroyPkt, PEVENT_WINDOW_DESTROY);
	MSG_HANDLER(mouse_moved, MouseMovePkt, PEVENT_MOUSE_MOVE);
	MSG_HANDLER(mouse_button, MouseButtonPkt, PEVENT_MOUSE_BUTTON);
	MSG_HANDLER(mouse_scrolled, MouseScrollPkt, PEVENT_MOUSE_SCROLL);
	MSG_HANDLER(mouse_left, MouseLeavePkt, PEVENT_MOUSE_LEAVE);
	MSG_HANDLER(key_event, KeyEventPkt, PEVENT_KEY);
	MSG_HANDLER(window_focus_changed, WindowFocusPkt, PEVENT_WINDOW_FOCUS);

	GET_FUNC(open_window, WindowOpenedPkt, OpenWindowPkt, open_window);
	GET_FUNC(destroy_window, void, WindowDestroyPkt, destroy_window);
	GET_FUNC(move_window, void, WindowMovePkt, move_window);
	GET_FUNC(resize_window, WindowResizedPkt, WindowResizePkt, resize_window);
	GET_FUNC(invalidate_window, void, WindowInvalidatePkt, invalidate_window);
	GET_FUNC(get_font, FontResponsePkt, GetFontPkt, get_font);
	GET_FUNC(set_title, void, SetTitlePkt, set_title);
	GET_FUNC(reparent, void, WindowReparentPkt, reparent);
	GET_FUNC(set_hint, void, SetHintPkt, set_hint);
	GET_FUNC(window_to_front, void, WindowToFrontPkt, window_to_front);
	GET_FUNC(get_display_info, DisplayInfoPkt, GetDisplayInfoPkt, get_display_info);
	GET_FUNC(set_app_info, void, App::Info, set_app_info);
	GET_FUNC(focus_window, void, WindowFocusPkt, focus_window);
}

void Context::read_events(bool block) {
	endpoint->bus()->read_and_handle_packets(block);
}

bool Context::has_event() {
	read_events(false);
	return !events.empty();
}

Event Context::next_event() {
	if(events.empty())
		read_events(true);
	else
		read_events(false);
	Event ret = events.front();
	events.pop_front();
	return ret;
}

Event Context::next_event(int type) {
	while(true) {
		if(events.empty())
			read_events(true);
		else
			read_events(false);

		auto it = events.begin();
		while(it != events.end()) {
			if(it->type == type) {
				Event ret = *it;
				events.erase(it);
				return ret;
			}
			it++;
		}
	}
}

Window* Context::create_window(Window* parent, Gfx::Rect rect, bool hidden) {
	auto resp = __river_open_window(OpenWindowPkt {parent ? parent->id() : 0, hidden, rect});
	Event evt;
	handle_window_opened(resp, evt);
	return evt.window_create.window;
}

Font* Context::get_font(const char* font) {
	//If we've already loaded the font, just return it
	if(fonts[font])
		return fonts[font];

	auto resp = __river_get_font({font});
	Event event = {PEVENT_FONT_RESPONSE};
	handle_font_response(resp, event);

	fonts[font] = event.font_response.font;

	return event.font_response.font;
}

int Context::connection_fd() {
	return endpoint->bus()->file_descriptor();
}

void Context::handle_window_opened(const WindowOpenedPkt& pkt, Event& event) {
	if(pkt.window_id < 0) {
		event.window_create.window = NULL;
		return;
	}

	//Open the shared memory for the framebuffer
	struct shm shm;
	if(shmattach(pkt.shm_id, NULL, &shm) < 0) {
		Log::errf("libpond: Failed to attach window shm!");
		event.window_create.window = NULL;
		return;
	}

	//Allocate the new window object and put it in the PEvent
	auto* window = new Window(pkt.window_id, pkt.rect, shm, this);
	event.window_create.window = window;

	//Add the window to the map
	windows[window->_id] = window;
	Log::dbgf("libpond: Opened window with id {}", pkt.window_id);
}

void Context::handle_window_destroyed(const WindowDestroyPkt& pkt, Event& event) {
	event.window_destroy.id = pkt.window_id;

	//Remove the window from the array
	auto window_pair = windows.find(pkt.window_id);
	if(window_pair != windows.end()) {
		//Detach the window's shared memory
		Log::dbgf("libpond: Destroying window with id {}", pkt.window_id);
		if(shmdetach(pkt.shm_id) < 0)
			Log::errf("libpond: Could not detach destroyed window shm!");
		windows.erase(window_pair);
	} else {
		Log::warnf("libpond: Failed to find window with id {} in map for removal!", pkt.window_id);
	}
}

void Context::handle_window_moved(const WindowMovePkt& pkt, Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.window_move.window = window;
		event.window_move.old_pos = window->position();
		window->_rect.set_position(pkt.pos);
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for window move event!", pkt.window_id);
	}
}

void Context::handle_window_resized(const WindowResizedPkt& pkt, Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.window_resize.window = window;
		event.window_resize.old_rect = window->_rect;
		window->_rect = pkt.rect;
		//Open the new shared memory for the framebuffer if necessary
		if(pkt.shm_id != window->_shm.id) {
			if(shmdetach(window->_shm.id) < 0)
				Log::errf("libpond: Failed to detach window shm on resize!");
			struct shm shm;
			if(shmattach(pkt.shm_id, NULL, &shm) < 0) {
				Log::errf("libpond: Failed to attach window shm on resize!");
				event.window_create.window = NULL;
				return;
			}
			window->_flipped = false;
			window->_shm = shm;
		}
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for resize event!", pkt.window_id);
	}
}

void Context::handle_mouse_moved(const MouseMovePkt& pkt, Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.mouse_move.window = window;
		event.mouse_move.delta = pkt.delta;
		event.mouse_move.new_pos = pkt.relative;
		event.mouse_move.abs_pos = pkt.absolute;
		window->_mouse_pos = pkt.relative;
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for window mouse move event!", pkt.window_id);
	}
}

void Context::handle_mouse_button(const MouseButtonPkt& pkt, Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.mouse_button.window = window;
		event.mouse_button.old_buttons = window->_mouse_buttons;
		event.mouse_button.new_buttons = pkt.buttons;
		window->_mouse_buttons = pkt.buttons;
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for window mouse button event!", pkt.window_id);
	}
}

void Context::handle_mouse_scrolled(const MouseScrollPkt& pkt, Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.mouse_scroll.window = window;
		event.mouse_scroll.scroll = pkt.scroll;
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for mouse scroll event!", pkt.window_id);
	}
}

void Context::handle_mouse_left(const MouseLeavePkt& pkt, Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.mouse_leave.window = window;
		event.mouse_leave.last_pos = window->_mouse_pos;
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for window mouse button event!", pkt.window_id);
	}
}

void Context::handle_key_event(const KeyEventPkt& pkt, Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.key.window = window;
		event.key.character = pkt.character;
		event.key.modifiers = pkt.modifiers;
		event.key.key = pkt.key;
		event.key.scancode = pkt.scancode;
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for window key event!", pkt.window_id);
	}
}

void Context::handle_font_response(const FontResponsePkt& pkt, Event& event) {
	if(pkt.font_shm_id < 0) {
		event.font_response.font = nullptr;
		return;
	}

	//Try attaching the shared memory
	shm fontshm;
	if(shmattach(pkt.font_shm_id, nullptr, &fontshm) < 0) {
		Log::errf("libpond: Failed to attach font shm!");
		event.font_response.font = nullptr;
		return;
	}

	//Load the font (if it errors out, a message should be printed and it will return nullptr)
	event.font_response.font = Font::load_from_shm(fontshm);
}

void Context::handle_window_focus_changed(const Pond::WindowFocusPkt& pkt, Pond::Event& event) {
	//Find the window and update the event & window
	Window* window = windows[pkt.window_id];
	if(window) {
		event.window_focus.window = window;
		event.window_focus.focused = pkt.focused;
	} else {
		event.type = PEVENT_UNKNOWN;
		Log::warnf("libpond: Could not find window {} for window focus changed event!", pkt.window_id);
	}
}

Gfx::Dimensions Context::get_display_dimensions() {
	return __river_get_display_info({}).dimensions;
}

void Context::set_app_info(App::Info& info) {
	__river_set_app_info(info);
}
