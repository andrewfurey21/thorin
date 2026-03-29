# thorin

Compare you're code to the compiled assembly with the dwarf debug format, basically compiler explorer or `objdump -d`.

very basic prototype, kind of buggy, I've only tested on statically linked binaries that come from one compilation unit with `-O0` and dwarf 5. It would be cool to extend it so that you could highlight the assembly for nested parts of code, like nested functions, a bit more granular to help explain what the assembly is doing. probably impossible for optimized code, but DWARFs DIE tree structure should allow for `-O0` without too much work.

![Video of parsing DWARF format to see what C code maps to assembly](./example/thorin.gif)

