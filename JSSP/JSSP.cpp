#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "JSSPLib.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdio>
#include <stdexcept>
#include <glfw3.h>
#include <cstdint>
#include <cmath>
#include <climits> // for INT_MAX / INT_MIN

#ifdef _WIN32
// Prevent windows.h from defining `min`/`max` macros which break std::min/std::max
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h> // IFileDialog
#endif

// Helper: enumerate files in a folder and attempt to load problems
static std::vector<std::pair<std::string, Problem>> loadProblemsFromFolder(const std::string& folder)
{
    std::vector<std::pair<std::string, Problem>> results;
#ifdef _WIN32
    std::string folderNormalized = folder;
    if (!folderNormalized.empty() && (folderNormalized.back() == '\\' || folderNormalized.back() == '/'))
        folderNormalized.pop_back();

    std::string pattern = folderNormalized + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return results;

    do
    {
        const char* name = findData.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue; // skip subfolders

        std::string fullpath = folderNormalized + "\\" + name;

        // try to load as a JSSP problem - loadProblem will throw on parse errors
        try
        {
            Problem p = loadProblem(fullpath);
            results.emplace_back(fullpath, std::move(p));
        }
        catch (...)
        {
            // ignore non-problem files
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);
#else
    // Non-Windows platforms: simple attempt using POSIX dirent if available
    // (left intentionally minimal since the project avoids std::filesystem)
    // If dirent isn't available, return empty list.
    // Keep this branch safe/portable.
#endif
    return results;
}

#ifdef _WIN32
// Open a native folder-selection dialog and return the selected path (UTF-8), or empty if cancelled.
static std::string pickFolderWindows()
{
    std::string result;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool coInitialized = SUCCEEDED(hr);
    IFileDialog* pfd = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (SUCCEEDED(hr) && pfd)
    {
        DWORD options = 0;
        if (SUCCEEDED(pfd->GetOptions(&options)))
        {
            options |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM;
            pfd->SetOptions(options);
        }

        // Show dialog
        hr = pfd->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi)) && psi)
            {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath)) && pszFilePath)
                {
                    // Convert wide string to UTF-8
                    int required = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, NULL, 0, NULL, NULL);
                    if (required > 0)
                    {
                        std::string utf8;
                        utf8.resize(required - 1);
                        WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, &utf8[0], required, NULL, NULL);
                        result = utf8;
                    }
                    CoTaskMemFree(pszFilePath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
    if (coInitialized)
        CoUninitialize();
    return result;
}
#endif

