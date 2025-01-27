#ifndef GIT_REPO_H
#define GIT_REPO_H

#include "git2.h"
#include "cpputils/windows/credential_utils.h"

#include <filesystem>
#include <array>
#include <mutex>
#include <optional>
#include <memory>
#include <thread>
#include <sstream>

constexpr const char* GIT_REPO_MANAGER_CREDENTIAL_TARGE_NAME = "StopwatchString/Git-Repo-Manager";

//--------------------------------------
// enum GitState
//--------------------------------------
enum class GitState {
    NONE,
    UPTODATE,
    PUSH,
    FASTFORWARD,
    DIVERGED,
    REBASE,
    PROCESSING,
    ERROR_STATE,
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
    case GitState::FASTFORWARD:
        stateStr = "FAST-FORWARD";
        break;
    case GitState::DIVERGED:
        stateStr = "DIVERGED";
        break;
    case GitState::REBASE:
        stateStr = "REBASE";
        break;
    case GitState::PROCESSING:
        stateStr = "PROCESSING";
        break;
    case GitState::ERROR_STATE:
        stateStr = "ERROR STATE";
        break;
    default:
        break;
    }
    return stateStr;
}

//--------------------------------------
// enum GitTask
//--------------------------------------
enum class GitTask {
    NONE,
    FETCH,
    FASTFORWARD,
    PUSH,
    PROCESSING,
};

//--------------------------------------
// struct GitRepo
//--------------------------------------
struct GitRepo {
    git_repository* repo{ nullptr };
    std::filesystem::path repoPath{ "" };
    GitState state{ GitState::NONE };
    std::string message{ "" };
    std::unique_ptr<std::mutex> processingMutex{ std::make_unique<std::mutex>() };
    GitTask task{ GitTask::NONE };

    GitRepo() = default;

    GitRepo(git_repository* repo, std::filesystem::path repoPath, GitState state, std::string message)
        : repo(repo),
          repoPath(repoPath),
          state(state),
          message(message)
    {
    }

    GitRepo(const GitRepo& other)
        : repo(other.repo),
          repoPath(other.repoPath),
          state(other.state),
          message(other.message)
    {
    }

    GitRepo(GitRepo&& other) = default;
};

const static std::array<GitRepo, 8> testRepos = {
    GitRepo(nullptr, "C:\\testRepo1\\.git\\", GitState::NONE, "test message 1"),
    GitRepo(nullptr, "C:\\testRepo2\\.git\\", GitState::UPTODATE, "test message 2"),
    GitRepo(nullptr, "C:\\testRepo3\\.git\\", GitState::PUSH, "test message 3"),
    GitRepo(nullptr, "C:\\testRepo4\\.git\\", GitState::FASTFORWARD, "test message 4 \n Is this on the next line?"),
    GitRepo(nullptr, "C:\\testRepo5\\.git\\", GitState::DIVERGED, "test message 5"),
    GitRepo(nullptr, "C:\\testRepo6\\.git\\", GitState::REBASE, "test message 6"),
    GitRepo(nullptr, "C:\\testRepo7\\.get\\", GitState::PROCESSING, "test message 7"),
    GitRepo(nullptr, "C:\\testRepo8\\.get\\", GitState::ERROR_STATE, "test message 8"),
};

//--------------------------------------
// credentialAcquireCallback()
//--------------------------------------
int credentialAcquireCallback(git_cred** out, const char* url, const char* username_from_url, unsigned int allowed_types, void* payload) {
    Credential credential = readCredential(GIT_REPO_MANAGER_CREDENTIAL_TARGE_NAME);
    return git_cred_userpass_plaintext_new(out, credential.username.c_str(), credential.credentialBlob.c_str());
}

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
            state = GitState::FASTFORWARD;
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
// fetchRepo()
//--------------------------------------
void fetchRepo(GitRepo& gitRepo)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    gitRepo.message = "Fetched";
    gitRepo.task = GitTask::NONE;
    gitRepo.state = getRepoState(gitRepo.repo);
}

