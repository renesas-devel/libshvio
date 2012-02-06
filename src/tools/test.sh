#!/bin/ash
#
# Script to test VIO operations using the tools


# Check all format conversions
for src in 888 rgb yuv x888; do \
  for dst in 888 rgb yuv x888; do \
    shvio-convert -s vga -S vga vga.${src} out_${src}.${dst}; \
  done; \
done

# Display them
for src in 888 rgb yuv x888; do \
  for dst in 888 rgb yuv x888; do \
    shvio-display -s vga out_${src}.${dst}; \
  done; \
done



# Check rotation & mirroring across all formats
for f in 0x0 0x1 0x2 0x30 0x10 0x20 0x11 0x21; do \
  for src in 888 rgb yuv x888; do \
    for dst in 888 rgb yuv x888; do \
      shvio-convert -s vga -f $f vga.${src} out_${f}_${src}.${dst}; \
    done; \
  done; \
done

# HOST: Convert RGB24 files to png using ImageMagick
cp /tftpboot/rootfs/root/*.888 .
rename "s/888$/rgb/" *
for dst in 888 rgb yuv x888; do \
  for f in 0x0 0x30 0x10 0x20; do \
    convert -size 640x480 -depth 8 out_${f}_${dst}.rgb  out_${f}_${dst}.png
  done; \
  for f in 0x1 0x2 0x11 0x21; do \
    convert -size 480x640 -depth 8 out_${f}_${dst}.rgb  out_${f}_${dst}.png
  done; \
done
