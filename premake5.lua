workspace "solum"
    configurations { "Debug", "Release" }

    architecture "x86_64"

    defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "configurations:Debug"
        defines { "DEBUG" }  
        symbols "On"
        optimize "Debug"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "Full"

    filter {}

project "solum-compiler"
    kind "ConsoleApp"
    language "C"

    flags { "MultiProcessorCompile" } -- specific to windows

    filter "configurations:Debug"
        targetdir "bin"
        targetsuffix "-d"
        staticruntime "on"
        runtime "Debug"

    filter "configurations:Release"
        targetdir "bin"
        targetsuffix "-r"
        staticruntime "on"
        runtime "Release"

    filter {}

    -- includedirs { }
    libdirs { "/usr/lib/llvm-14/lib" }
    links { "LLVM" }

    files { "./src/**.c" }

