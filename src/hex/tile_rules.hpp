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

#pragma once

#include <memory>
#include <map>
#include "geometry.hpp"
#include "variant.hpp"

#include "hex_fwd.hpp"

namespace hex
{
	class TileImageVariant
	{
	public:
		TileImageVariant(const variant& v);
	private:
		std::string tod_;
		std::string name_;
		bool random_start_;
		std::vector<std::string> has_flag_;
	};

	class TileImage
	{
	public:
		explicit TileImage(const variant& v);
		std::string getName() const;
		int getLayer() const { return layer_; }
		const point& getBase() const { return base_; }
		const point& getCenter() const { return center_; }
		float getOpacity() const { return opacity_; }
		const rect& getCropRect() const { return crop_; }
	private:
		int layer_;
		std::string image_name_;
		bool random_start_;
		point base_;
		point center_;
		float opacity_;
		rect crop_;
		// mask/crop/blit
		std::vector<TileImageVariant> variants_;
		std::vector<std::string> variations_;
	};

	class TileRule
	{
	public:
		explicit TileRule(TerrainRulePtr parent, const variant& v);
		explicit TileRule(TerrainRulePtr parent);
		bool hasPosition() const { return !position_.empty(); }
		const std::vector<point>& getPosition() const { return position_; }
		void addPosition(const point& p) { position_.emplace_back(p); }
		int getMapPos() const { return pos_; }
		bool match(const HexObject* obj, TerrainRule* tr, const std::vector<std::string>& rotations, int rot);
		std::string getImage(const HexObject* obj, const std::vector<std::string>& rs, int rot, int* layer);
		std::string toString();
		void applyImage(HexObject* hex, const std::vector<std::string>& rs, int rot);
	private:
		std::weak_ptr<TerrainRule> parent_;
		std::vector<point> position_;
		int pos_;
		std::vector<std::string> type_;
		std::vector<std::string> set_flag_;
		std::vector<std::string> no_flag_;
		std::vector<std::string> has_flag_;
		std::unique_ptr<TileImage> image_;
	};

	typedef std::unique_ptr<TileRule> TileRulePtr;

	class TerrainRule : public std::enable_shared_from_this<TerrainRule>
	{
	public:
		explicit TerrainRule(const variant& v);
		const std::vector<std::string>& getSetFlags() const { return set_flag_; }
		const std::vector<std::string>& getNoFlags() const { return no_flag_; }
		const std::vector<std::string>& getHasFlags() const { return has_flag_; }
		const std::vector<std::string>& getRotations() const { return rotations_; }
		const std::vector<std::string>& getMap() const { return map_; }
		const std::vector<std::unique_ptr<TileImage>>& getImages() const { return image_; }

		bool match(const HexMapPtr& hmap);
		void preProcessMap(const variant& tiles);

		static TerrainRulePtr create(const variant& v);
		void applyImage(HexObject* hex);
	private:
		// constrains the rule to given absolute map coordinates
		std::unique_ptr<point> absolute_position_;
		// constrains the rule to absolute map coordinates which are multiples of the given values
		std::unique_ptr<point> mod_position_;
		std::vector<std::string> rotations_;
		std::vector<std::string> set_flag_;
		std::vector<std::string> no_flag_;
		std::vector<std::string> has_flag_;
		std::vector<std::string> map_;
		// center co-ordinate.
		point center_;

		std::vector<TileRulePtr> tile_data_;
		std::vector<std::unique_ptr<TileImage>> image_;
	};
}
