#!/usr/bin/env -S bash ../.port_include.sh
port=zlib
version=1.2.11
useconfigure=true
files="https://www.zlib.net/zlib-${version}.tar.gz zlib-${version}.tar.gz
https://www.zlib.net/zlib-${version}.tar.gz.asc zlib-${version}.tar.gz.asc"

configure() {
    export CROSS_PREFIX=/usr/local/bin
    run ./configure
}

install() {
    run make DESTDIR=${SERENITY_INSTALL_ROOT} $installopts install
    ${CC} -shared -o ${SERENITY_INSTALL_ROOT}/usr/local/lib/libz.so -Wl,-soname,libz.so -Wl,--whole-archive ${SERENITY_INSTALL_ROOT}/usr/local/lib/libz.a -Wl,--no-whole-archive
    rm -f ${SERENITY_INSTALL_ROOT}/usr/local/lib/libz.la
}
