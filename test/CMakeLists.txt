configure_file(
  "${PROJECT_SOURCE_DIR}/cmake/CTestCustom.cmake"
  "${PROJECT_BINARY_DIR}/CTestCustom.cmake"
  COPYONLY)

add_executable(mio.test "test.cpp" "plain.cpp")
target_link_libraries(mio.test PRIVATE mio::mio)
add_test(NAME mio.test COMMAND mio.test)
