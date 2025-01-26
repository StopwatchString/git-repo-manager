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
#include <unordered_map>

//--------------------------------------
// enum GitState
//--------------------------------------
enum class GitState {
    NONE,
    UPTODATE,
    PUSH,
    PULL,
    DIVERGED,
    REBASE
};

//--------------------------------------
// GitStateToString()
//--------------------------------------
std::string GitStateToString(const GitState& state)
{
    std::string stateStr;
    switch (state) {
    case GitState::NONE:
        stateStr = "NONE";
        break;
    case GitState::UPTODATE:
        stateStr = "UP-TO-DATE";
        break;
    case GitState::PUSH:
        stateStr = "PUSH";
        break;
    case GitState::PULL:
        stateStr = "PULL";
        break;
    case GitState::DIVERGED:
        stateStr = "DIVERGED";
        break;
    case GitState::REBASE:
        stateStr = "REBASE";
        break;
    default:
        break;
    }
    return stateStr;
}

//--------------------------------------
// struct GitRepo
//--------------------------------------
struct GitRepo {
    GitState state{ GitState::NONE };
    git_repository* repo{ nullptr };
    std::filesystem::path repoPath{ "" };
};

bool startup = true;
std::string baseDirectory = "C:\\dev";
bool reloadDirectory = true;
std::vector<GitRepo> gitRepos;
std::mutex gitDirectoriesLock;
size_t gitStatusSize = 0;;

//--------------------------------------
// getRepoState()
//--------------------------------------
GitState getRepoState(git_repository* repo)
{
    // Get reference to repo head
    git_reference* head_ref = nullptr;
    int error = git_repository_head(&head_ref, repo);
    if (error != 0) {
        const git_error* e = git_error_last();
        std::cerr << "Error retrieving HEAD: " << git_error_last()->message << std::endl;
        return GitState::NONE;
    }

    // Get current branch name
    const char* branch_name = nullptr;
    git_branch_name(&branch_name, head_ref);
    if (branch_name == nullptr) {
        std::cerr << "Error determining branch name." << std::endl;
        git_reference_free(head_ref);
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

    // Determine repostate
    GitState state = GitState::NONE;
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
    return state;
}

//--------------------------------------
// makeGitRepo()
//--------------------------------------
std::optional<GitRepo> makeGitRepo(const std::filesystem::path& repoPath)
{
    GitRepo gitRepo;

    // Open Repo
    gitRepo.repo = nullptr;
    int error = git_repository_open(&gitRepo.repo, repoPath.string().c_str());
    if (error != 0) {
        const git_error* e = git_error_last();
        std::cerr << "Error opening repository: " << (e && e->message ? e->message : "Unknown error") << std::endl;
        return std::nullopt;
    }

    // Get repo state
    std::optional<GitState> state = getRepoState(gitRepo.repo);
    if (!state.has_value()) {
        std::cerr << "Error getting repository state: " << repoPath << std::endl;
        git_repository_free(gitRepo.repo);
        return std::nullopt;
    }
    gitRepo.state = state.value();

    // Set repo path
    gitRepo.repoPath = repoPath;

    return gitRepo;
}

//--------------------------------------
// renderGitState()
//--------------------------------------
void renderGitState(const GitState& state)
{
    std::string stateStr = GitStateToString(state);

    ImGui::Text("[");
    ImGui::SameLine();

    std::string displayStr = GitStateToString(state);
    ImVec2 stateSize = ImGui::CalcTextSize(displayStr.c_str());

    switch (state) {
        case GitState::NONE: {
            constexpr static ImVec4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::UPTODATE: {
            constexpr static ImVec4 color = { 0.21f, 0.77f, 0.1f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::PUSH: {
            constexpr static ImVec4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::PULL: {
            constexpr static ImVec4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::DIVERGED: {
            constexpr static ImVec4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::REBASE: {
            constexpr static ImVec4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        default:
            break;
    }

    ImGui::SameLine();

    ImGui::Text("]");

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(gitStatusSize - stateSize.x, 0));
    
    
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
        
        // Get max padding size for git status zone
        gitStatusSize = ImGui::CalcTextSize("[UP-TO-DATE]").x;

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
                    renderGitState(repo.state);
                    ImGui::SameLine();
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
        
        // Clear exiting repo references
        for (GitRepo& repo : gitRepos) {
            git_repository_free(repo.repo);
        }
        gitRepos.clear();

        std::filesystem::path root = baseDirectory.data();
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
                if (entry.is_directory() && entry.path().filename() == ".git") {
                    std::optional<GitRepo> repo = makeGitRepo(entry.path());
                    if (repo.has_value()) {
                        gitRepos.push_back(repo.value());
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