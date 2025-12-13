#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdio>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <cmath>

// no std::filesystem to keep dependencies minimal

struct Operation
{
    int jobId;
    int machineId;
    int duration;
};

struct Job
{
    std::vector<Operation> operations;
};

struct MachineSchedule
{
    int time = 0;
};

struct ScheduledOp
{
    int jobId;
    int machineId;
    int start;
    int finish;
};

struct Problem
{
    std::string name;
    int numJobs;
    int numMachines;
    std::vector<Job> jobs;
};

// Helper: extract filename from a path without <filesystem>
static std::string filenameFromPath(const std::string& path)
{
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

// Load JSSP problem from file
Problem loadProblem(const std::string& filepath)
{
    Problem p;
    p.name = filenameFromPath(filepath);
    std::ifstream in(filepath);
    if (!in)
        throw std::runtime_error("Cannot open file: " + filepath);

    in >> p.numJobs >> p.numMachines;
    if (!in || p.numJobs <= 0 || p.numMachines <= 0)
        throw std::runtime_error("Invalid header in problem file: " + filepath);

    p.jobs.clear();
    p.jobs.resize(p.numJobs);

    for (int j = 0; j < p.numJobs; j++)
    {
        for (int o = 0; o < p.numMachines; o++)
        {
            int m, d;
            in >> m >> d;
            if (!in)
                throw std::runtime_error("Unexpected EOF or parse error in file: " + filepath);
            if (m < 0 || m >= p.numMachines)
                throw std::runtime_error("Machine id out of range in file: " + filepath);
            if (d < 0)
                throw std::runtime_error("Negative duration in file: " + filepath);
            p.jobs[j].operations.push_back({ j, m, d });
        }
    }
    return p;
}

// Greedy solver
std::vector<ScheduledOp> solveJSSP(const Problem& p)
{
    std::vector<MachineSchedule> machines(p.numMachines);
    std::vector<int> jobCompletionTime(p.numJobs, 0);
    std::vector<ScheduledOp> schedule;

    for (auto& job : p.jobs)
    {
        for (auto& op : job.operations)
        {
            int startTime = std::max(jobCompletionTime[op.jobId], machines[op.machineId].time);
            int finishTime = startTime + op.duration;
            machines[op.machineId].time = finishTime;
            jobCompletionTime[op.jobId] = finishTime;
            schedule.push_back({ op.jobId, op.machineId, startTime, finishTime });
        }
    }
    return schedule;
}

// Export solution to simple TXT
void exportSolutionTXT(const Problem& p, const std::vector<ScheduledOp>& schedule, const std::string& filename)
{
    std::ofstream out(filename);
    if (!out)
    {
        std::cerr << "Error: Cannot write to " << filename << "\n";
        return;
    }
    int makespan = 0;
    for (auto& op : schedule)
        if (op.finish > makespan)
            makespan = op.finish;

    out << "Problem: " << p.name << "\n";
    out << "Jobs: " << p.numJobs << "\n";
    out << "Machines: " << p.numMachines << "\n";
    out << "Makespan: " << makespan << "\n";
    out << "Schedule (job machine start finish):\n";
    for (auto& op : schedule)
    {
        out << op.jobId << " " << op.machineId << " " << op.start << " " << op.finish << "\n";
    }
    std::cout << "Solution exported to " << filename << "\n";
}

// draw a simple Gantt chart inside ImGui
static void drawGanttUI(const std::vector<ScheduledOp>& schedule, int numMachines, int numJobs)
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

    // Simple file path input rather than folder scanning
    char filepath_buf[260] = "problems/example.txt";
    Problem selectedProblem;
    bool hasProblem = false;
    std::vector<ScheduledOp> lastSchedule;
    bool showGantt = false; // toggle to display in-GUI Gantt

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("JSSP Problem Browser");

        ImGui::Text("Load problem file (enter full path):");
        ImGui::InputText("File path", filepath_buf, sizeof(filepath_buf));
        ImGui::SameLine();
        if (ImGui::Button("Load Problem"))
        {
            try
            {
                selectedProblem = loadProblem(std::string(filepath_buf));
                lastSchedule.clear();
                hasProblem = true;
            }
            catch (const std::exception& e)
            {
                hasProblem = false;
                std::cerr << e.what() << "\n";
            }
        }

        if (hasProblem)
        {
            ImGui::Separator();
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
                    drawGanttUI(lastSchedule, selectedProblem.numMachines, selectedProblem.numJobs);
                }
            }
        }
        ImGui::End();
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
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
