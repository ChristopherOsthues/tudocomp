# find_python_module
# Used to find a certain Python module on the system.
#
# Source (Oct 11, 2016):
# http://public.kitware.com/pipermail/cmake/2011-January/041666.html
#
# Modified by Patrick Dinklage.

function(find_python_module module)
    find_package(PythonInterp)

	string(TOUPPER ${module} module_upper)
	if(NOT PY_${module_upper})
		if(ARGC GREATER 1 AND ARGV1 STREQUAL "REQUIRED")
			set(${module}_FIND_REQUIRED TRUE)
		endif()

        if(PYTHONINTERP_FOUND)
		    # A module's location is usually a directory, but for binary modules
		    # it's a .so file.
		    execute_process(COMMAND "${PYTHON_EXECUTABLE}" "-c" 
			    "import re, ${module}; print re.compile('/__init__.py.*').sub('',${module}.__file__)"
			    RESULT_VARIABLE _${module}_status 
			    OUTPUT_VARIABLE _${module}_location
			    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        endif(PYTHONINTERP_FOUND)

		if(NOT _${module}_status)
			set(PY_${module_upper} ${_${module}_location} CACHE STRING 
				"Location of Python module ${module}")
		endif(NOT _${module}_status)
	endif(NOT PY_${module_upper})
	find_package_handle_standard_args(PY_${module} DEFAULT_MSG PY_${module_upper})
endfunction(find_python_module)

