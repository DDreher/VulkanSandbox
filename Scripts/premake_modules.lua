function AddVulkan()
    defines { "MODULE_VULKAN" }
	includedirs { "$(VULKAN_SDK)/include" }
    
    postbuildcommands
    { 
    }
    links
    {
        "$(VULKAN_SDK)/lib/vulkan-1.lib"
    }
end

function AddGLM()
    defines { "MODULE_GLM" }

	includedirs "$(SolutionDir)/ThirdParty/glm/"

    filter {}
end

function AddSTB()  
    defines { "MODULE_STB", "STB_IMAGE_IMPLEMENTATION" }
    includedirs {
        "$(SolutionDir)/ThirdParty/stb/include/",
    } 

    filter {}
end

function AddTinyObjLoader()  
    defines { "MODULE_TINYOBJLOADER", "TINYOBJLOADER_IMPLEMENTATION" }
    includedirs {
        "$(SolutionDir)/ThirdParty/tinyobjloader/include/",
    } 

    filter {}
end

function AddSpdlog()  
    defines { "MODULE_SPDLOG" }
    includedirs {
        "$(SolutionDir)/ThirdParty/spdlog/include/",
    } 
        
    filter {}
end

function AddSDL2(isTarget)  
    defines { "MODULE_SDL2" }
	includedirs "$(SolutionDir)/ThirdParty/SDL2/include/"
    libdirs	"$(SolutionDir)/ThirdParty/SDL2/lib/x64"
    
    postbuildcommands
    { 
        "{COPY} \"$(SolutionDir)ThirdParty\\SDL2\\lib\\%{cfg.platform}\\SDL2.dll\" \"$(OutDir)\""
    }
    links
    {
        "SDL2.lib",
        "SDL2main.lib"
    }
end
