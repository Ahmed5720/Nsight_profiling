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

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Data.h"
#include <NvPerfCounterData.h>
#include <NvPerfHudDataModel.h>
#include <NvPerfPeriodicSamplerCommon.h>
#include <NvPerfMetricsEvaluator.h>
#include <NvPerfMiniTrace.h>
#include <nvperf_target.h>
#include <cmath>
#include <chrono>

// Note:
//   Do this in exactly one source file to add rapidyaml's symbols.
//   If Windows.h is included before ryml_all.hpp, it needs to be included with NOMINMAX defined.
//   Otherwise min/max-related errors occur.
#define RYML_SINGLE_HDR_DEFINE_NOW
#include <ryml_all.hpp>

namespace nv { namespace perf { namespace tool {

    static constexpr size_t MinNumSamples = 3; // 1 for the first sample, 1 for the last sample, and at least 1 in between.
    static constexpr size_t LODBaseCapacity = 256; // The displayed # of samples ranges from [LODBaseCapacity / 2, LODBaseCapacity]

    template <typename T>
    inline T RandomRange(T min, T max)
    {
        const T scale = rand() / (T)RAND_MAX;
        return min + scale * (max - min);
    }

    // the expected input string is like "#0057E7"
    std::array<float, 4> ColorHexStrToRGBA(const std::string& hexColor)
    {
        uint32_t rgb = 0;
        {
            std::stringstream ss;
            ss << std::hex << hexColor.substr(1);
            ss >> rgb;
        }

        std::array<float, 4> rgba = {};
        rgba[0] = ((rgb >> 16) & 0xFF) / 255.0f;
        rgba[1] = ((rgb >> 8) & 0xFF) / 255.0f;
        rgba[2] = (rgb & 0xFF) / 255.0f;
        rgba[3] = 1.0f;
        return rgba;
    }

    std::array<float, 4> RgbaToNormalizedFloatArray(uint32_t rgba)
    {
        std::array<float, 4> result;
        result[0] = ((rgba >> 24) & 0xFF) / 255.0f;
        result[1] = ((rgba >> 16) & 0xFF) / 255.0f;
        result[2] = ((rgba >> 8) & 0xFF)  / 255.0f;
        result[3] = (rgba & 0xFF)         / 255.0f;
        return result;
    }

    // returning a (size_t)-1 to indicate that the original data should be used
    size_t SelectLODLevelForDisplay(const std::vector<std::vector<double>>& lodList, size_t currentNumSamples)
    {
        if (currentNumSamples <= LODBaseCapacity)
        {
            return size_t(-1);
        }

        const size_t lod = static_cast<size_t>(std::ceil(std::log2(static_cast<double>(currentNumSamples) / LODBaseCapacity)));
        const size_t safeLod = std::min(static_cast<size_t>(lod), lodList.size()) - 1;
        return safeLod;
    }

    static bool GetRawTimestamps(const std::vector<uint8_t>& counterDataImage, std::vector<uint64_t>& rawTimestamps)
    {
        const size_t numSamples = CounterDataGetNumRanges(counterDataImage.data());
        if (numSamples < MinNumSamples)
        {
            NV_PERF_LOG_ERR(10, "Counter data image has too few samples. Expected at minimum: %llu, actual: %llu\n", MinNumSamples, numSamples);
            return false;
        }

        // collecting sample [1, numSamples - 1]
        rawTimestamps.resize(numSamples - 2);
        for (size_t ii = 0; ii < numSamples - 2; ++ii)
        {
            const size_t counterDataRangeIndex = ii + 1;
            sampler::SampleTimestamp timestamp = {};
            if (!sampler::CounterDataGetSampleTime(counterDataImage.data(), counterDataRangeIndex, timestamp))
            {
                return false;
            }
            rawTimestamps[ii] = timestamp.end;

            if (ii > 0 && (rawTimestamps[ii - 1] > rawTimestamps[ii]))
            {
                NV_PERF_LOG_ERR(
                    10,
                    "Invalid timestamps found in counter data image. A prior timestamp is larger than a succeeding timestamp. Prior timestamp: %llu, succeeding timestamp: %llu\n",
                    rawTimestamps[ii - 1],
                    rawTimestamps[ii]);
                return false;
            }
        }

        return true;
    }

