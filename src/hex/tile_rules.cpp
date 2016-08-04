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
#include "hex_map.hpp"
#include "tile_rules.hpp"

#include "random.hpp"
#include "unit_test.hpp"

namespace 
{
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
		  image_()
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
		for(const auto& map_line : map_) {
			std::vector<std::string> strs;
			std::string ml = boost::erase_all_copy(boost::erase_all_copy(map_line, "\t"), " ");
			boost::split(strs, ml, boost::is_any_of(","));
			// valid symbols are asterisk(*), period(.) and tile references(0-9).
			// '.' means this rule does not apply to this hex
			// '*' means this rule applies to this hex, but this hex can be any terrain type
			// an empty string is an odd line.
			int x = 0;
			for(auto& str : strs) {
				if(str == "." || str.empty()) {
					// ignore
				} else if(str == "*") {
					td->addPosition(point(x,y));
				} else {
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
			++y;
		}
		if(!td->getPosition().empty()) {
			tile_data_.emplace_back(std::move(td));
		}
	}

	void TerrainRule::applyImage(HexObject* hex)
	{
		/// XXX
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
		  image_(nullptr)
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
		  image_(nullptr)
	{
		type_.emplace_back("*");
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
		return ss.str();
	}

	std::string TileRule::getImage(const HexObject* obj, const std::vector<std::string>& rs, int rot, int* layer)
	{
		// this is still WIP
		if(image_) {
			if(layer) {
				*layer = image_->getLayer();
			}
			return rot_replace(image_->getName(), rs, rot);
		}

		// XXX Disabled for the since this needs re-working.
		auto& imgs = parent_.lock()->getImages();
		//if(img) {
		//	if(layer) {
		//		*layer = img->getLayer();
		//	}
		//	return rot_replace(img->getName(), rs, rot);
		//}
		return std::string();
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
		bool tile_match = false;
		for(auto& type : type_) {
			type = rot_replace(type, rs, rot);
			if(type == "!") {
				invert_match = !invert_match;
				continue;
			}
			const bool matches = type == "*" || string_match(type, hex_type);
			if(!matches && invert_match == true) {
				continue;
			}
			if(matches && invert_match == false) {
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

				tile_match = true;
				break;
			}
		}

		if(tile_match) {
			const auto& set_flag = set_flag_.empty() ? tr->getSetFlags() : set_flag_;
			for(auto& f : set_flag) {
				obj->addTempFlag(rot_replace(f, rs, rot));
			}
		}

		return tile_match;
	}

	void TileRule::applyImage(HexObject* hex, const std::vector<std::string>& rs, int rot)
	{
		if(image_) {
			/// XXX process mask and blit
			hex->addImage(image_->getName(), image_->getLayer(), image_->getBase(), image_->getCenter(), image_->getCropRect(), image_->getOpacity());
		}
	}

	TileImage::TileImage(const variant& v)
		: layer_(v["layer"].as_int32(-1000)),
		  image_name_(v["name"].as_string_default("")),
		  random_start_(v["random_start"].as_bool(true)),
		  base_(v["base"].as_int32(0)),
		  center_(v["center"].as_int32(0)),
		  opacity_(1.0f),
		  crop_(),
		  variants_(),
		  variations_()
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
		if(v.has_key("variations")) {
			variations_ = v["variations"].as_list_string();
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

	std::string TileImage::getName() const
	{
		// XXX WIP
		std::string name = image_name_;
		if(!variations_.empty()) {
			auto pos = name.find("@V");
			ASSERT_LOG(pos != std::string::npos, "Error have variations in image but no @V symbol found to replace in image name." << name);
			const std::string& var = variations_[rng::generate() % variations_.size()];
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

		// ignore rotations for the moment.
		ASSERT_LOG(rotations_.size() == 6 || rotations_.empty(), "Set of rotations not of size 6(" << rotations_.size() << ").");
		const int max_loop = rotations_.empty() ? 1 : rotations_.size();

		for(auto& hex : hmap->getTilesMutable()) {
			hex.clearImages();
			for(int rot = 0; rot != max_loop; ++rot) {

				if(mod_position_) {
					auto& pos = hex.getPosition();
					if((pos.x % mod_position_->x) != 0 || (pos.y % mod_position_->y) != 0) {
						continue;
					}
				}

				std::vector<const HexObject*> obj_to_set_flags;
				// We expect tiles to have position data.
				bool tile_match = true;

				for(const auto& td : tile_data_) {
					ASSERT_LOG(td->hasPosition(), "tile data doesn't have an x,y position.");
					const auto& pos_data = td->getPosition();

					bool match_pos = false;

					for(const auto& p : pos_data) {
						point rot_p = rotate_point(rot, add_hex_coord(center_, hex.getPosition()), add_hex_coord(p, hex.getPosition()));
						auto new_obj = hmap->getTileAt(rot_p);
						if(td->match(new_obj, this, rotations_, rot)) {
							match_pos = true;
							obj_to_set_flags.emplace_back(new_obj);
							break;
						} else {
							if(new_obj) {
								new_obj->clearTempFlags();
							}
						}
					}
					if(!match_pos) {
						tile_match = false;
						break;
					} else {
						obj_to_set_flags.clear();
					}
				}

				if(tile_match) {
					tile_data_.front()->applyImage(&hex, rotations_, rot);
					applyImage(&hex);

					//auto img = tile_data_.front()->getImage(&hex, rotations_, rot, &layer);
					//if(!img.empty()) {
					//	LOG_INFO("tile(" << hex.getFullTypeString() << ") at " << hex.getPosition() << ": " << img << ", layer=" << layer);
					//	for(const auto& td : tile_data_) {
					//		LOG_INFO(td->toString());
					//	}
					//}
				}

				for(auto& obj : obj_to_set_flags) {
					obj->setTempFlags();
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
