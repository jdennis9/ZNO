cmake_minimum_required(VERSION 3.8)
project(main)

file(GLOB sources code/*.h code/*.cpp code/resources.rc code/thirdparty/*.cpp code/thirdparty/*.c imgui/*.cpp imgui/*.h 
	imgui/backends/imgui_impl_dx10.* imgui/backends/imgui_impl_win32.* imgui/misc/freetype/imgui_freetype.*)

add_executable(main ${sources})
#add_compile_definitions("$<$<CONFIG:RelWithDebInfo>:DEF_WIN_MAIN>")
add_compile_definitions("$<$<CONFIG:Release>:DEF_WIN_MAIN>")
target_include_directories(main PRIVATE code/thirdparty imgui)
target_link_libraries(main PRIVATE winmm comdlg32 ole32 d3d10 shlwapi)
add_compile_definitions(_CRT_SECURE_NO_WARNINGS IMGUI_ENABLE_FREETYPE)

set_target_properties(main PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "$<CONFIG>"
	LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
	LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS"
)

# Packages
find_package(Freetype REQUIRED)
target_link_libraries(main PRIVATE Freetype::Freetype)
find_package(SndFile CONFIG REQUIRED)
target_link_libraries(main PRIVATE SndFile::sndfile)
find_package(SampleRate CONFIG REQUIRED)
target_link_libraries(main PRIVATE SampleRate::samplerate)
find_package(taglib CONFIG REQUIRED)
target_link_libraries(main PRIVATE TagLib::tag TagLib::tag_c TagLib::TagLib)
#target_link_libraries(main PRIVATE TagLib::tag)

set_target_properties(main PROPERTIES OUTPUT_NAME "ZNO")