    static void InitializeTimestamps(const std::vector<uint64_t>& rawTimestamps, uint64_t baseTimestamp, TimePlotsData& data)
    {
        data.timestampsData.timestamps.reserve(rawTimestamps.size());
        for (uint64_t rawTimestamp : rawTimestamps)
        {
            data.timestampsData.timestamps.push_back(static_cast<double>(rawTimestamp - baseTimestamp));
        }

        // Generate LODs
        // Always include the first and the last sample, so it will not have a "hole" at both sides of the timeline.
        // Example:
        //  Input:
        //      [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], where LODBaseCapacity is 5
        //  Output:
        //      LOD 0: [0, 2, 4, 6, 8, 9]
        //      LOD 1: [0, 4, 8, 9]
        if (data.timestampsData.timestamps.size() > LODBaseCapacity)
        {
            static_assert(LODBaseCapacity >= 3, "The following code assumes LODBaseCapacity is at least 3.");
            auto aggregateTimestamps = [](const std::vector<double>& samples) {
                std::vector<double> aggregated;
                aggregated.push_back(samples[0]);
                for (size_t ii = 1; ii < samples.size() - 2; ii += 2)
                {
                    aggregated.push_back(samples[ii + 1]);
                }
                aggregated.push_back(samples.back());
                assert(aggregated.size() == 2 + (samples.size() - 2) / 2);
                return aggregated;
            };

            std::vector<std::vector<double>>& lodList = data.timestampsData.timestampsLodList;
            std::vector<double>* pCurrentLevel = &data.timestampsData.timestamps;
            while (pCurrentLevel->size() > LODBaseCapacity)
            {
                std::vector<double> nextLevel = aggregateTimestamps(*pCurrentLevel);
                lodList.emplace_back(std::move(nextLevel));
                pCurrentLevel = &lodList.back();
            }
        }
    }

    static const char* GetChipNameFromCounterData(const std::vector<uint8_t>& counterDataImage)
    {
        NVPW_CounterData_GetChipName_Params getChipNameParams{ NVPW_CounterData_GetChipName_Params_STRUCT_SIZE };
        getChipNameParams.pCounterDataImage = counterDataImage.data();
        getChipNameParams.counterDataImageSize = counterDataImage.size();
        NVPA_Status nvpaStatus = NVPW_CounterData_GetChipName(&getChipNameParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Failed to get chip name from the input counter data image. Is this counter data image valid?\n");
            return nullptr;
        }
        return getChipNameParams.pChipName;
    }

    // metricSamples & multipliers should be in the same order as metricEvalRequests
    static bool GetMetricValues(
        const std::vector<uint8_t>& counterDataImage,
        size_t firstValidRangeIndex,
        size_t lastValidRangeIndex,
        MetricsEvaluator& metricsEvaluator,
        const std::vector<NVPW_MetricEvalRequest>& metricEvalRequests,
        const std::vector<double>& multipliers,
        std::vector<std::vector<double>*>& metricsSamples)
    {
        const size_t numSamples = lastValidRangeIndex - firstValidRangeIndex + 1;
        for (size_t metricIdx = 0; metricIdx < metricEvalRequests.size(); ++metricIdx)
        {
            metricsSamples[metricIdx]->reserve(numSamples);
        }

        std::vector<double> metricValuesScratchBuffer(metricEvalRequests.size());
        for (size_t rangeIndex = firstValidRangeIndex; rangeIndex <= lastValidRangeIndex; ++rangeIndex)
        {
            if (!EvaluateToGpuValues(
                    metricsEvaluator,
                    counterDataImage.data(),
                    counterDataImage.size(),
                    rangeIndex,
                    metricEvalRequests.size(),
                    metricEvalRequests.data(),
                    metricValuesScratchBuffer.data()))
            {
                return false;
            }

            for (size_t metricIdx = 0; metricIdx < metricValuesScratchBuffer.size(); ++metricIdx)
            {
                metricsSamples[metricIdx]->push_back(metricValuesScratchBuffer[metricIdx] * multipliers[metricIdx]);
            }
        }
        return true;
    }

