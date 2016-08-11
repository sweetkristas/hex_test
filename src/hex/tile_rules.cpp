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

#include "hex_helper.hpp"
#include "hex_loader.hpp"
#include "hex_map.hpp"
#include "tile_rules.hpp"

#include "random.hpp"
#include "unit_test.hpp"

namespace 
{
	const int HexTileSize = 72;	// XXX abstract this elsewhere.

	std::string rot_replace(const std::string& str, const std::vector<std::string>& rotations, int rot)
	{
		//if(rot == 0) {
		//	return str;
		//}
		auto pos = str.find("@R");
		if(pos == std::string::npos) {
			return str;
		}

		std::string res;
		bool done = false;
		auto start_pos = 0;
		while(pos != std::string::npos) {
			res += str.substr(start_pos, pos - start_pos);
			int index = str[pos + 2] - '0';
			ASSERT_LOG(index >= 0 && index <= 5, "Invalid @R value: " << index);
			res += rotations[(index + rot)%rotations.size()];
			start_pos = pos + 3;
			pos = str.find("@R", start_pos);
		}
		res += str.substr(start_pos);
		//LOG_DEBUG("replaced " << str << " with " << res);
		return res;
	}

	// n*60 is the degrees of rotation
	// c is the center point
	// p is the co-ordinate to rotate.
	point rotate_point(int n, const point& c, const point& p)
	{
		point res = p;
		int x_p, y_p, z_p;
		int x_c, y_c, z_c;
		if(n > 0) {
			hex::oddq_to_cube_coords(p, &x_p, &y_p, &z_p);
			hex::oddq_to_cube_coords(c, &x_c, &y_c, &z_c);

			const int p_from_c_x = x_p - x_c;
			const int p_from_c_y = y_p - y_c;
			const int p_from_c_z = z_p - z_c;

			int r_from_c_x = p_from_c_x, 
				r_from_c_y = p_from_c_y, 
				r_from_c_z = p_from_c_z;
			for(int i = 0; i != n; ++i) {
				const int x = r_from_c_x, y = r_from_c_y, z = r_from_c_z;
				r_from_c_x = -z;
				r_from_c_y = -x;
				r_from_c_z = -y;
			}
			const int r_x = r_from_c_x + x_c;
			const int r_y = r_from_c_y + y_c;
			const int r_z = r_from_c_z + z_c;
			res = hex::cube_to_oddq_coords(r_x, r_y, r_z);
		}
		return res;
	}

	bool string_match(const std::string& s1, const std::string& s2)
	{
		std::string::const_iterator s1it = s1.cbegin();
		std::string::const_iterator s2it = s2.cbegin();
		while(s1it != s1.cend() && s2it != s2.cend()) {
			if(*s1it == '*') {
				++s1it;
				if(s1it == s1.cend()) {
					// an asterisk at the end of the string matches all, so just return a match.
					return true;
				}
				while(s2it != s2.cend() && *s2it != *s1it) {
					++s2it;
				}
				if(s2it == s2.cend() && s1it != s1.cend()) {
					return false;
				}
				++s1it;
				++s2it;
			} else {
				if(*s1it++ != *s2it++) {
					return false;
				}
			}
		}
		if(s1it != s1.cend() || s2it != s2.cend()) {
			return false;
		}
		return true;
	}

	point add_hex_coord(const point& p1, const point& p2) 
	{
		int x_p1, y_p1, z_p1;
		int x_p2, y_p2, z_p2;
		hex::oddq_to_cube_coords(p1, &x_p1, &y_p1, &z_p1);
		hex::oddq_to_cube_coords(p2, &x_p2, &y_p2, &z_p2);
		return hex::cube_to_oddq_coords(x_p1 + x_p2, y_p1 + y_p2, z_p1 + z_p2);
	}

