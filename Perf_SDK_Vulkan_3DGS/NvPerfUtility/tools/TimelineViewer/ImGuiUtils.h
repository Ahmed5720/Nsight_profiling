/*
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once
#include <imgui.h>
#include <array>

namespace nv { namespace perf { namespace tool {

    const ImVec4 ColorBlack       = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 ColorWhite       = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 ColorRed         = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    const ImVec4 ColorGreen       = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    const ImVec4 ColorBlue        = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
    const ImVec4 ColorYellow      = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
    const ImVec4 ColorCyan        = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 ColorMagenta     = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
    const ImVec4 ColorOrange      = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
    const ImVec4 ColorPurple      = ImVec4(0.5f, 0.0f, 0.5f, 1.0f);
    const ImVec4 ColorDarkGreen   = ImVec4(0.0f, 0.5f, 0.0f, 1.0f);
    const ImVec4 ColorLightBlue   = ImVec4(0.68f, 0.85f, 0.9f, 1.0f);
    const ImVec4 ColorGrey        = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    const ImVec4 ColorDarkGrey    = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    const ImVec4 ColorLightGrey   = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);

    void ImGuiCustomDarkTheme();

    bool BeginMainViewportTopBar(const char* pLabel, float height, ImGuiWindowFlags flags);

    bool BeginMainViewportToolBar(const char* pLabel, const ImVec2& padding);

    inline ImVec4 ToImVec4(const std::array<float, 4>& color)
    {
        return ImVec4(color[0], color[1], color[2], color[3]);
    }

    inline void TextColoredUnformatted(const char* pText, const ImVec4& color)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(pText);
        ImGui::PopStyleColor();
    }

    void VerticalSeperator();

}}} // namespace nv::perf::tool