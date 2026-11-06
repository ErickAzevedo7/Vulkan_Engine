add_library(VKE_Physics STATIC)

find_package(Jolt CONFIG REQUIRED)
# Jolt is built without exceptions and asks its users for the same, which would turn them off
# in the STL of this module only. Jolt's headers don't need it, so the module keeps them.
get_target_property(_jolt_definitions Jolt::Jolt INTERFACE_COMPILE_DEFINITIONS)
list(FILTER _jolt_definitions EXCLUDE REGEX "_HAS_EXCEPTIONS")
set_target_properties(Jolt::Jolt PROPERTIES INTERFACE_COMPILE_DEFINITIONS "${_jolt_definitions}")

target_sources(VKE_Physics PRIVATE
    private/PhysicsWorld.cpp
    private/jolt/JoltWorld.cpp
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
