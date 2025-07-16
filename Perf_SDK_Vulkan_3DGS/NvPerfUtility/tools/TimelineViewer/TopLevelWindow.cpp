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

#include "TopLevelWindow.h"
#include "ImGuiUtils.h"
#include "OutputMessagesPanel.h"
#include "Utils.h"
#include "ViewerTab.h"
#include <NvPerfInit.h>
#include <NvPerfUtilities.h>
#include <IconsFontAwesome6.h>

namespace nv { namespace perf { namespace tool {

    // ===========================================================================================================================================
    //
    // TopLevelWindow::Impl
    //
    // ===========================================================================================================================================

    class TopLevelWindow::Impl
    {
    private:
        std::vector<std::unique_ptr<TabCreateRequest>> m_pendingRequests;
        std::vector<std::unique_ptr<ITab>> m_tabs;
        uint32_t m_activeTabId = (uint32_t)-1;
        std::map<ActivityType, uint32_t> m_activityCounter;
        OutputMessagesPanel m_outputMessagesPanel;

        // We use the term "window" to refer to ImGUI windows that are not dockable. Whereas a "panel" is a dockable window.
        bool m_showOutputMessagesPanel = true;

        bool m_showNewActivityWindow = true;
        bool m_showUserGuideWindow = false;
        bool m_showAboutWindow = false;

        bool m_shouldClose = false;

        struct LoadStates
        {
            bool isLoadingData = false;
            std::string counterDataImagePath;
            LoadFileStatus counterDataImageLoadStatus = LoadFileStatus::NotLoaded;
            std::string metricConfigPath;
            LoadFileStatus metricConfigLoadStatus = LoadFileStatus::NotLoaded;
            std::string metricDisplayConfigPath;
            LoadFileStatus metricDisplayConfigLoadStatus = LoadFileStatus::NotLoaded;
            std::string traceFilePath;
            LoadFileStatus traceLoadStatus = LoadFileStatus::NotLoaded;
            RawData scratchRawData; // scratch space for loading data
        } m_loadStates;

    private:
        ITab* GetCurrentActiveTab();
        void DrawOwnPanelVisibilityMenu();
        void DrawMenuBar();
        void DrawTabsBar();
        void DrawTab();
        void DrawOwnContent();
        void DrawNewActivityWindow();
        void DrawAboutWindow();
        void ProcessPendingWindowOpenRequests();
        void DrawUserGuideWindow();
        void RemoveClosedTabs();
        void ProcessPendingActivityCreateRequests();

    public:
        Impl() = default;
        Impl(const Impl&) = delete;
        Impl(Impl&&) = default;
        Impl& operator=(const Impl&) = delete;
        Impl& operator=(Impl&&) = default;
        ~Impl() = default;

        bool Initialize();
        bool ShouldClose() const { return m_shouldClose; }
        void Shutdown();
        void OnUpdate();
        void OnRender();
    };

    ITab* TopLevelWindow::Impl::GetCurrentActiveTab()
    {
        for (auto& pTab : m_tabs)
        {
            if (pTab->id == m_activeTabId)
            {
                return pTab.get();
            }
        }
        return nullptr;
    }

    void TopLevelWindow::Impl::DrawOwnPanelVisibilityMenu()
    {
        if (ImGui::MenuItem(m_showOutputMessagesPanel ? ICON_FA_CIRCLE_CHECK " Output Messages" : "Output Messages"))
        {
            m_showOutputMessagesPanel = !m_showOutputMessagesPanel;
        }
    }

    void TopLevelWindow::Impl::DrawMenuBar()
    {
        ITab* pCurrentActiveTab = GetCurrentActiveTab();
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        if (BeginMainViewportTopBar("##MainWindowMenuBar", ImGui::GetFrameHeight(), Flags))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("New", nullptr, false))
                    {
                        m_showNewActivityWindow = true;
                    }

