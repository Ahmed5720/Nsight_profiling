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

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <NvPerfInit.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <string>

namespace nv { namespace perf { namespace tool {

    struct LogEntry
    {
        LogSeverity severity;
        std::string message;
    };

    // A simple logging manager on top of the nvperf built-in logging system.
    // It supports storing/clearing logs and iterating logs with selected severity levels.
    class LoggingManager
    {
    private:
        std::vector<LogEntry> m_ringBuffer;
        size_t m_head;
        std::atomic<size_t> m_count;
        std::mutex m_mutex;

    private:
        LoggingManager(size_t capacity) 
            : m_ringBuffer(capacity)
            , m_head(0)
            , m_count(0)
        {
        }

    public:
        static size_t proposedCapacity;

        LoggingManager(const LoggingManager&) = delete;
        LoggingManager(LoggingManager&&) = delete;
        LoggingManager& operator=(const LoggingManager&) = delete;
        LoggingManager& operator=(LoggingManager&&) = delete;
        ~LoggingManager() = default;

        static LoggingManager& Instance()
        {
            static LoggingManager s_instance(proposedCapacity);
            return s_instance;
        }

        void AddEntry(LogSeverity severity, const std::string& message)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const size_t index = (m_head + m_count) % m_ringBuffer.size();
            m_ringBuffer[index] = {severity, message};

            if (m_count < m_ringBuffer.size())
            {
                ++m_count;
            }
            else
            {
                m_head = (m_head + 1) % m_ringBuffer.size();
            }
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_head = 0;
            m_count = 0;
        }

        void ForEachEntryUnderLock(const std::function<void(const LogEntry&)>& func)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (size_t ii = 0; ii < m_count; ++ii)
            {
                const size_t index = (m_head + ii) % m_ringBuffer.size();
                func(m_ringBuffer[index]);
            }
        }
    };

    void InitializeNvPerfLogging(size_t capacity, const char* pFilename = nullptr);

} } } // nv::perf::tool