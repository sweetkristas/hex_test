/*
	Copyright (C) 2014-2015 by Kristina Simpson <sweet.kristas@gmail.com>
	
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

#include <set>

#include "hex_helper.hpp"
#include "hex_loader.hpp"
#include "hex_map.hpp"
#include "hex_renderable.hpp"
#include "hex_tile.hpp"

#include "Shaders.hpp"
#include "SceneGraph.hpp"
#include "WindowManager.hpp"

#include "random.hpp"

namespace hex
{
	using namespace KRE;

	namespace 
	{
		rng::Seed hex_tile_seed;

		const int g_hex_tile_size = 72;
	}

	SceneNodeRegistrar<MapNode> psc_register("hex_map");

	MapNode::MapNode(std::weak_ptr<KRE::SceneGraph> sg, const variant& node)
		: SceneNode(sg, node),
		  layers_(),
		  changed_(false)
	{
	}

	MapNodePtr MapNode::create(std::weak_ptr<KRE::SceneGraph> sg, const variant& node)
	{
		return std::make_shared<MapNode>(sg, node);
	}

	void MapNode::notifyNodeAttached(std::weak_ptr<SceneNode> parent)
	{
		for(auto& layer : layers_) {
			attachObject(layer);
		}
	}

	void add_tex_coords(std::vector<KRE::vertex_texcoord>* coords, const rectf& uv, int w, int h, const std::vector<int>& borders, const point& base, const point& center, const point& hex_pixel_pos)
	{
		point p = hex_pixel_pos + base;
		if(!borders.empty()) {
			p.x -= borders[0];
			p.y -= borders[1];
		}
		const float vx1 = static_cast<float>(p.x);
		const float vy1 = static_cast<float>(p.y);
		const float vx2 = static_cast<float>(p.x + w);
		const float vy2 = static_cast<float>(p.y + h);

		coords->emplace_back(glm::vec2(vx1, vy1), glm::vec2(uv.x1(), uv.y1()));
		coords->emplace_back(glm::vec2(vx2, vy1), glm::vec2(uv.x2(), uv.y1()));
		coords->emplace_back(glm::vec2(vx2, vy2), glm::vec2(uv.x2(), uv.y2()));

		coords->emplace_back(glm::vec2(vx2, vy2), glm::vec2(uv.x2(), uv.y2()));
		coords->emplace_back(glm::vec2(vx1, vy1), glm::vec2(uv.x1(), uv.y1()));
		coords->emplace_back(glm::vec2(vx1, vy2), glm::vec2(uv.x1(), uv.y2()));
	}

	void MapNode::update(int width, int height, const std::vector<HexObject>& tiles)
	{
		layers_.clear();
		clear();

		std::map<int, std::pair<MapLayerPtr, std::vector<KRE::vertex_texcoord>>> map_layers;
		for(auto& hex : tiles) {
			auto images = hex.getImages();
			for(auto& img : images) {
				auto& layer = map_layers[img.layer];
				layer.first.reset(new MapLayer);
				rect area;
				std::vector<int> borders;
				auto& tex = get_terrain_texture(img.name, &area, &borders);
				if(tex) {
					layer.first->setTexture(tex);
					add_tex_coords(&layer.second, 
						tex->getTextureCoords(0, area), 
						area.w(), 
						area.h(), 
						borders, 
						img.base, 
						img.center, 
						get_pixel_pos_from_tile_pos(hex.getPosition(), 
						g_hex_tile_size));
				}
			}
		}

		for(auto& layer : map_layers) {
			layer.second.first->updateAttributes(&layer.second.second);
			layer.second.first->setOrder(layer.first);
			layers_.emplace_back(layer.second.first);
			attachObject(layer.second.first);
		}
	}

	MapLayer::MapLayer()
		: SceneObject("hex::MapLayer"),
		  attr_(nullptr)
	{
		setShader(ShaderProgram::getSystemDefault());

		//auto as = DisplayDevice::createAttributeSet(true, false ,true);
		auto as = DisplayDevice::createAttributeSet(true, false, false);
		as->setDrawMode(DrawMode::TRIANGLES);

		attr_ = std::make_shared<Attribute<vertex_texcoord>>(AccessFreqHint::STATIC);
		attr_->addAttributeDesc(AttributeDesc(AttrType::POSITION, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, vtx)));
		attr_->addAttributeDesc(AttributeDesc(AttrType::TEXTURE, 2, AttrFormat::FLOAT, false, sizeof(vertex_texcoord), offsetof(vertex_texcoord, tc)));

		as->addAttribute(attr_);
		addAttributeSet(as);
	}
	
	void MapLayer::preRender(const KRE::WindowPtr& wnd)
	{
	}

	void MapLayer::updateAttributes(std::vector<KRE::vertex_texcoord>* attrs)
	{
		attr_->update(attrs);
	}
}
