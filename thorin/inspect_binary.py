from elftools.elf.elffile import ELFFile, DWARFInfo

"""

Goal: write an alternative compiler explorer that uses the dwarf debug format.

pass in a function game and get each line and what is corresponds too.

"""


def process_elf(path: str):
    with open(path, "rb") as f:
        e: ELFFile = ELFFile(f)
        dwarf_info: DWARFInfo = e.get_dwarf_info()

        dwarf_info.get_CU_containing(refaddr)
    #
    #     # for section in e.iter_sections():
    #     #     if section.name.startswith(".debug"):
    #     #         print(section.name)


if __name__ == "__main__":
    binary_path = "test"
    code_path = "test.c"
