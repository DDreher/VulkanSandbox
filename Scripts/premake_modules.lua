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

function AddGLFW()
    defines { "MODULE_GLFW" }

	includedirs "$(SolutionDir)/ThirdParty/glfw/include/"
    libdirs	"$(SolutionDir)/ThirdParty/glfw/lib-vc2019/"
  
    postbuildcommands
    { 
        "{COPY} \"$(SolutionDir)ThirdParty\\glfw\\lib-vc2019\\glfw3.dll\" \"$(OutDir)\""
    }
    links
    { 
        "glfw3.lib"
    }

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