//--------------------------------------
// fastfowardRepo()
//--------------------------------------
void fastfowardRepo(GitRepo& gitRepo)
{
    bool ok = true;

    std::stringstream message;

    // Gross method of control loop that keeps indentation flat... not sure about it.
    auto fetch = [&]() {
        int error;
        git_reference* head_ref = NULL;
        const char* branch_name = NULL;

        // Get the current branch
        if ((error = git_repository_head(&head_ref, gitRepo.repo)) != 0) {
            message << "Error getting current branch: " << git_error_last()->message;
            ok = false;
            return;
        }

        if ((error = git_branch_name(&branch_name, head_ref)) != 0) {
            message << "Error getting branch name: " << git_error_last()->message;
            git_reference_free(head_ref);
            ok = false;
            return;
        }

        message << "Fast-forwarding branch: " << branch_name << '\n';

        // Get the remote for the branch
        git_remote* remote = NULL;
        if ((error = git_remote_lookup(&remote, gitRepo.repo, "origin")) != 0) {
            message << "Error looking up remote 'origin': " << git_error_last()->message;
            git_reference_free(head_ref);
            ok = false;
            return;
        }

        // Fetch from the remote
        git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
        fetch_opts.callbacks.credentials = credentialAcquireCallback;
        if ((error = git_remote_fetch(remote, NULL, &fetch_opts, NULL)) != 0) {
            message << "Error fetching from remote 'origin': " << git_error_last()->message;
            git_remote_free(remote);
            git_reference_free(head_ref);
            ok = false;
            return;
        }

        message << "Successfully fetched from remote 'origin'\n";

        // Get the remote branch reference
        char remote_branch_ref[256];
        snprintf(remote_branch_ref, sizeof(remote_branch_ref), "refs/remotes/origin/%s", branch_name);

        git_reference* remote_ref = NULL;
        if ((error = git_reference_lookup(&remote_ref, gitRepo.repo, remote_branch_ref)) != 0) {
            message << "Error looking up remote branch '" << remote_branch_ref << "': " << git_error_last()->message;
            git_remote_free(remote);
            git_reference_free(head_ref);
            ok = false;
            return;
        }

        // Perform the fast-forward
        const git_oid* remote_oid = git_reference_target(remote_ref);
        git_index* index = NULL;

        // Ensure the working directory is clean
        if ((error = git_repository_index(&index, gitRepo.repo)) != 0) {
            message << "Error accessing repository index: " << git_error_last()->message;
            git_reference_free(remote_ref);
            git_remote_free(remote);
            git_reference_free(head_ref);
            ok = false;
            return;
        }

        if (git_index_has_conflicts(index)) {
            message << "Working directory has conflicts; cannot fast-forward.";
            git_index_free(index);
            git_reference_free(remote_ref);
            git_remote_free(remote);
            git_reference_free(head_ref);
            ok = false;
            return;
        }

        // Update the branch reference to the remote commit
        if ((error = git_reference_set_target(&head_ref, head_ref, remote_oid, NULL)) != 0) {
            message << "Error updating branch to remote commit: " << git_error_last()->message;
        }
        else {
            message << "Fast-forward completed successfully.";
        }

        // Cleanup
        git_index_free(index);
        git_reference_free(remote_ref);
        git_remote_free(remote);
        git_reference_free(head_ref);
    };
    fetch();

    gitRepo.message = message.str();
    gitRepo.task = GitTask::NONE;

    if (ok) {
        gitRepo.state = getRepoState(gitRepo.repo);
    }
    else {
        gitRepo.state = GitState::ERROR_STATE;
    }
}

//--------------------------------------
// pushRepo()
//--------------------------------------
void pushRepo(GitRepo& gitRepo)
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    gitRepo.message = "Pushed";
    gitRepo.task = GitTask::NONE;
    gitRepo.state = getRepoState(gitRepo.repo);
}



#endif
