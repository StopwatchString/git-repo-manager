#include "glh/classes/OpenGLApplication.h"

#include "GLFW/glfw3.h"
#include "git2.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <array>
#include <filesystem>
#include <mutex>

constexpr size_t MAX_IMGUI_STRING_INPUT_SIZE = 512;

std::array<char, MAX_IMGUI_STRING_INPUT_SIZE> baseDirectory = { "C:\\dev\\" };
bool reloadDirectory = false;
std::vector<std::filesystem::path> gitDirectories;
std::mutex gitDirectoriesLock;
std::string gitStatus;

void check_git_status(const std::filesystem::path& repo_path) {
    // Initialize libgit2
    git_libgit2_init();

    git_repository* repo = nullptr;
    int error = git_repository_open(&repo, repo_path.string().c_str());

    if (error != 0) {
        const git_error* e = git_error_last();
        std::cerr << "Error opening repository: " << (e && e->message ? e->message : "Unknown error") << std::endl;
        git_libgit2_shutdown();
        return;
    }

    std::cout << "Repository opened successfully: " << repo_path << std::endl;

    // Get repository status
    git_status_options status_opts;
    git_status_options_init(&status_opts, GIT_STATUS_OPTIONS_VERSION);
    status_opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR; // Show both index and working directory
    status_opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED; // Include untracked files

    git_status_list* status_list = nullptr;
    error = git_status_list_new(&status_list, repo, &status_opts);

    if (error != 0) {
        const git_error* e = git_error_last();
        std::cerr << "Error getting repository status: " << (e && e->message ? e->message : "Unknown error") << std::endl;
        git_repository_free(repo);
        git_libgit2_shutdown();
        return;
    }

    size_t status_count = git_status_list_entrycount(status_list);
    std::cout << "Number of status entries: " << status_count << std::endl;

    for (size_t i = 0; i < status_count; ++i) {
        const git_status_entry* entry = git_status_byindex(status_list, i);
        if (!entry || !entry->head_to_index || !entry->head_to_index->old_file.path)
            continue;

        std::cout << "File: " << entry->head_to_index->old_file.path << " ";

        if (entry->status & GIT_STATUS_INDEX_NEW) std::cout << "[Index New]";
        if (entry->status & GIT_STATUS_INDEX_MODIFIED) std::cout << "[Index Modified]";
        if (entry->status & GIT_STATUS_INDEX_DELETED) std::cout << "[Index Deleted]";
        if (entry->status & GIT_STATUS_WT_NEW) std::cout << "[Working Tree New]";
        if (entry->status & GIT_STATUS_WT_MODIFIED) std::cout << "[Working Tree Modified]";
        if (entry->status & GIT_STATUS_WT_DELETED) std::cout << "[Working Tree Deleted]";
        if (entry->status & GIT_STATUS_IGNORED) std::cout << "[Ignored]";

        std::cout << std::endl;
    }

    // Cleanup
    git_status_list_free(status_list);
    git_repository_free(repo);
    git_libgit2_shutdown();
}

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
        if (gitDirectoriesLock.try_lock()) {
            for (const std::filesystem::path& gitDirectory : gitDirectories) {
                ImGui::Text(gitDirectory.string().c_str());
            }
            gitDirectoriesLock.unlock();
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
        std::lock_guard<std::mutex> lock(gitDirectoriesLock);
        gitDirectories.clear();

        std::filesystem::path root = baseDirectory.data();
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_directory() && entry.path().filename() == ".git") {
                    gitDirectories.push_back(entry.path());
                    check_git_status(entry.path());
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error accessing " << root << ": " << e.what() << std::endl;
        }
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