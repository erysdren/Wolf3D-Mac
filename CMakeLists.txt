cmake_minimum_required(VERSION 3.10)
project(MacWolf C)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

add_executable(MacWolf
	${PROJECT_SOURCE_DIR}/source/Burger.c
	${PROJECT_SOURCE_DIR}/source/Data.c
	${PROJECT_SOURCE_DIR}/source/Doors.c
	${PROJECT_SOURCE_DIR}/source/EnMove.c
	${PROJECT_SOURCE_DIR}/source/EnThink.c
	${PROJECT_SOURCE_DIR}/source/HideMenuBar.c
	${PROJECT_SOURCE_DIR}/source/InterMis.c
	${PROJECT_SOURCE_DIR}/source/Intro.c
	${PROJECT_SOURCE_DIR}/source/Level.c
	${PROJECT_SOURCE_DIR}/source/Mac.c
	${PROJECT_SOURCE_DIR}/source/Missiles.c
	${PROJECT_SOURCE_DIR}/source/Music.c
	${PROJECT_SOURCE_DIR}/source/PickAMonitor.c
	${PROJECT_SOURCE_DIR}/source/PlMove.c
	${PROJECT_SOURCE_DIR}/source/PlStuff.c
	${PROJECT_SOURCE_DIR}/source/PlThink.c
	${PROJECT_SOURCE_DIR}/source/PrefsFile.c
	${PROJECT_SOURCE_DIR}/source/PushWall.c
	${PROJECT_SOURCE_DIR}/source/RefBsp.c
	${PROJECT_SOURCE_DIR}/source/Refresh.c
	${PROJECT_SOURCE_DIR}/source/Refresh2.c
	${PROJECT_SOURCE_DIR}/source/RefSprite.c
	${PROJECT_SOURCE_DIR}/source/SetupScalersPPC.c
	${PROJECT_SOURCE_DIR}/source/Sight.c
	${PROJECT_SOURCE_DIR}/source/SnesMain.c
	${PROJECT_SOURCE_DIR}/source/StateDef.c
	${PROJECT_SOURCE_DIR}/source/WolfIO.c
	${PROJECT_SOURCE_DIR}/source/WolfMain.c
)

target_link_libraries(MacWolf PRIVATE SDL3::SDL3)
