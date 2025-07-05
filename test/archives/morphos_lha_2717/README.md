This is a set of archives generated using LhA 2.7.17 which shipped with
MorphOS 3.19:
```
h0_lh0.lzh        - Header level 0, -lh0- scheme (uncompressed)
h0_lh1.lzh        - Header level 0, -lh1- scheme
h0_lh5.lzh        - Header level 0, -lh5- scheme
h0_lh6.lzh        - Header level 0, -lh6- scheme
h0_metadata.lzh   - Header level 0, file with attached metadata content
h0_subdir.lzh     - Header level 0, file in subdirectories

h1_lh0.lzh        - Header level 1, -lh0- scheme (uncompressed)
h1_lh1.lzh        - Header level 1, -lh1- scheme
h1_lh5.lzh        - Header level 1, -lh5- scheme
h1_lh6.lzh        - Header level 1, -lh6- scheme
h1_metadata.lzh   - Header level 1, file with attached metadata content
h1_subdir.lzh     - Header level 1, file in subdirectories

h2_lh0.lzh        - Header level 2, -lh0- scheme (uncompressed)
h2_lh1.lzh        - Header level 2, -lh1- scheme
h2_lh5.lzh        - Header level 2, -lh5- scheme
h2_lh6.lzh        - Header level 2, -lh6- scheme
h2_metadata.lzh   - Header level 2, file with attached metadata content
h2_subdir.lzh     - Header level 2, file in subdirectories
h2_huge.lzh       - Header level 2, huge (>4GiB) file
```

## Embedded metadata contents

MorphOS allows comments to be attached to files, and these are stored as part
of the filename following a NUL (0) character, so that any other tools ignore
them since it's the C string terminator character:
```
00000000  44 42 2d 6c 68 30 2d 1d  00 00 00 1d 00 00 00 30  |DB-lh0-........0|
         |  |  |              |            |            \_ timestamp
         |  |  |              |             \_ uncompressed len
         |  |  |               \_ compressed len
         |  |   \_ compress type
         |   \_ checksum
          \_ header len

00000010  04 e3 5a 00 01 2b 6d 65  74 61 64 61 74 61 2e 74  |..Z..+metadata.t|
                  |  |  |   \_ filename
                  |  |   \_ length
                  |   \_ header level
                   \_ reserved

00000020  78 74 00 54 68 69 73 20  69 73 20 61 20 63 6f 6d  |xt.This is a com|
                   \_ comment following NUL?

00000030  6d 65 6e 74 20 6f 6e 20  74 68 65 20 66 69 6c 65  |ment on the file|
00000040  2e b8 d1 41 00 00 54 68  69 73 20 69 73 20 61 20  |...A..This is a |
            |     |   \_ Extended length = 0
            |      \_ OS ID = 'A', Amiga
             \_ CRC
```

## Large file support (>2GiB)

`h2_huge.lzh` contains a compressed file over 4GiB in size, which exceeds the
original lha 32-bit file size limit. The extended file size header is used to
support such files. MorphOS lha will only compress large files if using level 2
headers, which is why there is no equivalent test archive for level 0 or level
1 headers.

Note that the original uncompressed length field is overflowed (it records the
size as being around 400MiB):
```
00000000  3d 00 2d 6c 68 35 2d 53  5d 00 00 00 00 40 19 28  |=.-lh5-S]....@.(|
         |  |  |              |            |            \_ timestamp
         |  |  |              |             \_ uncompressed len
         |  |  |               \_ compressed len
         |  |   \_ compress type
         |   \_ checksum
          \_ header len

00000010  77 65 68 00 02 00 00 41  0b 00 01 7a 65 72 6f 2e  |weh....A...zero.|
                  |  |  |     |   |      \_ header type = 0x01 (filename)
                  |  |  |     |    \_ next header length
                  |  |  |      \_ OS ID = 'A', Amiga
                  |  |   \_ CRC16
                  |   \_ header level
                   \_ reserved

00000020  62 69 6e 13 00 42 53 5d  00 00 00 00 00 00 00 00  |bin..BS]........|
                  |     |  |                         \_ uncompressed size (64-bit)
                  |     |   \_ compressed size (64-bit)
                  |      \_ header type = 0x42 (file size header)
                   \_ next header length

00000030  40 19 01 00 00 00 05 00  00 6c fe 00 00 14 78 20  |@........l....x |
                           |      |  |      \_ end of headers
                           |      |   \_ CRC16
                           |       \_ header type = 0x00 (CRC)
                            \_ next header length
```
