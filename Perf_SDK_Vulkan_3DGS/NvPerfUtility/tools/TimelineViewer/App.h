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
#include "Logging.h"
#include <string>

struct ImFont;

namespace nv { namespace perf { namespace tool {

    class App
    {
    private:
        const std::string m_userIniFileName = "imgui.ini";
        const std::string m_outputLogFileName = "TimelineViewer.log";

        float m_dpiScale = 1.0f;
        std::string m_cwd = "";
        const char* m_pDefaultLayout = "";
        bool m_nvPerfInitialized = false;

        // fonts
        // Save the pointers to the fonts so we can use ImGui::PushFont()/PopFont() to explicitly select which font to use,
        // although currently FA is merged with the default font(with no overlapping ranges), so we don't need to manipulate
        // the font explciitly.
        // These fonts' lifetime are managed by ImGui, they're auto freed when ImGUI is shutdown.
        ImFont* pDefaultFont = nullptr;
        ImFont* pFAFont = nullptr; // https://github.com/FortAwesome/Font-Awesome

        App() = default;

    public:
        static App& Instance()
        {
            static App s_instance;
            return s_instance;
        }

        App(const App&) = delete;
        App(App&&) = delete;
        App& operator=(const App&) = delete;
        App& operator=(App&&) = delete;
        ~App() = default;

        const std::string& GetUserIniFileName() const { return m_userIniFileName; }
        const std::string& GetLogFileName() const { return m_outputLogFileName; }

        void SetDpiScale(float scale) { m_dpiScale = scale; }
        float GetDpiScale() const { return m_dpiScale; }
        void SetCwd(const std::string& cwd) { m_cwd = cwd; }
        const std::string& GetCwd() const { return m_cwd; }
        void SetDefaultFont(ImFont* pFont) { pDefaultFont = pFont; }
        ImFont* GetDefaultFont() const { return pDefaultFont; }
        void SetFAFont(ImFont* pFont) { pFAFont = pFont; }
        ImFont* GetFAFont() const { return pFAFont; }
        const char* GetDefaultLayout() const { return m_pDefaultLayout; }
        void SetDefaultLayout(const char* pLayout) { m_pDefaultLayout = pLayout; }
        bool IsNvPerfInitialized() const { return m_nvPerfInitialized; }
        void SetNvPerfInitialized() { m_nvPerfInitialized = true; }
    };

}}} // namespace nv::perf::tool