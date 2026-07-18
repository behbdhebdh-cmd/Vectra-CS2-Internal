#pragma once

namespace Config
{
	inline bool menuOpen = false;
	inline int currentTab = 0;

	namespace Aim
	{
		inline bool enabled = true;
		inline bool silent = false;
		inline bool showFovCircle = true;
		inline float fov = 5.f;
		inline int mode = 0;
		inline const char* modes[3] = { "Closest", "Crosshair", "Health" };
	}

	namespace Visual
	{
		inline bool enabled = true;
		inline bool box = true;
		inline bool healthBar = true;
		inline bool snaplines = false;
		inline int mode = 0;
		inline const char* modes[3] = { "Normal", "Glow", "Chams" };
		inline char filter[64] = "all";
	}

	namespace Misc
	{
		inline bool bhop = false;
		inline bool noFlash = true;
		inline bool radar = false;
		inline bool waterMark = true;
		inline char text[128] = "MenuExt";
	}

	namespace Skins
	{
		inline bool changer = false;
		inline bool gloves = false;
		inline int weapon = 0;
		inline const char* weapons[4] = { "AK-47", "M4A4", "AWP", "Deagle" };
		inline char skin[64] = "Dragon Lore";
		inline int quality = 0;
		inline const char* qualities[5] = { "Factory New", "Minimal Wear", "Field-Tested", "Well-Worn", "Battle-Scarred" };
	}

	namespace Gui
	{
		inline float width  = 580.f;
		inline float height = 400.f;
		inline int mode = 0;
		inline const char* modes[3] = { "Default", "Compact", "Full" };
		inline float opacity = 0.92f;
	}

	namespace General
	{
		inline bool saveOnExit  = true;
		inline bool loadOnStart = true;
		inline bool showFps     = false;
		inline int theme = 0;
		inline const char* themes[3] = { "Dark", "Darker", "Light" };
	}
}