                    if (ImGui::MenuItem("Quit", nullptr, false))
                    {
                        m_shouldClose = true;
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::BeginMenu("Windows"))
                    {
                        if (pCurrentActiveTab)
                        {
                            pCurrentActiveTab->DrawPanelVisibilityMenu();
                        }
                        else
                        {
                            DrawOwnPanelVisibilityMenu();
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::MenuItem("Restore Layout"))
                    {
                        ImGui::LoadIniSettingsFromMemory(App::Instance().GetDefaultLayout());
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Help"))
                {
                    if (ImGui::MenuItem("User Guide"))
                    {
                        m_showUserGuideWindow = true;
                    }
                    if (ImGui::MenuItem("About"))
                    {
                        m_showAboutWindow = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
    }

    void TopLevelWindow::Impl::DrawTabsBar()
    {
        // Those controls the size of the tabs, their inner spacing, and the rounding of the tabs.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 30.0f, 5.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{ 10.0f, 10.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 20.0f);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        if (BeginMainViewportTopBar("##MainWindowTabsBar", ImGui::GetFrameHeight(), Flags))
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginTabBar("##TabBar"))
                {
                    for (auto& pTable : m_tabs)
                    {
                        ImGuiTabItemFlags flags = ImGuiTabItemFlags_NoReorder;
                        if (pTable->isDirty)
                        {
                            flags |= ImGuiTabItemFlags_UnsavedDocument;
                        }
                        if (pTable->isNew)
                        {
                            // auto switch to the new tab
                            flags |= ImGuiTabItemFlags_SetSelected;
                            pTable->isNew = false;
                        }

                        ImGui::PushID(&pTable);
                        const bool tabSelected = ImGui::BeginTabItem(pTable->name.c_str(), &pTable->isOpen, flags);
                        if (tabSelected)
                        {
                            if (pTable->id != m_activeTabId)
                            {
                                m_activeTabId = pTable->id;
                            }
                            ImGui::EndTabItem();
                        }
                        ImGui::PopID();
                    }

                    // new tab icon
                    if (ImGui::TabItemButton(ICON_FA_PLUS))
                    {
                        m_showNewActivityWindow = true;
                    }
                    ImGui::EndTabBar();
                }
                ImGui::EndMenuBar();
            }
            ImGui::End();
        }
        ImGui::PopStyleVar(3);
    }

    void TopLevelWindow::Impl::DrawTab()
    {
        ITab* pCurrentActiveTab = GetCurrentActiveTab();
        if (pCurrentActiveTab)
        {
            if (pCurrentActiveTab->isOpen)
            {
                pCurrentActiveTab->DrawMainWindow();
            }
        }
    }

    void TopLevelWindow::Impl::DrawOwnContent()
    {
        // Create a full-screen window for the tab
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        if (m_showOutputMessagesPanel)
        {
            m_outputMessagesPanel.Render(&m_showOutputMessagesPanel);
        }
    }

    void TopLevelWindow::Impl::DrawNewActivityWindow()
    {
        const ImVec2 WindowSize = ImVec2(400 * App::Instance().GetDpiScale(), 250 * App::Instance().GetDpiScale());
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(displaySize.x / 2 - WindowSize.x / 2, displaySize.y / 2 - WindowSize.y / 2), ImGuiCond_Once);
        constexpr auto WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;

        if (!App::Instance().IsNvPerfInitialized())
        {
            ImGui::OpenPopup("Error");
            if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::PushTextWrapPos();
                TextColoredUnformatted("NVIDIA Nsight Perf SDK is not initialized. Please refer to the output messages for more details.", ColorRed);
                ImGui::PopTextWrapPos();
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    m_showNewActivityWindow = false;
                }
                ImGui::EndPopup();
            }
            return;
        }

