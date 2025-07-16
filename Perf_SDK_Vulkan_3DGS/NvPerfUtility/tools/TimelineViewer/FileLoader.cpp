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

#include "FileLoader.h"
#include "Utils.h"
#include <NvPerfInit.h>
#include <NvPerfUtilities.h>
#include <nfd.h>
#include <chrono>
#include <fstream>

namespace nv { namespace perf { namespace tool {

    static nfdresult_t SelectFile(std::string& lastDirectoryPath, std::string& filePath)
    {
        nfdchar_t* outPath = nullptr;
        nfdresult_t result = NFD_OpenDialog("", lastDirectoryPath.c_str(), &outPath);
        if (result != NFD_OKAY)
        {
            return result;
        }

        filePath = outPath;
        free(outPath);
        lastDirectoryPath = nv::perf::utilities::ExtractDirectoryPathOutOfFilePath(filePath);
        return NFD_OKAY;
    }

    static nfdresult_t SelectFiles(std::string& lastDirectoryPath, std::vector<std::string>& filePaths)
    {
        nfdpathset_t outPaths;
        nfdresult_t result = NFD_OpenDialogMultiple("", lastDirectoryPath.c_str(), &outPaths);
        if (result != NFD_OKAY)
        {
            return result;
        }

        filePaths.clear();
        for (size_t i = 0; i < outPaths.count; i++)
        {
            filePaths.push_back(outPaths.buf + outPaths.indices[i]);
        }

        free(outPaths.buf);
        free(outPaths.indices);
        if (filePaths.size())
        {
            lastDirectoryPath = nv::perf::utilities::ExtractDirectoryPathOutOfFilePath(filePaths.front());
        }
        return NFD_OKAY;
    }

    LoadFileStatus FileLoader::SelectAndLoadBinaryFile(std::vector<uint8_t>& data, std::string& filePath)
    {
        const nfdresult_t result = SelectFile(m_lastDirectoryPath, filePath);
        if (result != NFD_OKAY)
        {
            if (result == NFD_CANCEL)
            {
                return LoadFileStatus::NotLoaded;
            }
            return LoadFileStatus::Failed;
        }

        data.clear();
        NV_PERF_LOG_INF(50, "Loading file %s...\n", filePath.c_str());
        const auto start = std::chrono::high_resolution_clock::now();
        {
            std::ifstream stream(filePath, std::ios::in | std::ios::binary);
            if (stream.fail())
            {
                NV_PERF_LOG_ERR(50, "Failed to open file %s.\n", filePath.c_str());
                return LoadFileStatus::Failed;
            }
            data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }
        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> elapsed = end - start;
        NV_PERF_LOG_INF(50, "File Loaded(size = %s, elapsed time = %.2f ms).\n", FormatSizeInBytes(data.size()).c_str(), elapsed.count());

        return LoadFileStatus::Loaded;
    }

    LoadFileStatus FileLoader::SelectAndLoadBinaryFiles(std::vector<std::vector<uint8_t>>& dataList, std::vector<std::string>& filePaths)
    {
        const nfdresult_t result = SelectFiles(m_lastDirectoryPath, filePaths);
        if (result != NFD_OKAY)
        {
            if (result == NFD_CANCEL)
            {
                return LoadFileStatus::NotLoaded;
            }
            return LoadFileStatus::Failed;
        }

        dataList.clear();
        dataList.reserve(filePaths.size());
        for (const std::string& filePath : filePaths)
        {
            NV_PERF_LOG_INF(50, "Loading file %s...\n", filePath.c_str());
            const auto start = std::chrono::high_resolution_clock::now();
            {
                std::vector<uint8_t> data;
                std::ifstream stream(filePath, std::ios::in | std::ios::binary);
                if (stream.fail())
                {
                    NV_PERF_LOG_ERR(50, "Failed to open file %s.\n", filePath.c_str());
                    return LoadFileStatus::Failed;
                }
                data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
                dataList.emplace_back(std::move(data));
            }
            const auto end = std::chrono::high_resolution_clock::now();
            const std::chrono::duration<double, std::milli> elapsed = end - start;
            NV_PERF_LOG_INF(50, "File Loaded(size = %s, elapsed time = %.2f ms).\n", FormatSizeInBytes(dataList.back().size()).c_str(), elapsed.count());
        }

        return LoadFileStatus::Loaded;
    }

    LoadFileStatus FileLoader::SelectAndLoadTextFile(std::string& text, std::string& filePath)
    {
        const nfdresult_t result = SelectFile(m_lastDirectoryPath, filePath);
        if (result != NFD_OKAY)
        {
            if (result == NFD_CANCEL)
            {
                return LoadFileStatus::NotLoaded;
            }
            return LoadFileStatus::Failed;
        }

        text.clear();
        {
            NV_PERF_LOG_INF(50, "Loading file %s...\n", filePath.c_str());
            std::ifstream stream(filePath, std::ios::in);
            if (stream.fail())
            {
                NV_PERF_LOG_ERR(50, "Failed to open file %s.\n", filePath.c_str());
                return LoadFileStatus::Failed;
            }
            text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            NV_PERF_LOG_INF(50, "File Loaded(size = %s).\n", FormatSizeInBytes(text.size()).c_str());
        }

        return LoadFileStatus::Loaded;
    }

}}} // nv::perf::tool