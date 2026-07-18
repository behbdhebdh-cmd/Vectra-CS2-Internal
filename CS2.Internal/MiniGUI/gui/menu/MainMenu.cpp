#include "MainMenu.h"
#include "Widgets.h"
#include "core/Config.h"
#include "core/MenuItems.h"

static ImVec2 operator+(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
static ImVec2 operator-(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }
static ImVec2 operator*(ImVec2 a, float s)  { return ImVec2(a.x * s, a.y * s); }

static float g_menuX = -1.f, g_menuY = -1.f;
static float g_dragOffX, g_dragOffY;
static bool  g_dragging = false;
static float g_scrollY = 0.f;
static float g_tabAnimY = 0.f;

static void Shadow(ImVec2 a, ImVec2 b, float r)
{
	for (int i = 5; i >= 1; i--)
	{
		float o = i * 2.f;
		UI::RectFilled(a - ImVec2(o, o), b + ImVec2(o, o), IM_COL32(0, 0, 0, 8 * (6 - i)), r + o);
	}
}

static bool CloseButton(ImVec2 pos)
{
	ImVec2 a = pos, b = ImVec2(pos.x + 20, pos.y + 20);
	bool hover = UI::InRect(ImGui::GetMousePos(), a, b);
	if (hover) UI::RectFilled(a, b, IM_COL32(255, 60, 60, 180), 4.f);
	UI::Draw()->AddLine(ImVec2(a.x+5, a.y+5), ImVec2(b.x-5, b.y-5), IM_COL32(200,200,215,220), 2.f);
	UI::Draw()->AddLine(ImVec2(b.x-5, a.y+5), ImVec2(a.x+5, b.y-5), IM_COL32(200,200,215,220), 2.f);
	return hover && ImGui::IsMouseClicked(0);
}

static void Sidebar(ImVec2 sa, ImVec2 sb, float sideW)
{
	UI::RectFilled(sa, sb, UI::C_BG_SIDE, UI::R_PANEL);

	const char* tabs[] = { "Aim", "Visual", "Misc", "Skins", "Config", "GUI" };
	float tabH = 38.f;
	float gap = 6.f;
	int tabCount = 6;

	float totalTabsH = tabCount * tabH + (tabCount - 1) * gap;
	float startY = sa.y + 8.f;
	if (startY + totalTabsH > sb.y) startY = sa.y + 4.f;

	float targetY = startY + Config::currentTab * (tabH + gap) + 4.f;
	g_tabAnimY = UI::Lerp(g_tabAnimY, targetY, ImGui::GetIO().DeltaTime * 12.f);
	if (fabs(g_tabAnimY - targetY) < 0.5f) g_tabAnimY = targetY;

	UI::RectFilled(ImVec2(sa.x + 6.f, g_tabAnimY), ImVec2(sa.x + 9.f, g_tabAnimY + tabH - 8.f), UI::C_ACCENT, 1.5f);

	for (int i = 0; i < tabCount; i++)
	{
		float ty = startY + i * (tabH + gap);
		ImVec2 ta(sa.x + 6.f, ty);
		ImVec2 tb(sa.x + sideW - 6.f, ty + tabH);
		bool hover = UI::InRect(ImGui::GetMousePos(), ta, tb);
		bool act   = (Config::currentTab == i);

		if (act) UI::RectFilled(ta, tb, UI::C_ACTIVE, UI::R_ITEM);
		else if (hover) UI::RectFilled(ta, tb, UI::C_HOVER, UI::R_ITEM);

		ImU32 tc = act ? UI::C_ACCENT : hover ? UI::C_TEXT : UI::C_TEXT_DIM;
		UI::Draw()->AddText(ImVec2(ta.x + 14, ta.y + (tabH - ImGui::CalcTextSize(tabs[i]).y) * 0.5f), tc, tabs[i]);

		if (hover && ImGui::IsMouseClicked(0))
		{
			Config::currentTab = i;
			UI::ResetWidgets();
		}
	}
}

static void Section(ImVec2 pos, float w, const char* name)
{
	UI::Draw()->AddText(ImVec2(pos.x + 12, pos.y), UI::C_ACCENT, name);
	UI::Draw()->AddLine(ImVec2(pos.x + 12, pos.y + 20), ImVec2(pos.x + w - 12, pos.y + 20), UI::C_BORDER);
}

namespace Menu
{
	void Render()
	{
		if (!Config::menuOpen) { UI::ResetWidgets(); return; }

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 mp  = io.MousePos;
		ImVec2 scr = io.DisplaySize;

		if (g_menuX < 0)
		{
			g_menuX = (scr.x - Config::Gui::width)  * 0.5f;
			g_menuY = (scr.y - Config::Gui::height) * 0.5f;
		}

		float winW = Config::Gui::width;
		float winH = Config::Gui::height;

		if (g_menuX + winW > scr.x) g_menuX = scr.x - winW - 10;
		if (g_menuY + winH > scr.y) g_menuY = scr.y - winH - 10;
		if (g_menuX < 0) g_menuX = 0;
		if (g_menuY < 0) g_menuY = 0;

		ImVec2 wa(g_menuX, g_menuY), wb(g_menuX + winW, g_menuY + winH);

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(scr);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
		ImGui::Begin("##ov", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoBackground);

		Shadow(wa, wb, UI::R_WINDOW);
		UI::RectFilled(wa, wb, UI::C_BG_MAIN, UI::R_WINDOW);
		UI::RectBorder(wa, wb, UI::C_BORDER, UI::R_WINDOW, 1.f);

		float titleH = 42.f;
		UI::RectGradientV(wa, ImVec2(wb.x, wa.y + titleH), IM_COL32(16, 16, 24, 255), IM_COL32(10, 10, 14, 200), UI::R_WINDOW);
		UI::Draw()->PathClear();
		UI::Draw()->PathLineTo(wa + ImVec2(UI::R_WINDOW, titleH));
		UI::Draw()->PathLineTo(ImVec2(wb.x - UI::R_WINDOW, wa.y + titleH));
		UI::Draw()->PathStroke(UI::C_BORDER, 0, 1.f);

		UI::Draw()->AddText(ImVec2(wa.x + 16, wa.y + 11), UI::C_ACCENT, "MENU EXT");

		if (UI::InRect(mp, wa, ImVec2(wb.x, wa.y + titleH)))
		{
			if (ImGui::IsMouseClicked(0)) { g_dragging = true; g_dragOffX = mp.x - g_menuX; g_dragOffY = mp.y - g_menuY; }
		}
		if (g_dragging)
		{
			g_menuX = mp.x - g_dragOffX;
			g_menuY = mp.y - g_dragOffY;
			if (ImGui::IsMouseReleased(0)) g_dragging = false;
		}

		if (CloseButton(ImVec2(wb.x - 28, wa.y + 11))) Config::menuOpen = false;

		const float sideW = 120.f;
		ImVec2 sa(wa.x + 8, wa.y + titleH + 6);
		ImVec2 sb(wa.x + 8 + sideW, wb.y - 8);
		Sidebar(sa, sb, sideW);

		float cx = sa.x + sideW + 10;
		float cw = wb.x - cx - 8;
		float ch = sb.y - sa.y;
		ImVec2 ca(cx, sa.y), cb(cx + cw, sb.y);
		UI::RectFilled(ca, cb, UI::C_BG_CONT, UI::R_PANEL);

		ImGui::PushClipRect(ca + ImVec2(0, 1), cb - ImVec2(0, 1), true);

		float totalH = 12.f;
		float curY = ca.y + 12 - g_scrollY;
		float itemW = cw - 30;

		auto ItemPos = [&]() -> ImVec2 { return ImVec2(cx + 14, curY); };
		auto Next    = [&]() { curY += 30; totalH += 30; };
		auto ComboNext = [&]() { curY += 30 + UI::g_comboOffsetY; totalH += 30 + UI::g_comboOffsetY; };
		auto InputNext = [&]() { curY += 30; totalH += 30; };

		auto tabItems = MenuDef::GetTabItems(Config::currentTab);
		for (int i = 0; i < tabItems.count; i++)
		{
			const auto& item = tabItems.items[i];
			switch (item.type)
			{
			case MI_SECTION:
				Section(ImVec2(cx, curY), cw, item.label);
				curY += 32; totalH += 32;
				break;

			case MI_TOGGLE:
				UI::Toggle(item.label, (bool*)item.data, ItemPos(), itemW);
				Next();
				break;

			case MI_CHECKBOX:
				UI::Checkbox(item.label, (bool*)item.data, ItemPos(), itemW);
				Next();
				break;

			case MI_SLIDER:
				UI::Slider(item.label, (float*)item.data, item.min, item.max, ItemPos(), itemW, item.fmt);
				Next();
				break;

			case MI_COMBO:
				UI::Combo(item.label, (int*)item.data, item.items, item.itemCount, ItemPos(), item.widgetId, itemW);
				ComboNext();
				break;

			case MI_INPUT:
				UI::Input(item.label, (char*)item.data, item.strSize, ItemPos(), item.widgetId, itemW);
				InputNext();
				break;
			}
		}

		ImGui::PopClipRect();

		float maxScroll = totalH - ch + 10.f;
		if (maxScroll < 0) maxScroll = 0;
		if (g_scrollY > maxScroll) g_scrollY = maxScroll;
		if (g_scrollY < 0) g_scrollY = 0;

		bool inContent = UI::InRect(mp, ca, cb);
		if (inContent && maxScroll > 0.f)
		{
			float wheel = io.MouseWheel;
			if (wheel != 0.f)
			{
				g_scrollY -= wheel * 40.f;
				if (g_scrollY > maxScroll) g_scrollY = maxScroll;
				if (g_scrollY < 0) g_scrollY = 0;
			}
		}

		if (maxScroll > 0)
		{
			float sbW = 4.f;
			float sbH = ch * (ch / (totalH + ch));
			float sbY = sa.y + (g_scrollY / maxScroll) * (ch - sbH);
			ImVec2 sba(cb.x - sbW - 3, sbY);
			ImVec2 sbb(cb.x - 3, sbY + sbH);
			UI::RectFilled(sba, sbb, IM_COL32(255, 255, 255, 60), 2.f);

			if (UI::InRect(mp, sba - ImVec2(4, 0), sbb + ImVec2(4, 0)) && ImGui::IsMouseDown(0))
			{
				float ratio = (mp.y - sa.y - sbH * 0.5f) / (ch - sbH);
				if (ratio < 0) ratio = 0;
				if (ratio > 1) ratio = 1;
				g_scrollY = ratio * maxScroll;
			}
		}

		UI::RenderComboDropdown();

		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	void Toggle()
	{
		Config::menuOpen = !Config::menuOpen;
		if (!Config::menuOpen) UI::ResetWidgets();
	}
}
