add_library(VKE_Scene STATIC)

find_package(yaml-cpp CONFIG REQUIRED)

target_sources(VKE_Scene PRIVATE
    private/Scene.cpp
    private/GameObject.cpp
    private/Component.cpp
    private/ComponentRegistry.cpp
    private/ModelInstantiation.cpp
    private/SceneSerializer.cpp
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
            VKE_Reflection
    PRIVATE yaml-cpp::yaml-cpp
)

# The property tables of the engine's components, and the enums and structs their properties use.
vke_generate_reflection(VKE_Scene
    FUNCTION RegisterEngineComponents
    HEADERS
        ../Math/public/math/Transform.h
        ../Renderer/public/renderer/RenderTypes.h
        ../Physics/public/physics/PhysicsTypes.h
        public/scene/components/TransformComponent.h
        public/scene/components/MeshComponent.h
        public/scene/components/LightComponent.h
        public/scene/components/CameraComponent.h
        public/scene/components/ColliderComponent.h
)

set_target_properties(VKE_Scene PROPERTIES FOLDER "Engine")
