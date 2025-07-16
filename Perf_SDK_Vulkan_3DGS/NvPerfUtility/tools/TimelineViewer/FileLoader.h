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

#include <stdint.h>
#include <string>
#include <vector>

namespace nv { namespace perf { namespace tool {

    enum class LoadFileStatus
    {
        NotLoaded,
        Failed,
        Loaded,
    };

    class FileLoader
    {
    private:
        std::string m_lastDirectoryPath = ""; // note this is not thread-safe yet

    private:
        FileLoader() = default;

    public:
        static FileLoader& Instance()
        {
            static FileLoader s_instance;
            return s_instance;
        }

        FileLoader(const FileLoader&) = delete;
        FileLoader(FileLoader&&) = delete;
        FileLoader& operator=(const FileLoader&) = delete;
        FileLoader& operator=(FileLoader&&) = delete;
        ~FileLoader() = default;

        LoadFileStatus SelectAndLoadBinaryFile(std::vector<uint8_t>& data, std::string& filePath);
        LoadFileStatus SelectAndLoadBinaryFiles(std::vector<std::vector<uint8_t>>& dataList, std::vector<std::string>& filePaths);
        LoadFileStatus SelectAndLoadTextFile(std::string& text, std::string& filePath);
    };

}}} // namespace nv::perf::tool
