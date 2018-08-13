find_package(Git)
if(GIT_FOUND AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/.git)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always --tags
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
	if (EXISTS ${CMAKE_SOURCE_DIR}/.tarball-version)
		file(STRINGS ${CMAKE_SOURCE_DIR}/.tarball-version GIT_REVISION LIMIT_COUNT 1)
	else()
		set(GIT_REVISION "Unknown Source")
	endif()
	set(GIT_IS_DIRTY False)
endif()
