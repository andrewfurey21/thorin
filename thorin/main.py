import argparse, pathlib, webbrowser
from typing import List
from collections import namedtuple

from elftools.elf.elffile import ELFFile, DWARFInfo
from elftools.elf import constants
from elftools.dwarf.lineprogram import (
    LineProgram,
    LineProgramEntry,
)
import iced_x86


LineInfo = namedtuple("LineInfo", "filename line address")
InstructionInfo = namedtuple("InstructionInfo", "address inst")


def line_program_entry_file_name(
    line_program: LineProgram, line_program_entry: LineProgramEntry
) -> pathlib.Path:
    file_entries = line_program.header["file_entry"]
    file_index = line_program_entry.state.file

    assert line_program.header["version"] == 5
    assert file_index > 0

    file_entry = file_entries[file_index]
    dir_index = file_entry["dir_index"]

    if dir_index == 0:
        return pathlib.Path(file_entry["name"].decode("utf-8"))

    directory = line_program.header["include_directory"][dir_index]
    return pathlib.Path.joinpath(directory, file_entry.name)


js = """
<script>
const codeblocks = document.querySelectorAll('.code');

document.querySelectorAll('.line').forEach(line => {
  line.addEventListener('click', () => {
    const lineClass = [...line.classList].find(c => c.startsWith('line-'));
    if (!lineClass) return;

    codeblocks.forEach(block => {
      if (block.contains(line)) return;

      const target = block.querySelector(`.${lineClass}`);
      if (target) {
        target.scrollIntoView({
          behavior: 'smooth',
          block: 'center'
        });
      }
    });
  });
});
</script>

"""


def write_html(
    instructions: List[InstructionInfo], lines: List[LineInfo], output_path: str
):
    files = set(line.filename for line in lines)
    text_lines = []
    for name in files:
        with open(name, "r") as f:
            text_lines += f.read().split("\n")

    with open(output_path, "w") as f:
        f.write(
            """
            <!DOCTYPE html>
            <style>
                .wrap { display: flex; gap: 10px; }
            </style>
            """
        )

        line_nums = [line.line for line in lines]
        addresses = {line.address: line.line for line in lines}

        f.write('<div class="wrap">')

        f.write("<style>")
        for i, line in enumerate(text_lines):
            if i + 1 in line_nums:
                f.write(
                    f"*:has(.line-{i + 1}:hover) .line-{i + 1} {{\n background:red\n}}\n"
                )
        f.write("</style>")

        f.write('<pre style="overflow-y: auto; height: 1000px;"><code class="code">')
        for i, line in enumerate(text_lines):
            if line.strip().startswith("#include"):
                continue
            if i + 1 in line_nums:
                f.write(f'<span class="line-{i + 1} line">')
                f.write(line + "</span><br>")
            else:
                f.write(line + "<br>")
        f.write("</code></pre>")

        f.write('<pre style="overflow-y: auto; height: 1000px;"><code class="code">')
        for i, instruction in enumerate(instructions):
            if instruction.address in addresses:
                line_num = addresses[instruction.address]
                f.write(f'<span class="line-{line_num} line">')
                f.write(str(instruction.inst))
                f.write("</span>")
                f.write("<br>")
            else:
                f.write(str(instruction.inst))
                f.write("<br>")

        f.write("</code></pre>")

        f.write("</div>")

        f.write(js)


def display_function_assembly(input_path: str, output_path: str):
    elf = ELFFile.load_from_path(input_path)
    assert elf.has_dwarf_info(), input_path + " does not have DWARF debug info."

    num_executable = 0
    x_segment = None
    for segment in elf.iter_segments():
        if segment.header["p_flags"] & constants.P_FLAGS.PF_X == 1:
            assert num_executable == 0, "Can only parse one executable segment"
            num_executable += 1
            x_segment = segment

    assert x_segment is not None, "Could not find executable segment"
    offset = x_segment.header["p_offset"]
    vaddr = x_segment.header["p_vaddr"]
    assert offset is not None

    disassembly = iced_x86.Decoder(
        bitness=elf.elfclass, data=x_segment.data(), ip=vaddr
    )
    assert disassembly.can_decode, "Disassembly could not be decoded."

    # inst_infos = [InstructionInfo(disassembly.ip, inst) for inst in disassembly] # off by one
    inst_infos = []
    ip = disassembly.ip
    for inst in disassembly:
        inst_infos.append(InstructionInfo(ip, inst))
        ip = disassembly.ip

    dwarf: DWARFInfo = elf.get_dwarf_info()

    line_infos = []
    for cu in dwarf.iter_CUs():
        line_program: LineProgram = dwarf.line_program_for_CU(cu)
        assert line_program is not None, f"Line program for {cu.header} does not exist"

        for i, line_program_entry in enumerate(line_program.get_entries()):
            if line_program_entry.state is None:
                continue

            filename = line_program_entry_file_name(line_program, line_program_entry)

            line = line_program_entry.state.line
            address = line_program_entry.state.address

            line_infos.append(LineInfo(filename, line, address))

    write_html(inst_infos, line_infos, output_path)
    webbrowser.open(output_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--output", default="index.html", required=False)
    args = parser.parse_args()
    display_function_assembly(args.binary, args.output)
