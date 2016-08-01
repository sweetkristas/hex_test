/*
	Copyright (C) 2013-2014 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include "asserts.hpp"

#include "hex_loader.hpp"
#include "hex_tile.hpp"
#include "tile_rules.hpp"

namespace
{
	typedef std::map<std::string, hex::HexTilePtr> tile_map_type;
	tile_map_type& get_tile_map()
	{
		static tile_map_type res;
		return res;
	}

	typedef std::vector<hex::TerrainRulePtr> terrain_rule_type;
	terrain_rule_type& get_terrain_rules()
	{
		static terrain_rule_type res;
		return res;
	}
}

namespace hex
{
	void load_tile_data(const variant& v)
	{
		ASSERT_LOG(v.is_map() && v.has_key("terrain_type") && v["terrain_type"].is_list(), 
			"Expected hex tile data to be a map with 'terrain_type' key.");
		const auto& tt_data = v["terrain_type"].as_list();
		for(const auto& tt : tt_data) {
			ASSERT_LOG(tt.is_map(), "Expected inner items of 'terrain_type' to be maps." << tt.to_debug_string());
			const std::string string = tt["string"].as_string();

			auto tile = HexTile::create(string);

			if(tt.has_key("editor_group")) {
				tile->setEditorGroup(tt["editor_group"].as_string());
			}
			if(tt.has_key("id")) {
				tile->setId(tt["id"].as_string());
			}
			if(tt.has_key("name")) {
				tile->setName(tt["name"].as_string());
			}
			if(tt.has_key("editor_name")) {
				tile->setEditorName(tt["editor_name"].as_string());
			}
			if(tt.has_key("submerge")) {
				tile->setSubmerge(tt["submerge"].as_float());
			}
			if(tt.has_key("symbol_image")) {
				tile->setSymbolImage(tt["symbol_image"].as_string());
			}
			if(tt.has_key("icon_image")) {
				tile->setIconImage(tt["icon_image"].as_string());
			}
			if(tt.has_key("help_topic_text")) {
				tile->setHelpTopicText(tt["help_topic_text"].as_string());
			}
			if(tt.has_key("hidden")) {
				tile->setHidden(tt["hidden"].as_bool());
			}
			if(tt.has_key("recruit_onto")) {
				tile->setRecruitable(tt["recruit_onto"].as_bool());
			}
			if(tt.has_key("hide_help")) {
				tile->setHideHelp(tt["hide_help"].as_bool());
			}
			if(tt.has_key("gives_income")) {
				// XXX
			}
			if(tt.has_key("heals")) {
				// XXX
			}
			if(tt.has_key("recruit_from")) {
				// XXX
			}
			if(tt.has_key("unit_height_adjust")) {
				// XXX
			}
			if(tt.has_key("mvt_alias")) {
				// XXX
			}

			auto it = get_tile_map().find(string);
			ASSERT_LOG(it == get_tile_map().end(), "Duplicate tile string id's found: " << string);
			get_tile_map()[string] = tile;
		}
		LOG_INFO("Loaded " << get_tile_map().size() << " hex tiles into memory.");
	}

	void load_terrain_data(const variant& v)
	{
		ASSERT_LOG(v.is_map() && v.has_key("terrain_graphics") && v["terrain_graphics"].is_list(), 
			"Expected hex tile data to be a map with 'terrain_type' key.");
		const auto& tg_data = v["terrain_graphics"].as_list();
		for(const auto& tg : tg_data) {
			ASSERT_LOG(tg.is_map(), "Expected inner items of 'terrain_type' to be maps." << tg.to_debug_string());
			get_terrain_rules().emplace_back(TerrainRule::create(tg));
		}
		LOG_INFO("Loaded " << get_terrain_rules().size() << " terrain rules into memory.");
	}
}
