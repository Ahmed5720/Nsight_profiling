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

#include "App.h"
#include "Data.h"
#include "ITab.h"
#include <memory>

namespace nv { namespace perf { namespace tool {

    class ViewerTabCreateRequest : public TabCreateRequest
    {
    public:
        struct LoadFilePaths
        {
            std::string counterDataImage;
            std::string metricConfig;
            std::string metricDisplayConfig;
            std::string trace;
        };

    private:
        LoadFilePaths m_loadFilePaths;
        TimePlotsData m_timePlotsData;

    public:
        ViewerTabCreateRequest(LoadFilePaths && loadFilePaths, TimePlotsData && timePlotsData)
            : TabCreateRequest(ActivityType::Viewer)
            , m_loadFilePaths(std::move(loadFilePaths))
            , m_timePlotsData(std::move(timePlotsData))
        {
        }
        ViewerTabCreateRequest(const ViewerTabCreateRequest&) = delete;
        ViewerTabCreateRequest(ViewerTabCreateRequest&&) = default;
        ViewerTabCreateRequest& operator=(const ViewerTabCreateRequest&) = delete;
        ViewerTabCreateRequest& operator=(ViewerTabCreateRequest&&) = default;
        virtual ~ViewerTabCreateRequest() = default;

        LoadFilePaths& GetLoadFilePaths() { return m_loadFilePaths; }
        TimePlotsData& GetTimePlotsData() { return m_timePlotsData; }
    };

    class ViewerTab : public ITab
    {
    private:
        class Impl;
        std::unique_ptr<Impl> m_pImpl;

    public:
        ViewerTab(const std::string& name, uint32_t id, ViewerTabCreateRequest&& request);
        ViewerTab(const ViewerTab&) = delete;
        ViewerTab(ViewerTab&&) = default;
        ViewerTab& operator=(const ViewerTab&) = delete;
        ViewerTab& operator=(ViewerTab&&) = default;
        virtual ~ViewerTab();
        virtual void DrawPanelVisibilityMenu() override;
        virtual void DrawMainWindow() override;
    };

}}} // namespace nv::perf::tool