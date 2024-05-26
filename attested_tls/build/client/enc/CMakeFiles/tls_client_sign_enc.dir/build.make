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
CMAKE_SOURCE_DIR = /home/azureuser/testSample/attested_tls

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/azureuser/testSample/attested_tls/build

# Utility rule file for tls_client_sign_enc.

# Include the progress variables for this target.
include client/enc/CMakeFiles/tls_client_sign_enc.dir/progress.make

client/enc/CMakeFiles/tls_client_sign_enc: client/enc/tls_client_enc.signed


client/enc/tls_client_enc.signed: client/enc/tls_client_enc
client/enc/tls_client_enc.signed: ../client/enc/enc.conf
client/enc/tls_client_enc.signed: ../client/enc/private.pem
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/azureuser/testSample/attested_tls/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Generating tls_client_enc.signed"
	cd /home/azureuser/testSample/attested_tls/build/client/enc && /opt/openenclave/bin/oesign sign -e /home/azureuser/testSample/attested_tls/build/client/enc/tls_client_enc -c /home/azureuser/testSample/attested_tls/client/enc/enc.conf -k /home/azureuser/testSample/attested_tls/client/enc/private.pem

tls_client_sign_enc: client/enc/CMakeFiles/tls_client_sign_enc
tls_client_sign_enc: client/enc/tls_client_enc.signed
tls_client_sign_enc: client/enc/CMakeFiles/tls_client_sign_enc.dir/build.make

.PHONY : tls_client_sign_enc

# Rule to build all files generated by this target.
client/enc/CMakeFiles/tls_client_sign_enc.dir/build: tls_client_sign_enc

.PHONY : client/enc/CMakeFiles/tls_client_sign_enc.dir/build

client/enc/CMakeFiles/tls_client_sign_enc.dir/clean:
	cd /home/azureuser/testSample/attested_tls/build/client/enc && $(CMAKE_COMMAND) -P CMakeFiles/tls_client_sign_enc.dir/cmake_clean.cmake
.PHONY : client/enc/CMakeFiles/tls_client_sign_enc.dir/clean

client/enc/CMakeFiles/tls_client_sign_enc.dir/depend:
	cd /home/azureuser/testSample/attested_tls/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/azureuser/testSample/attested_tls /home/azureuser/testSample/attested_tls/client/enc /home/azureuser/testSample/attested_tls/build /home/azureuser/testSample/attested_tls/build/client/enc /home/azureuser/testSample/attested_tls/build/client/enc/CMakeFiles/tls_client_sign_enc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : client/enc/CMakeFiles/tls_client_sign_enc.dir/depend
