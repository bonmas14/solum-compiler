workspace "solum"
    configurations { "Debug", "Release" }

    startproject "native-solum-compiler"

    warnings "Extra"
 --    architecture "x86_64"

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

    files { "./src/**.cpp", "./native/win32-x86_64-impl.cpp" }
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


project "llvm-solum-compiler"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    -- disablewarnings { "unused-parameter" }

    defines "BACKEND_LLVM"
    files { "./src/**.cpp", "./llvm/impl.cpp" }

    filter "configurations:Debug"
        targetdir "bin"
        targetsuffix "-d"
        targetname "slm-llvm"
        staticruntime "off"
        runtime "Release"
    filter {}

    filter "configurations:Release"
        targetdir "bin"
        targetsuffix "-r"
        targetname "slm-llvm"
        staticruntime "off"
        runtime "Release"
    filter {}

    filter "system:windows"
        flags { "MultiProcessorCompile" }
        defines "_CRT_SECURE_NO_WARNINGS"


        includedirs { "./include", "E:/LLVM/clang+llvm-18.1.8-x86_64-pc-windows-msvc/include" }
        libdirs { "E:/LLVM/clang+llvm-18.1.8-x86_64-pc-windows-msvc/lib" }
        links { "Ws2_32",
                "LLVMAArch64AsmParser",
                "LLVMAArch64CodeGen",
                "LLVMAArch64Desc",
                "LLVMAArch64Disassembler",
                "LLVMAArch64Info",
                "LLVMAArch64Utils",

                "LLVMAggressiveInstCombine",

                "LLVMAMDGPUAsmParser",
                "LLVMAMDGPUDesc",
                "LLVMAMDGPUInfo",
                "LLVMAMDGPUUtils",

                "LLVMAnalysis",

                "LLVMARMAsmParser",
                "LLVMARMCodeGen",
                "LLVMARMDesc",
                "LLVMARMDisassembler",
                "LLVMARMInfo",
                "LLVMARMUtils",

                "LLVMAsmParser",
                "LLVMAsmPrinter",

                "LLVMAVRCodeGen",
                "LLVMAVRDisassembler",

                "LLVMBinaryFormat",
                "LLVMBitReader",
                "LLVMBitstreamReader",
                "LLVMBitWriter",

                "LLVMBPFAsmParser",
                "LLVMBPFDesc",
                "LLVMBPFInfo",

                "LLVMCFGuard",
                "LLVMCFIVerify",
                "LLVMCodeGen",
                "LLVMCodeGenTypes",
                "LLVMCore",
                "LLVMCoroutines",
                "LLVMCoverage",
                "LLVMDebugInfoBTF",
                "LLVMDebugInfoCodeView",
                "LLVMDebuginfod",
                "LLVMDebugInfoDWARF",
                "LLVMDebugInfoGSYM",
                "LLVMDebugInfoLogicalView",
                "LLVMDebugInfoMSF",
                "LLVMDebugInfoPDB",
                "LLVMDemangle",
                "LLVMDiff",
                "LLVMDlltoolDriver",

                "LLVMDWARFLinker",
                "LLVMDWARFLinkerClassic",
                "LLVMDWARFLinkerParallel",
                "LLVMDWP",

                "LLVMExecutionEngine",
                "LLVMExegesis",
                "LLVMExegesisAArch64",
                "LLVMExegesisMips",
                "LLVMExegesisX86",
                "LLVMExtensions",

                "LLVMFileCheck",

                "LLVMFrontendDriver",
                "LLVMFrontendHLSL",
                "LLVMFrontendOffloading",
                "LLVMFrontendOpenACC",
                "LLVMFrontendOpenMP",

                "LLVMFuzzerCLI",
                "LLVMFuzzMutate",
                "LLVMGlobalISel",
                "LLVMHexagonAsmParser",
                "LLVMHexagonDesc",
                "LLVMHexagonInfo",

                "LLVMHipStdPar",

                "LLVMInstCombine",
                "LLVMInstrumentation",
                "LLVMInterfaceStub",
                "LLVMInterpreter",

                "LLVMipo",

                "LLVMIRPrinter",
                "LLVMIRReader",

                "LLVMJITLink",

                "LLVMLanaiCodeGen",
                "LLVMLanaiDisassembler",

                "LLVMLibDriver",
                "LLVMLineEditor",
                "LLVMLinker",

                "LLVMLoongArchAsmParser",
                "LLVMLoongArchDesc",
                "LLVMLoongArchInfo",

                "LLVMLTO",
                "LLVMMC",
                "LLVMMCA",
                "LLVMMCDisassembler",
                "LLVMMCJIT",
                "LLVMMCParser",
                "LLVMMipsCodeGen",
                "LLVMMipsDisassembler",
                "LLVMMIRParser",
                "LLVMMSP430AsmParser",
                "LLVMMSP430Desc",
                "LLVMMSP430Info",
                "LLVMNVPTXDesc",
                "LLVMObjCARCOpts",
                "LLVMObjCopy",
                "LLVMObject",
                "LLVMObjectYAML",
                "LLVMOption",
                "LLVMOrcDebugging",
                "LLVMOrcJIT",
                "LLVMOrcShared",
                "LLVMOrcTargetProcess",
                "LLVMPasses",
                "LLVMPowerPCCodeGen",
                "LLVMPowerPCDisassembler",
                "LLVMProfileData",
                "LLVMRemarks",
                "LLVMRISCVAsmParser",
                "LLVMRISCVDesc",
                "LLVMRISCVInfo",
                "LLVMRuntimeDyld",
                "LLVMScalarOpts",
                "LLVMSelectionDAG",
                "LLVMSparcCodeGen",
                "LLVMSparcDisassembler",
                "LLVMSupport",
                "LLVMSymbolize",
                "LLVMSystemZAsmParser",
                "LLVMSystemZDesc",
                "LLVMSystemZInfo",
                "LLVMTableGen",
                "LLVMTableGenCommon",
                "LLVMTableGenGlobalISel",
                "LLVMTarget",
                "LLVMTargetParser",
                "LLVMTextAPI",
                "LLVMTextAPIBinaryReader",
                "LLVMTransformUtils",
                "LLVMVECodeGen",
                "LLVMVectorize",
                "LLVMVEDisassembler",

                "LLVMWebAssemblyAsmParser",
                "LLVMWebAssemblyDesc",
                "LLVMWebAssemblyInfo",

                "LLVMWindowsDriver",
                "LLVMWindowsManifest",

                "LLVMX86AsmParser",
                "LLVMX86CodeGen",
                "LLVMX86Desc",
                "LLVMX86Disassembler",
                "LLVMX86Info",
                "LLVMX86TargetMCA",

                "LLVMXCoreCodeGen",
                "LLVMXCoreDisassembler",
                "LLVMXRay", }

    filter {}

    filter "system:linux"
        includedirs { "./include", "/usr/lib/llvm-14/include" } 
        libdirs { "/usr/lib/llvm-14/lib/" }
        links { "LLVM" }
    filter {}
