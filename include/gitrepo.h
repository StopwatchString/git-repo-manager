#ifndef GIT_REPO_H
#define GIT_REPO_H

#include "git2.h"

#include <filesystem>
#include <array>

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
    std::string message{ "" };
};

const static std::array<GitRepo, 6> testRepos = {
    GitRepo(GitState::NONE, nullptr, "C:\\testRepo1\\.git\\"),
    GitRepo(GitState::UPTODATE, nullptr, "C:\\testRepo2\\.git\\"),
    GitRepo(GitState::PUSH, nullptr, "C:\\testRepo3\\.git\\"),
    GitRepo(GitState::PULL, nullptr, "C:\\testRepo4\\.git\\"),
    GitRepo(GitState::DIVERGED, nullptr, "C:\\testRepo5\\.git\\"),
    GitRepo(GitState::REBASE, nullptr, "C:\\testRepo6\\.git\\")
};

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

#endif
