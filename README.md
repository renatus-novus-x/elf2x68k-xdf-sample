# elf2x68k .xdf/.hdf Sample

[English](README.md) | [日本語](README.ja.md)

<p align="center">
  <img src="images/teapot.png" alt="X68000 / Human68k sample" width="640">
</p>

<p align="center">
  <strong><a href="https://uraraworks.github.io/WebX68k/?cpu=100&ram=12&hdd=https%3A%2F%2Fraw.githubusercontent.com%2Frenatus-novus-x%2Felf2x68k-xdf-sample%2Frefs%2Fheads%2Fmain%2Fimages%2Fteapot.zip&run=1">▶ Run teapot.zip in WebX68k</a></strong>
</p>

A small collection of Makefile examples for building X68000 programs with
[elf2x68k](https://github.com/yunkya2/elf2x68k) and packaging them into
bootable Human68k disk images.

The repository demonstrates two simple workflows:

- creating a bootable `.xdf` floppy disk image
- creating a bootable SASI `.hdf` hard disk image

The Makefiles download the official Human68k 3.02 distribution at build time
and use [xdftool](https://github.com/yunkya2/x68kmisc/tree/main/xdftool) to
create the disk images.

## Repository layout

```text
.
├── README.md
├── README.ja.md
├── images/
│   ├── teapot.png
│   └── teapot.zip
└── samples/
    ├── hello/
    │   ├── Makefile
    │   └── hello.c
    └── teapot/
        ├── Makefile
        ├── teapot.c
        └── model.obj
```

## Requirements

The Makefiles are intended for Linux environments such as Ubuntu 24.04 LTS,
including Ubuntu running under WSL2.

Install [elf2x68k](https://github.com/yunkya2/elf2x68k) first and make sure
`m68k-xelf-gcc` is available in `PATH`.

Install the required host tools:

```sh
sudo apt update
sudo apt install make curl unar python3
```

`unar` is used to extract the official Human68k LZH archive while interpreting
the archived Japanese filenames as Shift_JIS.

## .xdf sample

The `samples/hello` directory contains a minimal .xdf example.

```sh
cd samples/hello
make
```

The build creates:

```text
hello.x
hello.xdf
hello.zip
```

`hello.xdf` is a bootable Human68k floppy disk image containing `hello.x`.
The Makefile also updates `AUTOEXEC.BAT` so that `hello.x` runs automatically
after Human68k boots.

To inspect the generated image:

```sh
make check-xdf
```

## .hdf sample

The `samples/teapot` directory demonstrates creation of a bootable SASI hard
disk image.

```sh
cd samples/teapot
make
```

The build creates:

```text
teapot.x
teapot.hdf
teapot.zip
```

The .hdf uses xdftool's `/h10` format and is generated as a 10 MB SASI hard
disk image.

To inspect the generated image:

```sh
make check-hdf
```

## Build process

Both samples follow the same basic process:

```text
C source
   |
   | m68k-xelf-gcc
   v
.X executable

HUMN302I.LZH
   |
   | unar -e shift_jis
   v
HUMAN302.XDF
   |
   | xdftool.py
   v
Human68k files
   |
   | add application files
   | update AUTOEXEC.BAT
   v
bootable .xdf / .hdf
```

The official Human68k 3.02 archive is downloaded from:

```text
http://retropc.net/x68000/software/sharp/human302/HUMN302I.LZH
```

Human68k itself is not stored in this repository.

## Human68k distribution terms

Generated .xdf/.hdf images contain Human68k files.

Before redistributing generated images, please read and follow the
distribution terms included in the official Human68k 3.02 archive.

The Makefiles preserve the original permission text and include it in the
generated ZIP archives.

## Clean

Remove generated application and disk-image files:

```sh
make clean
```

Remove downloaded and intermediate build files as well:

```sh
make distclean
```

## References

- [elf2x68k](https://github.com/yunkya2/elf2x68k)
- [elf2x68k-sample](https://github.com/yunkya2/elf2x68k-sample)
- [xdftool / x68kmisc](https://github.com/yunkya2/x68kmisc/tree/main/xdftool)
- [Human68k 3.02](http://retropc.net/x68000/software/sharp/human302/)

## Acknowledgements

Thanks to the authors and contributors of elf2x68k and xdftool, and to the
X68000 community.
