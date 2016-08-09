#include <clocale>
#include <locale>
#include <iostream>
#include <fstream>

#include "asserts.hpp"
#include "filesystem.hpp"
#include "Blittable.hpp"
#include "CameraObject.hpp"
#include "Canvas.hpp"
#include "ClipScope.hpp"
#include "Font.hpp"
#include "FontDriver.hpp"
#include "RenderManager.hpp"
#include "RenderTarget.hpp"
#include "SceneGraph.hpp"
#include "SceneNode.hpp"
#include "SceneTree.hpp"
#include "SDLWrapper.hpp"
#include "SurfaceBlur.hpp"
#include "WindowManager.hpp"
#include "profile_timer.hpp"
#include "variant_utils.hpp"
#include "unit_test.hpp"
#include "json.hpp"
#include "hex.hpp"
#include "unit_test.hpp"

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifdef _MSC_VER
std::string wide_string_to_utf8(const std::wstring& ws)
{
	std::string res;

	int ret = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), ws.size(), nullptr, 0, nullptr, nullptr);
	if(ret > 0) {
		std::vector<char> str;
		str.resize(ret);
		ret = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), ws.size(), str.data(), str.size(), nullptr, nullptr);
		res = std::string(str.begin(), str.end());
	}

	return res;
}
#endif

void read_system_fonts(sys::file_path_map* res)
{
#if defined(_MSC_VER)
	// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders\Fonts

	// should enum the key for the data type here, then run a query with a null buffer for the size

	HKEY font_key;
	LONG err = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", 0, KEY_READ, &font_key);
	if(err == ERROR_SUCCESS) {
		std::vector<wchar_t> data;
		DWORD data_size = 0;

		err = RegQueryValueExW(font_key, L"Fonts", 0, 0, nullptr, &data_size);
		if(err != ERROR_SUCCESS) {
			return;
		}
		data.resize(data_size);

		err = RegQueryValueExW(font_key, L"Fonts", 0, 0, reinterpret_cast<LPBYTE>(data.data()), &data_size);
		if(err == ERROR_SUCCESS) {
			if(data[data_size-2] == 0 && data[data_size-1] == 0) {
				// was stored with null terminator
				data_size -= 2;
			}
			data_size >>= 1;
			std::wstring base_font_dir(data.begin(), data.begin()+data_size);
			std::wstring wstr = base_font_dir + L"\\*.?tf";
			WIN32_FIND_DATA find_data;
			HANDLE hFind = FindFirstFileW(wstr.data(), &find_data);
			(*res)[wide_string_to_utf8(std::wstring(find_data.cFileName))] = wide_string_to_utf8(base_font_dir + L"\\" + std::wstring(find_data.cFileName));
			if(hFind != INVALID_HANDLE_VALUE) {			
				while(FindNextFileW(hFind, &find_data)) {
					(*res)[wide_string_to_utf8(std::wstring(find_data.cFileName))] = wide_string_to_utf8(base_font_dir + L"\\" + std::wstring(find_data.cFileName));
				}
			}
			FindClose(hFind);
		} else {
			LOG_WARN("Unable to read \"Fonts\" sub-key");
		}
		RegCloseKey(font_key);
	} else {
		LOG_WARN("Unable to read the shell folders registry key");
		// could try %windir%\fonts as a backup
	}
#elif defined(linux) || defined(__linux__)
	// use maybe XListFonts or fontconfig
#else
#endif
}

void log_output(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
	static bool file_opened = false;
	static std::string file_name = *static_cast<std::string*>(userdata);
	std::ofstream file(file_name, std::ios_base::out | std::ios_base::app);
	switch(priority) {
		case SDL_LOG_PRIORITY_VERBOSE:	file << "VERBOSE: "; break;
		case SDL_LOG_PRIORITY_DEBUG:	file << "DEBUG: "; break;
		case SDL_LOG_PRIORITY_INFO:		file << "INFO: "; break;
		case SDL_LOG_PRIORITY_WARN:		file << "WARN: "; break;
		case SDL_LOG_PRIORITY_ERROR:	file << "ERROR: "; break;
		case SDL_LOG_PRIORITY_CRITICAL:	file << "CRITICAL: "; break;
		default: break;
	}
	file << message << "\n";
}

