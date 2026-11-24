#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>

const int MAX_POINTS = 4000;

struct Point {
    float I;
    float Q;
};
bool load_file(const char *path, std::vector<Point> &out){
    std::ifstream f(path);
    if (!f.is_open())
        return false;

    out.clear();
    float I, Q;
    while (f >> I >> Q) {
        out.push_back({I, Q});
    }

    if ((int)out.size() > MAX_POINTS)
        out.erase(out.begin(), out.end() - MAX_POINTS);

    return !out.empty();
}

int main(){
    if (!glfwInit())
        return 1;

    const char *glsl = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow *window = glfwCreateWindow(1000, 700, "RX: time + constellation", NULL, NULL);
    if (!window)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl);

    std::vector<Point> pts;
    std::vector<float> xs, Is, Qs;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        load_file("qpsk_symbols.txt", pts);

        xs.clear();
        Is.clear();
        Qs.clear();
        for (int i = 0; i < (int)pts.size(); i++) {
            xs.push_back((float)i);
            Is.push_back(pts[i].I);
            Qs.push_back(pts[i].Q);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("RX viewer", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

ImVec2 avail = ImGui::GetContentRegionAvail();

float plot_width = (avail.x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;

float plot_height = std::min(avail.y, 450.0f);

// левый график — I/Q
if (ImPlot::BeginPlot("I/Q", ImVec2(plot_width, plot_height))) {
    ImPlot::SetupAxes("sample", "amplitude");

    if (!xs.empty()) {
        ImPlot::PlotLine("I", xs.data(), Is.data(), (int)xs.size());
        ImPlot::PlotLine("Q", xs.data(), Qs.data(), (int)xs.size());
    }

    ImPlot::EndPlot();
}

ImGui::SameLine();

if (ImPlot::BeginPlot("BPSK/QPSK", ImVec2(plot_width, plot_height))) {
    ImPlot::SetupAxes("I", "Q");
    ImPlot::SetupAxesLimits(-2500, 2500, -2500, 2500, ImPlotCond_Once);

    if (!Is.empty()) {
        ImPlot::PlotScatter("symbols", Is.data(), Qs.data(), (int)Is.size());
    }

    ImPlot::EndPlot();
}

        ImGui::End();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}