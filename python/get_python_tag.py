"""
Get the python tag for the wheel name based on the current interpreter.
"""
import packaging.tags

name = packaging.tags.interpreter_name()
version = packaging.tags.interpreter_version()
print(f"{name}{version}", end="")
