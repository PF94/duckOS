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

#pragma once

#include <libpond/Window.h>
#include <vector>
#include "../DrawContext.h"
#include "libui/Menu.h"
#include <libduck/Object.h>

#define WIDGET_DEF(name) DUCK_OBJECT_DEF(name)
#define VIRTUAL_WIDGET_DEF(name) DUCK_OBJECT_VIRTUAL(name)

namespace UI {
	enum SizingMode {
		PREFERRED, FILL
	};

	enum PositioningMode {
		AUTO, ABSOLUTE
	};

	template<typename T>
	using Ptr = Duck::Ptr<T>;
	template<typename T>
	using PtrRef = Duck::PtrRef<T>;
	template<typename T>
	using WeakPtr = Duck::WeakPtr<T>;

	class Window;
	class Widget: public Duck::Object {
	public:
		DUCK_OBJECT_DEF(Widget)

		~Widget() override;

		/**
		 * Returns the preferred size of the widget (which may not be its current size).
		 * @return The preferred size of the widget.
		 */
		virtual Gfx::Dimensions preferred_size();

		/**
		 * Returns the current size of the widget (may not be its preferred size).
		 * @return The current size of the widget.
		 */
		Gfx::Dimensions current_size();

		/**
		 * Returns the positioning mode of the widget.
		 * @return The positioning mode of the widget.
		 */
		PositioningMode positioning_mode();

		/**
		 * Sets the positioning mode of the widget.
		 * @param mode The positioning mode of the widget.
		 */
		void set_positioning_mode(PositioningMode mode);

		/**
		 * Returns the sizing mode of the widget.
		 * @return The sizing mode of the widget.
		 */
		SizingMode sizing_mode();

		/**
		 * Sets the sizing mode of the widget.
		 * @param mode The sizing mode of the widget.
		 */
		void set_sizing_mode(SizingMode mode);

		/**
		 * This function is called to scheduel a repaint of the widget.
		 */
		void repaint();

		/**
		 * This function immediately repaints the contents of the widget if needed.
		 */
		void repaint_now();

		/**
		 * This function is called whenever a keyboard event happens on the widget.
		 * @param evt The event in question.
		 * @return Whether or not the event was handled and should stop propagating to the parent.
		 */
		virtual bool on_keyboard(Pond::KeyEvent evt);

		/**
		 * This function is called whenever a mouse movement event happens on the widget.
		 * @param evt The event in question.
		 * @return Whether or not the event was handled and should stop propagating to the parent.
		 */
		virtual bool on_mouse_move(Pond::MouseMoveEvent evt);

		/**
		 * This function is called whenever a mouse button event happens on the widget.
		 * @param evt The event in question.
		 * @return Whether or not the event was handled and should stop propagating to the parent.
		 */
		virtual bool on_mouse_button(Pond::MouseButtonEvent evt);

		/**
		 * This function is called whenever a mouse scroll event happens on the widget.
		 * @param evt The event in question.
		 * @return Whether or not the event was handled and should stop propagating to the parent.
		 */
		virtual bool on_mouse_scroll(Pond::MouseScrollEvent evt);

		/**
		 * This function is called whenever the mouse leaves the widget (or enters a child widget).
		 * @param evt The event in question.
		 */
		virtual void on_mouse_leave(Pond::MouseLeaveEvent evt);

		/**
		 * The parent of this widget.
		 * @return A pointer to the parent widget, or nullptr if there isn't one.
		 */
		std::shared_ptr<Widget> parent();

		/**
		 * The parent window of this widget. Will only be non-null if this is a top-level widget.
		 * @return A pointer to the parent window, or nullptr if the parent is another widget.
		 */
		std::shared_ptr<Window> parent_window();

		/**
		 * The root window of this widget.
		 * @return A pointer to the root window of this widget.
		 */
		std::shared_ptr<Window> root_window();

		/**
		 * Adds a child to the widget.
		 * @param child The child to add.
		 */
		void add_child(PtrRef<Widget> child);

		/**
		 * Removes a child from the widget.
		 * @param child The child to remove.
		 * @return Whether or not the removal was successful (i.e. if the specified widget was a child of this widget)
		 */
		bool remove_child(PtrRef<Widget> child);

		/**
		 * Sets the position of the widget.
		 * The behavior of this depends on the positioning mode of the widget and the parent's layout specifications.
		 * @param position The new position of the widget.
		 */
		void set_position(const Gfx::Point& position);

		/**
		 * Sets the position of the widget without recalculating layouts.
		 * @param position The new position of the widget.
		 */
		void set_position_nolayout(const Gfx::Point& position);

