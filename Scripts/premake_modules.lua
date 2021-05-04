function AddVulkan()
    defines { "MODULE_VULKAN" }
	includedirs { "$(VULKAN_SDK)/include" }
    
    postbuildcommands
    { 
        --"{COPY} \"$(SolutionDir)ThirdParty\\SDL2\\lib\\%{cfg.platform}\\SDL2.dll\" \"$(OutDir)\""
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