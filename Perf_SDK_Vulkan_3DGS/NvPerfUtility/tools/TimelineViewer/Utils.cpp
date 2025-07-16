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

#include "Utils.h"
#include <cstdlib>
#include <climits>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <limits.h>
#endif // _WIN32

namespace nv { namespace perf { namespace tool {

    void OpenWebpage(const char* pUrl)
    {
    #ifdef _WIN32
        ShellExecuteA(nullptr, nullptr, pUrl, nullptr, nullptr, 0);
    #else
        std::stringstream ss;
        ss << "xdg-open " << pUrl;
        int ret = system(ss.str().c_str());
        (void)ret;
    #endif
    }


    bool CopyFileContents(const std::string& dstPath, const std::string& srcPath)
    {
        std::ifstream src(srcPath, std::ios::binary);
        std::ofstream dst(dstPath, std::ios::binary);

        if (!src.is_open() || !dst.is_open())
        {
            return false;
        }

        dst << src.rdbuf();

        return true;
    }

    std::string FormatTimestamp(uint64_t timestampInNs)
    {
        const uint64_t s = timestampInNs / 1000000000;
        const uint64_t ms = (timestampInNs / 1000000) % 1000;
        const uint64_t us = (timestampInNs / 1000) % 1000;
        const uint64_t ns = timestampInNs % 1000;

        std::ostringstream oss;
        if (s)
        {
            oss << s << "s";
            if (ms > 0 || us > 0 || ns > 0)
            {
                oss << " ";
            }
        }
        if (ms)
        {
            oss << ms << "ms";
            if (us > 0 || ns > 0)
            {
                oss << " ";
            }
        }
        if (us)
        {
            oss << us << "us";
            if (ns > 0)
            {
                oss << " ";
            }
        }
        if (ns)
        {
            oss << ns << "ns";
        }
        return oss.str();
    }

    std::string ExtractFileNameOutOfPath(const std::string& filePath)
    {
        const size_t pos = filePath.find_last_of("/\\");
        if (pos == std::string::npos)
        {
            return filePath;
        }
        return filePath.substr(pos + 1);
    }

    std::string FormatSizeInBytes(size_t bytes)
    {
        std::stringstream ss;
        if (bytes >= 1024ul * 1024 * 1024)
        {
            ss << std::fixed << std::setprecision(2) << (double)bytes / (1024ul * 1024 * 1024) << " GB";
        }
        else if (bytes >= 1024ul * 1024)
        {
            ss << std::fixed << std::setprecision(2) << (double)bytes / (1024ul * 1024) << " MB";
        }
        else if (bytes >= 1024)
        {
            ss << std::fixed << std::setprecision(2) << (double)bytes / 1024 << " KB";
        }
        else
        {
            ss << bytes << " B";
        }
        return ss.str();
    }

    std::string GetCurrentWorkingDirectory()
    {
        std::string cwd;
#ifdef _WIN32
        char buffer[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, buffer))
        {
            cwd = buffer;
        }
#else
        char buffer[PATH_MAX];
        if (getcwd(buffer, sizeof(buffer)) != nullptr)
        {
            cwd = buffer;
        }
#endif
        return cwd;
    }

}}} // namespace nv::perf::tool