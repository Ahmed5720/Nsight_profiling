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

#include "ViewerTab.h"
#include "App.h"
#include "ImGuiUtils.h"
#include "OutputMessagesPanel.h"
#include "Utils.h"
#include <imgui.h>
#include <implot.h>
#include <IconsFontAwesome6.h>
#include <NvPerfScopeExitGuard.h>
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>

namespace nv { namespace perf { namespace tool {

    // ===========================================================================================================================================
    //
    // Plot Panel
    //
    // ===========================================================================================================================================

    class PlotPanel
    {
    private:
        static constexpr size_t NumTicks = 10;
        static constexpr double ZoomInLimitInNanoseconds = 200.0f * 1000; // TODO: should this be dynamically determined?
        static constexpr float TracePerNestingLevelPlotHeight = 40.0f;
        static constexpr float MinimumProfilerPlotHeight = 100.0f;
        // ImPlotFlags_NoChild is used for several reasons:
        // 1. So it will show the window's background, useful in ImGuiTableFlags_RowBg(although we can set ImGuiCol_ChildBg to transparent, and restore it back)
        // 2. So the child window will not capture the mouse scroll
        // 2. So in the stacked plot, the line will be on top of the plot
        static constexpr ImPlotFlags CommonPlotFlags =  ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText | ImPlotFlags_NoFrame | ImPlotFlags_NoTitle | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoLegend | ImPlotFlags_NoChild;
        static constexpr ImPlotAxisFlags CommonAxisFlags = ImPlotAxisFlags_NoMenus | ImPlotAxisFlags_NoSideSwitch | ImPlotAxisFlags_NoTickMarks | ImPlotAxisFlags_NoHighlight;

        const TimePlotsData& m_timePlotsData;

        // This is used to link the plots, see also ImPlot::SetupAxisLinks()
        ImPlotRange m_xLimits = {0, 0};
        bool m_tracePlotHovered = false;
        const TimePlotData* m_pTimePlotHovered = nullptr; // nullptr if not hovered
        std::unordered_map<std::string, bool> m_plotVisibility;

    private:
        std::vector<double> GetTicks(const ImPlotRange& range, size_t numTicks) const;
        size_t GetValueIndex(const std::vector<double>& values, double value) const;
        void RenderTracePlot();
        void RenderTimePlot(const TimePlotData& currentPlot, float rowHeightInPixels);

    public:
        PlotPanel(const TimePlotsData& timePlotsData)
            : m_timePlotsData(timePlotsData)
        {
            ResetZoom();

            m_plotVisibility["Trace"] = true;
            for (const auto& timePlot : m_timePlotsData.plots)
            {
                m_plotVisibility[timePlot.name] = true;
            }
        }
        PlotPanel(const PlotPanel&) = delete;
        PlotPanel(PlotPanel&&) = default;
        PlotPanel& operator=(const PlotPanel&) = delete;
        PlotPanel& operator=(PlotPanel&&) = default;
        ~PlotPanel() = default;

        void Render(bool* pOpen);

        void ResetZoom()
        {
            m_xLimits = {0, m_timePlotsData.endTime};
        }

        // nullptr if no plot is hovered
        const TimePlotData* GetCurrentActivePlot() const
        {
            return m_pTimePlotHovered;
        }

        bool IsTracePlotActive() const
        {
            return m_tracePlotHovered;
        }
    };

    // Divide a range into approximately `numTicks` intervals, with each tick rounded to the nearest power of 10.
    std::vector<double> PlotPanel::GetTicks(const ImPlotRange& range, size_t numTicks) const
    {
        const double tickInterval = range.Size() / numTicks;
        const int orderOfMagnitude = (int)std::floor(std::log10((int)tickInterval));
        const double roundedTickInterval = std::round(tickInterval / std::pow(10, orderOfMagnitude)) * std::pow(10, orderOfMagnitude);
        const double start = std::floor(range.Min / roundedTickInterval) * roundedTickInterval;
        const double end = std::ceil(range.Max / roundedTickInterval) * roundedTickInterval;

        std::vector<double> ticks;
        for (double tick = start; tick <= end; tick += roundedTickInterval)
        {
            ticks.push_back(tick);
        }
        return ticks;
    }

    // This function returns the index of the target value if it exists in the vector.
    // If the target value does not exist but falls between two elements, the function returns the index of the smaller element.
    // Otherwise it returns -1.
    // Example
    // For values = {500, 600, 700}, expected results:
    //  Input,       Result
    //  value = 499, return -1
    //  value = 500, return 0(500)
    //  value = 501, return 0(500)
    //  value = 599, return 0(500)
    //  value = 600, return 1(600)
    //  value = 700, return 2(700)
    //  value = 701, return -1
    size_t PlotPanel::GetValueIndex(const std::vector<double>& values, double value) const
    {
        constexpr size_t NotFound = (size_t)-1;
        if (values.empty())
        {
            return NotFound;
        }
        if (value < values.front() || value > values.back())
        {
            return NotFound;
        }

        const auto it = std::lower_bound(values.begin(), values.end(), value);
        assert(it != values.end()); // since we already checked (value < values.front())
        if (*it == value)
        {
            return std::distance(values.begin(), it);
        }
        if (it == values.begin())
        {
            return NotFound;
        }
        return std::distance(values.begin(), it) - 1;
    }

