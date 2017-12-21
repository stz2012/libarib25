
find_program(OBJCOPY_EXECUTABLE "objcopy")
if(OBJCOPY_EXECUTABLE)
	set(ELF_INTERP_BIN ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/elf_interp)
	set(ELF_INTERP_SRC ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/elf_interp.c)
	set(ELF_INTERP_DAT ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/elf_interp.dat)
	
	file(WRITE ${ELF_INTERP_SRC} "int main(int argc, char **argv) { return 0; }")
	
	execute_process(COMMAND ${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} -o ${ELF_INTERP_BIN} ${ELF_INTERP_SRC})
	execute_process(COMMAND ${OBJCOPY_EXECUTABLE} -O binary --only-section=.interp ${ELF_INTERP_BIN} ${ELF_INTERP_DAT})

	if(EXISTS ${ELF_INTERP_DAT})
		file(READ ${ELF_INTERP_DAT} ELF_INTERP)
		string(STRIP ${ELF_INTERP} ELF_INTERP)
	endif()
endif()

message(STATUS "ELF Interpreter: ${ELF_INTERP}")
