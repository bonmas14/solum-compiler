workspace "solum"
    configurations { "Debug", "Release" }
    platforms { "x64", "x32" }

    startproject "native-solum-compiler"

    warnings "Extra"

    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"
        optimize "Debug"
    filter {}

    filter "configurations:Release"
        defines "NDEBUG"
        optimize "Full"
    filter {}

    filter "platforms:x64"
        architecture "x86_64"
    filter {}

    filter "platforms:x32"
        architecture "x86"
    filter {}

project "native-solum-compiler"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++14"

    files { "./src/**.cpp" }
    includedirs { "./include" }

    filter "configurations:Debug"
        targetdir "bin"
        targetsuffix "-d"
        targetname "slm"
        staticruntime "on"
        runtime "Debug"
    filter {}

    filter "configurations:Release"
        targetdir "bin"
        targetsuffix "-r"
        targetname "slm"
        staticruntime "on"
        runtime "Release"
    filter {}

    filter "system:windows"
        flags { "MultiProcessorCompile" }
        defines "_CRT_SECURE_NO_WARNINGS"
    filter {}

    filter "system:linux"
    filter {}
