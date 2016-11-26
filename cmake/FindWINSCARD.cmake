if(UNIX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(WINSCARD REQUIRED libpcsclite)
endif()
