add_library(VKE_Physics STATIC)

find_package(Jolt CONFIG REQUIRED)

target_sources(VKE_Physics PRIVATE
    private/PhysicsWorld.cpp
    private/Broadphase.cpp
    private/Narrowphase.cpp
    private/TriangleTree.cpp
)

target_include_directories(VKE_Physics
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(VKE_Physics
    PUBLIC  VKE_Core
            VKE_Math
            VKE_Reflection
    # Only the module's private code sees Jolt
    PRIVATE Jolt::Jolt
)

set_target_properties(VKE_Physics PROPERTIES FOLDER "Engine")
