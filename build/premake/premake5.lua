-- Function to recursively delete a directory
function delete_directory(dir)
    if os.isdir(dir) then
        os.rmdir(dir)
    end
end

-- Delete the directory '../vs2022' before generating files
delete_directory("../vs2022")

-- premake5.lua
workspace "GitRepoManager"
    configurations { "Debug", "Release" }
    architecture "x64"

    -- Set the location of generated Visual Studio files
    location "../vs2022"

    -- Use the latest C++ standard
    cppdialect "C++latest"

    flags { "MultiProcessorCompile" }

    -- Platform-specific settings
    filter "system:windows"
        systemversion "latest"
    filter {}

project "GitRepoManager"
    kind "ConsoleApp"  -- Change this to "WindowedApp" or "StaticLib"/"SharedLib" if needed
    language "C++"

    debugdir "../../"

    -- Specify the directories for source and include files
    files {
        "../../src/**.cpp",
        "../../include/**.h",
        "../../extern/cpputils/include/**.h",
        "../../extern/glh/include/**.h",
        "../../extern/glh/src/**.cpp",
        "../../extern/glh/src/**.c",
        "../../extern/vectorclass/*h"
    }

    removefiles {
        "../../extern/glh/include/glad/glx.h",
        "../../extern/glh/src/glad/glx.c"
    }

    includedirs {
        "../../include",
        "../../extern/cpputils/include",
        "../../extern/glh/include",
        "../../extern/glh/include/dearimgui",
        "../../extern/vectorclass"
    }

    libdirs {
        "../../lib"
    }

    links {
        "glfw3.lib",
        "git2.lib", -- LibGit2
        "Winhttp.lib", -- Windows HTTP lib for LibGit2
        "Crypt32.lib", -- Windows Crypto lib for LibGit2
        "Rpcrt4.lib" -- Windows Remote Procedure Call lib for LibGit2
    }

    linkoptions {
        "/LTCG" -- MSIL .netmodule or module compiled with /GL found; restarting link with /LTCG; add /LTCG to the link command line to improve linker performance
    }

    -- Output directories for build files
    targetdir "../../bin/%{cfg.buildcfg}"
    objdir "../../bin/%{cfg.buildcfg}"

    -- Debug and Release-specific settings
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter {}

-- Note:
-- Make sure to run this script from the /build/premake directory
-- and execute `premake5 vs2022` to generate the Visual Studio project files.
