#!/bin/bash
set -e

cd /root/github/weron-signaler-c

MUSL_ROOT=/opt/musl
LWS_BUILDS=/tmp/lws-build-weron

# Define all targets
# Format: name:cc:ldflags:out_dir:out_name:lws_dir:extra_libs
declare -a TARGETS=(
  "linux:x86_64-linux-musl-gcc:-static:build-linux:weron-signaler-c:$LWS_BUILDS/linux_musl_static:"
  "linux_x86_64_musl_static:$MUSL_ROOT/x86_64/x86_64-linux-musl-cross/bin/x86_64-linux-musl-gcc:-static:build-linux-x86_64-musl-static:weron-signaler-c:$LWS_BUILDS/linux_musl_static:"
  "linux_i686_musl_static:$MUSL_ROOT/i686/i686-linux-musl-cross/bin/i686-linux-musl-gcc:-static:build-linux-i686-musl-static:weron-signaler-c:$LWS_BUILDS/linux_x86_musl_static:"
  "linux_aarch64_musl_static:$MUSL_ROOT/aarch64/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc:-static:build-linux-aarch64-musl-static:weron-signaler-c:$LWS_BUILDS/linux_aarch64_musl_static:"
  "linux_armhf:arm-linux-gnueabihf-gcc:-static:build-armhf:weron-signaler-c-armhf:/tmp/lws-build/armhf:"
  "linux_armv7_musleabihf_static:$MUSL_ROOT/armv7l/armv7l-linux-musleabihf-cross/bin/armv7l-linux-musleabihf-gcc:-static:build-linux-armv7-musleabihf-static:weron-signaler-c:$LWS_BUILDS/linux_armv7_musleabihf_static:"
  "linux_mipsel:mipsel-linux-gnu-gcc:-static:build-mipsel:weron-signaler-c-mipsel:/tmp/lws-build/mipsel:"
  "linux_mipsel_musl_sf:$MUSL_ROOT/mipsel_le/mipsel-linux-muslsf-cross/bin/mipsel-linux-muslsf-gcc:-static:build-mipsel-musl-sf:weron-signaler-c-mipsel-sf:$LWS_BUILDS/linux_mipsel_musl_static:"
  "linux_mips_be_musl_static:$MUSL_ROOT/mips_be/mips-linux-muslsf-cross/bin/mips-linux-muslsf-gcc:-static:build-linux-mips-be-musl-static:weron-signaler-c:$LWS_BUILDS/linux_mips_musl_static:"
  "linux_mipsisa64r6:mipsisa64r6-linux-gnuabi64-gcc:-static:build-mipsisa64r6:weron-signaler-c-mipsisa64r6:/tmp/lws-build/mipsisa64r6:"
  "linux_mipsisa64r6el:mipsisa64r6el-linux-gnuabi64-gcc:-static:build-mipsisa64r6el:weron-signaler-c-mipsisa64r6el:$LWS_BUILDS/mipsisa64r6el:"
  "win_x64:x86_64-w64-mingw32-gcc:-static-libgcc:build-win:weron-signaler-c.exe:/tmp/libwebsockets-4.1.6/build-win-static-crt:"
  "win_x64_musl:$MUSL_ROOT/mingw_x64/x86_64-w64-mingw32-cross/bin/x86_64-w64-mingw32-gcc:-static-libgcc:build-win-x64-musl:weron-signaler-c.exe:/tmp/libwebsockets-4.1.6/build-win-x64-musl-static-crt:"
  "win_i686_musl:$MUSL_ROOT/mingw_i686/i686-w64-mingw32-cross/bin/i686-w64-mingw32-gcc:-static:build-win-i686-musl:weron-signaler-c.exe:$LWS_BUILDS/win_i686_musl:"
)

for target in "${TARGETS[@]}"; do
  IFS=: read -r name cc ldflags out_dir out_name lws_dir extra_libs <<< "$target"
  echo "=== Building $name ==="

  LWS_CFLAGS="-I$lws_dir/include"

  if [[ "$name" == win_* ]]; then
    WINPTHREAD=$($cc -print-file-name=libwinpthread.a)
    LWS_LIBS="$lws_dir/lib/libwebsockets_static.a -lws2_32 -lbcrypt -liphlpapi -lole32 $WINPTHREAD $extra_libs"
    LINK_EXTRA=""
  else
    LWS_LIBS="$lws_dir/lib/libwebsockets.a"
    LINK_EXTRA="-lpthread"
  fi

  mkdir -p "$out_dir"

  $cc -Wall -Wextra -O2 -g $LWS_CFLAGS -c -o "$out_dir/main.o" main.c 2>&1 | tail -3
  $cc -Wall -Wextra -O2 -g $LWS_CFLAGS -c -o "$out_dir/relay.o" relay.c 2>&1 | tail -3

  if [[ "$name" == win_x64_musl ]]; then
    $cc $LWS_CFLAGS -Wl,--allow-multiple-definition -o "$out_dir/$out_name" \
      "$out_dir/main.o" "$out_dir/relay.o" $LWS_LIBS $ldflags $LINK_EXTRA 2>&1 | tail -3
  else
    $cc $LWS_CFLAGS -o "$out_dir/$out_name" "$out_dir/main.o" "$out_dir/relay.o" \
      $LWS_LIBS $ldflags $LINK_EXTRA 2>&1 | tail -3
  fi

  echo "  Result: $(ls -la "$out_dir/$out_name" 2>/dev/null | awk '{print $5}') bytes"
done

echo ""
echo "=== All builds complete ==="
ls -la build-*/