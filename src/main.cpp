#include "glh/classes/OpenGLApplication.h"

#include "GLFW/glfw3.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <array>
#include <filesystem>

constexpr size_t MAX_IMGUI_STRING_INPUT_SIZE = 512;

std::array<char, MAX_IMGUI_STRING_INPUT_SIZE> baseDirectory = { "C:\\dev\\" };
bool reloadDirectory = false;
std::vector<std::filesystem::path> gitDirectories;
std::string gitStatus;

//--------------------------------------
// render()
//--------------------------------------
void render(GLFWwindow* window)
{
    glfwMakeContextCurrent(window);

    while (!glfwWindowShouldClose(window)) {

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Create window which fills viewport
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Imgui Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::InputText("Base Directory", baseDirectory.data(), baseDirectory.size());
        reloadDirectory = ImGui::Button("Reload Directory");
        for (const std::filesystem::path& gitDirectory : gitDirectories) {
            ImGui::Text(gitDirectory.string().c_str());
        }

        ImGui::Spacing();

        ImGui::Text(gitStatus.c_str());

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glhErrorCheck("End of render loop");
    }
}

//--------------------------------------
// poll()
//--------------------------------------
void poll()
{
    if (reloadDirectory) {
        gitDirectories.clear();

        std::filesystem::path root = baseDirectory.data();
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_directory() && entry.path().filename() == ".git") {
                    gitDirectories.push_back(entry.path());
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error accessing " << root << ": " << e.what() << std::endl;
        }

        gitDirectories.push_back(std::filesystem::path("Done!"));

        static const std::string command("git status");
        std::shared_ptr<FILE> pipe(_popen(command.c_str(), "r"), _pclose);

        if (!pipe) {
            std::cout << "PIPE FAILED" << std::endl;
        }
        else {
            char buffer[128];
            gitStatus.clear();
            while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
                gitStatus += buffer;
            }
        }

        reloadDirectory = false;
    }
}

//--------------------------------------
// main()
//--------------------------------------
int main()
{
    OpenGLApplication::ApplicationConfig appConfig;
    appConfig.windowName = "GitRepoManager";
    appConfig.windowInitWidth = 2000;
    appConfig.windowInitHeight = 800;
    appConfig.windowPosX = 100;
    appConfig.windowPosY = 100;
    appConfig.windowBorderless = false;
    appConfig.windowResizeEnable = false;
    appConfig.windowDarkmode = true;
    appConfig.windowRounded = true;
    appConfig.windowAlwaysOnTop = false;
    appConfig.vsyncEnable = true;
    appConfig.transparentFramebuffer = false;
    appConfig.glVersionMajor = 4;
    appConfig.glVersionMinor = 6;
    appConfig.glslVersionString = "#version 460"; // Used for DearImgui, leave default unless you know what to put here
    appConfig.customDrawFunc = render;      // std::function<void(GLFWwindow*)>
    appConfig.customKeyCallback = nullptr;   // std::function<void(GLFWwindow* window, int key, int scancode, int action, int mods)>
    appConfig.customErrorCallback = nullptr; // std::function<void(int error_code, const char* description)>
    appConfig.customDropCallback = nullptr;  // std::function<void(GLFWwindow* window, int count, const char** paths)>
    appConfig.customPollingFunc = poll;   // std::function<void()>

    try {
        OpenGLApplication application(appConfig);
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}