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

#include <NvPerfMetricsEvaluator.h>
#include <NvPerfVulkan.h>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace nv { namespace perf { namespace tool {

    extern const std::string HtmlTemplate;
    const char* const DefaultOutputFileName = "MetricsEnumerator";

    struct Options
    {
        enum class Output
        {
            json,
            html,
            csv
        };
        Output output = Output::json;
        std::string chipName;
        std::string outputFileName = DefaultOutputFileName;
    };

    struct MetricAttribute 
    {
        std::string name;
        std::string type;
        std::string description;
        std::string unit;
        std::string category;
        MetricAttribute(const std::string& name, const std::string& type, const std::string& description,
                        const std::string& unit, const std::string& category)
            : name(name), type(type), description(description), unit(unit), category(category) {}
    };

    inline std::string GetMetricMappedHwUnit(const std::string& originHwUnit) 
    {
        static const std::unordered_map<std::string, std::string> HwUnitMap = 
        {
            {"gr",          "GR Engine"},
            {"lts",         "L2 Cache"},
            {"nvlrx",       "NVLINK"},
            {"nvltx",       "NVLINK"},
            {"pcie",        "PCIe"},
            {"raster_zcull","ZCULL"},
        };

        auto it = HwUnitMap.find(originHwUnit);
        if (it != HwUnitMap.end()) 
        {
            return it->second;
        } 
        else 
        {
            std::string upperCategory = originHwUnit;
            std::transform(upperCategory.begin(), upperCategory.end(), upperCategory.begin(), ::toupper);
            return upperCategory;
        }
    }

    inline std::string ChipToArch(const std::string& chipName){
        if(chipName == "TU102" || chipName == "TU104" || chipName == "TU106")
        {
            return "TU10X";
        }
        else if(chipName == "TU116" || chipName == "TU117")
        {
            return "TU11X";
        }
        else if(chipName == "GA102" || chipName == "GA103" || chipName == "GA104" ||
                chipName == "GA106" || chipName == "GA107")
        {
            return "GA10X";
        }
        else if(chipName == "AD102" || chipName == "AD103" || chipName == "AD104" || chipName == "AD106" || chipName == "AD107")
        {
            return "AD10X";
        }
        else if(chipName == "GA10B")
        {
            return chipName;
        }
        else if(chipName == "GB202" || chipName == "GB203" || chipName == "GB205" || chipName == "GB206" || chipName == "GB207")
        {
            return "GB20X";
        }
        return "";
    }

    inline void InitializeJson(const Options& options, const std::vector<MetricAttribute>& allMetrics, nlohmann::ordered_json& jsonRoot)
    {
        jsonRoot["chipName"] = ChipToArch(options.chipName);
        for (const auto& metric : allMetrics) 
        {
            nlohmann::ordered_json metricNode;
            metricNode["name"] = metric.name;
            metricNode["type"] = metric.type;
            metricNode["description"] = metric.description;
            metricNode["dimensional_units"] = metric.unit;
            std::string category = metric.category;
            if (jsonRoot.find(category) == jsonRoot.end()) 
            {
                jsonRoot[category] = nlohmann::ordered_json::array();
            }
            jsonRoot[category].push_back(metricNode);
        }
    }

    bool OutputCSV(const std::vector<MetricAttribute>& allMetrics, const std::string& outputFileName)
    {
        std::string csvPath = outputFileName + ".csv";
        NV_PERF_LOG_INF(10, "Writing a csv report to %s\n", csvPath.c_str());

        std::ofstream csvFile(csvPath);
        if (!csvFile.is_open())
        {
            std::cerr << "Failed to open CSV file: " << csvPath << "\n";
            return false;
        }
        csvFile << "Category,Metric Name,Type,Description,Dimensional Units\n";
        std::map<std::string, std::vector<MetricAttribute>> categorizedMetrics;
        for (size_t i = 0; i < allMetrics.size(); ++i) 
        {
            const MetricAttribute& metric = allMetrics[i];
            categorizedMetrics[metric.category].push_back(metric);
        }
        for (auto it = categorizedMetrics.begin(); it != categorizedMetrics.end(); ++it) 
        {
            const std::string& category = it->first;
            const std::vector<MetricAttribute>& metrics = it->second;
            for (size_t j = 0; j < metrics.size(); ++j) 
            {
                const MetricAttribute& metric = metrics[j];
                csvFile << category << "," << metric.name << "," << metric.type << ","
                        << metric.description << "," << metric.unit << "\n";
            }
        }

        return true;
    }

    bool OutputHTML(const std::string& jsonStr, const std::string& outputFileName)
    {
        std::string htmlPath = outputFileName + ".html";
        std::ofstream html(htmlPath);
        NV_PERF_LOG_INF(10, "Writing a html report to %s\n", htmlPath.c_str());
        if (!html.is_open())
        {
            NV_PERF_LOG_ERR(10, "Failed to open file: %s\n", htmlPath.c_str());
            return false;
        }
        const char* pJsonReplacementMarker = "/***JSON_DATA_HERE***/";
        const size_t insertPoint = HtmlTemplate.find(pJsonReplacementMarker);
        if (insertPoint == std::string::npos)
        {
            NV_PERF_LOG_ERR(10, "Invalid HTML template!\n");
            assert(!"Invalid HTML template!");
            return false;
        }
        html << HtmlTemplate.substr(0, insertPoint);
        html << jsonStr;
        html << HtmlTemplate.substr(insertPoint + strlen(pJsonReplacementMarker));

        return true;
    }

}}} // nv::perf::tool
