/*
	Copyright (C) 2013-2016 by Kristina Simpson <sweet.kristas@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <boost/algorithm/string.hpp>

#include "tile_rules.hpp"
#include "hex_map.hpp"

namespace hex
{
	TerrainRule::TerrainRule(const variant& v)
		: absolute_position_(nullptr),
		  mod_position_(nullptr),
		  rotations_(),
		  set_flag_(),
		  no_flag_(),
		  has_flag_(),
		  map_(),
		  tile_data_(),
		  image_(nullptr)
	{
		if(v.has_key("x")) {
			absolute_position_ = std::unique_ptr<point>(new point(v["x"].as_int32()));
		}
		if(v.has_key("y")) {
			if(absolute_position_ != nullptr) {
				absolute_position_->y = v["y"].as_int32();
			} else {
				absolute_position_ = std::unique_ptr<point>(new point(0, v["y"].as_int32()));
			}
		}
		if(v.has_key("mod_x")) {
			mod_position_ = std::unique_ptr<point>(new point(v["mod_x"].as_int32()));
		}
		if(v.has_key("mod_y")) {
			if(mod_position_ != nullptr) {
				mod_position_->y = v["mod_y"].as_int32();
			} else {
				mod_position_ = std::unique_ptr<point>(new point(0, v["mod_y"].as_int32()));
			}
		}
		if(v.has_key("rotations")) {
			rotations_ = v["rotations"].as_list_string();
		}
		std::vector<std::string> set_no_flag;
		if(v.has_key("set_no_flag")) {
			set_no_flag = v["set_no_flag"].as_list_string();
		}
		if(v.has_key("set_flag")) {
			set_flag_ = v["set_flag"].as_list_string();
			for(const auto& snf : set_no_flag) {
				set_flag_.emplace_back(snf);
			}
		}
		if(v.has_key("no_flag")) {
			no_flag_ = v["no_flag"].as_list_string();
			for(const auto& snf : set_no_flag) {
				no_flag_.emplace_back(snf);
			}
		}
		if(v.has_key("has_flag")) {
			has_flag_ = v["has_flag"].as_list_string();
		}
		if(v.has_key("map")) {
			map_ = v["map"].as_list_string();
		}
		

		if(v.has_key("image")) {
		}
	}

	TerrainRulePtr TerrainRule::create(const variant& v)
	{
		auto tr = std::make_shared<TerrainRule>(v);
		if(v.has_key("tile")) {
			if(v["tile"].is_list()) {
				for(const auto& tile : v["tile"].as_list()) {
					auto td = std::unique_ptr<TileRule>(new TileRule(tr, tile));
					if(td->hasPosition()) {
						tr->tile_data_.emplace_back(std::move(td));
					} else {
						tr->tile_map_.emplace(td->getMapPos(), std::move(td));
					}
				}
			} else if(v["tile"].is_map()) {
				tr->tile_data_.emplace_back(std::unique_ptr<TileRule>(new TileRule(tr, v["tile"])));
			} else {
				ASSERT_LOG(false, "Tile data was neither list or map.");
			}
		}
		return tr;
	}

	TileRule::TileRule(TerrainRulePtr parent, const variant& v)
		: parent_(parent),
		  position_(nullptr),
		  pos_(v["pos"].as_int32(0)),
		  type_(),
		  set_flag_(),
		  no_flag_(),
		  has_flag_(),
		  image_(nullptr)
	{
		if(v.has_key("x")) {
			position_.reset((new point(v["x"].as_int32())));
		}
		if(v.has_key("y")) {
			if(position_ != nullptr) {
				position_->y = v["y"].as_int32();
			} else {
				position_.reset(new point(0, v["y"].as_int32()));
			}
		}
		std::vector<std::string> set_no_flag;
		if(v.has_key("set_no_flag")) {
			set_no_flag = v["set_no_flag"].as_list_string();
		}
		if(v.has_key("set_flag")) {
			set_flag_ = v["set_flag"].as_list_string();
			for(const auto& snf : set_no_flag) {
				set_flag_.emplace_back(snf);
			}
		}
		if(v.has_key("no_flag")) {
			no_flag_ = v["no_flag"].as_list_string();
			for(const auto& snf : set_no_flag) {
				no_flag_.emplace_back(snf);
			}
		}
		if(v.has_key("has_flag")) {
			has_flag_ = v["has_flag"].as_list_string();
		}
		if(v.has_key("type")) {
			type_ = v["type"].as_list_string();
		}
		// ignore name as I've not seen any instances in a tile def.
		if(v.has_key("image")) {
			image_.reset(new TileImage(v["image"]));
		}
	}

	bool TileRule::match(const HexObject* obj, TerrainRule* tr)
	{
		return false;
	}

	TileImage::TileImage(const variant& v)
		: layer_(v["layer"].as_int32(-1000)),
		  image_name_(v["name"].as_string_default("")),
		  random_start_(v["random_start"].as_bool(true)),
		  base_(v["base"].as_int32(0)),
		  center_(v["center"].as_int32(0)),
		  opacity_(1.0f),
		  crop_(),
		  variants_()
	{
		if(v.has_key("O")) {
			opacity_ = v["O"]["param"].as_float();
		}
		if(v.has_key("CROP")) {
			crop_ = rect(v["CROP"]["param"]);
		}
		
		if(v.has_key("variant")) {
			for(const auto& ivar : v["variant"].as_list()) {
				variants_.emplace_back(ivar);
			}
		}
	}

	TileImageVariant::TileImageVariant(const variant& v)
		: tod_(v["tod"].as_string_default("")),
		  name_(v["name"].as_string_default("")),
		  random_start_(true),
		  has_flag_()
	{
		if(v.has_key("has_flag")) {
			has_flag_ = v["has_flag"].as_list_string();
		}
	}

	bool TerrainRule::match(const HexMapPtr& hmap)
	{
		if(absolute_position_) {
			ASSERT_LOG(tile_data_.size() != 1, "Number of tiles is not correct in rule.");
			if(!tile_data_[0]->match(hmap->getTileAt(*absolute_position_), this)) {
				return false;
			}
		}

		const auto& map_tiles = hmap->getTiles();

		for(const auto& hex : map_tiles) {
			if(mod_position_) {
				auto& pos = hex.getPosition();
				if((pos.x % mod_position_->x) != 0 || (pos.y % mod_position_->y) != 0) {
					continue;
				}
			}

			if(map_.empty()) {
				// No map we expect tiles to have position data.
				for(const auto& td : tile_data_) {
					ASSERT_LOG(td->hasPosition(), "No map and tile data doesn't have an x,y position.");
					const point& p = td->getPosition();
					int index = p.x + p.y * hmap->getWidth();
					ASSERT_LOG(index >= 0 && index < map_tiles.size(), "Invalid index for point " << p << " in map.");
					auto new_obj = hex.getTileAt(p);
					if(!td->match(new_obj, this)) {
						continue;
					}
				}
			} else {
				// Have map, tiles should have map position data.
				// XXX process map here 
				// XXX we should transform the map declaration into something more useful.
				int y = 0;
				for(const auto& map_line : map_) {
					std::vector<std::string> strs;
					boost::split(strs, boost::erase_all_copy(map_line, " \t"), boost::is_any_of(","));
					// valid symbols are asterisk(*), period(.) and tile references(0-9).
					// '.' means this rule does not apply to this hex
					// '*' means this rule applies to this hex, but this hex can be any terrain type
					// an empty string is an odd line.
					int x = strs.front().empty() ? 1 : 0;
					for(auto& str : strs) {
						
					}
				}
			}
		}
		return false;
	}
}

