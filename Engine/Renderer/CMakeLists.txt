add_library(VKE_Renderer STATIC)

target_sources(VKE_Renderer PRIVATE
    private/Renderer.cpp
    private/RenderScene.cpp
    private/Camera.cpp
    private/ForwardPath.cpp
    private/GpuCache.cpp
    private/TonemapPass.cpp
    private/SkyboxPass.cpp
    private/EnvironmentBaker.cpp
    private/ShadowPass.cpp
)

target_include_directories(VKE_Renderer
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Renderer
    PUBLIC  VKE_Core
            VKE_Math
            VKE_Assets
            VKE_Reflection
    PRIVATE VKE_RHI
            VKE_Platform
)

set_target_properties(VKE_Renderer PROPERTIES FOLDER "Engine")

vke_add_shaders(RendererShaders
    SOURCES
        shaders/forward.vert
        shaders/forward.frag
        shaders/fullscreen.vert
        shaders/tonemap.frag
        shaders/cubemap.vert
        shaders/equirect_to_cubemap.frag
        shaders/procedural_sky.frag
        shaders/skybox.vert
        shaders/skybox.frag
        shaders/irradiance.frag
        shaders/prefilter.frag
        shaders/brdf_lut.frag
        shaders/shadow.vert
    INCLUDE_DIRS
        shaders
)
