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

#pragma once

#include <memory>
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
	private:
		int layer_;
		std::string image_name_;
		bool random_start_;
		point base_;
		point center_;
		float opacity_;
		// mask/crop/blit
		std::vector<TileImageVariant> variants_;
	};

	class TileRule
	{
	public:
		explicit TileRule(TerrainRulePtr parent, const variant& v);
	private:
		std::weak_ptr<TerrainRule> parent_;
		std::unique_ptr<point> position_;
		int pos_;
		std::vector<std::string> type_;
		std::vector<std::string> set_flag_;
		std::vector<std::string> no_flag_;
		std::vector<std::string> has_flag_;
		std::unique_ptr<TileImage> image_;
	};

	class TerrainRule : public std::enable_shared_from_this<TerrainRule>
	{
	public:
		explicit TerrainRule(const variant& v);

		static TerrainRulePtr create(const variant& v);
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

		std::vector<TileRule> tile_data_;
		std::unique_ptr<TileImage> image_;
	};
}
