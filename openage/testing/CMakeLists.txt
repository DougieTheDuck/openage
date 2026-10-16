add_cython_modules(
	cpp_testing.pyx
	misc_cpp.pyx
)

add_py_modules(
	__init__.py
	doctest.py
	list_processor.py
	main.py
	network_two_process.py
	testing.py
	testlist.py
	benchmark.py
)
