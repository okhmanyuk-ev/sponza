cmake_minimum_required(VERSION 3.10)
project(skygfx-sponza-demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
	set(CMAKE_MACOSX_BUNDLE YES)
endif()

set(RESOURCES
	assets
)

set(LIBS_FOLDER "libs")

file(GLOB ALL_SRC 
	src/*.cpp
	src/*.h
	lib/skygfx/examples/utils/*.cpp
	lib/skygfx/examples/utils/*.h
	lib/skygfx/examples/lib/imgui/backends/imgui_impl_glfw.cpp
	${RESOURCES}
)

add_executable(${PROJECT_NAME} ${ALL_SRC})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_target_properties(${PROJECT_NAME} PROPERTIES RESOURCE ${RESOURCES})
add_definitions(-DNOMINMAX)
	
# skygfx

add_subdirectory(lib/skygfx)
set_property(TARGET skygfx PROPERTY FOLDER ${LIBS_FOLDER})
target_link_libraries(${PROJECT_NAME} skygfx)

# glfw

if(NOT TARGET glfw)
	set(GLFW_INSTALL OFF)
	set(GLFW_BUILD_DOCS OFF)
	add_subdirectory(lib/skygfx/examples/lib/glfw)
	set_property(TARGET glfw PROPERTY FOLDER ${LIBS_FOLDER}/glfw)
	set_property(TARGET update_mappings PROPERTY FOLDER ${LIBS_FOLDER}/glfw)
	target_link_libraries(${PROJECT_NAME} glfw)
endif()

# tinygltf

set(TINYGLTF_HEADER_ONLY OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "" FORCE)
add_subdirectory(lib/tinygltf)
target_link_libraries(${PROJECT_NAME} tinygltf)
set_property(TARGET tinygltf PROPERTY FOLDER ${LIBS_FOLDER})

# imgui

if(NOT TARGET imgui)
	file(GLOB IMGUI_SRC 
		lib/skygfx/examples/lib/imgui/*.cpp 
		lib/skygfx/examples/lib/imgui/*.h
	)
	target_include_directories(${PROJECT_NAME} PUBLIC
		lib/skygfx/examples/lib/imgui
		lib/skygfx/examples/lib/imgui/backends
	)
	add_library(imgui STATIC ${IMGUI_SRC})
	target_link_libraries(${PROJECT_NAME} imgui)
	set_property(TARGET imgui PROPERTY FOLDER ${LIBS_FOLDER})
endif()
