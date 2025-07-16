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

#include "ImGuiUtils.h"
#include "Logging.h"
#include <NvPerfInit.h>
#include <imgui.h>
#include <IconsFontAwesome6.h>

namespace nv { namespace perf { namespace tool {

    class OutputMessagesPanel
    {
    private:
        bool m_showInfo = true;
        bool m_showWarning = true;
        bool m_showError = true;

    public:
        OutputMessagesPanel() = default;
        OutputMessagesPanel(const OutputMessagesPanel&) = delete;
        OutputMessagesPanel(OutputMessagesPanel&&) = default;
        OutputMessagesPanel& operator=(const OutputMessagesPanel&) = delete;
        OutputMessagesPanel& operator=(OutputMessagesPanel&&) = default;
        ~OutputMessagesPanel() = default;

        void Render(bool* pOpen)
        {
            if (ImGui::Begin("Output Messages", pOpen))
            {
                bool copy = false;
                if (ImGui::Button(ICON_FA_COPY))
                {
                    copy = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Copy to Clipboard");
                }

                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_TRASH))
                {
                    LoggingManager::Instance().Clear();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Clear All Messages");
                }

                ImGui::SameLine();
                ImGui::Checkbox("INF", &m_showInfo);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Show Info Messages");
                }

                ImGui::SameLine();
                ImGui::Checkbox("WRN", &m_showWarning);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Show Warning Messages");
                }

                ImGui::SameLine();
                ImGui::Checkbox("ERR", &m_showError);
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Show Error Messages");
                }

                if (ImGui::BeginChild("ScrollingOutputMessages", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                    if (copy)
                    {
                        ImGui::LogToClipboard();
                    }
                    LoggingManager::Instance().ForEachEntryUnderLock([&](const LogEntry& entry) {
                        if ((entry.severity == LogSeverity::Err) && m_showError)
                        {
                            TextColoredUnformatted(entry.message.c_str(), ColorRed);
                        }
                        else if ((entry.severity == LogSeverity::Wrn) && m_showWarning)
                        {
                            TextColoredUnformatted(entry.message.c_str(), ColorYellow);
                        }
                        else if ((entry.severity == LogSeverity::Inf) && m_showInfo)
                        {
                            ImGui::TextUnformatted(entry.message.c_str());
                        }
                    });
                    if (copy)
                    {
                        ImGui::LogFinish();
                    }
                    ImGui::PopStyleVar();

                    // auto scroll(keep at the bottom)
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    {
                        ImGui::SetScrollHereY(1.0f);
                    }
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
    };

} } } // nv::perf::tool