    void PlotPanel::RenderTracePlot()
    {
        const TraceData& traceData = m_timePlotsData.traceData;
        if (traceData.events.empty())
        {
            return;
        }

        const float PlotHeight = TracePerNestingLevelPlotHeight * App::Instance().GetDpiScale() * traceData.maximumDisplayLevel;
        if (ImPlot::BeginPlot("Trace", ImVec2(-1, PlotHeight), CommonPlotFlags))
        {
            auto endPlot = ScopeExitGuard([&]() {
                ImPlot::EndPlot();
            });

            // setup axes
            {
                constexpr ImPlotAxisFlags YAxisFlags = CommonAxisFlags | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines;
                ImPlot::SetupAxes(nullptr, nullptr, CommonAxisFlags, YAxisFlags);
                ImPlot::SetupAxisLinks(ImAxis_X1, &m_xLimits.Min, &m_xLimits.Max); // m_xLimits is required to keep x-axis synced
                ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, (double)traceData.maximumDisplayLevel); // SetupAxisLimits() controls the current display range
                ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0.0, m_timePlotsData.endTime); // SetupAxisLimitsConstraints() controls the absolute viewable range. e.g. you can not drag beyond m_timePlotsData.endTime
                ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, (double)traceData.maximumDisplayLevel);
                // setup custom ticks
                {
                    const std::vector<double> ticks = GetTicks(m_xLimits, NumTicks);
                    std::vector<std::string> tickLabels;
                    std::vector<const char*> tickLabelPtrs;
                    tickLabels.reserve(ticks.size());
                    tickLabels.reserve(ticks.size());
                    for (size_t ii = 0; ii < ticks.size(); ++ii)
                    {
                        tickLabels.emplace_back(FormatTimestamp((uint64_t)ticks[ii]));
                        tickLabelPtrs.push_back(tickLabels.back().c_str());
                    }
                    ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(), (int)ticks.size(), tickLabelPtrs.data());
                }
            }

            const ImPlotRange currentXAxisRange = ImPlot::GetPlotLimits().X; // ImPlot::GetPlotLimits() locks the set up, so it must be called after all the set up code

            const std::vector<TraceData::TraceEvent>& traceEvents = traceData.events;
            std::map<const TraceData::TraceEvent*, std::function<bool(double, double)>> isEventHoveredFunctors;
            for (const auto& event : traceEvents)
            {
                // if this has no overlap with the current x-axis range, skip it. This is to avoid later rendering texts for invisible events.
                if (event.endTime <= currentXAxisRange.Min || event.startTime >= currentXAxisRange.Max)
                {
                    continue;
                }

                // render the event
                constexpr double EventHeight = 1.0;
                const std::array<double, 2> x = { event.startTime, event.endTime }; // use std::array so it can be captured by value
                const std::array<double, 2> y = { double(traceData.maximumDisplayLevel - event.displayLevel) * EventHeight, double(traceData.maximumDisplayLevel - event.displayLevel) * EventHeight };
                // must capture by value
                isEventHoveredFunctors[&event] = [x, y, EventHeight](double mouseX, double mouseY) -> bool {
                    return mouseX >= x[0] && mouseX <= x[1] && mouseY >= y[0] && mouseY <= y[1] + EventHeight;
                };
                ImPlot::SetNextFillStyle(ToImVec4(event.color));
                ImPlot::PlotShaded(event.name.c_str(), x.data(), y.data(), 2, double(y[0] + EventHeight), 0);

                // render the event border
                ImPlot::PushStyleColor(ImPlotCol_Line, ColorBlack);
                {
                    const double xPositions[2] = { x[0], x[1] };
                    const double yPositions[2] = { y[0], y[1] };
                    ImPlot::PlotLine((event.name + "_Down").c_str(), xPositions, yPositions, 2);
                }
                {
                    const double xPositions[2] = { x[0], x[1] };
                    const double yPositions[2] = { y[0] + EventHeight, y[1] + EventHeight };
                    ImPlot::PlotLine((event.name + "_Up").c_str(), xPositions, yPositions, 2);
                }
                {
                    const double xPositions[2] = { x[0], x[0] };
                    const double yPositions[2] = { y[0], y[1] + EventHeight };
                    ImPlot::PlotLine((event.name + "_Left").c_str(), xPositions, yPositions, 2);
                }
                {
                    const double xPositions[2] = { x[1], x[1] };
                    const double yPositions[2] = { y[0], y[1] + EventHeight };
                    ImPlot::PlotLine((event.name + "_Right").c_str(), xPositions, yPositions, 2);
                }
                ImPlot::PopStyleColor();

                // render the text
                {
                    // It's drawn at the middle of the event, but in case the event is partially overlapped with the current x-axis range, it's drawn at the middle of the overlap.
                    // Furthermore, when the current event(or the overlap) is small, the text shall be clamped to not entering entering another event's territory.
                    const double overlapStart = std::max(x[0], currentXAxisRange.Min);
                    const double overlapEnd = std::min(x[1], currentXAxisRange.Max);
                    const double middleX = (overlapStart + overlapEnd) / 2;
                    const double middleY = y[0] + EventHeight / 2;
                    const float textWidthInPixels = ImGui::CalcTextSize(event.name.c_str()).x;
                    const float overlapWidthInPixels = ImPlot::PlotToPixels(ImPlotPoint(overlapEnd, 0.0)).x - ImPlot::PlotToPixels(ImPlotPoint(overlapStart, 0.0)).x;

                    std::string eventName = event.name;
                    if (textWidthInPixels > overlapWidthInPixels)
                    {
                        // the current text is too wide for the overlap, clamp it
                        const double perCharWidthInPixels = textWidthInPixels / eventName.size();
                        const size_t maxChars = size_t(overlapWidthInPixels / perCharWidthInPixels);
                        eventName = eventName.substr(0, maxChars);
                    }
                    ImPlot::PushStyleColor(ImPlotCol_InlayText, ColorBlack);
                    ImPlot::PlotText(eventName.c_str(), middleX, middleY);
                    ImPlot::PopStyleColor();
                }
            }

