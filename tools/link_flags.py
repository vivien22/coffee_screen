# PlatformIO extra script: gives the link step the section flags of the compile step.
# The build is LTO: the code is generated at LINK time, and without -ffunction-sections / -fdata-sections there every unit stays in
# one piece, so the linker cannot drop the functions nobody calls (the text engine of Adafruit_GFX and its 5 x 7 font, ...).
Import("env")
env.Append(LINKFLAGS=["-ffunction-sections", "-fdata-sections"])
