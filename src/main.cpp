#include "glh/classes/OpenGLApplication.h"
#include "cpputils/windows/selectors.h"

#include "GLFW/glfw3.h"
#include "git2.h"
#include "gitrepo.h"
#include "cpputils/windows/credential_utils.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <array>
#include <filesystem>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <algorithm>

constexpr bool TEST_REPOS_OVERRIDE = false;
std::string baseDirectory = "C:\\dev";
bool reloadDirectory = true;
std::vector<GitRepo> gitRepos;
std::mutex gitReposLock;
float gitStatusSize = 0.0f;

// Credential Input
std::array<char, 1000> usernameInput;
std::array<char, 1000> credentialInput;
bool credentialHasBeenInput = false;
bool credentialResult = false;

//--------------------------------------
// renderGitState()
//--------------------------------------
void renderGitState(const GitState& state)
{
    std::string stateStr = GitStateToString(state);

    ImGui::Text("[");

    std::string displayStr = GitStateToString(state);
    ImVec2 stateSize = ImGui::CalcTextSize(displayStr.c_str());

    ImGui::SameLine();
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
            constexpr static ImVec4 color = { 0.77f, 0.459f, 0.09f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::FASTFORWARD: {
            constexpr static ImVec4 color = { 0.77f, 0.8f, 0.145f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::DIVERGED: {
            constexpr static ImVec4 color = { 1.0f, 0.0f, 0.0f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::REBASE: {
            constexpr static ImVec4 color = { 0.784f, 0.22f, 0.82f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::PROCESSING: {
            constexpr static ImVec4 color = { 0.1f, 0.1f, 0.9f, 1.0f };
            ImGui::TextColored(color, displayStr.c_str());
            break;
        }
        case GitState::ERROR_STATE: {
            constexpr static ImVec4 color = { 1.0f, 0.1f, 0.1f, 1.0f };
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

        // Directory Selection bar
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

        // Mass repo tools
        ImGui::Text("All Repos: ");
        ImGui::SameLine();
        if (ImGui::Button("Fetch")) {
            if (gitReposLock.try_lock()) {
                for (GitRepo& repo : gitRepos) {
                    if (repo.task == GitTask::NONE) {
                        repo.task = GitTask::FETCH;
                    }
                }
                gitReposLock.unlock();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Fast Forward")) {
            if (gitReposLock.try_lock()) {
                for (GitRepo& repo : gitRepos) {
                    if (repo.task == GitTask::NONE) {
                        repo.task = GitTask::FASTFORWARD;
                    }
                }
                gitReposLock.unlock();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Push")) {
            if (gitReposLock.try_lock()) {
                for (GitRepo& repo : gitRepos) {
                    if (repo.task == GitTask::NONE) {
                        repo.task = GitTask::PUSH;
                    }
                }
                gitReposLock.unlock();
            }
        }

        // Git Repo list
        if (gitReposLock.try_lock()) {
            if (gitRepos.size() > 0) {
                uint16_t id = 0;
                for (GitRepo& repo : gitRepos) {
                    ImGui::PushID(id);

                    if (ImGui::Button("Fetch") && repo.task == GitTask::NONE) {
                        repo.task = GitTask::FETCH;
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Fast Forward") && repo.task == GitTask::NONE) {
                        repo.task = GitTask::FASTFORWARD;
                    }
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Push") && repo.task == GitTask::PUSH) {
                        repo.task = GitTask::PUSH;
                    }

                    ImGui::SameLine();
                    renderGitState(repo.state);

                    ImGui::SameLine();
                    ImGui::Text(repo.repoPath.parent_path().string().c_str());

                    if (ImGui::CollapsingHeader("Info")) {
                        if (repo.task == GitTask::NONE) {
                            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), repo.message.c_str());
                        }
                        else {
                            ImGui::TextColored(ImVec4(0.5f, 0.0f, 0.0f, 1.0f), "Task in progress");
                        }
                    }

                    ImGui::PopID();
                    id++;
                }
            }
            else {
                ImGui::Text("No Git Directories loaded");
            }
            gitReposLock.unlock();
        }
        else {
            ImGui::Text("Scanning....");
        }

        // Credential Input
        ImGui::Spacing();
        ImGui::Text("Credential Input");

        ImGui::InputText("Username", usernameInput.data(), usernameInput.size());
        
        //ImGui::SameLine();
        ImGui::InputText("Git Personal Access Token", credentialInput.data(), credentialInput.size());
        
        //ImGui::SameLine();
        if (ImGui::Button("Submit")) {
            Credential credential = { usernameInput.data(), credentialInput.data() };
            credentialResult = writeCredential(GIT_REPO_MANAGER_CREDENTIAL_TARGE_NAME, credential);
            credentialHasBeenInput = true;
            std::fill(usernameInput.begin(), usernameInput.end(), 0);
            std::fill(credentialInput.begin(), credentialInput.end(), 0);
        }

        if (credentialHasBeenInput) {
            ImGui::SameLine();
            if (credentialResult) {
                ImGui::TextColored(ImVec4(0.21f, 0.77f, 0.1f, 1.0f), "Saved Successfully");
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.1f, 0.1f, 1.0f), "Error Saving Credential");
            }
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
    for (GitRepo& repo : gitRepos) {
        switch (repo.task) {
            case GitTask::FETCH: {
                repo.state = GitState::PROCESSING;
                repo.task = GitTask::PROCESSING;
                std::thread t = std::thread([&]()
                    {
                        fetchRepo(repo);
                    });
                t.detach();
                break;
            }
            case GitTask::FASTFORWARD: {
                repo.state = GitState::PROCESSING;
                repo.task = GitTask::PROCESSING;
                std::thread t = std::thread([&]() 
                    {
                        fastfowardRepo(repo);
                    });
                t.detach();
                break;
            }
            case GitTask::PUSH: {
                repo.state = GitState::PROCESSING;
                repo.task = GitTask::PROCESSING;
                std::thread t = std::thread([&]()
                    {
                        pushRepo(repo);
                    });
                t.detach();
            }
        }
    }

    if (reloadDirectory) {
        std::lock_guard<std::mutex> lock(gitReposLock);
        
        // Clear exiting repo references
        for (GitRepo& repo : gitRepos) {
            git_repository_free(repo.repo);
        }
        gitRepos.clear();

        if (TEST_REPOS_OVERRIDE) {
            for (const GitRepo& repo : testRepos) {
                gitRepos.push_back(repo);
            }
        }
        else {
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
        }

        reloadDirectory = false;
    }
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