	point center_point(const point& from_center, const point& to_center, const point& p)
	{
		int x_p, y_p, z_p;
	    int x_c, y_c, z_c;
        hex::oddq_to_cube_coords(p, &x_p, &y_p, &z_p);
        hex::oddq_to_cube_coords(from_center, &x_c, &y_c, &z_c);

        const int p_from_c_x = x_p - x_c;
        const int p_from_c_y = y_p - y_c;
        const int p_from_c_z = z_p - z_c;
			
		int x_r, y_r, z_r;
		hex::oddq_to_cube_coords(to_center, &x_r, &y_r, &z_r);
		return hex::cube_to_oddq_coords(x_r + p_from_c_x, y_r + p_from_c_y, z_r + p_from_c_z);
	}

	point pixel_distance(const point& from, const point& to, int hex_size)
	{
		auto f = hex::get_pixel_pos_from_tile_pos(from, hex_size);
		auto t = hex::get_pixel_pos_from_tile_pos(to, hex_size);
		return f - t;
	}
}

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
		  center_(),
		  tile_data_(),
		  image_(),
		  pos_offset_()
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
		}
		for(const auto& snf : set_no_flag) {
			set_flag_.emplace_back(snf);
		}
		if(v.has_key("no_flag")) {
			no_flag_ = v["no_flag"].as_list_string();
		}
		for(const auto& snf : set_no_flag) {
			no_flag_.emplace_back(snf);
		}
		if(v.has_key("has_flag")) {
			has_flag_ = v["has_flag"].as_list_string();
		}
		if(v.has_key("map")) {
			map_ = v["map"].as_list_string();
		}
		
		if(v.has_key("image")) {
			const auto& img_v = v["image"];
			if(img_v.is_list()) {
				for(const auto& img : img_v.as_list()) {
					image_.emplace_back(new TileImage(img));
				}
			} else if(v.is_map()) {
				image_.emplace_back(new TileImage(v["image"]));
			}
		}
	}

	std::string TerrainRule::toString() const
	{
		std::stringstream ss;
		if(absolute_position_) {
			ss << "x,y: " << *absolute_position_ << "; ";
		}
		if(mod_position_) {
			ss << "mod_x/y: " << *mod_position_ << "; ";
		}
		if(!rotations_.empty()) {
			ss << "rotations:";
			for(const auto& rot : rotations_) {
				ss << " " << rot;
			}
			ss << "; ";
		}
		if(!image_.empty()) {
			ss << "images: ";
			for(const auto& img : image_) {
				ss << " " << img->toString();
			}
			ss << "; ";
		}
		if(!tile_data_.empty()) {
			ss << "tiles: ";
			for(const auto& td : tile_data_) {
				ss << " " << td->toString();
			}
			ss << "; ";
		}
	
		return ss.str();
	}

	void TerrainRule::preProcessMap(const variant& tiles)
	{
		if(!tiles.is_null()) {
			if(tiles.is_list()) {
				for(const auto& tile : tiles.as_list()) {
					auto td = std::unique_ptr<TileRule>(new TileRule(shared_from_this(), tile));
					tile_data_.emplace_back(std::move(td));
				}
			} else if(tiles.is_map()) {
				tile_data_.emplace_back(std::unique_ptr<TileRule>(new TileRule(shared_from_this(), tiles)));
			} else {
				ASSERT_LOG(false, "Tile data was neither list or map.");
			}
		}

		// Map processing.
		if(map_.empty()) {
			return;
		}
		std::string first_line = boost::trim_copy(map_.front());		
		int y = first_line.front() == ',' ? 1 : 0;		
		auto td = std::unique_ptr<TileRule>(new TileRule(shared_from_this()));
		std::vector<point> coord_list;
		for(const auto& map_line : map_) {
			std::vector<std::string> strs;
			std::string ml = boost::erase_all_copy(boost::erase_all_copy(map_line, "\t"), " ");
			boost::split(strs, ml, boost::is_any_of(","));
			// valid symbols are asterisk(*), period(.) and tile references(0-9).
			// '.' means this rule does not apply to this hex
			// '*' means this rule applies to this hex, but this hex can be any terrain type
			// an empty string is an odd line.
			int x = 0;
			bool is_odd_line = ml.front() == ',';
			for(auto& str : strs) {
				if(str == ".") {
					coord_list.emplace_back(x, y);
				} else if(str.empty()) {
					// ignore
				} else if(str == "*") {
					td->addPosition(point(x,y));
					coord_list.emplace_back(x, y);
				} else {
					coord_list.emplace_back(x, y);
					try {
						int pos = boost::lexical_cast<int>(str);
						bool found = false;
						for(auto& td : tile_data_) {
							if(td->getMapPos() == pos) {
								td->addPosition(point(x,y));
								if(pos == 1) {
									center_ = point(x,y);
								}
								found = true;
							}
						}
						ASSERT_LOG(found, "No tile for pos: " << pos);
					} catch(boost::bad_lexical_cast&) {
						ASSERT_LOG(false, "Unable to convert to number" << str);
					}
				}
				++x;
			}
			if(is_odd_line) {
				++y;
			}
		}
		
		// XXX I really don't like this code.
		if(/*!rotations_.empty() &&*/ !image_.empty()) {
			const int max_loops = rotations_.empty() ? 1 : rotations_.size();
			pos_offset_.resize(max_loops);
			for(auto& p : coord_list) {
				//LOG_INFO("coord: " << p);
				p = center_point(center_, point(), p);
				//LOG_INFO("centerd coords: " << p);
			}
			for(int rot = 0; rot != max_loops; ++rot) {
				point min_coord;
				for(const auto& p : coord_list) {
					const point rotated = rotate_point(rot, point(), p);
					const point rp = pixel_distance(rotated, point(), HexTileSize);
					//LOG_INFO("rotated(" << rot << "): " << rotated);
					//LOG_INFO("rotated(" << rot << "): " << rp);
					
					if(rp.x < min_coord.x) {
						min_coord.x = rp.x;
					}
					if(rp.y < min_coord.y) {
						min_coord.y = rp.y;
					}
				}

				bool touches_single_hex = false;
				std::vector<point> touched;
				for(const auto& p : coord_list) {
					const point rp = rotate_point(rot, point(), p);
					const point d = pixel_distance(rp, point(), HexTileSize);
					if(d == min_coord) {
						touches_single_hex = true;
						break;
					} else if(d.x == min_coord.x || d.y == min_coord.y) {
						bool doadd = true;
						for(auto& tch : touched) {
							if(tch.x == rp.x) {
								if(tch.y > rp.y) {
									tch.y = rp.y;
								}
								doadd = false;
							}
						}
						if(doadd) {
							touched.emplace_back(rp);
						}
					} else {
						//LOG_INFO("distance" << p << " to (0,0): " << pixel_distance(p, point(), HexTileSize) << "; min: " << min_coord);
					}
				}
				if(!touches_single_hex) {
					// insert a imaginary hex in the upper-left.
					ASSERT_LOG(touched.size() == 2, "Number of hexes touched != 2(" << touched.size() << "): " << toString());
					// find leftmost hex.
					if(touched[0].x < touched[1].x) {
						min_coord = pixel_distance(point(touched[0].x, touched[0].y-1), point(), HexTileSize);
					} else {
						min_coord = pixel_distance(point(touched[1].x, touched[1].y-1), point(), HexTileSize);
					}
				}

				//LOG_INFO("rot: " << rot << "; min: " << min_coord << "; single: " << (touches_single_hex ? "true" : "false"));
				pos_offset_[rot] = min_coord;
			}
		}

		if(!td->getPosition().empty()) {
			tile_data_.emplace_back(std::move(td));
		}

		if(!map_.empty()) {
			for(auto& td : tile_data_) {
				td->center(center_, point());
			}
			center_ = point();
		}
	}

	point TerrainRule::calcOffsetForRotation(int rot)
	{
		if(image_.empty()) {
			return point();
		}
		return pos_offset_[rot];
	}

	// return false to remove this rule. true if it should be kept.
	bool TerrainRule::tryEliminate()
	{
		// If rule has no images, we keep it since it may have flags.
		bool has_image = false;
		for(auto& td : tile_data_) {
			has_image |= td->hasImage();
		}
		if(!has_image && image_.empty()) {
			return true;
		}

		// Basically we should check these as part of initialisation and discard if the particular combination specified in the 
		// image tag isn't valid.
		bool ret = false;
		for(auto& td : tile_data_) {
			ret |= td->eliminate(rotations_);
		}
		for(auto& img : image_) {
			const bool keep = img->eliminate(rotations_);
			ret |= keep;
			//if(!keep) {
			//	LOG_INFO("would eliminate: " << img->toString());
			//}
		}
		return ret;
	}

	void TerrainRule::applyImage(HexObject* hex, int rot)
	{
		point offs = calcOffsetForRotation(rot);
		for(const auto& img : image_) {
			if(img) {
				/// XXX process mask and blit
				std::string fname = img->getNameForRotation(rot);
				hex->addImage(fname, 
					img->getLayer(), 
					img->getBase(), 
					img->getCenter(),
					offs,
					img->getCropRect(),
					img->getOpacity());
			}
		}
	}

	TerrainRulePtr TerrainRule::create(const variant& v)
	{
		auto tr = std::make_shared<TerrainRule>(v);
		tr->preProcessMap(v["tile"]);
		return tr;
	}

	TileRule::TileRule(TerrainRulePtr parent, const variant& v)
		: parent_(parent),
		  position_(),
		  pos_(v["pos"].as_int32(0)),
		  type_(),
		  set_flag_(),
		  no_flag_(),
		  has_flag_(),
		  image_(nullptr),
		  pos_rotations_(),
		  min_pos_()
	{
		if(v.has_key("x") || v.has_key("y")) {
			position_.emplace_back(v["x"].as_int32(0), v["y"].as_int32(0));
		}
		std::vector<std::string> set_no_flag;
		if(v.has_key("set_no_flag")) {
			set_no_flag = v["set_no_flag"].as_list_string();
		}
		if(v.has_key("set_flag")) {
			set_flag_ = v["set_flag"].as_list_string();
		}
		for(const auto& snf : set_no_flag) {
			set_flag_.emplace_back(snf);
		}
		if(v.has_key("no_flag")) {
			no_flag_ = v["no_flag"].as_list_string();
		}
		for(const auto& snf : set_no_flag) {
			no_flag_.emplace_back(snf);
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

	// To match * type
	TileRule::TileRule(TerrainRulePtr parent)
		: parent_(parent),
		  position_(),
		  pos_(0),
		  type_(),
		  set_flag_(),
		  no_flag_(),
		  has_flag_(),
		  image_(nullptr),
		  pos_rotations_(),
		  min_pos_()
	{
		type_.emplace_back("*");
	}

	void TileRule::center(const point& from_center, const point& to_center)
	{
		for(auto& p : position_) {
	        int x_p, y_p, z_p;
	        int x_c, y_c, z_c;
            hex::oddq_to_cube_coords(p, &x_p, &y_p, &z_p);
            hex::oddq_to_cube_coords(from_center, &x_c, &y_c, &z_c);

            const int p_from_c_x = x_p - x_c;
            const int p_from_c_y = y_p - y_c;
            const int p_from_c_z = z_p - z_c;
			
			int x_r, y_r, z_r;
			hex::oddq_to_cube_coords(to_center, &x_r, &y_r, &z_r);
			p = hex::cube_to_oddq_coords(x_r + p_from_c_x, y_r + p_from_c_y, z_r + p_from_c_z);
		}
	}

	bool TileRule::eliminate(const std::vector<std::string>& rotations)
	{
		if(image_) {
			return image_->eliminate(rotations);
		}
		//return true;
		return false;
	}

	std::string TileRule::toString()
	{
		std::stringstream ss;
		ss << "TileRule: ";
		bool no_comma = true;
		if(!has_flag_.empty()) {
			ss << "has:";
			for(auto& hf : has_flag_) {
				ss << (no_comma ? "" : ",") << " \"" << hf << "\"";
				if(no_comma) {
					no_comma = false;
				}
			}
		}
		
		if(!set_flag_.empty()) {
			no_comma = true;
			ss << "; set:";
			for(auto& sf : set_flag_) {
				ss << (no_comma ? "" : ",") << " \"" << sf << "\"";
				if(no_comma) {
					no_comma = false;
				}
			}
		}

		if(!no_flag_.empty()) {
			no_comma = true;
			ss << "; no:";
			for(auto& nf : no_flag_) {
				ss << (no_comma ? "" : ",") << " \"" << nf << "\"";
				if(no_comma) {
					no_comma = false;
				}
			}
		}

		no_comma = true;
		ss << "; types:";
		for(auto& type : type_) {
			ss << (no_comma ? "" : ",") << " \"" << type << "\"";
			if(no_comma) {
				no_comma = false;
			}
		}
		
		no_comma = true;
		ss << "; positions:";
		for(auto& pos : position_) {
			ss << (no_comma ? "" : ", ") << pos;
			if(no_comma) {
				no_comma = false;
			}
		}

		if(image_) {
			ss << "; image: " << image_->toString();
		}
		return ss.str();
	}

	bool TileRule::matchFlags(const HexObject* obj, TerrainRule* tr, const std::vector<std::string>& rs, int rot)
	{
		const auto& has_flag = has_flag_.empty() ? tr->getHasFlags() : has_flag_;
		for(auto& f : has_flag) {
			if(!obj->hasFlag(rot_replace(f, rs, rot))) {
				return false;
			}
		}
		const auto& no_flag = no_flag_.empty() ? tr->getNoFlags() : no_flag_;
		for(auto& f : no_flag) {
			if(obj->hasFlag(rot_replace(f, rs, rot))) {
				return false;
			}
		}
		return true;
	}

	bool TileRule::match(const HexObject* obj, TerrainRule* tr, const std::vector<std::string>& rs, int rot)
	{
		if(obj == nullptr) {
			for(auto& type : type_) {
				if(type == "*") {
					return true;
				}
			}
			return false;
		}
		const std::string& hex_type_full = obj->getFullTypeString();
		const std::string& hex_type = obj->getTypeString();
		bool invert_match = false;
		bool tile_match = true;
		for(auto& type : type_) {
			type = rot_replace(type, rs, rot);
			if(type == "!") {
				invert_match = !invert_match;
				continue;
			}
			const bool matches = type == "*" || string_match(type, hex_type_full);
			if(!matches) {
				if(invert_match == true) {
					continue;
				} else {
					return false;
				}
			}
			if(matches && invert_match == false) {
				break;
			} else {
				return false;
			}
		}

		if(tile_match) {
			if(!matchFlags(obj, tr, rs, rot)) {
				tile_match = false;
				return false;
			}

			const auto& set_flag = set_flag_.empty() ? tr->getSetFlags() : set_flag_;
			for(auto& f : set_flag) {
				obj->addTempFlag(rot_replace(f, rs, rot));
			}
		}

		return tile_match;
	}

	void TileRule::applyImage(HexObject* hex, int rot)
	{
		if(image_) {
			/// XXX process mask and blit
			std::string fname = image_->getNameForRotation(rot);
			hex->addImage(fname, 
				image_->getLayer(), 
				image_->getBase(), 
				image_->getCenter(),
				point(),
				image_->getCropRect(),
				image_->getOpacity());
		}
	}

	TileImage::TileImage(const variant& v)
		: layer_(v["layer"].as_int32(-1000)),
		  image_name_(v["name"].as_string_default("")),
		  random_start_(v["random_start"].as_bool(true)),
		  base_(),
		  center_(),
		  opacity_(1.0f),
		  crop_(),
		  variants_(),
		  variations_(),
		  image_files_()
	{
		if(v.has_key("O")) {
			opacity_ = v["O"]["param"].as_float();
		}
		if(v.has_key("CROP")) {
			crop_ = rect(v["CROP"]["param"]);
		}
		if(v.has_key("base")) {
			base_ = point(v["base"]);
		}
		if(v.has_key("center")) {
			center_ = point(v["center"]);
		}
		if(v.has_key("variant")) {
			for(const auto& ivar : v["variant"].as_list()) {
				variants_.emplace_back(ivar);
			}
		}
		if(v.has_key("variations")) {
			auto vars = v["variations"].as_list_string();
			auto pos = image_name_.find("@V");
			auto pos_r = image_name_.find("@R");
			if(pos_r != std::string::npos) {
				variations_ = vars;
			} else {
				for(const auto& var : vars) {
					std::string name = image_name_.substr(0, pos) + var + image_name_.substr(pos + 2);
					if(terrain_info_exists(name)) {
						variations_.emplace_back(var);
					}
				}
			}
		}
	}

	const std::string& TileImage::getNameForRotation(int rot)
	{
		auto it = image_files_.find(rot);
		ASSERT_LOG(it != image_files_.end(), "No image for rotation: " << rot << " : " << toString());
		ASSERT_LOG(!it->second.empty(), "No files for rotation: " << rot);
		return it->second[rng::generate() % it->second.size()];
	}

	bool TileImage::isValidForRotation(int rot)
	{
		return image_files_.find(rot) != image_files_.end();
	}

	std::string TileImage::toString() const
	{
		std::stringstream ss;
		ss << "name:" << image_name_ << "; layer(" << layer_ << "); base: " << base_;
		if(!variations_.empty()) {
			ss << "; variations:";
			for(auto& var : variations_) {
				ss << " " << var;
			}
		}
		return ss.str();
	}

	bool TileImage::eliminate(const std::vector<std::string>& rotations)
	{
		// Calculate whether particular rotations are valid.
		// return true if we should keep this, false if there are
		// no valid terrain images available.
		auto pos_v = image_name_.find("@V");
		auto pos_r = image_name_.find("@R");
		if(pos_r == std::string::npos) {
			if(!variations_.empty()) {
				for(const auto& var : variations_) {
					pos_v = image_name_.find("@V");
					std::string img_name = image_name_.substr(0, pos_v) + var + image_name_.substr(pos_v + 2);
					if(terrain_info_exists(img_name)) {
						image_files_[0].emplace_back(img_name);
					}
				}
			} else {
				if(terrain_info_exists(image_name_)) {
					image_files_[0].emplace_back(image_name_);
				}
			}
			return !image_files_.empty();
		}
		// rotate all the combinations and test them
		for(int rot = 0; rot != 6; ++rot) {
			std::string name = rot_replace(image_name_, rotations, rot);
			if(pos_v == std::string::npos) {
				if(terrain_info_exists(name)) {
					image_files_[rot].emplace_back(name);
				}
				continue;
			}
			for(const auto& var : variations_) {
				pos_v = name.find("@V");
				std::string img_name = name.substr(0, pos_v) + var + name.substr(pos_v + 2);
				if(terrain_info_exists(img_name)) {
					image_files_[rot].emplace_back(img_name);
				} else {
				}
			}
		}

		return !image_files_.empty();
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

	std::string TileImage::getName() const
	{
		// XXX WIP
		std::string name = image_name_;
		auto pos = name.find("@V");
		if(!variations_.empty() && pos != std::string::npos) {
			int index = rng::generate() % variations_.size();
			const std::string& var = variations_[index];
			name = name.substr(0, pos) + var + name.substr(pos + 2);
		}
		return name;
	}

	bool TerrainRule::match(const HexMapPtr& hmap)
	{
		if(absolute_position_) {
			ASSERT_LOG(tile_data_.size() != 1, "Number of tiles is not correct in rule.");
			if(!tile_data_[0]->match(hmap->getTileAt(*absolute_position_), this, std::vector<std::string>(), 0)) {
				return false;
			}
		}

		// check rotations.
		ASSERT_LOG(rotations_.size() == 6 || rotations_.empty(), "Set of rotations not of size 6(" << rotations_.size() << ").");
		const int max_loop = rotations_.empty() ? 1 : rotations_.size();

		for(auto& hex : hmap->getTilesMutable()) {
			for(int rot = 0; rot != max_loop; ++rot) {
				if(mod_position_) {
					auto& pos = hex.getPosition();
					if((pos.x % mod_position_->x) != 0 || (pos.y % mod_position_->y) != 0) {
						continue;
					}
				}

				std::vector<std::pair<HexObject*, TileRule*>> obj_to_set_flags;
				// We expect tiles to have position data.
				bool tile_match = true;

				if(!image_.empty()) {
					bool res = false;
					for(const auto& img : image_) {
						res |= img->isValidForRotation(rot);
					}
					if(!res) {
						// XXX should we check for images in tile tags?
						continue;
					}
				}

				for(const auto& td : tile_data_) {
					ASSERT_LOG(td->hasPosition(), "tile data doesn't have an x,y position.");
					const auto& pos_data = td->getPosition();

					bool match_pos = false;

					for(const auto& p : pos_data) {
						point rot_p = rotate_point(rot, add_hex_coord(center_, hex.getPosition()), add_hex_coord(p, hex.getPosition()));
						auto new_obj = const_cast<HexObject*>(hmap->getTileAt(rot_p));
						if(td->match(new_obj, this, rotations_, rot)) {
							match_pos = true;
							if(new_obj) {
								obj_to_set_flags.emplace_back(std::make_pair(new_obj, td.get()));
							}
							break;
						} else {
							if(new_obj) {
								new_obj->clearTempFlags();
							}
						}
					}
					if(!match_pos) {
						tile_match = false;
						obj_to_set_flags.clear();
						break;
					}
				}

				if(tile_match) {
					// XXX need to fix issues when other tiles have images that need to match a different hex
					//tile_data_.front()->applyImage(&hex, rotations_, rot);
					applyImage(&hex, rot);
				}

				for(auto& obj : obj_to_set_flags) {
					obj.first->setTempFlags();
					obj.second->applyImage(obj.first, rot);
				}

			}
		}
		return false;
	}
}

UNIT_TEST(point_rotate)
{
	CHECK_EQ(rotate_point(0, point(2, 2), point(2, 1)), point(2, 1));
	CHECK_EQ(rotate_point(1, point(2, 2), point(2, 1)), point(3, 1));
	CHECK_EQ(rotate_point(2, point(2, 2), point(2, 1)), point(3, 2));
	CHECK_EQ(rotate_point(3, point(2, 2), point(2, 1)), point(2, 3));
	CHECK_EQ(rotate_point(4, point(2, 2), point(2, 1)), point(1, 2));
	CHECK_EQ(rotate_point(5, point(2, 2), point(2, 1)), point(1, 1));
	CHECK_EQ(rotate_point(1, point(3, 3), point(3, 2)), point(4, 3));
}

UNIT_TEST(string_match)
{
	CHECK_EQ(string_match("*", "Any string"), true);
	CHECK_EQ(string_match("Chs", "Ch"), false);
	CHECK_EQ(string_match("G*", "Gg"), true);
	CHECK_EQ(string_match("G*^Fp", "Gg^Fp"), true);
	CHECK_EQ(string_match("Re", "Rd"), false);
	CHECK_EQ(string_match("*^Bsb|", "Gg^Bsb|"), true);
	CHECK_EQ(string_match("*^Bsb|", "Gg^Fp"), false);
}

UNIT_TEST(rot_replace)
{
	CHECK_EQ(rot_replace("transition-@R0-@R1-x", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 1), "transition-ne-se-x");
	CHECK_EQ(rot_replace("xyzzy", std::vector<std::string>{}, 0), "xyzzy");
	CHECK_EQ(rot_replace("transition-@R0", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 0), "transition-n");
	CHECK_EQ(rot_replace("transition-@R0", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 1), "transition-ne");
	CHECK_EQ(rot_replace("transition-@R0", std::vector<std::string>{"n", "ne", "se", "s", "sw", "nw"}, 5), "transition-nw");
}
