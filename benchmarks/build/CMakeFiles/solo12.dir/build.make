# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/build

# Include any dependencies generated for this target.
include CMakeFiles/solo12.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/solo12.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/solo12.dir/flags.make

CMakeFiles/solo12.dir/solo12.o: CMakeFiles/solo12.dir/flags.make
CMakeFiles/solo12.dir/solo12.o: ../solo12.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/solo12.dir/solo12.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/solo12.dir/solo12.o -c /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/solo12.cpp

CMakeFiles/solo12.dir/solo12.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/solo12.dir/solo12.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/solo12.cpp > CMakeFiles/solo12.dir/solo12.i

CMakeFiles/solo12.dir/solo12.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/solo12.dir/solo12.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/solo12.cpp -o CMakeFiles/solo12.dir/solo12.s

# Object files for target solo12
solo12_OBJECTS = \
"CMakeFiles/solo12.dir/solo12.o"

# External object files for target solo12
solo12_EXTERNAL_OBJECTS =

solo12: CMakeFiles/solo12.dir/solo12.o
solo12: CMakeFiles/solo12.dir/build.make
solo12: libProject_BENCHMARK_TOOLS.so
solo12: CMakeFiles/solo12.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable solo12"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/solo12.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/solo12.dir/build: solo12

.PHONY : CMakeFiles/solo12.dir/build

CMakeFiles/solo12.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/solo12.dir/cmake_clean.cmake
.PHONY : CMakeFiles/solo12.dir/clean

CMakeFiles/solo12.dir/depend:
	cd /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/build /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/build /home/jianghan/Devel/workspace/src/mim_solvers/benchmarks/build/CMakeFiles/solo12.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/solo12.dir/depend

