find_package(Git)
if(GIT_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	execute_process(
		COMMAND ${GIT_EXECUTABLE} diff --shortstat --exit-code --quiet
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE GIT_IS_DIRTY
	)

	if(GIT_IS_DIRTY)
		set(GIT_REVISION "${GIT_REVISION} dirty")
	endif()
else()
	set(GIT_REVISION "n/a")
	set(GIT_IS_DIRTY False)
endif()