            if (ImPlot::IsPlotHovered())
            {
                m_tracePlotHovered = true;

                ImGui::BeginTooltip();
                auto endTooltip = ScopeExitGuard([&]() {
                    ImGui::EndTooltip();
                });

                ImGui::Text("At %s", FormatTimestamp((uint64_t)ImPlot::GetPlotMousePos().x).c_str());
                for (const auto& pair: isEventHoveredFunctors)
                {
                    const auto& isEventHoveredFunctor = pair.second;
                    if (isEventHoveredFunctor(ImPlot::GetPlotMousePos().x, ImPlot::GetPlotMousePos().y))
                    {
                        const TraceData::TraceEvent& event = *pair.first;
                        const char* pIndent = "  ";
                        ImGui::TextUnformatted(pIndent);
                        ImGui::SameLine();
                        ImPlot::ItemIcon(ToImVec4(event.color));
                        ImGui::SameLine();
                        ImGui::TextUnformatted(event.name.c_str());

                        // embed %s(pIndent) into the below ImGui::Text() will cause a slight shift, until we have a better solution, explicitly
                        // output pIndent in a separate ImGui::TextUnformatted().
                        ImGui::TextUnformatted(pIndent);
                        ImGui::SameLine();
                        ImGui::Text("Start Time: %s", FormatTimestamp((uint64_t)event.startTime).c_str());

                        ImGui::TextUnformatted(pIndent);
                        ImGui::SameLine();
                        ImGui::Text("End Time:   %s", FormatTimestamp((uint64_t)event.endTime).c_str());

                        ImGui::TextUnformatted(pIndent);
                        ImGui::SameLine();
                        ImGui::Text("Duration:   %s", FormatTimestamp((uint64_t)event.duration).c_str());

                        ImGui::TextUnformatted(pIndent);
                        ImGui::SameLine();
                        ImGui::Text("Nesting Level:   %u", (uint32_t)event.nestingLevel);
                        break;
                    }
                }
            }
        }
    }

    void PlotPanel::RenderTimePlot(const TimePlotData& currentPlot, float rowHeightInPixels)
    {
        const float height = std::max(rowHeightInPixels, MinimumProfilerPlotHeight * App::Instance().GetDpiScale());
        if (ImPlot::BeginPlot(currentPlot.name.c_str(), ImVec2(-1, height), CommonPlotFlags))
        {
            auto endPlot = ScopeExitGuard([&]() {
                ImPlot::EndPlot();
            });

            const size_t lodLevel = [&]() -> size_t {
                // The displayed range has no overlap with our data. Technically there is nothing to draw, but given ImPlot will still call into getter for the data, for the best perf
                // return the lowest resolution LOD.
                if (  m_xLimits.Max <= m_timePlotsData.timestampsData.timestamps.front()
                   || m_xLimits.Min >= m_timePlotsData.timestampsData.timestamps.back())
                {
                    return m_timePlotsData.timestampsData.timestampsLodList.empty() ? (size_t)-1 : m_timePlotsData.timestampsData.timestampsLodList.size() - 1;
                }

                const double scale = m_xLimits.Size() / m_timePlotsData.endTime;
                // The number of samples that would be displayed if the x-axis range is fully covered by the m_timePlotsData.
                const size_t currentHypothesisNumSamples = size_t(scale * m_timePlotsData.timestampsData.timestamps.size());
                return SelectLODLevelForDisplay(m_timePlotsData.timestampsData.timestampsLodList, currentHypothesisNumSamples);
            }(); // if it's (size_t)-1, then it indicates that the original data should be used

            auto selectLOD = [&](const std::vector<double>& original, const std::vector<std::vector<double>>& lodList) {
                return (lodLevel == (size_t)-1) ? &original : &lodList[lodLevel];
            };

            // Setup axes
            {
                // Do not use AutoFit as that prevents zoom in. In addition, do not lock min or max as that prevents dragging.
                constexpr ImPlotAxisFlags YAxisFlags = CommonAxisFlags | ImPlotAxisFlags_Lock;
                ImPlot::SetupAxes(nullptr, currentPlot.yAxesName.c_str(), CommonAxisFlags, YAxisFlags);
                ImPlot::SetupAxisLinks(ImAxis_X1, &m_xLimits.Min, &m_xLimits.Max);  // m_xLimits is required to keep x-axis synced
                if (!std::isnan(currentPlot.plotMaxValue))
                {
                    // SetupAxisLimits() controls the current display range. But since y-axis is locked, you cannot zoom in or drag vertically
                    constexpr double Offset = 0.5; // in order to draw values that exactly equal to min or max
                    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0 - Offset, currentPlot.plotMaxValue + Offset, ImGuiCond_Always);
                }
                ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0.0, m_timePlotsData.endTime); // SetupAxisLimitsConstraints() controls the absolute viewable range. e.g. you can not drag beyond m_timePlotsData.endTime
                ImPlot::SetupAxisZoomConstraints(ImAxis_X1, ZoomInLimitInNanoseconds, m_timePlotsData.endTime);

                // setup custom x-axis ticks
                {
                    const std::vector<double> ticks = GetTicks(m_xLimits, NumTicks);
                    std::vector<std::string> tickLabels;
                    std::vector<const char*> tickLabelPtrs;
                    tickLabels.reserve(ticks.size());
                    tickLabels.reserve(ticks.size());
                    for (size_t ii = 0; ii < ticks.size(); ++ii)
                    {
                        tickLabels.emplace_back(FormatTimestamp((uint64_t)ticks[ii]));
                        tickLabelPtrs.push_back(tickLabels.back().c_str());
                    }
                    ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(), (int)ticks.size(), tickLabelPtrs.data());
                }
            }

            const std::vector<double>* pTimestampsLOD = selectLOD(m_timePlotsData.timestampsData.timestamps, m_timePlotsData.timestampsData.timestampsLodList);
            const bool isUsingOriginalDataAsLOD = (pTimestampsLOD == &m_timePlotsData.timestampsData.timestamps);
            if (currentPlot.type == TimePlotData::PlotType::Stacked)
            {
                // For stacked view, plots must be drawn opposite from their stack values(highest -> lowest). This way, colors with lowest values will be drawn at the front.
                for (auto it = currentPlot.metricsData.rbegin(); it != currentPlot.metricsData.rend(); ++it)
                {
                    const TimePlotData::MetricData& metricData = *it;
                    const auto color = ToImVec4(metricData.color);
                    ImPlot::SetNextLineStyle(color);
                    ImPlot::SetNextFillStyle(color);
                    const std::vector<double>* pStackedSamplesLOD = selectLOD(metricData.stackedSamples, metricData.stackedSamplesLodList);
                    assert(pTimestampsLOD->size() == pStackedSamplesLOD->size());
                    if (isUsingOriginalDataAsLOD) // only draw stairs at the highest resolution, i.e. each sample is raw data, so it must not be interpolated.
                    {
                        ImPlot::PlotStairs((currentPlot.name + metricData.name).c_str(), pTimestampsLOD->data(), pStackedSamplesLOD->data(), (int)pTimestampsLOD->size(), ImPlotStairsFlags_Shaded);
                    }
                    else
                    {
                        ImPlot::PlotShaded((currentPlot.name + metricData.name).c_str(), pTimestampsLOD->data(), pStackedSamplesLOD->data(), (int)pTimestampsLOD->size(), 0.0, 0);
                    }
                }
            }
            else if (currentPlot.type == TimePlotData::PlotType::Overlay)
            {
                for (const TimePlotData::MetricData& metricData : currentPlot.metricsData)
                {
                    ImPlot::SetNextLineStyle(ToImVec4(metricData.color));
                    const std::vector<double>* pSamplesLOD = selectLOD(metricData.samples, metricData.samplesLodList);

                    if (isUsingOriginalDataAsLOD) // only draw stairs at the highest resolution, i.e. each sample is raw data, so it must not be interpolated.
                    {
#if defined(USE_CUSTOM_GETTER)
                        struct UserData
                        {
                            const std::vector<double>* pTimestamps;
                            const std::vector<double>* pSamples;
                        };

                        auto getter = [](int index, void* pData) -> ImPlotPoint {
                            UserData& data = *static_cast<UserData*>(pData);
                            if (index < 0 || index >= data.pTimestamps->size())
                            {
                                return ImPlotPoint(0, 0);
                            }
                            ImPlotPoint point(data.pTimestamps->at(index), data.pSamples->at(index));
                            return point;
                        };

                        UserData userData;
                        userData.pTimestamps = pTimestampsLOD;
                        userData.pSamples = pSamplesLOD;
                        ImPlot::PlotStairsG((currentPlot.name + metricData.name).c_str(), getter, &userData, (int)pTimestampsLOD->size());
#else
                        ImPlot::PlotStairs((currentPlot.name + metricData.name).c_str(), pTimestampsLOD->data(), pSamplesLOD->data(), (int)pTimestampsLOD->size());
#endif // defined(USE_CUSTOM_GETTER)
                    }
                    else
                    {
                        ImPlot::PlotLine((currentPlot.name + metricData.name).c_str(), pTimestampsLOD->data(), pSamplesLOD->data(), (int)pTimestampsLOD->size());
                    }
                }
            }

            // Tooltip
            if (ImPlot::IsPlotHovered())
            {
                m_pTimePlotHovered = &currentPlot;

                ImGui::BeginTooltip();
                auto endTooltip = ScopeExitGuard([&]() {
                    ImGui::EndTooltip();
                });

                struct MetricTooltipInfo
                {
                    std::string displayName;
                    std::string unit;
                    ImVec4 color;
                    double value;
                };

                const double xPosition = ImPlot::GetPlotMousePos().x;
                const size_t index = GetValueIndex(*pTimestampsLOD, xPosition);
                std::vector<MetricTooltipInfo> tooltipInfoVector;
                for (const TimePlotData::MetricData& metricData : currentPlot.metricsData)
                {
                    tooltipInfoVector.emplace_back(MetricTooltipInfo());
                    MetricTooltipInfo& info = tooltipInfoVector.back();
                    info.displayName = metricData.name;
                    info.unit = metricData.unit;
                    info.color = ToImVec4(metricData.color);
                    const std::vector<double>* pSamplesLOD = selectLOD(metricData.samples, metricData.samplesLodList);
                    info.value = [&]() -> double {
                        if (index >= pSamplesLOD->size())
                        {
                            return 0.0;
                        }
                        if (isUsingOriginalDataAsLOD)
                        {
                            // Avoid interpolation at the highest resolution, where each sample is a stair line.
                            return pSamplesLOD->at(index);
                        }
                        if (index == pSamplesLOD->size() - 1)
                        {
                            return pSamplesLOD->at(index);
                        }
                        const double x1 = pTimestampsLOD->at(index);
                        const double x2 = pTimestampsLOD->at(index + 1);
                        const double y1 = pSamplesLOD->at(index);
                        const double y2 = pSamplesLOD->at(index + 1);
                        const double slope = (y2 - y1) / (x2 - x1);
                        return y1 + slope * (xPosition - x1);
                    }();
                }
                // List from highest value to lowest value
                std::stable_sort(tooltipInfoVector.begin(), tooltipInfoVector.end(), [](const MetricTooltipInfo& lhs, const MetricTooltipInfo& rhs) {
                    // treat NaN as the lowest value
                    if (std::isnan(lhs.value))
                    {
                        return false;
                    }
                    else if (std::isnan(rhs.value))
                    {
                        return true;
                    }
                    else
                    {
                        return lhs.value > rhs.value;
                    }
                });

                ImGui::Text("At %s", FormatTimestamp((uint64_t)xPosition).c_str());
                size_t maxMetricNameLength = 0;
                for (const auto& metricData : currentPlot.metricsData)
                {
                    maxMetricNameLength = std::max(maxMetricNameLength, metricData.name.length());
                }
                for (const MetricTooltipInfo& info : tooltipInfoVector)
                {
                    const char* pIndent = "  ";
                    ImGui::TextUnformatted(pIndent);
                    ImGui::SameLine();
                    ImPlot::ItemIcon(info.color);
                    ImGui::SameLine();
                    ImGui::Text("%-*s: %.2f %s", (int)maxMetricNameLength, info.displayName.c_str(), info.value, info.unit.c_str());
                }
            }
        }
    }

    void PlotPanel::Render(bool *pOpen)
    {
        if (ImGui::Begin("Plot", pOpen))
        {
            m_tracePlotHovered = false;
            m_pTimePlotHovered = nullptr;

            ImPlot::GetStyle().Colors[ImPlotCol_PlotBg].w = 0.0; // fully transparent(setting ImPlotCol_FrameBg's alpha to 0 will make the horizontal axis disappear)
            constexpr int NumColumns = 2;
            const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
            if (ImGui::BeginTable("##PlotTable", NumColumns, TableFlags, ImVec2(-1,0)))
            {
                auto endTable = ScopeExitGuard([&]() {
                    ImGui::EndTable();
                });
                const float plotNameWidth = 250.0f * App::Instance().GetDpiScale();
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, plotNameWidth);
                ImGui::TableSetupColumn(nullptr);
                // TODO: why it must have an empty line here? Commenting it out will make all plots disappear.
                ImGui::TableNextRow();
                for (int ii = 0; ii < NumColumns; ++ii)
                {
                    ImGui::TableSetColumnIndex(ii);
                }
                if (ImPlot::BeginAlignedPlots("AlignedGroup"))
                {
                    auto endAlignedPlots = ScopeExitGuard([&]() {
                        ImPlot::EndAlignedPlots();
                    });
                    const ImVec2 tableTop = ImGui::GetCursorScreenPos();

                    // trace plot
                    if (!m_timePlotsData.traceData.events.empty())
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        {
                            const char* pPlotName = "Trace";
                            const bool isVisible = m_plotVisibility[pPlotName];
                            const std::string buttonLabel = std::string(isVisible ? ICON_FA_CARET_DOWN : ICON_FA_CARET_RIGHT) + "##Trace";
                            if (ImGui::Button(buttonLabel.c_str()))
                            {
                                m_plotVisibility[pPlotName] = !isVisible;
                            }
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::BeginTooltip();
                                ImGui::TextUnformatted("Click to toggle display or collapse the plot.");
                                ImGui::EndTooltip();
                            }

                            ImGui::SameLine();
                            ImGui::TextUnformatted("Trace");
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::BeginTooltip();
                                ImGui::Text("Trace Data: %d events", (int)m_timePlotsData.traceData.events.size());
                                ImGui::EndTooltip();
                            }
                        }

                        ImGui::TableSetColumnIndex(1);
                        if (m_plotVisibility["Trace"])
                        {
                            RenderTracePlot();
                        }
                    }

                    // sampler plots
                    for (const TimePlotData& timePlot : m_timePlotsData.plots)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + plotNameWidth);
                        float rowHeight = 0.0;
                        {
                            const float cursorPosYBegin = ImGui::GetCursorPosY();
                            const char* pPlotName = timePlot.name.c_str();
                            const bool isVisible = m_plotVisibility[pPlotName];
                            const std::string buttonLabel = std::string(isVisible ? ICON_FA_CARET_DOWN : ICON_FA_CARET_RIGHT) + "##" + pPlotName;
                            if (ImGui::Button(buttonLabel.c_str()))
                            {
                                m_plotVisibility[pPlotName] = !isVisible;
                            }
                            const size_t buttonSizeInNumChars = (size_t)(ImGui::GetItemRectSize().x / ImGui::CalcTextSize(" ").x);
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::BeginTooltip();
                                ImGui::TextUnformatted("Click to toggle display or collapse the plot.");
                                ImGui::EndTooltip();
                            }

                            ImGui::SameLine();
                            ImGui::TextUnformatted(pPlotName);

                            if (m_plotVisibility[timePlot.name])
                            {
                                const std::string indent = std::string(buttonSizeInNumChars + 2u, ' ');
                                for (const TimePlotData::MetricData& metricData : timePlot.metricsData)
                                {
                                    ImGui::TextUnformatted(indent.c_str());
                                    ImGui::SameLine();
                                    ImPlot::ItemIcon(ToImVec4(metricData.color));
                                    ImGui::SameLine();
                                    ImGui::Text("%s", metricData.name.c_str());
                                }
                            }
                            rowHeight = ImGui::GetCursorPosY() - cursorPosYBegin;
                        }
                        ImGui::PopTextWrapPos();

                        ImGui::TableSetColumnIndex(1);
                        if (m_plotVisibility[timePlot.name])
                        {
                            RenderTimePlot(timePlot, rowHeight);
                        }
                    }

                    // draw a vertical line that across the whole table if any plot is hovered
                    if (m_tracePlotHovered || m_pTimePlotHovered)
                    {
                        const ImVec2 tableBottom = ImGui::GetCursorScreenPos();
                        const ImVec2 mousePos = ImGui::GetMousePos();
                        ImDrawList* pDrawList = ImPlot::GetPlotDrawList();
                        pDrawList->AddLine(
                            ImVec2(mousePos.x, tableTop.y), 
                            ImVec2(mousePos.x, tableBottom.y), 
                            IM_COL32(255, 255, 255, 255), // white
                            2.0f // line thickness
                        );
                    }
                } // if ImPlot::BeginAlignedPlots()
            } // if ImGui::BeginTable
        }
        ImGui::End();
    }

    // ===========================================================================================================================================
    //
    // DataSummaryPanel
    //
    // ===========================================================================================================================================

    class DataSummaryPanel
    {
    private:
        ViewerTabCreateRequest::LoadFilePaths m_loadFilePaths;
        const TimePlotsData& m_timePlotsData;

    public:
        DataSummaryPanel(ViewerTabCreateRequest::LoadFilePaths&& loadFilePaths, const TimePlotsData& timePlotsData)
            : m_loadFilePaths(std::move(loadFilePaths))
            , m_timePlotsData(timePlotsData)
        {
        }
        DataSummaryPanel(const DataSummaryPanel&) = delete;
        DataSummaryPanel(DataSummaryPanel&&) = default;
        DataSummaryPanel& operator=(const DataSummaryPanel&) = delete;
        DataSummaryPanel& operator=(DataSummaryPanel&&) = default;
        ~DataSummaryPanel() = default;

        void Render(bool* pOpen);
    };

    void DataSummaryPanel::Render(bool* pOpen)
    {
        if (ImGui::Begin("Data Summary", pOpen))
        {
            constexpr int NumColumns = 2;
            const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##DataSummaryTable", NumColumns, TableFlags, ImVec2(-1,0)))
            {
                auto endTable = ScopeExitGuard([&]() {
                    ImGui::EndTable();
                });
                ImGui::TableSetupColumn(nullptr);
                ImGui::TableSetupColumn(nullptr);

                std::vector<std::pair<std::string, std::string>> infoParis;
                {
                    infoParis.emplace_back(std::make_pair("CounterData", ExtractFileNameOutOfPath(m_loadFilePaths.counterDataImage)));
                    infoParis.emplace_back(std::make_pair("MetricConfig", ExtractFileNameOutOfPath(m_loadFilePaths.metricConfig)));
                    infoParis.emplace_back(std::make_pair("MetricDisplayConfig", ExtractFileNameOutOfPath(m_loadFilePaths.metricDisplayConfig)));
                    if (!m_loadFilePaths.trace.empty())
                    {
                        infoParis.emplace_back(std::make_pair("Trace", ExtractFileNameOutOfPath(m_loadFilePaths.trace)));
                    }
                    infoParis.emplace_back(std::make_pair("GPU", m_timePlotsData.gpu));
                    infoParis.emplace_back(std::make_pair("Duration", FormatTimestamp(uint64_t(m_timePlotsData.endTime))));
                    infoParis.emplace_back(std::make_pair("# of Samples", std::to_string(m_timePlotsData.timestampsData.timestamps.size())));
                    const double avgFrequency = m_timePlotsData.timestampsData.timestamps.size() / (m_timePlotsData.endTime / 1e9);
                    infoParis.emplace_back(std::make_pair("Avg Sampl Freq", std::to_string((uint32_t)avgFrequency) + " Hz"));
                    infoParis.emplace_back(std::make_pair("# of Metrics", std::to_string(m_timePlotsData.plots.size())));
                }

                for (const auto& pair : infoParis)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(pair.first.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushTextWrapPos(ImGui::GetWindowWidth());
                    ImGui::TextUnformatted(pair.second.c_str());
                    ImGui::PopTextWrapPos();
                }
            }
        }
        ImGui::End();
    }

    // ===========================================================================================================================================
    //
    // PlotInfoPanel
    //
    // ===========================================================================================================================================

    class PlotInfoPanel
    {
    private:
        const TimePlotsData& m_timePlotsData;
        size_t m_currentPlotIndex;
        std::vector<const char*> m_plotNames;

    public:
        PlotInfoPanel(const TimePlotsData& timePlotsData)
            : m_timePlotsData(timePlotsData)
            , m_currentPlotIndex(0)
        {
            for (const TimePlotData& plot : m_timePlotsData.plots)
            {
                m_plotNames.push_back(plot.name.c_str());
            }
        }
        PlotInfoPanel(const PlotInfoPanel&) = delete;
        PlotInfoPanel(PlotInfoPanel&&) = default;
        PlotInfoPanel& operator=(const PlotInfoPanel&) = delete;
        PlotInfoPanel& operator=(PlotInfoPanel&&) = default;
        ~PlotInfoPanel() = default;

        void Render(bool* pOpen);
    };

    void PlotInfoPanel::Render(bool* pOpen)
    {
        if (ImGui::Begin("Plot Info", pOpen))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Plot:");
            ImGui::SameLine();
            if (ImGui::BeginCombo("##PlotInfoCombo", m_plotNames[m_currentPlotIndex]))
            {
                for (size_t ii = 0; ii < m_plotNames.size(); ++ii)
                {
                    const bool isSelected = (m_currentPlotIndex == ii);
                    if (ImGui::Selectable(m_plotNames[ii], isSelected))
                    {
                        m_currentPlotIndex = ii;
                    }
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const TimePlotData& currentPlot = m_timePlotsData.plots[m_currentPlotIndex];
            constexpr int NumColumns = 1;
            const ImGuiTableFlags TableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##PlotInfoTable", NumColumns, TableFlags, ImVec2(-1,0)))
            {
                auto endTable = ScopeExitGuard([&]() {
                    ImGui::EndTable();
                });
                ImGui::TableSetupColumn(nullptr);

                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(currentPlot.name.c_str());
                    ImGui::Text("Maximum Value: %f", currentPlot.plotMaxValue);
                }

                for (const TimePlotData::MetricData& metricData : currentPlot.metricsData)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushTextWrapPos(ImGui::GetWindowWidth());
                    ImPlot::ItemIcon(ToImVec4(metricData.color));
                    ImGui::SameLine();
                    TextColoredUnformatted(metricData.name.c_str(), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

                    TextColoredUnformatted(metricData.metric.c_str(), ImVec4(0.5f, 0.8f, 0.8f, 1.0f));

                    TextColoredUnformatted(metricData.description.c_str(), ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

                    ImGui::PopTextWrapPos();
                }
            }
        }
        ImGui::End();
    }

    // ===========================================================================================================================================
    //
    // ViewerTab::Impl
    //
    // ===========================================================================================================================================

    class ViewerTab::Impl
    {
    private:
        std::string m_name;
        uint32_t m_id;
        TimePlotsData m_timePlotsData;

        PlotPanel m_plotPanel;
        DataSummaryPanel m_dataSummaryPanel;
        PlotInfoPanel m_plotInfoPanel;
        OutputMessagesPanel m_outputMessagesPanel;

        bool m_showDataSummary = true;
        bool m_showPlotInfo = true;
        bool m_showPlot = true;
        bool m_showOutputMessagesPanel = true;

        void DrawToolbar();
        void DrawPanels();

    public:
        Impl(const std::string& name, uint32_t id, ViewerTabCreateRequest&& request);
        Impl(const Impl&) = delete;
        Impl(Impl&&) = default;
        Impl& operator=(const Impl&) = delete;
        Impl& operator=(Impl&&) = default;
        ~Impl() = default;

        void DrawPanelVisibilityMenu();
        void DrawMainWindow();
    };

    ViewerTab::Impl::Impl(const std::string& name, uint32_t id, ViewerTabCreateRequest&& request)
        : m_name(name)
        , m_id(id)
        , m_timePlotsData(std::move(request.GetTimePlotsData()))
        , m_plotPanel(m_timePlotsData)
        , m_dataSummaryPanel(std::move(request.GetLoadFilePaths()), m_timePlotsData)
        , m_plotInfoPanel(m_timePlotsData)
    {
    }

    void ViewerTab::Impl::DrawToolbar()
    {
        ImGui::PushID(this);
        if (BeginMainViewportToolBar((std::string("##Toolbar") + m_name).c_str(), { 5.0f, 5.0f }))
        {
            const float maximumWidth = 150.0f * App::Instance().GetDpiScale(); // assuming the maximum width of the toolbar is 200.0f
            const float windowVisibleWidth = ImGui::GetContentRegionAvail().x;
            const float startOffset = windowVisibleWidth - ImGui::GetCursorPosX() - maximumWidth;

            ImGui::SameLine(startOffset);
            if (ImGui::Button(ICON_FA_EXPAND " Reset Zoom"))
            {
                m_plotPanel.ResetZoom();
                printf("%s: Reset Zoom Clicked!\n", m_name.c_str());
            }
            // TODO: support zoom to selection
            // ImGui::SameLine();
            // if (ImGui::Button(ICON_FA_MAXIMIZE " Zoom To Selection"))
            // {
            //     printf("%s: Zoom To Selection Clicked!\n", m_name.c_str());
            // }

            ImGui::SameLine();
            VerticalSeperator();
            ImGui::SameLine();
        }
        ImGui::End();
        ImGui::PopID();
    }

    void ViewerTab::Impl::DrawPanels()
    {
        if (m_showDataSummary)
        {
            m_dataSummaryPanel.Render(&m_showDataSummary);
        }

        if (m_showPlot)
        {
            m_plotPanel.Render(&m_showPlot);
        }

        if (m_showPlotInfo)
        {
            m_plotInfoPanel.Render(&m_showPlotInfo);
        }

        if (m_showOutputMessagesPanel)
        {
            m_outputMessagesPanel.Render(&m_showOutputMessagesPanel);
        }
    }

    void ViewerTab::Impl::DrawPanelVisibilityMenu()
    {
        ImGui::PushID(this);
        if (ImGui::MenuItem(m_showDataSummary ? ICON_FA_CIRCLE_CHECK " Data Summary" : "Data Summary"))
        {
            m_showDataSummary = !m_showDataSummary;
        }
        if (ImGui::MenuItem(m_showPlotInfo ? ICON_FA_CIRCLE_CHECK " Plot Info" : "Plot Info"))
        {
            m_showPlotInfo = !m_showPlotInfo;
        }
        if (ImGui::MenuItem(m_showPlot ? ICON_FA_CIRCLE_CHECK " Plot" : "Plot"))
        {
            m_showPlot = !m_showPlot;
        }
        if (ImGui::MenuItem(m_showOutputMessagesPanel ? ICON_FA_CIRCLE_CHECK " Output Messages" : "Output Messages"))
        {
            m_showOutputMessagesPanel = !m_showOutputMessagesPanel;
        }
        ImGui::PopID();
    }

    void ViewerTab::Impl::DrawMainWindow()
    {
        // Create a full-screen window for the tab
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        DrawToolbar();
        DrawPanels();
    }

    // ===========================================================================================================================================
    //
    // ViewerTab
    //
    // ===========================================================================================================================================

    ViewerTab::ViewerTab(const std::string& name, uint32_t id, ViewerTabCreateRequest&& request)
        : ITab(ActivityType::Viewer, name, id)
        , m_pImpl(std::make_unique<Impl>(name, id, std::move(request)))
    {
    }

    // Note the implementation even if =default must be implemented in the CPP as the Impl being a incomplete type in the header
    // (compiler cannot generate ~Impl for destructing the unique_ptr)
    ViewerTab::~ViewerTab() = default;

    void ViewerTab::DrawPanelVisibilityMenu()
    {
        m_pImpl->DrawPanelVisibilityMenu();
    }

    void ViewerTab::DrawMainWindow()
    {
        m_pImpl->DrawMainWindow();
    }

}}} // namespace nv::perf::tool