    // The aggregation algorithm should be identical to the one in InitializeTimestamps(), so that the timestamps & metric values are aligned.
    static std::vector<uint8_t> GenerateLODCounterData(
        const std::vector<uint8_t>& counterDataPrefix,
        const std::vector<uint8_t>& inputCounterDataImage,
        size_t firstValidRangeIndex,
        size_t lastValidRangeIndex)
    {
        const size_t numValidInputSamples = lastValidRangeIndex - firstValidRangeIndex + 1;
        if (numValidInputSamples <= 3)
        {
            return {}; // should never call into this
        }

        const size_t numOutputSamples = 2 + (numValidInputSamples - 2) / 2;

        CounterDataCombiner combiner;
        {
            if (!combiner.Initialize(counterDataPrefix.data(), counterDataPrefix.size(), (uint32_t)numOutputSamples, inputCounterDataImage.data()))
            {
                return {};
            }

            for (size_t rangeIndex = 0; rangeIndex < numOutputSamples; ++rangeIndex)
            {
                if (!combiner.CreateRange(rangeIndex))
                {
                    return {};
                }
            }
        }

        // always include the first and the last sample
        size_t rangeIndexDst = 0;
        if (!combiner.SumIntoRange(rangeIndexDst, inputCounterDataImage.data(), firstValidRangeIndex))
        {
            return {};
        }

        for (size_t ii = firstValidRangeIndex + 1; ii < lastValidRangeIndex - 1; ii += 2)
        {
            ++rangeIndexDst;
            if (  !combiner.SumIntoRange(rangeIndexDst, inputCounterDataImage.data(), ii)
            || !combiner.SumIntoRange(rangeIndexDst, inputCounterDataImage.data(), ii + 1))
            {
                return {};
            }
        }

        if (!combiner.SumIntoRange(++rangeIndexDst, inputCounterDataImage.data(), lastValidRangeIndex))
        {
            return {};
        }

        const std::vector<uint8_t>& outputCounterData = combiner.GetCounterData();
        assert(CounterDataGetNumRanges(outputCounterData.data()) == numOutputSamples);

        return outputCounterData;
    }

