# Dependencies

- clang-14 (and associated tools)
  - Why clang over gcc? Because clang is already a cross-compiler and doesn't require me building a gcc toolchain targeting i386 from scratch.
  - Why v14? This is the version of lld that supports binary output format.
- python3
- tar (GNU) v1.34
- Tools needed by grub-mkrescue
  - mformat
    - `apt-get install mtools`
  - xorriso
    - `apt-get install xorriso`
- QEMU (for i386 emulation)
  - `sudo apt-get install qemu-kvm qemu`

# Building

```
# Download dependencies.
# clang-14 (https://github.com/llvm/llvm-project/releases/tag/llvmorg-14.0.0)
$ wget https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
$ tar -xf clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz

# Setup the build.
$ mkdir build
$ cd build
$ cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=$(realpath --no-symlinks ../clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04/bin/clang++) -DCMAKE_C_COMPILER=$(realpath --no-symlinks ../clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04/bin/clang) -DCMAKE_EXPORT_COMPILE_COMMANDS=1

# Do the actual build.
$ ninja os.img
```

NOTE: Depending on your environmental setup, the `cmake` and `ninja` invocations may
not work as smoothly as just entering them above. (If I remember to, I'll list some
workflows/issues I ran into.)

# Running

```
$ qemu-system-i386 -kernel kernel/kernel.debug -nographic -no-reboot

# If initrd is built:
$ qemu-system-i386 -kernel kernel/kernel.debug -nographic -no-reboot -initrd userboot/userboot.img
```

# Formatting

```
$ clang-format -style=file -i  $(find . -name '*.h') $(find . -name '*.cpp') libcxx/include/*
```

# Debugging flat binary
```
# Checked with v2.38.
$ objdump -b binary -m i386 userboot/userboottest1.bin -D
```

Note that llvm-objdump doesn't support `-b`.

# Debugging crashes in ELF files
```
# Assuming the stack trace is saved in `/tmp/out`.
# The binary in this example is `userboot/userboot-stage2`.
# In this example, the binary was loaded at 0x00c00000.
$ cat /tmp/out | ../clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04/bin/llvm-symbolizer --obj=userboot/userboot-stage2 -p -a --adjust-vma=0x00c00000
```
