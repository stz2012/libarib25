message(STATUS "Running: ldconfig")

execute_process(COMMAND ${CMD_LDCONFIG} RESULT_VARIABLE ldconfig_result)
if (NOT ldconfig_result EQUAL 0)
	message(WARNING "ldconfig failed")
endif()
