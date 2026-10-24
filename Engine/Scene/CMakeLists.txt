add_library(VKE_Scene STATIC)

target_sources(VKE_Scene PRIVATE
    private/Scene.cpp
    private/GameObject.cpp
    private/Component.cpp
    private/ModelInstantiation.cpp
    private/MeshShapeCache.cpp
    private/components/TransformComponent.cpp
    private/components/MeshComponent.cpp
    private/components/LightComponent.cpp
    private/components/CameraComponent.cpp
    private/components/ColliderComponent.cpp
)

target_include_directories(VKE_Scene
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Scene
    PUBLIC  VKE_Core
            VKE_Math
            VKE_Events
            VKE_Assets
            VKE_Renderer
            VKE_Physics
)

set_target_properties(VKE_Scene PROPERTIES FOLDER "Engine")
