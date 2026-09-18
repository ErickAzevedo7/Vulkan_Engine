add_library(VKE_Assets STATIC)

find_package(assimp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Stb REQUIRED)

target_sources(VKE_Assets PRIVATE
    private/AssetManager.cpp
    private/importers/ModelImporter.cpp
    private/importers/TextureImporter.cpp
    private/importers/MaterialImporter.cpp
)

target_include_directories(VKE_Assets
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/public
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/private
            ${Stb_INCLUDE_DIR}
)

target_link_libraries(VKE_Assets
    PUBLIC  VKE_Core
            VKE_Math
    PRIVATE VKE_Platform
            assimp::assimp
            nlohmann_json::nlohmann_json
)

set_target_properties(VKE_Assets PROPERTIES FOLDER "Engine")
