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

    enum class ActivityType
    {
        Viewer,
        Profiler
    };

    class TabCreateRequest
    {
    protected:
        ActivityType m_activityType;

    public:
        explicit TabCreateRequest(ActivityType activityType) : m_activityType(activityType) {}
        TabCreateRequest(const TabCreateRequest&) = delete;
        TabCreateRequest(TabCreateRequest&&) = default;
        TabCreateRequest& operator=(const TabCreateRequest&) = delete;
        TabCreateRequest& operator=(TabCreateRequest&&) = default;
        virtual ~TabCreateRequest() = default;

        ActivityType GetActivityType() const { return m_activityType; }
    };

    class ITab
    {
    public:
        ActivityType activityType = ActivityType::Viewer;
        std::string name;
        uint32_t id;

        // states
        bool isNew = true; // will be set to false after the first draw call
        bool isOpen = true;
        bool isDirty = false; // reserved for future use

    public:
        ITab(ActivityType activityType_, const std::string& name_, uint32_t id_)
            : activityType(activityType_)
            , name(name_)
            , id(id_)
        {
        }
        ITab(const ITab&) = delete;
        ITab(ITab&&) = default;
        ITab& operator=(const ITab&) = default;
        ITab& operator=(ITab&&) = delete;
        virtual ~ITab() = default;
        virtual void DrawPanelVisibilityMenu() = 0;
        virtual void DrawMainWindow() = 0;
    };

}}} // namespace nv::perf::tool