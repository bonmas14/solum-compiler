workspace "solum"
    configurations { "Debug", "Release" }
    platforms {  "native", "llvm" }

    architecture "x86_64"

    defines "_CRT_SECURE_NO_WARNINGS"

    filter "configurations:Debug"
        defines "DEBUG"  
        symbols "On"
        optimize "Debug"

    filter "configurations:Release"
        defines "NDEBUG"
        optimize "Full"

    filter "platforms:llvm" 
        defines "BACKEND_LLVM" 

    filter "platforms:native" 
        defines "BACKEND_NATIVE"

    filter {}

    warnings "Extra"

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


    filter "platforms:llvm" 
        targetprefix "llvm-"
        -- libdirs { "/usr/lib/llvm-14/lib" }
        -- includedirs { }
        links { "LLVM" }

    filter "platforms:native" 
        targetprefix "native-"
        -- nothing for now
    filter {}

    files { "./src/**.c" }

