cmake_minimum_required(VERSION 3.10)
project(MacWolf C)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

add_library(Burger STATIC
	${PROJECT_SOURCE_DIR}/source/Burger.c
)

target_link_libraries(Burger PRIVATE SDL3::SDL3)
