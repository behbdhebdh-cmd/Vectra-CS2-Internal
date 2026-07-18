#pragma once
#include "Config.h"

#define MI_SECTION  0
#define MI_TOGGLE   1
#define MI_CHECKBOX 2
#define MI_SLIDER   3
#define MI_COMBO    4
#define MI_INPUT    5

namespace MenuDef
{
	struct Item
	{
		int type;
		const char* label;
		void* data;
		float min, max;
		const char* fmt;
		const char** items;
		int itemCount;
		int strSize;
		int widgetId;
	};

	struct TabItems { const Item* items; int count; };

	inline TabItems GetTabItems(int tab)
	{
		static const Item aim[] =
		{
			{ MI_SECTION,  "A I M" },
			{ MI_TOGGLE,   "Enabled",       &Config::Aim::enabled },
			{ MI_CHECKBOX, "Silent Aim",    &Config::Aim::silent },
			{ MI_CHECKBOX, "FOV Circle",    &Config::Aim::showFovCircle },
			{ MI_SLIDER,   "FOV",           &Config::Aim::fov, 0.f, 180.f, "%.0f" },
			{ MI_COMBO,    "Mode",          &Config::Aim::mode, 0,0,nullptr, Config::Aim::modes, 3, 0, 0 },
		};

		static const Item visual[] =
		{
			{ MI_SECTION,  "V I S U A L" },
			{ MI_TOGGLE,   "ESP Enabled",   &Config::Visual::enabled },
			{ MI_CHECKBOX, "Box ESP",       &Config::Visual::box },
			{ MI_CHECKBOX, "Health Bar",    &Config::Visual::healthBar },
			{ MI_CHECKBOX, "Snaplines",     &Config::Visual::snaplines },
			{ MI_COMBO,    "ESP Mode",      &Config::Visual::mode, 0,0,nullptr, Config::Visual::modes, 3, 0, 1 },
		};

		static const Item misc[] =
		{
			{ MI_SECTION,  "M I S C" },
			{ MI_TOGGLE,   "Bunny Hop",     &Config::Misc::bhop },
			{ MI_TOGGLE,   "No Flash",      &Config::Misc::noFlash },
			{ MI_CHECKBOX, "Radar",         &Config::Misc::radar },
			{ MI_CHECKBOX, "Watermark",     &Config::Misc::waterMark },
		};

		static const Item skins[] =
		{
			{ MI_SECTION,  "S K I N S" },
			{ MI_TOGGLE,   "Skin Changer",  &Config::Skins::changer },
			{ MI_CHECKBOX, "Gloves",        &Config::Skins::gloves },
			{ MI_COMBO,    "Weapon",        &Config::Skins::weapon, 0,0,nullptr, Config::Skins::weapons, 4, 0, 4 },
			{ MI_COMBO,    "Quality",       &Config::Skins::quality, 0,0,nullptr, Config::Skins::qualities, 5, 0, 6 },
		};

		static const Item config[] =
		{
			{ MI_SECTION,  "C O N F I G" },
			{ MI_TOGGLE,   "Save on exit",  &Config::General::saveOnExit },
			{ MI_TOGGLE,   "Load on start", &Config::General::loadOnStart },
			{ MI_COMBO,    "Theme",         &Config::General::theme, 0,0,nullptr, Config::General::themes, 3, 0, 7 },
			{ MI_CHECKBOX, "Show FPS",      &Config::General::showFps },
		};

		static const Item gui[] =
		{
			{ MI_SECTION,  "G U I" },
			{ MI_SLIDER,   "Width",         &Config::Gui::width,  400.f, 900.f, "%.0f" },
			{ MI_SLIDER,   "Height",        &Config::Gui::height, 300.f, 700.f, "%.0f" },
			{ MI_COMBO,    "Menu Mode",     &Config::Gui::mode, 0,0,nullptr, Config::Gui::modes, 3, 0, 8 },
			{ MI_SLIDER,   "Opacity",       &Config::Gui::opacity, 0.5f, 1.f, "%.2f" },
		};

		switch (tab)
		{
		case 0: return { aim,   sizeof(aim)   / sizeof(aim[0])   };
		case 1: return { visual,sizeof(visual)/ sizeof(visual[0])};
		case 2: return { misc,  sizeof(misc)  / sizeof(misc[0])  };
		case 3: return { skins, sizeof(skins) / sizeof(skins[0]) };
		case 4: return { config,sizeof(config)/ sizeof(config[0])};
		case 5: return { gui,   sizeof(gui)   / sizeof(gui[0])   };
		default: return { nullptr, 0 };
		}
	}
}
