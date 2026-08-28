add_executable(EditorApp EditorApp.cpp)

target_include_directories(EditorApp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(EditorApp PRIVATE
    Engine
)
