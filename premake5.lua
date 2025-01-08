workspace "solum"
    configurations { "Debug", "Release" }

    startproject "native-solum-compiler"

    warnings "Extra"
    architecture "x86_64"

    filter "configurations:Debug"
        defines "DEBUG"
        symbols "On"
        optimize "Debug"
    filter {}

    filter "configurations:Release"
        defines "NDEBUG"
        optimize "Full"
    filter {}

project "native-solum-compiler"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++14"

    defines "BACKEND_NATIVE"
    files { "./src/**.cpp" }

    filter "configurations:Debug"
        targetdir "bin"
        targetsuffix "-d"
        staticruntime "on"
        runtime "Debug"
    filter {}

    filter "configurations:Release"
        targetdir "bin"
        targetsuffix "-r"
        staticruntime "on"
        runtime "Release"
    filter {}

    filter "system:windows"
        flags { "MultiProcessorCompile" }
        defines "_CRT_SECURE_NO_WARNINGS"
        -- includedirs { "C:/clang+llvm-18.1.8-x86_64-pc-windows-msvc/include" }
        -- libdirs { "C:/clang+llvm-18.1.8-x86_64-pc-windows-msvc/lib" }
        -- links { "LLVM-C" }
    filter {}

    filter "system:linux"
        -- includedirs { "/usr/lib/llvm-14/include/" } 
        -- libdirs { "/usr/lib/llvm-14/lib/" }
        -- links { "LLVMCore" }
    filter {}


project "llvm-solum-compiler"
    kind "ConsoleApp"
    language "C"
    cdialect "c11"

    defines "BACKEND_LLVM"
    files { "./src/**.cpp" }

    filter "configurations:Debug"
        targetdir "bin"
        targetsuffix "-d"
        staticruntime "on"
        runtime "Debug"
    filter {}

    filter "configurations:Release"
        targetdir "bin"
        targetsuffix "-r"
        staticruntime "on"
        runtime "Release"
    filter {}

    filter "system:windows"
        flags { "MultiProcessorCompile" }
        defines "_CRT_SECURE_NO_WARNINGS"
        includedirs { "C:/clang+llvm-18.1.8-x86_64-pc-windows-msvc/include" }
        libdirs { "C:/clang+llvm-18.1.8-x86_64-pc-windows-msvc/lib" }
        links { "LLVM-C" }
    filter {}

    filter "system:linux"
        includedirs { "/usr/lib/llvm-14/include/" } 
        libdirs { "/usr/lib/llvm-14/lib/" }
        links { "LLVMCore" }
    filter {}
