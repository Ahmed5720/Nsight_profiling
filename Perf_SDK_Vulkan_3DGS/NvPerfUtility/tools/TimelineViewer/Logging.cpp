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

#include "Logging.h"
#include <cstring>
#include <iostream>
#include <sstream>

namespace nv { namespace perf { namespace tool {

    size_t LoggingManager::proposedCapacity = 512; // This defines the size of the ring buffer and can be overriden at initialization

    void NvPerfLoggingCallback(const char* pPrefix, const char* pDate, const char* pTime, const char* pFunctionName, const char* pMessage, void* pData)
    {
        const LogSeverity severity = [&]() {
            if (!std::strcmp(pPrefix, "NVPERF|ERR|"))
            {
                return LogSeverity::Err;
            }
            else if (!std::strcmp(pPrefix, "NVPERF|WRN|"))
            {
                return LogSeverity::Wrn;
            }
            else
            {
                return LogSeverity::Inf;
            }
        }();

        const std::string prefix = [&]() {
            if (severity == LogSeverity::Err)
            {
                return "ERR|";
            }
            else if (severity == LogSeverity::Wrn)
            {
                return "WRN|";
            }
            else
            {
                return "INF|";
            }
        }();
        const std::string messageStr = prefix + pTime + "|" + pMessage;
        LoggingManager::Instance().AddEntry(severity, messageStr);
    }

    void InitializeNvPerfLogging(size_t capacity, const char* pFilename)
    {
        LoggingManager::proposedCapacity = capacity;
        SetLogDate(false);
        SetLogTime(true);
        UserLogEnablePlatform(false);
        UserLogEnableStderr(false);
        UserLogEnableCustom(NvPerfLoggingCallback, nullptr);
        if (pFilename)
        {
            SetLogAppendToFile(false);
            UserLogEnableFile(pFilename);
        }
    }

} } } // nv::perf::tool