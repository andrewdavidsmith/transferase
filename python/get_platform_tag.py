# MIT License
#
# Copyright (c) 2025 Andrew Smith
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

"""
Get the platform tag most appropriate for the given wheel file.
"""
import sys, os, errno, platform

def get_platform_tags_linux(whl_file):
    """
    For the given wheel file, get the 'overall_tag' that is deemed
    most appropriate by the functions in the auditwheel package
    """
    from auditwheel import wheel_abi
    import auditwheel.policy

    wheel_policy = auditwheel.policy.WheelPolicies()
    winfo = wheel_abi.analyze_wheel_abi(wheel_policy, whl_file, ())
    return winfo.overall_tag


def get_platform_tags_macos(whl_file):
    """
    For the given wheel file, get the platform tag from the packaging
    module that comes first in the list, and so is the most specific
    that is compatible with the build system
    """
    import packaging.tags
    return next(packaging.tags.platform_tags())


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Give exactly one arg to get_platform_tags", file=sys.stderr)
        sys.exit(-1)
    if not os.path.exists(sys.argv[1]):
        print(f"wheel file not found {sys.argv[1]}", file=sys.stderr)
        sys.exit(-1)
    the_tag = None
    if platform.system() == "Linux":
        the_tag = get_platform_tags_linux(sys.argv[1])
    elif platform.system() == "Darwin":
        the_tag = get_platform_tags_macos(sys.argv[1])
    else:
        print(f"System not currently supported: {platform.system()}")
        sys.exit(-1)
    if not the_tag:
        print("Failed to get platform tag")
        sys.exit(-1)
    print(the_tag, end="")
