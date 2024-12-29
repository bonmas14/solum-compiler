workspace "solum"
    configurations { "Debug", "Release" }
    platforms { "native", "llvm" }

    warnings "Extra"
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

project "solum-compiler"
    kind "ConsoleApp"
    language "C"
    cdialect "c11"

    files { "./src/**.c" }

    filter "system:windows" 
        flags { "MultiProcessorCompile" }
    filter {}

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

        filter "system:windows"
            -- includedirs { "" }
            -- libdirs { "" }
            links { "LLVM-C" }
            
        filter "system:linux"  -- i need to find a better way...
            includedirs { "/usr/lib/llvm-14/include/" } 
            libdirs { "/usr/lib/llvm-14/lib/" }
            links { "LLVMCore" }

    filter "platforms:native" 
        targetprefix "native-"

    filter {}


