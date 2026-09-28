add_executable(bstep-host-clock-tests HostClockTests.cpp)
target_compile_features(bstep-host-clock-tests PRIVATE cxx_std_17)
target_include_directories(bstep-host-clock-tests PRIVATE ../src)
target_compile_definitions(bstep-host-clock-tests PRIVATE
  BSTEP_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
if(NOT MSVC)
  target_compile_options(bstep-host-clock-tests PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME bstep-host-clock COMMAND bstep-host-clock-tests)
