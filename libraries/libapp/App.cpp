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

#include "App.h"
#include <libgraphics/PNG.h>
#include <libduck/Config.h>
#include <unistd.h>

using namespace App;
using Duck::Result, Duck::ResultRet, Duck::Path;

ResultRet<Info> Info::from_app_directory(const Path& app_directory) {
	auto config_res = Duck::Config::read_from(app_directory / "app.conf");
	if(config_res.is_error())
		return config_res.result();
	auto& cfg = config_res.value();

	if(cfg.has_section("app")) {
		auto& app_config = cfg["app"];
		auto has_name = app_config.find("name") != app_config.end();
		auto has_exec = app_config.find("exec") != app_config.end();
		if(!has_name || !has_exec)
			return Result(EINVAL);
		auto info = Info(app_directory, app_config["name"],  app_config["exec"]);
		info._hidden = app_config["hidden"] == "true";
		return info;
	}

	return Result(EINVAL);
}

ResultRet<Info> Info::from_app_name(const std::string& app_name) {
	return from_app_directory(Path(LIBAPP_BASEPATH) / (app_name + ".app"));
}

ResultRet<Info> Info::from_current_app() {
	char exe_path[512];
	if(!readlink("/proc/self/exe", exe_path, 512)) {
		return from_app_directory(Path(exe_path).parent());
	} else {
		return Result(errno);
	}
}

Info::Info(Path base_path, std::string name, std::string exec):
	_exists(true), _base_path(std::move(base_path)), _name(std::move(name)), _exec(std::move(exec)) {}

Duck::Ptr<const Gfx::Image> Info::icon() {
	if(!_icon) {
		auto icon_res = Gfx::Image::load(_base_path / "icon");
		if(icon_res.is_error())
			icon_res = Gfx::Image::load(LIBAPP_MISSING_ICON);
		if(icon_res.is_error())
			icon_res = Gfx::Image::empty({16, 16});
		_icon = icon_res.value();
		_icon->set_size({16, 16});
	}
	return _icon;
}

const std::string& Info::name() const {
	return _name;
}

const std::string Info::exec() const {
	if(_exec[0] != '/')
		return _base_path / _exec;
	else
		return _exec;
}

bool Info::exists() const {
	return _exists;
}

bool Info::hidden() const {
	return _hidden;
}

Path Info::base_path() const {
	return _base_path;
}

Path Info::resource_path(const Path& path) const {
	if(path.is_absolute())
		return path;
	else
		return _base_path / path;
}

std::shared_ptr<const Gfx::Image> Info::resource_image(const Path& path) {
	auto it = _images.find(path);
	if(it == _images.end()) {
		auto img = Gfx::Image::load(resource_path(path));
		if(img.has_value())
			_images[path.string()] = img.value();
		else
			return Gfx::Image::empty();
	}
	
	return _images[path.string()];
}

size_t Info::serialized_size() const {
	bool has_icon;
	if(_icon)
		return Duck::Serialization::buffer_size(_name, _base_path.string(), _hidden, has_icon, *_icon);
	else
		return Duck::Serialization::buffer_size(_name, _base_path.string(), _hidden, has_icon);
}

void Info::serialize(uint8_t*& buf) const {
	Duck::Serialization::serialize(buf, _name, _base_path.string(), _hidden, _icon.operator bool());
	if(_icon) {
		Gfx::Framebuffer icon_buf(_icon->size().width, _icon->size().height);
		_icon->draw(icon_buf, {{0, 0}, _icon->size()});
		Duck::Serialization::serialize(buf, icon_buf);
	}
}

void Info::deserialize(const uint8_t*& buf) {
	std::string base_path;
	bool has_icon;
	Duck::Serialization::deserialize(buf, _name, base_path, _hidden, has_icon);
	_base_path = base_path;
	if(has_icon) {
		auto* iconbuf = new Gfx::Framebuffer();
		Duck::Serialization::deserialize(buf, *iconbuf);
		_icon = Gfx::Image::take(iconbuf);
	}
}

std::vector<Info> App::get_all_apps() {
	std::vector<Info> ret;
	auto ent_res = Path(LIBAPP_BASEPATH).get_directory_entries();
	if(ent_res.is_error())
		return ret;
	for(const auto& ent : ent_res.value()) {
		if(ent.is_directory()) {
			if(ent.path().extension() == "app") {
				auto app_res = Info::from_app_directory(ent.path());
				if(!app_res.is_error())
					ret.push_back(app_res.value());
			}
		}
	}
	return ret;
}