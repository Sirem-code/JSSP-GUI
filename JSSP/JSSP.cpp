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
    char folderpath_buf[260] = "problems";
    Problem selectedProblem;
    bool hasProblem = false;
    std::vector<ScheduledOp> lastSchedule;
    bool showGantt = false; // toggle to display in-GUI Gantt

    // container for discovered problems: pair<fullpath, Problem>
    std::vector<std::pair<std::string, Problem>> problems;
    int selectedIndex = -1;

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
                hasProblem = true;
                lastSchedule.clear();
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
                    exportSolutionTXT(selectedProblem, lastSchedule, "solution.txt");
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
    float row_h = 22.0f;
    float spacing = 6.0f;
    float chart_w = avail_w;
    float px_per_time = chart_w / (float)makespan;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // header with time axis
    ImGui::Text("Time axis: 0 .. %d", makespan);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // Reserve space for chart
    float total_h = (row_h + spacing) * numMachines + 20.0f;
    ImGui::BeginChild("gantt_child", ImVec2(0, total_h), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImVec2 cp = ImGui::GetCursorScreenPos();

    // draw background rows
    for (int m = 0; m < numMachines; ++m)
    {
        float y0 = cp.y + m * (row_h + spacing);
        float y1 = y0 + row_h;
        ImU32 col = ImGui::GetColorU32((m % 2 == 0) ? ImGuiCol_FrameBg : ImGuiCol_FrameBgHovered);
        draw_list->AddRectFilled(ImVec2(cp.x, y0), ImVec2(cp.x + chart_w, y1), col, 2.0f);
        // machine label
        draw_list->AddText(ImVec2(cp.x + 4, y0 + 4), ImGui::GetColorU32(ImGuiCol_Text), ("M" + std::to_string(m)).c_str());
    }

    // draw operations
    for (const auto& op : schedule)
    {
        if (op.machineId < 0 || op.machineId >= numMachines)
            continue;
        float x0 = cp.x + op.start * px_per_time;
        float x1 = cp.x + op.finish * px_per_time;
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
        // label
        std::string label = "J" + std::to_string(op.jobId) + " (" + std::to_string(op.start) + "-" + std::to_string(op.finish) + ")";
        draw_list->AddText(ImVec2(x0 + 4, y0 + 2), IM_COL32(10, 10, 10, 255), label.c_str());
    }

    // time ticks
    int tickCount = std::min(20, makespan);
    for (int t = 0; t <= tickCount; ++t)
    {
        int tv = (int)std::round((double)t * makespan / (double)tickCount);
        float x = cp.x + tv * px_per_time;
        draw_list->AddLine(ImVec2(x, cp.y), ImVec2(x, cp.y + total_h - 20), IM_COL32(0, 0, 0, 40));
        draw_list->AddText(ImVec2(x + 2, cp.y + total_h - 18), IM_COL32(0, 0, 0, 200), std::to_string(tv).c_str());
    }

    ImGui::Dummy(ImVec2(0.0f, total_h));
    ImGui::EndChild();
}