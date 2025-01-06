# MIT License
#
# Copyright (c) 2024 Andrew Smith
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# cmake --install build_dir --prefix /path/to/install/root

install(TARGETS transferase
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install an abbreviated alias
install(PROGRAMS $<TARGET_FILE:transferase>
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  RENAME xfrase
)

# Install the versioned JSON file to the appropriate share directory
# (e.g., /usr/local/share/transferase)
install(FILES ${PROJECT_BINARY_DIR}/share/transferase/transferase_data_${PROJECT_VERSION}.json
  DESTINATION ${CMAKE_INSTALL_DATADIR}/transferase
)

# Install the bash completions script to the appropriate directory
# (e.g., /usr/share/bash-completion/completions/)
install(FILES ${CMAKE_SOURCE_DIR}/data/transferase-bash-completion
  DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
  RENAME transferase
)
