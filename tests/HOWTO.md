## Debian Bookworm

```bash
CXX='x86_64-linux-gnu-g++' make SUFFIX='linuxdebian12-amd64-gcc12-dynamic'
CXX='i686-w64-mingw32-g++' make SUFFIX='linuxdebian12-i686-gcc12-dynamic'
CXX='aarch64-linux-gnu-g++' make SUFFIX='linuxdebian12-arm64-gcc12-dynamic'
CXX='arm-linux-gnueabihf-g++' make SUFFIX='linuxdebian12-armhf-gcc12-dynamic'
CXX='powerpc64le-linux-gnu-g++' make SUFFIX='linuxdebian12-ppc64el-gcc12-dynamic'
```

## Debian Trixie

```bash
CXX='riscv64-linux-gnu-g++' make SUFFIX='linuxdebian13-riscv64-gcc14-dynamic'
CXX='x86_64-w64-mingw32-g++-posix' CXXFLAGS='-static -static-libstdc++ -static-libgcc -D_WIN32_WINNT=0x0601 -DWINVER=0x0601' make SUFFIX='windows7-amd64-mingwgcc14-static'
CXX='i686-w64-mingw32-g++-posix' CXXFLAGS='-static -static-libstdc++ -static-libgcc -D_WIN32_WINNT=0x0601 -DWINVER=0x0601' make SUFFIX='windows7-i686-mingwgcc14-static'
```

# macOS

```bash
CXX='clang' CXXFLAGS='-target x86_64-apple-darwin -mmacos-version-min=10.12' make SUFFIX='macos10.12-amd64-appleclang21-dynamic'
CXX='clang' CXXFLAGS='-target arm64-apple-darwin -mmacos-version-min=11.7' make SUFFIX='macos11.7-arm64-appleclang21-dynamic'
```

## FreeBSD 15

```bash
CXX='clang' CXXFLAGS='-target x86_64-unknown-freebsd' gmake SUFFIX='freebsd15-amd64-clang17-dynamic'
```

## Saigo

```bash
CXX='x86_64-nacl-clang++' make SUFFIX='nacl-amd64-saigoclang21-static'
CXX='i686-nacl-clang++' make SUFFIX='nacl-i686-saigoclang21-static'
CXX='arm-nacl-clang++' make SUFFIX='nacl-armhf-saigoclang21-static'
```
