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
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace nv { namespace perf { namespace tool {

    using CounterDataImage = std::vector<uint8_t>;

    struct RawData
    {
        CounterDataImage counterDataImage;
        std::string metricConfig;
        std::string metricDisplayConfig;
        std::string trace;
    };

    struct TimestampsData
    {
        std::vector<double> timestamps;

        // derived data for rendering
        std::vector<std::vector<double>> timestampsLodList;
    };

    struct TimePlotData
    {
        enum class PlotType
        {
            Overlay,
            Stacked,
        };

        struct MetricData
        {
            std::string metric;
            std::string name;
            std::string description;
            double maxValue; // nan if not specified
            double multiplier; // 1.0 by default
            std::string unit;
            std::array<float, 4> color; // rgba
            std::vector<double> samples;// one for each timestamp

            // derived data for rendering
            std::vector<double> stackedSamples; // only used in stacked view
            std::vector<std::vector<double>> samplesLodList;
            std::vector<std::vector<double>> stackedSamplesLodList; // only used in stacked view
        };

        std::string name;
        PlotType type;
        std::string xAxesName;
        std::string yAxesName;
        double plotMaxValue = std::nan(""); // max value for all the metrics in the plot
        std::vector<MetricData> metricsData;
    };

    struct TraceData
    {
        struct TraceEvent
        {
            std::string name;
            size_t nestingLevel;
            size_t displayLevel; // this is different from nesting level, it instructs the renderer which line to display the event on
            double startTime;
            double endTime;
            double duration; // in nanoseconds
            std::array<float, 4> color; // rgba
        };

        std::vector<TraceEvent> events; // sorted by nesting level in ascending order; may be empty

        // derived data for rendering
        double firstEventStartTime;
        double lastEventEndTime;
        size_t maximumDisplayLevel;
    };

    struct TimePlotsData
    {
        std::string gpu;
        std::vector<TimePlotData> plots;
        TimestampsData timestampsData;
        TraceData traceData;
        double endTime = 0.0; // Time relative to the start time. Calculated as max(trace, sampling). min(trace, sampling) is always 0.0
        bool isValid = false;
    };

    bool InitializeTimePlotsData(const RawData& rawData, TimePlotsData& data);

    // returning a size_t(-1) to indicate that the original data should be used
    size_t SelectLODLevelForDisplay(const std::vector<std::vector<double>>& lodList, size_t currentNumSamples);

}}} // namespace nv::perf::tool
