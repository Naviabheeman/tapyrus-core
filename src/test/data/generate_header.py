#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin developers
# Copyright (c) 2019 Chaintope Inc.

import os
import sys


def main(test_name, input_file):
    with open(input_file, "rb") as f:
        contents = f.read()

    print("#include <string_view>")
    print("")
    print("namespace json_tests {")
    print("inline constexpr char detail_{}_bytes[] {{".format(test_name))
    print(", ".join(map(lambda x: "0x{:02x}".format(x if sys.version_info.major >= 3 else ord(x)), contents)))
    print("};")
    print("")
    print("inline constexpr std::string_view {}{{".format(test_name))
    print("    detail_{}_bytes,".format(test_name))
    print("    sizeof(detail_{}_bytes)".format(test_name))
    print("};")
    print("}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("We need additional pylons!")
        os.exit(1)

    main(sys.argv[1], sys.argv[2])