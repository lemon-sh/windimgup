cmake_minimum_required(VERSION 3.12)
project(windimgup)

aux_source_directory(src WINDIMGUP_SOURCE)
add_executable(windimgup ${WINDIMGUP_SOURCE} windimgup.rc)
target_link_libraries(windimgup gdiplus wininet comctl32)
