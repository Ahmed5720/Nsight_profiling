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
#include "FileLoader.h"
#include <map>
#include <memory>
#include <vector>

namespace nv { namespace perf { namespace tool {

    class TopLevelWindow
    {
    private:
        class Impl;
        std::unique_ptr<Impl> m_pImpl;

    public:
        TopLevelWindow();
        TopLevelWindow(const TopLevelWindow&) = delete;
        TopLevelWindow(TopLevelWindow&&) = default;
        TopLevelWindow& operator=(const TopLevelWindow&) = delete;
        TopLevelWindow& operator=(TopLevelWindow&&) = default;
        ~TopLevelWindow();

        bool Initialize();
        bool ShouldClose() const;
        void Shutdown();
        void OnUpdate();
        void OnRender();
    };

}}} // namespace nv::perf::tool