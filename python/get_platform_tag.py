"""
Get the platform tag most appropriate for the given wheel file.
"""
import sys, os, errno
from auditwheel import wheel_abi
import auditwheel.policy

def get_platform_tags(whl_file):
    """
    For the given wheel file, get the 'overall_tag' that is deemed
    most appropriate by the functions in the auditwheel package
    """
    wheel_policy = auditwheel.policy.WheelPolicies()
    winfo = wheel_abi.analyze_wheel_abi(wheel_policy, whl_file, ())
    return winfo.overall_tag

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Give exactly one arg to get_platform_tags", file=sys.stderr)
        sys.exit(-1)
    if not os.path.exists(sys.argv[1]):
        print(f"wheel file not found {sys.argv[1]}", file=sys.stderr)
        sys.exit(-1)
    print(get_platform_tags(sys.argv[1]), end="")
