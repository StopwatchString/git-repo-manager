#include "glh/classes/OpenGLApplication.h"
#include "cpputils/windows/selectors.h"

#include "GLFW/glfw3.h"
#include "git2.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <array>
#include <filesystem>
#include <mutex>
#include <optional>

enum class GitState {
    NONE,
    UPTODATE,
    PUSH,
    PULL,
    DIVERGED,
    REBASE
};

struct GitRepo {
    GitState state{ GitState::NONE };
    git_repository* repo{ nullptr };
    std::filesystem::path repoPath{ "" };
};

std::string baseDirectory = "C:\\dev";
bool reloadDirectory = true;
bool startup = true;
std::vector<GitRepo> gitRepos;
std::mutex gitDirectoriesLock;

GitState git_check(const std::filesystem::path& repo_path) {
    // Open Repo
    git_repository* repo = nullptr;
    int error = git_repository_open(&repo, repo_path.string().c_str());
    if (error != 0) {
        const git_error* e = git_error_last();
        std::cerr << "Error opening repository: " << (e && e->message ? e->message : "Unknown error") << std::endl;
        return GitState::NONE;
    }

    // Get reference to repo head
    git_reference* head_ref = nullptr;
    error = git_repository_head(&head_ref, repo);
    if (error != 0) {
        const git_error* e = git_error_last();
        std::cerr << "Error retrieving HEAD: " << git_error_last()->message << std::endl;
        git_repository_free(repo);
        return GitState::NONE;
    }

    // Get current branch name
    const char* branch_name = nullptr;
    git_branch_name(&branch_name, head_ref);
    if (branch_name == nullptr) {
        std::cerr << "Error determining branch name." << std::endl;
        git_reference_free(head_ref);
        git_repository_free(repo);
        return GitState::NONE;
    }

    // Get upstream branch
    git_reference* upstream_ref = nullptr;
    error = git_branch_upstream(&upstream_ref, head_ref);
    if (error != 0) {
        if (error == GIT_ENOTFOUND) {
            std::cout << "No upstream branch configured." << std::endl;
        }
        else {
            std::cerr << "Error getting upstream branch: " << git_error_last()->message << std::endl;
        }
        git_reference_free(head_ref);
        git_repository_free(repo);
        return GitState::NONE;
    }

    // Get upstream branch name
    const char* upstream_branch_name = nullptr;
    git_branch_name(&upstream_branch_name, upstream_ref);

    // Compare local and upstream branches
    const git_oid* local_oid = git_reference_target(head_ref);
    const git_oid* upstream_oid = git_reference_target(upstream_ref);
    size_t ahead = 0, behind = 0;
    error = git_graph_ahead_behind(&ahead, &behind, repo, local_oid, upstream_oid);

    // Get repoState string
    GitState state;
    if (error != 0) {
        std::cerr << "Error calculating ahead/behind: " << git_error_last()->message << std::endl;
    }
    else {
        if (ahead == 0 && behind == 0) {
            state = GitState::UPTODATE;
        }
        else if (ahead == 0 && behind > 0) {
            state = GitState::PULL;
        }
        else if (ahead > 0 && behind == 0) {
            state = GitState::PUSH;
        }
        else {
            state = GitState::DIVERGED;
        }
    }

    git_reference_free(upstream_ref);
    git_reference_free(head_ref);
    git_repository_free(repo);
    return state;
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
        
        reloadDirectory = ImGui::Button("Rescan");
        ImGui::SameLine();
        if (ImGui::Button("Choose Folder")) {
            std::string result = OpenWindowsFolderDialogue();
            if (result.size() > 0) {
                baseDirectory = std::move(result);
                reloadDirectory = true;
            }
        }
        ImGui::SameLine();
        ImGui::Text(baseDirectory.c_str());

        if (gitDirectoriesLock.try_lock()) {
            if (gitRepos.size() > 0) {
                for (const GitRepo& repo : gitRepos) {
                    ImGui::Text(repo.repoPath.parent_path().string().c_str());
                }
            }
            else {
                ImGui::Text("No Git Directories found!");
            }
            gitDirectoriesLock.unlock();
        }
        else {
            ImGui::Text("Scanning....");
        }

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
    if (reloadDirectory || startup) {
        std::lock_guard<std::mutex> lock(gitDirectoriesLock);
        gitRepos.clear();

        std::filesystem::path root = baseDirectory.data();
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_directory() && entry.path().filename() == ".git") {
                    GitState repoState = git_check(entry.path());
                    if (repoState != GitState::NONE) {
                        GitRepo repo;
                        repo.repo;
                        repo.repoPath = entry.path();
                        repo.state = repoState;
                        gitRepos.push_back(repo);
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Error accessing " << root << ": " << e.what() << std::endl;
        }

        reloadDirectory = false;
    }

    startup = false;
}

//--------------------------------------
// main()
//--------------------------------------
int main()
{
    git_libgit2_init();

    OpenGLApplication::ApplicationConfig appConfig;
    appConfig.windowName = "GitRepoManager";
    appConfig.windowInitWidth = 1000;
    appConfig.windowInitHeight = 600;
    appConfig.windowPosX = 100;
    appConfig.windowPosY = 100;
    appConfig.windowBorderless = false;
    appConfig.windowResizeEnable = true;
    appConfig.windowDarkmode = true;
    appConfig.windowRounded = true;
    appConfig.windowAlwaysOnTop = false;
    appConfig.vsyncEnable = true;
    appConfig.transparentFramebuffer = false;
    appConfig.glVersionMajor = 4;
    appConfig.glVersionMinor = 6;
    appConfig.glslVersionString = "#version 460"; // Used for DearImgui, leave default unless you know what to put here
    appConfig.imguiIniFileName = nullptr;
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

    git_libgit2_shutdown();

    return EXIT_SUCCESS;
}

int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd
)
{
    main();
}