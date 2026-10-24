add_executable(SourceBroomVmfCheck)
EMBED_UTF8_MANIFEST(SourceBroomVmfCheck)

target_sources(SourceBroomVmfCheck PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp
)

target_link_libraries(SourceBroomVmfCheck PRIVATE CompilerConfig TbMdlLib)
