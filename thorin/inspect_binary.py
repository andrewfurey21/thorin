import argparse
from elftools.elf.elffile import ELFFile, DWARFInfo

"""
Goal: take a function name, display the function seperated line by line
with it's generated assembly alongside it.

generate a html file with lines of c code next to assembly, when assembly/c is highlighted highlight the corresponding c/assembly.

"""


def display_function_assembly(path: str):
    e: ELFFile = ELFFile.load_from_path(path)

    for i in range(100):
        s = e.get_segment(i)
        print(s.data())


if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("binary")

    args = arg_parser.parse_args()
    display_function_assembly(args.binary)
