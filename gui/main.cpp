#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <Windows.h>

#include "imgui_toggle.h"

struct Settings {
    bool active;
    int cps;
    bool breakBlocks;
    bool destruct;
};

void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

bool HandleToggleOn(bool* value) {
    bool previous_value = *value;
    ImGui::Toggle("ON", value, ImGuiToggleFlags_Animated);
    ImGui::SameLine();
    return previous_value != *value;
}

bool HandleToggleBreakBlocks(bool* value) {
    bool previous_value = *value;
    ImGui::Toggle("BREAK BLOCKS", value, ImGuiToggleFlags_Animated);
    return previous_value != *value;
}

bool HandleSliderCPS(int* cpsValue) {
    int previous_value = *cpsValue;
    ImGui::SliderInt("CPS", cpsValue, 1, 20);
    return previous_value != *cpsValue;
}

void sendChange(Settings settings)
{
    HWND hwndReceiver = FindWindowW(L"SettingsReceiverClass", L"SettingsReceiver");

    COPYDATASTRUCT cds;
    cds.dwData = 1;
    cds.cbData = sizeof(Settings);
    cds.lpData = &settings;

    SendMessage(hwndReceiver, WM_COPYDATA, 0, (LPARAM)&cds);
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return -1;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(235, 100, "flexclicker lc-1.21", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_Button] = ImVec4(0.235f, 0.518f, 0.878f, 1.0f);
    style.FrameRounding = 10.0f;
    style.GrabRounding = 10.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    Settings settings = { false, 12, true, false };

    while (!glfwWindowShouldClose(window)) {
        size_t value_index = 0;

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(300, 100));
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("autoclicker", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        if (HandleToggleOn(&settings.active))
        {
            sendChange(settings);
        }

        if (HandleToggleBreakBlocks(&settings.breakBlocks))
        {
            sendChange(settings);
        }

        if (HandleSliderCPS(&settings.cps))
        {
            sendChange(settings);
        }

        ImGui::Spacing();

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.949f, 0.949f, 0.953f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    settings.destruct = true;
    sendChange(settings);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