		/**
		 * Gets the position of the widget.
		 * @return The position of the widget.
		 */
		Gfx::Point position();

		 /**
		  * Hides the widget.
		  */
		void hide();

		/**
		* Shows the widget.
		*/
		void show();

		/**
		* Sets the bounds for the widget. (called during a layout update)
		* @param new_bounds The new bounds of the widget.
		*/
		void set_layout_bounds(Gfx::Rect new_bounds);

		/**
		 * Whether or not the widget needs a layout update when a child is added or removed.
		 * @return If a layout update should be preformed on child addition/removal.
		 */
		virtual bool needs_layout_on_child_change();

		/**
		 * Focuses the widget. Focused widgets will receive keyboard events.
		 */
		void focus();

		/**
		 * Opens a menu at the cursor location.
		 * @param menu The menu to open.
		 */
		virtual void open_menu(UI::Menu::Ptr menu);

	protected:
		explicit Widget() = default;

		/**
		 * Sets the parent window of the widget.
		 * @param window The parent window of the widget.
		 */
		void set_window(const std::shared_ptr<Window>& window);

		/**
		 * Called when the root window is set for a parent widget.
		 */
		void set_root_window(Window* window);

		/**
		 * Sets the parent widget of the widget.
		 * @param widget The parent widget.
		 */
		void set_parent(Widget* widget);

		/**
		 * Removes the parent of the widget.
		 */
		void remove_parent();

		/**
		 * Moves and resizes the widget according to the sizing mode, positioning mode, and parent's specifications.
		 */
		void update_layout();

		/**
		 * Called when the widget needs to be repainted.
		 * @param ctx The drawing context of the widget.
		 */
		virtual void do_repaint(const DrawContext& ctx);

		/**
		 * Called when a child is added to the widget.
		 * @param child The child added.
		 */
		virtual void on_child_added(PtrRef<Widget> child);

		/**
		 * Called when a child is removed from the widget.
		 * @param child The child removed.
		 */
		virtual void on_child_removed(PtrRef<Widget> child);

		/**
		 * This function is called whenever the widget's position or size are changed.
		 * @param old_rect The old rect of the widget.
		 */
		virtual void on_layout_change(const Gfx::Rect& old_rect);

		/**
		 * Sets whether the widget should use alpha blending or not.
		 * @param uses_alpha Whether the widget should use alpha blending.
		 */
		void set_uses_alpha(bool uses_alpha);

		/**
		 * Sets whether the widget should recieve global mouse events or not.
		 * @param global_mouse Whether the widget should recieve global mouse events.
		 */
		void set_global_mouse(bool global_mouse);

		/**
		 * Gets the last-reported position of the mouse in the widget.
		 * @return The position of the mouse in the widget.
		 */
		Gfx::Point mouse_position();

		/**
		 * Gets the last-reported pressed mouse buttons within the widget.
		 * @return The mouse buttons pressed within the widget.
		 */
		unsigned int mouse_buttons();

		/**
		 * Sets the bounds for each child of the widget according to its layout.
		 */
		virtual void calculate_layout();

		/**
		 * Recalculates the drawing-related rects of the widget.
		 */
		void recalculate_rects();

		/**
		 * Called after the constructor to initialize the widget.
		 */
		virtual void initialize();

		std::vector<Ptr<Widget>> children;

	private:
		friend class Window;
		friend class ScrollView; //TODO: Better way for ScrollView to access this stuff

		bool evt_mouse_move(Pond::MouseMoveEvent evt);
		bool evt_mouse_button(Pond::MouseButtonEvent evt);
		bool evt_mouse_scroll(Pond::MouseScrollEvent evt);
		void evt_mouse_leave(Pond::MouseLeaveEvent evt);
		void evt_keyboard(Pond::KeyEvent evt);

		// TODO: Should be weakptr
		Widget* _parent = nullptr;
		// TODO: Should be weakptr
		Window* _parent_window = nullptr;
		// TODO: Should be weakptr
		Window* _root_window = nullptr;
		Gfx::Rect _rect = {0, 0, 0, 0};
		Gfx::Rect _absolute_rect = {0, 0, 0, 0};
		Gfx::Rect _visible_rect = {0, 0, 0, 0};
		Gfx::Point _absolute_position = {0, 0};
		Gfx::Point _mouse_pos = {0, 0};
		unsigned int _mouse_buttons = 0;
		bool _initialized_size = false;
		bool _uses_alpha = false;
		bool _global_mouse = false;
		bool _hidden = false;
		bool _dirty = false;
		bool _first_layout_done = false;
		PositioningMode _positioning_mode = AUTO;
		SizingMode _sizing_mode = FILL;
		Gfx::Image _image;
	};
}