        if (ImGui::Begin("New Activity", &m_showNewActivityWindow, WindowFlags))
        {
            ImGui::Columns(2);

            ImGui::SetColumnWidth(0, 200.0f * App::Instance().GetDpiScale());
            ImGui::SetColumnWidth(1, 200.0f * App::Instance().GetDpiScale());

            // left column
            {
                ImGui::TextDisabled("Profiler");
                ImGui::Dummy({0.0f, 2.0f});
                ImGui::Dummy({0.0f, 1.0f*ImGui::GetTextLineHeight()});

                ImGui::TextDisabled("Viewer");
                ImGui::Dummy({0.0f, 2.0f});
                if (ImGui::MenuItem(ICON_FA_EYE " View Collected Data"))
                {
                    m_loadStates.isLoadingData = true;
                }
            }

            ImGui::NextColumn();

            // right column
            {
                if (m_loadStates.isLoadingData)
                {
                    auto getLoadStatusString = [](LoadFileStatus status, bool isMandatory) {
                        switch (status)
                        {
                            case LoadFileStatus::NotLoaded: return isMandatory ? "Mandatory" : "Optional";
                            case LoadFileStatus::Failed:    return "Failed";
                            case LoadFileStatus::Loaded:    return "Loaded";
                            default:                        return "Unknown";
                        }
                    };

                    auto getLoadStatusColor = [](LoadFileStatus status, bool isMandatory) {
                        switch (status)
                        {
                            case LoadFileStatus::NotLoaded: return isMandatory ? ColorRed : ColorWhite;
                            case LoadFileStatus::Failed:    return ColorRed;
                            case LoadFileStatus::Loaded:    return ColorGreen;
                            default:                        return ColorRed;
                        }
                    };

                    // counter data image
                    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Counter Data Image"))
                    {
                        m_loadStates.counterDataImageLoadStatus = FileLoader::Instance().SelectAndLoadBinaryFile(m_loadStates.scratchRawData.counterDataImage, m_loadStates.counterDataImagePath);
                    }
                    ImGui::TextUnformatted("    ");
                    ImGui::SameLine();
                    TextColoredUnformatted(getLoadStatusString(m_loadStates.counterDataImageLoadStatus, true), getLoadStatusColor(m_loadStates.counterDataImageLoadStatus, true));

                    // metric config
                    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Metric Config"))
                    {
                        m_loadStates.metricConfigLoadStatus = FileLoader::Instance().SelectAndLoadTextFile(m_loadStates.scratchRawData.metricConfig, m_loadStates.metricConfigPath);
                    }
                    ImGui::TextUnformatted("    ");
                    ImGui::SameLine();
                    TextColoredUnformatted(getLoadStatusString(m_loadStates.metricConfigLoadStatus, true), getLoadStatusColor(m_loadStates.metricConfigLoadStatus, true));

                    // metric display config
                    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Metric Display Config"))
                    {
                        m_loadStates.metricDisplayConfigLoadStatus = FileLoader::Instance().SelectAndLoadTextFile(m_loadStates.scratchRawData.metricDisplayConfig, m_loadStates.metricDisplayConfigPath);
                    }
                    ImGui::TextUnformatted("    ");
                    ImGui::SameLine();
                    TextColoredUnformatted(getLoadStatusString(m_loadStates.metricDisplayConfigLoadStatus, true), getLoadStatusColor(m_loadStates.metricDisplayConfigLoadStatus, true));

                    // trace
                    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Trace"))
                    {
                        m_loadStates.traceLoadStatus = FileLoader::Instance().SelectAndLoadTextFile(m_loadStates.scratchRawData.trace, m_loadStates.traceFilePath);
                    }
                    ImGui::TextUnformatted("    ");
                    ImGui::SameLine();
                    TextColoredUnformatted(getLoadStatusString(m_loadStates.traceLoadStatus, false), getLoadStatusColor(m_loadStates.traceLoadStatus, false));

                    const bool allNecessaryFilesLoaded = (m_loadStates.counterDataImageLoadStatus == LoadFileStatus::Loaded)
                                                       && (m_loadStates.metricConfigLoadStatus == LoadFileStatus::Loaded)
                                                       && (m_loadStates.metricDisplayConfigLoadStatus == LoadFileStatus::Loaded);

                    if (allNecessaryFilesLoaded)
                    {
                        ImGui::Dummy({0.0f, 1.0f*ImGui::GetTextLineHeight()});
                        if (ImGui::Button(ICON_FA_PLAY " Start Processing"))
                        {
                            TimePlotsData timePlotsData;
                            if (InitializeTimePlotsData(m_loadStates.scratchRawData, timePlotsData))
                            {
                                ViewerTabCreateRequest::LoadFilePaths loadFilePaths;
                                loadFilePaths.counterDataImage = m_loadStates.counterDataImagePath;
                                loadFilePaths.metricConfig = m_loadStates.metricConfigPath;
                                loadFilePaths.metricDisplayConfig = m_loadStates.metricDisplayConfigPath;
                                loadFilePaths.trace = m_loadStates.traceFilePath;
                                std::unique_ptr<ViewerTabCreateRequest> pRequest = std::make_unique<ViewerTabCreateRequest>(std::move(loadFilePaths), std::move(timePlotsData));
                                m_pendingRequests.emplace_back(std::move(pRequest));
                                m_showNewActivityWindow = false;
                                m_loadStates.isLoadingData = false;
                                m_loadStates.counterDataImageLoadStatus = LoadFileStatus::NotLoaded;
                                m_loadStates.metricConfigLoadStatus = LoadFileStatus::NotLoaded;
                                m_loadStates.metricDisplayConfigLoadStatus = LoadFileStatus::NotLoaded;
                                m_loadStates.traceLoadStatus = LoadFileStatus::NotLoaded;
                            }
                            else
                            {
                                NV_PERF_LOG_ERR(50, "Failed to initialize time plots data. The input files may be invalid. Please select new files\n");
                                m_loadStates.counterDataImageLoadStatus = LoadFileStatus::Failed;
                                m_loadStates.metricConfigLoadStatus = LoadFileStatus::Failed;
                                m_loadStates.metricDisplayConfigLoadStatus = LoadFileStatus::Failed;
                                // trace data is optional, so if user hasn't loaded any data, it will be odd to display as "failed". Instead we leave it as-is(likely in the "NotLoaded" state)
                                if (m_loadStates.traceLoadStatus == LoadFileStatus::Loaded)
                                {
                                    m_loadStates.traceLoadStatus = LoadFileStatus::Failed;
                                }
                            }
                        }
                    }
                } // if (m_loadStates.isLoadingData)
            } // right column

            ImGui::Columns();

            if (!m_showNewActivityWindow)
            {
                m_loadStates = {};
            }
        }
        ImGui::End();
    }

    void TopLevelWindow::Impl::DrawAboutWindow()
    {
        constexpr float Column0Width = 200.0f;
        constexpr float Column1Width = 100.0f;
        const ImVec2 windowSize = ImVec2((Column0Width + Column1Width) * App::Instance().GetDpiScale(), 100 * App::Instance().GetDpiScale());
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        constexpr auto WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
        if (ImGui::Begin("About", &m_showAboutWindow, WindowFlags))
        {
            ImGui::Dummy({windowSize.x, 0});

            ImGui::Columns(2);

            ImGui::SetColumnWidth(0, Column0Width * App::Instance().GetDpiScale());
            ImGui::SetColumnWidth(1, Column1Width * App::Instance().GetDpiScale());

            ImGui::TextUnformatted("Timeline Viewer");
            ImGui::NextColumn();
            ImGui::TextUnformatted("v0.1");
            ImGui::NextColumn();
            ImGui::Separator();

            ImGui::TextUnformatted("NVIDIA Nsight Perf SDK");
            ImGui::NextColumn();
            if (ImGui::Button(ICON_FA_LINK " Open"))
            {
                OpenWebpage("https://developer.nvidia.com/nsight-perf-sdk");
            }

            ImGui::Columns();
        }
        ImGui::End();
    }

    void TopLevelWindow::Impl::DrawUserGuideWindow()
    {
        constexpr auto WindowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
        if (ImGui::Begin("User Guide", &m_showUserGuideWindow, WindowFlags))
        {
            ImGui::Text("CONTROLS:");
            ImGui::BulletText("Hold ctrl and scroll in the plot area to zoom the view.");
            ImGui::BulletText("Left-click drag within the plot area to pan the view horizontally.");
            ImGui::BulletText("Double left-click to fit all visible data.");
            ImGui::Separator();
            ImGui::Text("TIPS:");
            ImGui::BulletText("If you encounter any error, open the output message window via \"View\", and then \"Output Messages\".");
        }
        ImGui::End();
    }

    void TopLevelWindow::Impl::ProcessPendingWindowOpenRequests()
    {
        // those booleans will be modified internally based on user interaction
        if (m_showNewActivityWindow)
        {
            DrawNewActivityWindow();
        }

        if (m_showUserGuideWindow)
        {
            DrawUserGuideWindow();
        }

        if (m_showAboutWindow)
        {
            DrawAboutWindow();
        }
    }

    void TopLevelWindow::Impl::RemoveClosedTabs()
    {
        for (auto itr = m_tabs.begin(); itr != m_tabs.end();)
        {
            if (!(*itr)->isOpen)
            {
                NV_PERF_LOG_INF(50, "Closing tab: %s\n", (*itr)->name.c_str());
                itr = m_tabs.erase(itr);
            }
            else
            {
                ++itr;
            }
        }
    }

    void TopLevelWindow::Impl::ProcessPendingActivityCreateRequests()
    {
        // process eah request, forward/move the request unique ptr to the tab creation function,
        // erase it from pending requests
        const bool newTabAdded = m_pendingRequests.size();
        for (auto itr = m_pendingRequests.begin(); itr != m_pendingRequests.end();)
        {
            const ActivityType activityType = (*itr)->GetActivityType();
            const uint32_t tabId = m_activityCounter[activityType]++;
            switch (activityType)
            {
            case ActivityType::Viewer:
            {
                ViewerTabCreateRequest* pRequest = dynamic_cast<ViewerTabCreateRequest*>(itr->release());
                const std::string counterDataImageFileName = ExtractFileNameOutOfPath(pRequest->GetLoadFilePaths().counterDataImage);
                const size_t MaxNameLength = 20;
                const std::string tabName = counterDataImageFileName.length() > MaxNameLength ? counterDataImageFileName.substr(0, 20) + "..." : counterDataImageFileName;
                NV_PERF_LOG_INF(50, "Creating a new viewer tab: %s\n", tabName.c_str());
                std::unique_ptr<ViewerTab> pViewerTab = std::make_unique<ViewerTab>(tabName, tabId, std::move(*pRequest));
                m_tabs.emplace_back(std::move(pViewerTab));
                break;
            }
            case ActivityType::Profiler:
            {
                // TODO: support profiler
                // note to prevent overlap, profiler ids shall start at an offset(e.g. (uint32_t)0xFFFF | tabId)
                break;
            }
            default:
                break;
            }
            itr = m_pendingRequests.erase(itr);
        }
    }

    bool TopLevelWindow::Impl::Initialize()
    {
        // If user ini does not exist, load the default ini
        {
            const std::string userIniFilePath = nv::perf::utilities::JoinDriectoryAndFileName(App::Instance().GetCwd(), App::Instance().GetUserIniFileName());
            NV_PERF_LOG_INF(50, "Loading .ini file from %s\n", userIniFilePath.c_str());
            if (!nv::perf::utilities::FileExists(userIniFilePath))
            {
                NV_PERF_LOG_INF(50, "Didn't find the %s. Loading from the default layout.\n", App::Instance().GetUserIniFileName().c_str());
                ImGui::LoadIniSettingsFromMemory(App::Instance().GetDefaultLayout());
            }
        }

        return true;
    }

    void TopLevelWindow::Impl::Shutdown()
    {
        ImGui::SaveIniSettingsToDisk(App::Instance().GetUserIniFileName().c_str());
    }

    void TopLevelWindow::Impl::OnUpdate()
    {
        RemoveClosedTabs();
        ProcessPendingActivityCreateRequests();
    }

    void TopLevelWindow::Impl::OnRender()
    {
        DrawMenuBar();
        DrawTabsBar();
        if (GetCurrentActiveTab())
        {
            DrawTab();
        }
        else
        {
            DrawOwnContent();
        }
        ProcessPendingWindowOpenRequests();
    }

    // ===========================================================================================================================================
    //
    // TopLevelWindow
    //
    // ===========================================================================================================================================

    TopLevelWindow::TopLevelWindow()
        : m_pImpl(std::make_unique<Impl>())
    {
    }

    TopLevelWindow::~TopLevelWindow() = default;

    bool TopLevelWindow::Initialize()
    {
        return m_pImpl->Initialize();
    }

    bool TopLevelWindow::ShouldClose() const
    {
        return m_pImpl->ShouldClose();
    }

    void TopLevelWindow::Shutdown()
    {
        m_pImpl->Shutdown();
    }

    void TopLevelWindow::OnUpdate()
    {
        m_pImpl->OnUpdate();
    }

    void TopLevelWindow::OnRender()
    {
        m_pImpl->OnRender();
    }

}}} // namespace nv::perf::tool
