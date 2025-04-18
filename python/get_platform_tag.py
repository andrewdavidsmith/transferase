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
import sys, os, errno, platform, subprocess, re

def get_platform_tags_linux(whl_file):
    """
    For the given wheel file, get the 'overall_tag' that is deemed
    most appropriate by the functions in the auditwheel package
    """
    cmd = f"auditwheel show {whl_file}"
    p = subprocess.run(
        cmd,
        capture_output=True,
        shell=True,
        check=False,
    )
    text = p.stdout.decode().strip().replace("\n", " ").replace("  ", " ")
    # ADS: Updated match here grabs the part of the auditwheel output that is
    # below all the dependencies and seems to be more specific.
    tag_match = re.search(
        'This constrains the platform tag to "([a-zA-Z0-9_]*)"', text
    )
    # ADS: previous pattern below didn't always grab the right tag
    # tag_match = re.search('tag: "([a-zA-Z0-9_]*)"', text)
    return tag_match.group(1) if tag_match and tag_match.groups() else None


def get_platform_tags_macos(whl_file):
    """
    For the given wheel file, get the platform tag from the packaging
    module that comes first in the list, and so is the most specific
    that is compatible with the build system
    """
    import packaging.tags
    return next(packaging.tags.platform_tags())


def get_platform_tags_from_so_macos(so_file):
    """
    For the given shared library file, get the platform tag from the packaging
    module that comes first in the list, and so is the most specific that is
    compatible with the build system
    """
    # get the architecture
    arch_cmd = f"lipo -archs {so_file}"
    p = subprocess.run(
        arch_cmd,
        capture_output=True,
        shell=True,
        check=False,
    )
    arch = p.stdout.decode().strip()

    # plat_cmd = f"vtool -show-build {so_file} | grep platform"
    # p = subprocess.run(
    #     plat_cmd,
    #     capture_output=True,
    #     shell=True,
    #     check=False,
    # )
    # plat = p.stdout.decode().strip().split()[-1]
    # plat = plat.lower()
    plat = "macosx"

    minos_cmd = f"vtool -show-build {so_file} | grep minos"
    p = subprocess.run(
        minos_cmd,
        capture_output=True,
        shell=True,
        check=False,
    )
    minos = p.stdout.decode().strip().split()[-1]
    minos = minos.replace(".", "_")

    return f"{plat}_{minos}_{arch}"


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
        if sys.argv[1].endswith(".whl"):
            the_tag = get_platform_tags_macos(sys.argv[1])
        elif sys.argv[1].endswith(".so"):
            the_tag = get_platform_tags_from_so_macos(sys.argv[1])
        else:
            print("Invalid file type (not .so or .whl)")
            sys.exit(-1)
    else:
        print(f"System not currently supported: {platform.system()}")
        sys.exit(-1)
    if not the_tag:
        print("Failed to get platform tag")
        sys.exit(-1)
    print(the_tag, end="")
