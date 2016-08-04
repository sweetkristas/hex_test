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

#include "hex_renderable.hpp"
#include "hex_tile.hpp"
#include "hex_map.hpp"

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

	void MapNode::update(int width, int height, const std::vector<HexObject>& tiles)
	{
		layers_.clear();
		clear();

		std::map<int, MapLayer> test;
		for(auto& hex : tiles) {
			auto images = hex.getImages();
			for(auto& img : images) {
				auto& XX = test[img.layer];
			}
		}
		std::vector<KRE::vertex_texcoord> coords;
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
