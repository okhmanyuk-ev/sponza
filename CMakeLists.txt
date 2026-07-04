cmake_minimum_required(VERSION 3.10)
project(sponza)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
	set(CMAKE_MACOSX_BUNDLE YES)
endif()

set(LIBS_FOLDER "libs")

file(GLOB ALL_SRC
	src/*.cpp
	src/*.h
)

if(WIN32)
	add_executable(${PROJECT_NAME} ${ALL_SRC})

	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	add_definitions(-DNOMINMAX)
elseif(EMSCRIPTEN)
	set(ASSETS_FOLDER_NAME assets)
	set(ASSETS_FOLDER_PATH ${CMAKE_SOURCE_DIR}/${ASSETS_FOLDER_NAME})

	add_executable(${PROJECT_NAME} ${ALL_SRC} ${ASSETS_FOLDER_PATH})

	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")
	set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR})
	set(CMAKE_EXECUTABLE_SUFFIX .html)

	target_link_options(${PROJECT_NAME} PUBLIC --preload-file ${ASSETS_FOLDER_PATH}@${ASSETS_FOLDER_NAME})
endif()

# sky

add_subdirectory(sky)
target_link_libraries(${PROJECT_NAME} sky)
copy_required_libs(${PROJECT_NAME})

# tinygltf

set(TINYGLTF_HEADER_ONLY OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "" FORCE)
add_subdirectory(lib/tinygltf)
target_link_libraries(${PROJECT_NAME} tinygltf)
set_property(TARGET tinygltf PROPERTY FOLDER ${LIBS_FOLDER})