int main()
{
    // Init GLFW + ImGui
    if (!glfwInit())
        return -1;
    GLFWwindow* window = glfwCreateWindow(900, 600, "JSSP Problem Browser", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Simple folder path input for scanning a folder of problems
    char folderpath_buf[260] = "data";
    Problem selectedProblem;
    bool hasProblem = false;
    std::vector<ScheduledOp> lastSchedule;
    bool showGantt = false; // toggle to display in-GUI Gantt

    // container for discovered problems: pair<fullpath, Problem>
    std::vector<std::pair<std::string, Problem>> problems;
    int selectedIndex = -1;

    // store selected problem source path so exports default next to the problem file
    std::string selectedProblemPath;
    // last exported path (display feedback)
    std::string lastExportPath;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("JSSP Problem Browser");

        ImGui::Text("Load a folder containing JSSP problem files:");
        ImGui::InputText("Folder path", folderpath_buf, sizeof(folderpath_buf));
        ImGui::SameLine();
        if (ImGui::Button("Load Folder"))
        {
            problems = loadProblemsFromFolder(std::string(folderpath_buf));
            selectedIndex = -1;
            hasProblem = false;
            lastSchedule.clear();
            selectedProblemPath.clear();
            lastExportPath.clear();
        }

        ImGui::Separator();

        // Problems list
        ImGui::Text("Problems found: %d", (int)problems.size());
        ImGui::BeginChild("problems_list", ImVec2(300, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (int i = 0; i < (int)problems.size(); ++i)
        {
            const Problem& p = problems[i].second;
            const std::string& fullpath = problems[i].first;
            // show filename in list
            if (ImGui::Selectable((std::to_string(i) + " - " + p.name).c_str(), selectedIndex == i))
            {
                selectedIndex = i;
                selectedProblem = p;
                selectedProblemPath = fullpath; // remember source path for export location
                hasProblem = true;
                lastSchedule.clear();
                lastExportPath.clear();
            }

            // Hover tooltip with problem details
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("File: %s", fullpath.c_str());
                ImGui::Separator();
                ImGui::Text("Name: %s", p.name.c_str());
                ImGui::Text("Jobs: %d", p.numJobs);
                ImGui::Text("Machines: %d", p.numMachines);
                int totalOps = 0;
                int minDur = INT_MAX, maxDur = INT_MIN;
                long long sumDur = 0;
                for (const auto& job : p.jobs)
                {
                    totalOps += (int)job.operations.size();
                    for (const auto& op : job.operations)
                    {
                        sumDur += op.duration;
                        minDur = std::min(minDur, op.duration);
                        maxDur = std::max(maxDur, op.duration);
                    }
                }
                double avgDur = (totalOps > 0) ? (double)sumDur / (double)totalOps : 0.0;
                ImGui::Text("Total operations: %d", totalOps);
                ImGui::Text("Operations per job: %d", p.numMachines);
                ImGui::Text("Avg duration / op: %.2f", avgDur);
                if (totalOps > 0)
                {
                    ImGui::Text("Min duration: %d", minDur);
                    ImGui::Text("Max duration: %d", maxDur);
                }
                ImGui::EndTooltip();
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Selected problem panel, solver and export controls (keeps original behavior)
        ImGui::BeginGroup();
        if (hasProblem)
        {
            ImGui::Text("Problem: %s", selectedProblem.name.c_str());
            ImGui::Text("Jobs: %d", selectedProblem.numJobs);
            ImGui::Text("Machines: %d", selectedProblem.numMachines);
            ImGui::Text("Operations per job: %d", selectedProblem.numMachines);

            if (ImGui::Button("Solve Problem"))
            {
                lastSchedule = solveJSSP(selectedProblem);
                showGantt = true;
                lastExportPath.clear();
            }

            if (!lastSchedule.empty())
            {
                int makespan = 0;
                for (auto& op : lastSchedule)
                    if (op.finish > makespan)
                        makespan = op.finish;
                ImGui::Text("Solution Makespan: %d", makespan);

                if (ImGui::Button("Export Solution (TXT)"))
                {
                    // Open native folder-selection dialog so user can choose where to save solution
                    std::string chosenFolder;
#ifdef _WIN32
                    chosenFolder = pickFolderWindows();
#else
                    // Fallback: default to current folder if no native dialog implemented for the platform
                    chosenFolder = "";
#endif
                    if (!chosenFolder.empty())
                    {
#ifdef _WIN32
                        const char sep = '\\';
#else
                        const char sep = '/';
#endif
                        std::string outpath = chosenFolder + sep + selectedProblem.name + "_solution.txt";
                        exportSolutionTXT(selectedProblem, lastSchedule, outpath);
                        lastExportPath = outpath;
                    }
                    else
                    {
                        // user cancelled folder pick: do nothing (optional: you can show a small popup/notification)
                    }
                }
                // feedback showing where file was written (helps avoid confusion about working directory)
                if (!lastExportPath.empty())
                {
                    ImGui::Text("Exported file: %s", lastExportPath.c_str());
                }

                ImGui::SameLine();
                ImGui::Checkbox("Show Gantt Chart", &showGantt);

                if (showGantt)
                {
                    ImGui::Separator();
                    // drawGanttUI implementation is unchanged and still present below
                    extern void drawGanttUI(const std::vector<ScheduledOp>&schedule, int numMachines, int numJobs);
                    drawGanttUI(lastSchedule, selectedProblem.numMachines, selectedProblem.numJobs);
                }
            }
        }
        else
        {
            ImGui::TextWrapped("No problem selected. Load a folder and select a problem from the list.");
        }
        ImGui::EndGroup();

        ImGui::End();
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        // Correct glViewport usage: x, y, width, height
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// Provide global (non-static) Gantt UI implementation so the symbol used in main is defined.
void drawGanttUI(const std::vector<ScheduledOp>& schedule, int numMachines, int /*numJobs*/)
{
    if (schedule.empty())
    {
        ImGui::Text("No schedule to display.");
        return;
    }

    // compute makespan
    int makespan = 0;
    for (const auto& op : schedule)
        makespan = std::max(makespan, op.finish);
    if (makespan <= 0)
        makespan = 1;

    // layout
    float avail_w = ImGui::GetContentRegionAvail().x;
    float row_h = 18.0f;         // slightly reduced to be more compact
    float spacing = 4.0f;        // less vertical spacing for compactness
    float total_h = (row_h + spacing) * numMachines + 36.0f; // extra for ticks

    // persistent zoom state for the Gantt (persists across frames)
    static float zoom = 1.0f; // 1.0 == fit to available width
    // persistent horizontal scroll offset in pixels (manual)
    static float scroll_px = 0.0f;

    // Compute pixel-per-time that exactly fits the entire makespan into available width
    float px_per_time_fit = avail_w / (float)makespan;
    // Provide a small UI to control zoom; default zoom=1 fits chart into available width
    ImGui::Text("Gantt zoom:");
    ImGui::SameLine();
    if (ImGui::Button("Fit"))
    {
        zoom = 1.0f;
        scroll_px = 0.0f;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("##zoom", &zoom, 0.25f, 32.0f, "%.2fx"); // zoom range 

    // final px_per_time is the fit value multiplied by zoom factor.
    float px_per_time = px_per_time_fit * zoom;
    if (px_per_time < 0.5f) px_per_time = 0.5f; // safety clamp

    // chart width derived from px_per_time
    float chart_w = px_per_time * (float)makespan;
    // Ensure chart is at least available width so it occupies the region when zoom <= 1
    chart_w = std::max(chart_w, avail_w);

    // Maximum allowed scroll (in pixels) when chart is wider than visible area
    float scroll_max = std::max(0.0f, chart_w - avail_w);

    // Clamp scroll_px to valid range
    if (scroll_px < 0.0f) scroll_px = 0.0f;
    if (scroll_px > scroll_max) scroll_px = scroll_max;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // header with time axis
    ImGui::Text("Time axis: 0 .. %d", makespan);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // Cap the child height so the control stays compact; vertical scrollbar will appear if needed.
    const float maxChildHeight = std::min(total_h, 480.0f);

    // Begin child sized to available width (we implement manual horizontal scroll with slider).
    ImGui::BeginChild("gantt_child", ImVec2(avail_w, maxChildHeight), true, ImGuiWindowFlags_NoScrollWithMouse);
    ImVec2 cp = ImGui::GetCursorScreenPos();

    // base X for drawing (apply manual scroll offset)
    float baseX = cp.x - scroll_px;

    // Push a clipping rectangle equal to the visible area of the child so anything outside is hidden
    draw_list->PushClipRect(ImVec2(cp.x, cp.y), ImVec2(cp.x + avail_w, cp.y + maxChildHeight), true);

    // draw background rows (cover full chart_w)
    for (int m = 0; m < numMachines; ++m)
    {
        float y0 = cp.y + m * (row_h + spacing);
        float y1 = y0 + row_h;
        ImU32 col = ImGui::GetColorU32((m % 2 == 0) ? ImGuiCol_FrameBg : ImGuiCol_FrameBgHovered);
        draw_list->AddRectFilled(ImVec2(baseX, y0), ImVec2(baseX + chart_w, y1), col, 2.0f);
        // machine label (fixed at left inside the visible area)
        draw_list->AddText(ImVec2(cp.x + 4, y0 + 2), ImGui::GetColorU32(ImGuiCol_Text), ("M" + std::to_string(m)).c_str());
    }

    // draw operations (shifted by baseX)
    for (const auto& op : schedule)
    {
        if (op.machineId < 0 || op.machineId >= numMachines)
            continue;
        float x0 = baseX + op.start * px_per_time;
        float x1 = baseX + op.finish * px_per_time;
        // don't draw zero-width bars
        if (x1 <= x0 + 1.0f)
            x1 = x0 + 1.0f;
        float y0 = cp.y + op.machineId * (row_h + spacing) + 2.0f;
        float y1 = y0 + row_h - 4.0f;
        // color by job id
        int j = op.jobId % 7;
        ImU32 color;
        switch (j)
        {
        case 0:
            color = IM_COL32(102, 194, 165, 255);
            break;
        case 1:
            color = IM_COL32(252, 141, 98, 255);
            break;
        case 2:
            color = IM_COL32(141, 160, 203, 255);
            break;
        case 3:
            color = IM_COL32(231, 138, 195, 255);
            break;
        case 4:
            color = IM_COL32(166, 216, 84, 255);
            break;
        case 5:
            color = IM_COL32(255, 217, 47, 255);
            break;
        default:
            color = IM_COL32(204, 204, 204, 255);
            break;
        }
        draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, 3.0f);

        // label: only draw when there is enough horizontal space to avoid overlap
        std::string label = "J" + std::to_string(op.jobId) + " (" + std::to_string(op.start) + "-" + std::to_string(op.finish) + ")";
        ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        const float labelPadding = 8.0f;
        float barWidth = x1 - x0;
        if (barWidth > (labelSize.x + labelPadding))
        {
            // draw label inside the bar
            draw_list->AddText(ImVec2(x0 + 4, y0 + 2), IM_COL32(10, 10, 10, 255), label.c_str());
        }
        else
        {
            // compact label (job id only) centered if possible
            std::string compact = "J" + std::to_string(op.jobId);
            ImVec2 compactSize = ImGui::CalcTextSize(compact.c_str());
            if (barWidth > (compactSize.x + 6.0f))
            {
                float cx = x0 + (barWidth) * 0.5f - compactSize.x * 0.5f;
                draw_list->AddText(ImVec2(cx, y0 + 2), IM_COL32(10, 10, 10, 255), compact.c_str());
            }
            // otherwise skip label to avoid visual clutter
        }
    }

    // time ticks - space tick labels by pixel distance to avoid overlaps
    const float minLabelSpacingPx = 60.0f; // minimal pixels between tick labels
    int tickStep = 1;
    if (px_per_time > 0.0f)
    {
        tickStep = std::max(1, (int)std::ceil(minLabelSpacingPx / px_per_time));
    }

    for (int tv = 0; tv <= makespan; tv += tickStep)
    {
        float x = baseX + tv * px_per_time;
        draw_list->AddLine(ImVec2(x, cp.y), ImVec2(x, cp.y + total_h - 20.0f), IM_COL32(0, 0, 0, 40));
        std::string tlabel = std::to_string(tv);
        draw_list->AddText(ImVec2(x + 2.0f, cp.y + total_h - 25.0f), IM_COL32(255, 255, 255, 200), tlabel.c_str()); // color and height of time ticks
    }
    // ensure last tick at makespan is drawn
    {
        float x = baseX + makespan * px_per_time;
        std::string tlabel = std::to_string(makespan);
        draw_list->AddLine(ImVec2(x, cp.y), ImVec2(x, cp.y + total_h - 20.0f), IM_COL32(0, 0, 0, 40));
        draw_list->AddText(ImVec2(x + 2.0f, cp.y + total_h - 25.0f), IM_COL32(255, 255, 255, 200), tlabel.c_str());
    }

    // Pop the clip rectangle so subsequent UI isn't clipped
    draw_list->PopClipRect();

    // allow mouse-wheel horizontal scroll when hovering the child
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
    {
        // horizontal wheel if available; fallback to vertical wheel
        float wheelH = io.MouseWheelH;
        float wheelV = io.MouseWheel;
        if (wheelH != 0.0f)
            scroll_px = std::clamp(scroll_px - wheelH * 60.0f, 0.0f, scroll_max);
        else if (wheelV != 0.0f && io.KeyShift) // common convention: shift+wheel -> horizontal
            scroll_px = std::clamp(scroll_px - wheelV * 60.0f, 0.0f, scroll_max);
    }

    ImGui::Dummy(ImVec2(0.0f, total_h));
    ImGui::EndChild();

    // Horizontal slider below the child to control manual scroll_px
    if (scroll_max > 0.0f)
    {
        // keep slider width same as available content
        ImGui::SetNextItemWidth(avail_w);
        float prev = scroll_px;
        if (ImGui::SliderFloat("Scroll", &scroll_px, 0.0f, scroll_max, "Offset: %.0f px"))
        {
            // changed by user via slider; clamp handled above
            scroll_px = std::clamp(scroll_px, 0.0f, scroll_max);
        }
        // small tip: dragging inside the chart also updates scroll_px via mouse wheel handling above
        if (prev != scroll_px)
        {
            // nothing extra needed; drawing uses scroll_px on next frame
        }
    }
    else
    {
        // chart fits in view: show disabled slider to indicate no scrolling available
        ImGui::SetNextItemWidth(avail_w);
        ImGui::BeginDisabled();
        float dummy = 0.0f;
        ImGui::SliderFloat("Scroll", &dummy, 0.0f, 0.0f, "No horizontal scroll");
        ImGui::EndDisabled();
    }
}