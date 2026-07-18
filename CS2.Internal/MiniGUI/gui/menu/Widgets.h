#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace UI
{
	inline constexpr ImU32 C_ACCENT    = IM_COL32(255, 255, 255, 235);
	inline constexpr ImU32 C_ACCENT2   = IM_COL32(200, 210, 255, 60);
	inline constexpr ImU32 C_BG_MAIN   = IM_COL32(10, 10, 14, 250);
	inline constexpr ImU32 C_BG_SIDE   = IM_COL32(13, 13, 18, 248);
	inline constexpr ImU32 C_BG_CONT   = IM_COL32(16, 16, 22, 242);
	inline constexpr ImU32 C_TEXT      = IM_COL32(215, 215, 225, 255);
	inline constexpr ImU32 C_TEXT_DIM  = IM_COL32(120, 125, 140, 255);
	inline constexpr ImU32 C_BORDER    = IM_COL32(45, 45, 58, 200);
	inline constexpr ImU32 C_HOVER     = IM_COL32(255, 255, 255, 8);
	inline constexpr ImU32 C_ACTIVE    = IM_COL32(255, 255, 255, 16);
	inline constexpr ImU32 C_TOGGLE_ON = IM_COL32(255, 255, 255, 225);
	inline constexpr ImU32 C_TOGGLE_OFF= IM_COL32(50, 50, 62, 210);
	inline constexpr ImU32 C_FILL      = IM_COL32(45, 45, 58, 180);
	inline constexpr ImU32 C_INPUT_BG  = IM_COL32(35, 35, 48, 195);

	inline constexpr float R_WINDOW = 12.f;
	inline constexpr float R_PANEL  = 8.f;
	inline constexpr float R_ITEM   = 4.f;

	inline ImDrawList* Draw() { return ImGui::GetWindowDrawList(); }

	inline void RectFilled(ImVec2 a, ImVec2 b, ImU32 col, float r)
	{
		Draw()->AddRectFilled(a, b, col, r);
	}

	inline void RectBorder(ImVec2 a, ImVec2 b, ImU32 col, float r, float th = 1.f)
	{
		Draw()->AddRect(a, b, col, r, ImDrawFlags_RoundCornersAll, th);
	}

	inline void RectGradientV(ImVec2 a, ImVec2 b, ImU32 top, ImU32 bot, float r)
	{
		Draw()->AddRectFilledMultiColor(a, b, top, top, bot, bot);
	}

	inline bool InRect(ImVec2 p, ImVec2 a, ImVec2 b)
	{
		return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y;
	}

	inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

	inline float CtrlWidth(float w) { return (w > 180.f) ? std::min(w * 0.45f, 160.f) : 100.f; }

	inline ImU32 LerpColor(ImU32 from, ImU32 to, float t)
	{
		int a = (from >> 24) & 0xFF, r = (from >> 16) & 0xFF, g = (from >> 8) & 0xFF, b = from & 0xFF;
		int ta = (to >> 24) & 0xFF, tr = (to >> 16) & 0xFF, tg = (to >> 8) & 0xFF, tb = to & 0xFF;
		return IM_COL32(
			(int)Lerp((float)r, (float)tr, t),
			(int)Lerp((float)g, (float)tg, t),
			(int)Lerp((float)b, (float)tb, t),
			(int)Lerp((float)a, (float)ta, t)
		);
	}

	inline int g_activeCombo  = -1;
	inline int g_activeInput  = -1;
	inline int g_inputLen     = 0;
	inline int g_sliderDrag   = -1;
	inline int g_comboHover   = -1;
	inline float g_comboOffsetY = 0.f;
	inline float g_comboOffsetYTarget = 0.f;

	inline void ResetWidgets()
	{
		g_activeCombo = -1;
		g_activeInput = -1;
		g_sliderDrag  = -1;
		g_comboOffsetY = 0.f;
		g_comboOffsetYTarget = 0.f;
	}

	inline float g_toggleAnim[16]{};
	inline int g_toggleCount = 0;

	inline bool Toggle(const char* label, bool* value, ImVec2 pos, float w = 200.f)
	{
		float h  = 18.f;
		float tw = 34.f;
		float cy = pos.y + 1.f;
		ImVec2 ta(pos.x + w - tw, cy);
		ImVec2 tb(pos.x + w, cy + h);

		bool hover = InRect(ImGui::GetMousePos(), ta, tb);
		bool hit   = hover && ImGui::IsMouseClicked(0);
		if (hit) *value = !*value;

		int idx = int(label[0]) % 16;
		float target = *value ? 1.f : 0.f;
		g_toggleAnim[idx] = Lerp(g_toggleAnim[idx], target, ImGui::GetIO().DeltaTime * 12.f);
		if (fabs(g_toggleAnim[idx] - target) < 0.005f) g_toggleAnim[idx] = target;
		float ratio = g_toggleAnim[idx];

		ImU32 trackCol = LerpColor(C_TOGGLE_OFF, C_TOGGLE_ON, ratio);
		RectFilled(ta, tb, trackCol, h * 0.5f);

		float thumbMin = ta.x + 2.f;
		float thumbMax = tb.x - h + 2.f;
		float thumbX = Lerp(thumbMin, thumbMax, ratio);
		ImU32 thumbCol = LerpColor(IM_COL32(160, 165, 180, 225), IM_COL32(10, 10, 14, 255), ratio);
		Draw()->AddCircleFilled(ImVec2(thumbX + h * 0.5f - 1.f, cy + h * 0.5f), h * 0.5f - 2.f, thumbCol);

		Draw()->AddText(ImVec2(pos.x, cy - 1.f), C_TEXT, label);
		return hit;
	}

	inline bool Checkbox(const char* label, bool* value, ImVec2 pos, float w = 200.f)
	{
		float s  = 15.f;
		float cy = pos.y + 1.f;
		ImVec2 ca(pos.x + w - s - 4.f, cy + 2.f);
		ImVec2 cb(ca.x + s, ca.y + s);

		bool hit = InRect(ImGui::GetMousePos(), ca, cb) && ImGui::IsMouseClicked(0);
		if (hit) *value = !*value;

		ImU32 bg = *value ? C_ACCENT : C_FILL;
		RectFilled(ca, cb, bg, 3.f);

		if (*value)
		{
			ImU32 ck = IM_COL32(10, 10, 14, 255);
			Draw()->AddLine(ImVec2(ca.x + 3, ca.y + 8), ImVec2(ca.x + 7, ca.y + 12), ck, 2.f);
			Draw()->AddLine(ImVec2(ca.x + 7, ca.y + 12), ImVec2(ca.x + 12, ca.y + 3), ck, 2.f);
		}
		Draw()->AddText(ImVec2(pos.x, cy - 1.f), C_TEXT, label);
		return hit;
	}

	inline bool Slider(const char* label, float* value, float minV, float maxV,
					   ImVec2 pos, float w = 200.f, const char* fmt = "%.1f")
	{
		float sw = w - 80.f;
		float textY = pos.y;
		float trackY = pos.y + 16.f;
		ImVec2 sa(pos.x, trackY);
		ImVec2 sb(pos.x + sw, trackY + 4.f);
		float ratio = (*value - minV) / (maxV - minV);
		ratio = (ratio < 0.f) ? 0.f : (ratio > 1.f) ? 1.f : ratio;
		float gx = sa.x + ratio * sw;

		int slot = (int)(intptr_t)label;
		bool hover = InRect(ImGui::GetMousePos(), ImVec2(sa.x, sa.y - 4), ImVec2(sb.x, sa.y + 9));
		if (hover && ImGui::IsMouseClicked(0)) g_sliderDrag = slot;
		if (g_sliderDrag == slot && ImGui::IsMouseDown(0))
		{
			float nx = ImGui::GetMousePos().x;
			ratio = (nx - sa.x) / sw;
			ratio = (ratio < 0.f) ? 0.f : (ratio > 1.f) ? 1.f : ratio;
			*value = minV + ratio * (maxV - minV);
		}
		if (g_sliderDrag == slot && ImGui::IsMouseReleased(0)) g_sliderDrag = -1;

		RectFilled(sa, sb, C_FILL, 2.f);
		if (ratio > 0.01f)
			RectFilled(sa, ImVec2(gx, sb.y), C_ACCENT, 2.f);
		float thumbR = 5.f;
		ImU32 dotCol = (g_sliderDrag == slot) ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 215, 230, 240);
		Draw()->AddCircleFilled(ImVec2(gx, sa.y + 2.f), thumbR, dotCol);

		char buf[48];
		snprintf(buf, sizeof(buf), fmt, *value);
		Draw()->AddText(ImVec2(pos.x + sw + 12.f, textY), C_TEXT, buf);
		Draw()->AddText(ImVec2(pos.x, textY), C_TEXT_DIM, label);
		return g_sliderDrag == slot;
	}

	struct ComboData
	{
		bool active = false;
		int idx = -1;
		int* value = nullptr;
		const char** items = nullptr;
		int count = 0;
		ImVec2 pos;
		float w = 0.f;
	};

	inline ComboData g_comboPending = {};

	inline void Combo(const char* label, int* value, const char** items, int count,
					  ImVec2 pos, int idx, float w = 200.f)
	{
		float ctrlW = CtrlWidth(w);
		float cy = pos.y;
		ImVec2 ca(pos.x + w - ctrlW, cy);
		ImVec2 cb(pos.x + w, cy + 22.f);

		float dt = ImGui::GetIO().DeltaTime;
		g_comboOffsetY = Lerp(g_comboOffsetY, g_comboOffsetYTarget, dt * 12.f);
		if (fabs(g_comboOffsetY - g_comboOffsetYTarget) < 0.5f) g_comboOffsetY = g_comboOffsetYTarget;

		bool hover = InRect(ImGui::GetMousePos(), ca, cb);
		if (hover && ImGui::IsMouseClicked(0))
			g_activeCombo = (g_activeCombo == idx) ? -1 : idx;
		ImU32 bg = C_INPUT_BG;
		if (g_activeCombo == idx) bg = IM_COL32(55, 55, 72, 230);
		else if (hover)           bg = IM_COL32(45, 45, 60, 210);
		RectFilled(ca, cb, bg, R_ITEM);
		Draw()->AddText(ImVec2(ca.x + 8, cy + 3), C_TEXT, items[*value]);
		Draw()->AddText(ImVec2(cb.x - 16, cy + 3), C_TEXT_DIM, "v");
		Draw()->AddText(ImVec2(pos.x, cy + 2), C_TEXT, label);

		if (g_activeCombo == idx)
		{
			g_comboPending = { true, idx, value, items, count, pos, w };
			g_comboOffsetYTarget = count * 24.f + 8.f;
		}
		else
			g_comboOffsetYTarget = 0.f;
	}

	inline void RenderComboDropdown()
	{
		if (g_activeCombo < 0 || !g_comboPending.active) return;

		auto& c = g_comboPending;
		float ctrlW = CtrlWidth(c.w);
		ImVec2 cb(c.pos.x + c.w, c.pos.y + 22.f);
		ImVec2 da(c.pos.x + c.w - ctrlW + 2.f, cb.y + 4.f);
		float dh = c.count * 24.f;

		RectFilled(da, ImVec2(cb.x, da.y + dh), IM_COL32(25, 25, 34, 250), R_ITEM);
		RectBorder(da, ImVec2(cb.x, da.y + dh), C_BORDER, R_ITEM, 1.f);

		float ctrlW2 = CtrlWidth(c.w);
		ImVec2 ta(c.pos.x + c.w - ctrlW2, c.pos.y);
		ImVec2 tb(c.pos.x + c.w, c.pos.y + 22.f);
		bool clickOnTrigger = InRect(ImGui::GetMousePos(), ta, tb);
		bool clickOnDropdown = InRect(ImGui::GetMousePos(), da, ImVec2(cb.x, da.y + dh));
		if (ImGui::IsMouseClicked(0) && !clickOnTrigger && !clickOnDropdown)
		{
			g_activeCombo = -1;
			g_comboOffsetYTarget = 0.f;
			return;
		}

		for (int i = 0; i < c.count; i++)
		{
			ImVec2 ia(da.x, da.y + i * 24.f);
			ImVec2 ib(cb.x, ia.y + 22.f);
			bool ih = InRect(ImGui::GetMousePos(), ia, ib);

			if (ih && ImGui::IsMouseClicked(0)) { *c.value = i; g_activeCombo = -1; g_comboOffsetYTarget = 0.f; }

			ImU32 ibg = (i == *c.value) ? C_ACCENT
					  : ih             ? C_HOVER
					                   : IM_COL32(0, 0, 0, 0);
			if (i == *c.value || ih) RectFilled(ia, ib, ibg, 0.f);
			ImU32 tc = (i == *c.value) ? IM_COL32(10, 10, 14, 255) : C_TEXT;
			Draw()->AddText(ImVec2(ia.x + 8, ia.y + 3), tc, c.items[i]);
		}
	}

	inline void Input(const char* label, char* buf, int bufSize, ImVec2 pos, int idx, float w = 200.f)
	{
		float ctrlW = CtrlWidth(w);
		ImVec2 ia(pos.x + w - ctrlW, pos.y);
		ImVec2 ib(pos.x + w, pos.y + 22.f);

		bool hover = InRect(ImGui::GetMousePos(), ia, ib);
		if (hover && ImGui::IsMouseClicked(0))
		{
			g_activeInput = idx;
			g_inputLen = (int)strlen(buf);
		}
		else if (ImGui::IsMouseClicked(0) && !InRect(ImGui::GetMousePos(), ia, ib))
		{
			if (g_activeInput == idx) g_activeInput = -1;
		}

		ImU32 bg = C_INPUT_BG;
		if (g_activeInput == idx) bg = IM_COL32(50, 50, 68, 235);
		else if (hover)           bg = IM_COL32(42, 42, 56, 215);

		if (g_activeInput == idx)
		{
			ImGuiIO& io = ImGui::GetIO();
			for (int n = 0; n < io.InputQueueCharacters.Size; n++)
			{
				ImWchar c = io.InputQueueCharacters[n];
				if (c >= 32 && c < 127 && g_inputLen < bufSize - 1)
				{
					buf[g_inputLen++] = (char)c;
					buf[g_inputLen] = 0;
				}
			}
			io.InputQueueCharacters.resize(0);

			if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && g_inputLen > 0)
				buf[--g_inputLen] = 0;
			if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
				ImGui::IsKeyPressed(ImGuiKey_Escape))
				g_activeInput = -1;
		}

		RectFilled(ia, ib, bg, R_ITEM);
		Draw()->AddText(ImVec2(ia.x + 8, pos.y + 3), (g_activeInput == idx) ? C_ACCENT : C_TEXT, buf);
		if (g_activeInput == idx)
		{
			float tx = ia.x + 10.f + ImGui::CalcTextSize(buf).x;
			Draw()->AddLine(ImVec2(tx, pos.y + 3), ImVec2(tx, pos.y + 19), C_ACCENT, 1.f);
		}
		Draw()->AddText(ImVec2(pos.x, pos.y + 2), C_TEXT, label);
	}
}