int main(int argc, char* argv[]) 
{
	std::string log_file_name;
	std::vector<std::string> args;
	for(int i = 1; i < argc; ++i) {
		if(argv[i] == std::string("--log-to")) {
			++i;
			ASSERT_LOG(i < argc, "No argument for --log-to");
			log_file_name = argv[i];
		} else {
			args.emplace_back(argv[i]);
		}
	}
	int width = 1024;
	int height = 768;
	using namespace KRE;
	SDL::SDL_ptr manager(new SDL::SDL());
	//SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

	if(!log_file_name.empty()) {
		SDL_LogSetOutputFunction(log_output, &log_file_name);
	}

	if(!test::run_tests()) {
		// Just exit if some tests failed.
		exit(1);
}

#if defined(__linux__)
	const std::string data_path = "data/";
#else
	const std::string data_path = "../data/";
#endif

	sys::file_path_map font_files;
	sys::get_unique_files(data_path + "fonts/", font_files);
	read_system_fonts(&font_files);
	KRE::FontDriver::setAvailableFonts(font_files);
	KRE::FontDriver::setFontProvider("stb");

	WindowManager wm("SDL");

	variant_builder hints;
	hints.add("renderer", "opengl");
	hints.add("dpi_aware", true);
	hints.add("use_vsync", true);
	hints.add("resizeable", true);

	LOG_DEBUG("Creating window of size: " << width << "x" << height);
	auto main_wnd = wm.createWindow(width, height, hints.build());
	main_wnd->enableVsync(true);
	const float aspect_ratio = static_cast<float>(width) / height;

#if defined(__linux__)
	LOG_DEBUG("setting image file filter to 'images/'");
	Surface::setFileFilter(FileFilterType::LOAD, [](const std::string& fname) { return "images/" + fname; });
	Surface::setFileFilter(FileFilterType::SAVE, [](const std::string& fname) { return "images/" + fname; });
#else
	LOG_DEBUG("setting image file filter to '../images/'");
	Surface::setFileFilter(FileFilterType::LOAD, [](const std::string& fname) { return "../images/" + fname; });
	Surface::setFileFilter(FileFilterType::SAVE, [](const std::string& fname) { return "../images/" + fname; });
#endif
	Font::setAvailableFonts(font_files);

	SceneGraphPtr scene = SceneGraph::create("main");
	SceneNodePtr root = scene->getRootNode();
	root->setNodeName("root_node");

	DisplayDevice::getCurrent()->setDefaultCamera(std::make_shared<Camera>("ortho1", 0, width, 0, height));

	auto rman = std::make_shared<RenderManager>();
	auto rq = rman->addQueue(0, "opaques");

	hex::load(data_path);

	std::string map_to_use = data_path + "maps/test01.map";
	if(!args.empty()) {
		map_to_use = data_path + "maps/" + args[0];
	}
	auto hmap = hex::HexMap::create(map_to_use);
	hmap->build();
	hex::MapNodePtr hex_renderable;
	hex_renderable = std::dynamic_pointer_cast<hex::MapNode>(scene->createNode("hex_map"));
	hmap->setRenderable(hex_renderable);
	scene->getRootNode()->attachNode(hex_renderable);

	SDL_Event e;
	bool done = false;
	Uint32 last_tick_time = SDL_GetTicks();
	SDL_StartTextInput();
	while(!done) {
		while(SDL_PollEvent(&e)) {
			// XXX we need to add some keyboard/mouse callback handling here for "doc".
			if(e.type == SDL_KEYUP) {
				if(e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
					done = true;
				} else {
				}				
			} else if(e.type == SDL_KEYDOWN) {
				//LOG_DEBUG("KEY PRESSED: " << SDL_GetKeyName(e.key.keysym.sym) << " : " << e.key.keysym.sym << " : " << e.key.keysym.scancode);
			} else if(e.type == SDL_QUIT) {
				done = true;
			} else if(e.type == SDL_MOUSEMOTION) {
				bool claimed = false;
			} else if(e.type == SDL_MOUSEBUTTONDOWN) {
				bool claimed = false;
			} else if(e.type == SDL_MOUSEBUTTONUP) {
				bool claimed = false;
			} else if(e.type == SDL_MOUSEWHEEL) {
				if(e.wheel.which != SDL_TOUCH_MOUSEID) {
					bool claimed = false;
					point p;
					unsigned state = SDL_GetMouseState(&p.x, &p.y);
				}
			} else if(e.type == SDL_WINDOWEVENT) {
				const SDL_WindowEvent& wnd = e.window;
				if(wnd.event == SDL_WINDOWEVENT_RESIZED) {
					width = wnd.data1;
					height = wnd.data2;
					main_wnd->notifyNewWindowSize(width, height);
					DisplayDevice::getCurrent()->setDefaultCamera(std::make_shared<Camera>("ortho1", 0, width, 0, height));
				}
			}
		}

		//main_wnd->setClearColor(KRE::Color::colorWhite());
		main_wnd->clear(ClearFlags::ALL);

		hmap->process();

		scene->renderScene(rman);
		rman->render(main_wnd);

		// Called once a cycle before rendering.
		Uint32 current_tick_time = SDL_GetTicks();
		float dt = (current_tick_time - last_tick_time) / 1000.0f;
		scene->process(dt);
		last_tick_time = current_tick_time;

		main_wnd->swap();
	}
	SDL_StopTextInput();

	return 0;
}