    static bool InitializePlotData(MetricsEvaluator& metricsEvaluator, const std::vector<uint8_t>& counterDataPrefix, const std::vector<uint8_t>& counterDataImage, TimePlotData& plot)
    {
        if (plot.metricsData.empty())
        {
            NV_PERF_LOG_ERR(10, "No metric found in plot %s.\n", plot.name.c_str());
            return false;
        }

        const size_t numSamples = CounterDataGetNumRanges(counterDataImage.data());
        if (numSamples < MinNumSamples)
        {
            NV_PERF_LOG_ERR(10, "Counter data image has too few samples. Expected at minimum: %llu, actual: %llu\n", MinNumSamples, numSamples);
            return false;
        }

        const size_t firstValidRangeIndex = 1;
        const size_t lastValidRangeIndex = numSamples - 2; // collecting samples [1, numSamples - 1]
        std::vector<NVPW_MetricEvalRequest> requests;
        std::vector<double> multipliers;
        {
            std::vector<std::vector<double>*> metricsSamples;
            for (TimePlotData::MetricData& metricData : plot.metricsData)
            {
                NVPW_MetricEvalRequest request{};
                if (!ToMetricEvalRequest(metricsEvaluator, metricData.metric.c_str(), request))
                {
                    NV_PERF_LOG_ERR(50, "Metric %s is not recognized.\n", metricData.metric.c_str());
                    return false;
                }
                requests.emplace_back(std::move(request));
                multipliers.push_back(metricData.multiplier);
                metricsSamples.push_back(&metricData.samples);
            }

            if (!GetMetricValues(counterDataImage, firstValidRangeIndex, lastValidRangeIndex, metricsEvaluator, requests, multipliers, metricsSamples))
            {
                return false;
            }
        }

        // generate LOD
        {
            size_t numSamplesCurrentLevel = plot.metricsData.front().samples.size();
            std::vector<uint8_t> counterDataCurrentLevel = counterDataImage;
            size_t firstValidRangeIndexCurrentLevel = firstValidRangeIndex;
            size_t lastValidRangeIndexCurrentLevel = lastValidRangeIndex;
            while (numSamplesCurrentLevel > LODBaseCapacity)
            {
                std::vector<uint8_t> counterDataNextLevel = GenerateLODCounterData(counterDataPrefix, counterDataCurrentLevel, firstValidRangeIndexCurrentLevel, lastValidRangeIndexCurrentLevel);
                if (counterDataNextLevel.empty())
                {
                    return false;
                }

                const size_t numSamplesNextLevel = CounterDataGetNumRanges(counterDataNextLevel.data());
                if (!numSamplesNextLevel)
                {
                    return false;
                }

                numSamplesCurrentLevel = numSamplesNextLevel;
                counterDataCurrentLevel = std::move(counterDataNextLevel);
                firstValidRangeIndexCurrentLevel = 0; // we only filter the first and the last sample in the top level counter data, otherwise all samples are valid
                lastValidRangeIndexCurrentLevel = numSamplesNextLevel - 1;

                std::vector<std::vector<double>*> metricsSamples;
                for (TimePlotData::MetricData& metricData : plot.metricsData)
                {
                    metricData.samplesLodList.emplace_back();
                    metricsSamples.push_back(&metricData.samplesLodList.back());
                }
                if (!GetMetricValues(counterDataCurrentLevel, firstValidRangeIndexCurrentLevel, lastValidRangeIndexCurrentLevel, metricsEvaluator, requests, multipliers, metricsSamples))
                {
                    return false;
                }
            }
        }

        // calculate stacked metric values
        if (plot.type == TimePlotData::PlotType::Stacked)
        {
            for (TimePlotData::MetricData& metricData : plot.metricsData)
            {
                metricData.stackedSamples.resize(metricData.samples.size()); // stack sample is 1:1 to a sample,
                metricData.stackedSamplesLodList.resize(metricData.samplesLodList.size());
                for (size_t lodListIdx = 0; lodListIdx < metricData.samplesLodList.size(); ++lodListIdx)
                {
                    metricData.stackedSamplesLodList[lodListIdx].resize(metricData.samplesLodList[lodListIdx].size());
                }
            }

            const size_t numMetrics = plot.metricsData.size();
            const size_t numSamples = plot.metricsData.front().samples.size();
            for (size_t sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
            {
                double accumulated = 0.0;
                for (size_t metricIdx = 0; metricIdx < numMetrics; ++metricIdx)
                {
                    accumulated += plot.metricsData[metricIdx].samples[sampleIdx];
                    plot.metricsData[metricIdx].stackedSamples[sampleIdx] = accumulated;
                }
            }

            // LODs for stacked samples
            const size_t numLodLevels = plot.metricsData.front().samplesLodList.size();
            for (size_t lodListIdx = 0; lodListIdx < numLodLevels; ++lodListIdx)
            {
                const size_t numSamplesLod = plot.metricsData.front().samplesLodList[lodListIdx].size();
                for (size_t sampleIdx = 0; sampleIdx < numSamplesLod; ++sampleIdx)
                {
                    double accumulated = 0.0;
                    for (size_t metricIdx = 0; metricIdx < numMetrics; ++metricIdx)
                    {
                        accumulated += plot.metricsData[metricIdx].samplesLodList[lodListIdx][sampleIdx];
                        plot.metricsData[metricIdx].stackedSamplesLodList[lodListIdx][sampleIdx] = accumulated;
                    }
                }
            }
        }

        // set plotMaxValue based on the maximum value of all samples if it's not specified
        if (std::isnan(plot.plotMaxValue))
        {
            for (const TimePlotData::MetricData& metricData : plot.metricsData)
            {
                // original resolution
                const std::vector<double>* const pSamples = (plot.type == TimePlotData::PlotType::Stacked) ? &metricData.stackedSamples : &metricData.samples;
                for (double sample : *pSamples)
                {
                    if (!std::isnan(sample))
                    {
                        plot.plotMaxValue = std::isnan(plot.plotMaxValue) ? sample : std::max(plot.plotMaxValue, sample);
                    }
                }

                // LODs
                const size_t numLodLevels = plot.metricsData.front().samplesLodList.size();
                for (size_t lodListIdx = 0; lodListIdx < numLodLevels; ++lodListIdx)
                {
                    const std::vector<double>* const pSamplesLod = (plot.type == TimePlotData::PlotType::Stacked) ? &metricData.stackedSamplesLodList[lodListIdx] : &metricData.samplesLodList[lodListIdx];
                    for (double sample : *pSamplesLod)
                    {
                        if (!std::isnan(sample))
                        {
                            plot.plotMaxValue = std::isnan(plot.plotMaxValue) ? sample : std::max(plot.plotMaxValue, sample);
                        }
                    }
                }
            }
        }

        return true;
    }

    static void InitializeTraceData(const std::vector<mini_trace::APITraceData>& rawTraceEvents, uint64_t startTime, TimePlotsData& data)
    {
        TraceData& traceData = data.traceData;
        std::vector<TraceData::TraceEvent>& traceEvents = traceData.events;
        traceEvents.reserve(rawTraceEvents.size());
        for (const auto& rawEvent : rawTraceEvents)
        {
            TraceData::TraceEvent event;
            event.name = rawEvent.name;
            event.nestingLevel = rawEvent.nestingLevel;
            event.startTime = static_cast<double>(rawEvent.startTimestamp - startTime);
            event.endTime = static_cast<double>(rawEvent.endTimestamp - startTime);
            event.duration = event.endTime - event.startTime;
            switch (event.nestingLevel)
            {
            case 0u:
                event.color = ColorHexStrToRGBA("#78d860"); // https://imagecolorpicker.com/
                break;
            case 1u:
                event.color = ColorHexStrToRGBA("#eab676");
                break;
            case 2u:
                event.color = ColorHexStrToRGBA("#377eb8");
                break;
            case 3u:
                event.color = ColorHexStrToRGBA("#505050");
                break;
            default:
                event.color = ColorHexStrToRGBA("#ADD8E6");
                break;
            }
            traceEvents.emplace_back(std::move(event));
        }

        std::sort(traceEvents.begin(), traceEvents.end(), [](const TraceData::TraceEvent& lhs, const TraceData::TraceEvent& rhs) {
            if (lhs.nestingLevel != rhs.nestingLevel)
            {
                return lhs.nestingLevel < rhs.nestingLevel;
            }
            return lhs.startTime < rhs.startTime;
        });

        // Determine the display level
        // The simple algorithm considers both the nesting level and the overlap of the events within the same nesting level.
        // It works as follows:
        //     Presumed that events are already sorted by nesting level, and then start time, in ascending order.
        //     Iterate each event:
        //        If the event is a deeper nesting level compared with its predecessor, increment the line number(start a new line), and assign the line number to this event.
        //        Otherwise, look for if any open slots of the display lines belong to this nesting level can fit this event.
        //            If found, assign the line number to this event and extend the end time.
        //            Otherwise, increment the line number(start a new line), and assign the line number to this event.
        {
            using DisplayLevelEndTimeMap = std::map<size_t, double>; // { displayLevel, endTime }
            std::map<size_t, DisplayLevelEndTimeMap> lastEndTimePerLevel; // { nestingLevel, DisplayLevelEndTimeMap } tracks the last end time for events at each display level for each nesting level.
            size_t currentDisplayLevel = 0;
            for (TraceData::TraceEvent& event : traceEvents)
            {
                bool placed = false;
                // Check if we can place the event in any of the existing lines without overlap.
                auto& displayLevelEndTimeMap = lastEndTimePerLevel[event.nestingLevel];
                for (const auto& pair : displayLevelEndTimeMap)
                {
                    const size_t displayLevel = pair.first;
                    const double endTime = pair.second;
                    if (endTime <= event.startTime)
                    {
                        // We found a slot for this event!
                        event.displayLevel = displayLevel;
                        displayLevelEndTimeMap[event.displayLevel] = event.endTime; // extend the end time
                        placed = true;
                        break;
                    }
                }

                // No slot found, start a new line. 
                if (!placed)
                {
                    ++currentDisplayLevel;
                    event.displayLevel = currentDisplayLevel;
                    displayLevelEndTimeMap[currentDisplayLevel] = event.endTime;
                }
            }
        }

        traceData.firstEventStartTime = std::numeric_limits<double>::max();
        traceData.lastEventEndTime = std::numeric_limits<double>::min();
        traceData.maximumDisplayLevel = 0;
        for (const auto& event : traceEvents)
        {
            traceData.firstEventStartTime = std::min(traceData.firstEventStartTime, event.startTime);
            traceData.lastEventEndTime = std::max(traceData.lastEventEndTime, event.endTime);
            traceData.maximumDisplayLevel = std::max(traceData.maximumDisplayLevel, event.displayLevel);
        }
    }

    bool LoadMetricDisplayConfiguration(const char* pChipName, const RawData& rawData, const MetricConfigObject& metricConfigObject, TimePlotsData& data)
    {
        hud::HudPresets hudPresets;
        constexpr bool LoadPredefinedPresets = false;
        if (!hudPresets.Initialize(pChipName, LoadPredefinedPresets)) // we only use the config loaded from input file
        {
            NV_PERF_LOG_ERR(50, "Failed to initialize HudPresets.\n");
            return false;
        }
        if (!hudPresets.LoadFromString(rawData.metricDisplayConfig.c_str()))
        {
            NV_PERF_LOG_ERR(50, "Failed to parse the input metric display config.\n");
            return false;
        }
        const std::vector<hud::HudPreset>& presets = hudPresets.GetPresets();
        if (presets.empty())
        {
            NV_PERF_LOG_ERR(50, "No preset loaded.\n");
            return false;
        }

        if (presets.size() > 1)
        {
            NV_PERF_LOG_WRN(50, "Multiple presets are specified in the config, will use the first one.\n");
        }
        const hud::HudPreset& hudPreset = presets.front();
        NV_PERF_LOG_INF(50, "Using preset \"%s\".\n", presets.front().name.c_str());

        hud::HudDataModel hudDataModel;
        if (!hudDataModel.Load(hudPreset))
        {
            NV_PERF_LOG_ERR(50, "Failed to load the metric display config.\n");
            return false;
        }

        constexpr size_t SamplingFrequency = 10; // required by the API, but unused in this context
        constexpr double PlotTimeWidthInSeconds = 1.0; // required by the API, but unused in this context
        if (!hudDataModel.Initialize(1.0 / (double)SamplingFrequency, PlotTimeWidthInSeconds, metricConfigObject))
        {
            NV_PERF_LOG_ERR(50, "Failed to initialize hudDataModel.\n");
            return false;
        }

        //hudDataModel.Print(std::cout);

        const std::vector<hud::HudConfiguration>& hudConfigurations = hudDataModel.GetConfigurations();
        if (hudConfigurations.size() != 1)
        {
            NV_PERF_LOG_ERR(50, "Unexpected number of hud configurations. Expected: %llu, actual: %llu\n", 1, hudConfigurations.size());
            return false;
        }

        const hud::HudConfiguration& hudConfiguration = hudConfigurations.front();
        for (const auto& panel : hudConfiguration.panels)
        {
            if (panel.widgets.size() != 1)
            {
                NV_PERF_LOG_ERR(50, "Only supports a single widget per each panel.\n");
                return false;
            }
            const auto& pWidget = panel.widgets.front();
            if (pWidget->type != hud::Widget::Type::TimePlot)
            {
                NV_PERF_LOG_ERR(50, "The only supported widget type is \"TimePlot\".\n");
                return false;
            }

            TimePlotData timePlotData;
            const hud::TimePlot& inputTimePlot = *static_cast<hud::TimePlot*>(pWidget.get());
            timePlotData.name = inputTimePlot.label.text.empty() ? panel.name : inputTimePlot.label.text;
            if (inputTimePlot.chartType == hud::TimePlot::ChartType::Stacked)
            {
                timePlotData.type = TimePlotData::PlotType::Stacked;
            }
            else if (inputTimePlot.chartType == hud::TimePlot::ChartType::Overlay)
            {
                timePlotData.type = TimePlotData::PlotType::Overlay;
            }
            timePlotData.xAxesName = "ns";
            timePlotData.yAxesName = (inputTimePlot.unit != hud::MetricSignal::HideUnit()) ? inputTimePlot.unit : "";
            timePlotData.plotMaxValue = inputTimePlot.valueMax;
            timePlotData.metricsData.reserve(inputTimePlot.signals.size());
            double plotMaxValue = std::nan("");
            for (const hud::MetricSignal& signal : inputTimePlot.signals)
            {
                TimePlotData::MetricData metricData;
                metricData.metric = signal.metric;
                metricData.name = signal.label.text;
                metricData.description = signal.description;
                metricData.maxValue = signal.maxValue;
                if (!std::isnan(signal.maxValue))
                {
                    plotMaxValue = std::isnan(plotMaxValue) ? signal.maxValue : std::max(plotMaxValue, signal.maxValue);
                }
                metricData.multiplier = std::isnan(signal.multiplier) ? 1.0 : signal.multiplier; 
                metricData.unit = (signal.unit != hud::MetricSignal::HideUnit()) ? signal.unit : "";
                metricData.color = RgbaToNormalizedFloatArray(signal.color.Rgba());
                timePlotData.metricsData.emplace_back(std::move(metricData));
            }
            if (std::isnan(timePlotData.plotMaxValue) && !std::isnan(plotMaxValue))
            {
                timePlotData.plotMaxValue = plotMaxValue;
            }
            data.plots.emplace_back(std::move(timePlotData));
        }

        return true;
    }

    bool InitializeTimePlotsData(const RawData& rawData, TimePlotsData& data)
    {
        if (rawData.counterDataImage.empty() || rawData.metricDisplayConfig.empty())
        {
            NV_PERF_LOG_ERR(50, "No counter data image or no metric display config.\n");
            return false;
        }

        NV_PERF_LOG_INF(50, "Processing input files into time plots data...\n");
        const auto start = std::chrono::high_resolution_clock::now();

        data = TimePlotsData();

        const CounterDataImage& counterDataImage = rawData.counterDataImage;
        const char* pChipName = GetChipNameFromCounterData(counterDataImage);
        if (!pChipName)
        {
            return false;
        }
        data.gpu = pChipName;

        std::vector<uint8_t> counterDataPrefix;
        if (!ExtractCounterDataPrefixFromCounterData(counterDataImage.data(), counterDataImage.size(), counterDataPrefix))
        {
            return false;
        }

        MetricConfigObject metricConfigObject;
        if (!rawData.metricConfig.empty())
        {
            if (!metricConfigObject.InitializeFromYaml(rawData.metricConfig))
            {
                NV_PERF_LOG_ERR(50, "Failed to initialize metricConfigObject.\n");
                return false;
            }
        }

        if (!LoadMetricDisplayConfiguration(pChipName, rawData, metricConfigObject, data))
        {
            return false;
        }

        std::vector<uint64_t> rawTimestamps;
        if (!GetRawTimestamps(counterDataImage, rawTimestamps))
        {
            return false;
        }

        MetricsEvaluator metricsEvaluator;
        {
            std::vector<uint8_t> scratchBuffer;
            NVPW_MetricsEvaluator* pMetricsEvaluator = sampler::DeviceCreateMetricsEvaluator(scratchBuffer, pChipName);
            if (!pMetricsEvaluator)
            {
                return false;
            }
            if (!MetricsEvaluatorSetDeviceAttributes(pMetricsEvaluator, counterDataImage.data(), counterDataImage.size()))
            {
                return false;
            }
            metricsEvaluator = MetricsEvaluator(pMetricsEvaluator, std::move(scratchBuffer)); // transfer ownership to metricsEvaluator
        }

        const std::string allUserMetricsScript = metricConfigObject.GenerateScriptForAllNamespacedUserMetrics();
        if (!allUserMetricsScript.empty())
        {
            if (!metricsEvaluator.UserDefinedMetrics_Initialize())
            {
                return false; // error logged in the function
            }
            if (!metricsEvaluator.UserDefinedMetrics_Execute(allUserMetricsScript))
            {
                NV_PERF_LOG_ERR(50, "Failed to execute the user-defined metrics script. Is the script valid?\n");
                return false;
            }
            if (!metricsEvaluator.UserDefinedMetrics_Commit())
            {
                return false; // error logged in the function
            }
        }

        for (TimePlotData& plot : data.plots)
        {
            if (!InitializePlotData(metricsEvaluator, counterDataPrefix, counterDataImage, plot))
            {
                return false;
            }
        }

        // Trace
        std::vector<mini_trace::APITraceData> rawTraceEvents;
        if (!rawData.trace.empty())
        {
            rawTraceEvents = mini_trace::DeserializeAPITraceDataFromYaml(rawData.trace);
        }

        // Determine the absolute start time and end time
        uint64_t startTime = rawTimestamps.front();
        uint64_t endTime = rawTimestamps.back();
        for (const mini_trace::APITraceData& event : rawTraceEvents)
        {
            startTime = std::min(startTime, event.startTimestamp);
            endTime = std::max(endTime, event.endTimestamp);
        }
        data.endTime = double(endTime - startTime);

        // actually initialize the timestamps & trace data based ont eh calculated start time
        InitializeTimestamps(rawTimestamps, startTime, data);
        InitializeTraceData(rawTraceEvents, startTime, data);

        // validate the data
        for (TimePlotData& plot : data.plots)
        {
            for (const TimePlotData::MetricData& metricData : plot.metricsData)
            {
                assert(metricData.samples.size() == data.timestampsData.timestamps.size());
                assert(metricData.samplesLodList.size() == data.timestampsData.timestampsLodList.size());
                for (size_t lodListIdx = 0; lodListIdx < metricData.samplesLodList.size(); ++lodListIdx)
                {
                    assert(metricData.samplesLodList[lodListIdx].size() == data.timestampsData.timestampsLodList[lodListIdx].size());
                }
                if (plot.type == TimePlotData::PlotType::Stacked)
                {
                    assert(metricData.stackedSamples.size() == data.timestampsData.timestamps.size());
                    assert(metricData.stackedSamplesLodList.size() == data.timestampsData.timestampsLodList.size());
                    for (size_t lodListIdx = 0; lodListIdx < metricData.stackedSamplesLodList.size(); ++lodListIdx)
                    {
                        assert(metricData.stackedSamplesLodList[lodListIdx].size() == data.timestampsData.timestampsLodList[lodListIdx].size());
                    }
                }
            }
        }

        data.isValid = true;

        const auto end = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> elapsed = end - start;
        NV_PERF_LOG_INF(50, "Data Processed(elapsed time = %.2f ms).\n", elapsed.count());

        return true;
    }

}}} // namespace nv::perf::tool
