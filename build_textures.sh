#!/bin/sh
# Converts gfx/*.png into 3DS textures.
# _a = cut-out (rgba5551), _u = UI with soft alpha (rgba4444), _f = font (la44), no suffix = opaque (rgb565)
mkdir -p romfs/gfx
rm -f romfs/gfx/*.t3x
for f in gfx/*.png; do
  n=$(basename "$f" .png)
  case "$n" in
    *_a) fmt=rgba5551 ;;
    *_u) fmt=rgba4444 ;;
    *_f) fmt=la44 ;;
    *) fmt=rgb565 ;;
  esac
  echo "tex3ds $n ($fmt)"
  tex3ds -f $fmt -o "romfs/gfx/$n.t3x" "$f" || exit 1
done
echo "textures done"
