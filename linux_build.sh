#!/usr/bin/env bash

cc="clang++"

src_dir="./src"
bin_dir="./bin"
obj_dir="./obj"
log_dir="./log"

rm -rf "$bin_dir"
rm -rf "$obj_dir"
rm -rf "$log_dir"

mkdir "$bin_dir"
mkdir "$obj_dir"
mkdir "$log_dir"

config=${1,-"Debug"}
shift
arch=${1,-"x64"}

defines="-D_UNICODE -DUNICODE"

name="$bin_dir/slm"
obj="$obj_dir"

if [ "$arch" == "x32" ]; then
    arch="-m32"
else
    arch="-m64 -fPIC"
fi

cflags="$arch -std=c++14 -I./include -g -Wall -Wno-format $defines"

if [ "$config" == "Release" ]; then
    name="$name-r"
    obj="$obj/Release"
    cflags="$cflags -O3 -DNDEBUG"
else 
    name="$name-d"
    obj="$obj/Debug"
    cflags="$cflags -Og -DDEBUG"
fi

mkdir "$obj"

for file in "$src_dir"/*.cpp; do
    echo "Compiling: $file"
    fname=$(basename -- "$file")
    fname=${fname%.*}
    $cc -c $cflags "$file" -o "$obj/$fname.o" 2> "$log_dir/log_$fname.txt" &
done

wait

echo 
echo "Log output:"
for file in "$log_dir"/*.txt; do
    cat $file
done

obj_files=""

for file in "$obj"/*.o; do
    obj_files="$obj_files $file"
done

echo
echo "Linking:"

echo "$cc -o"$name" $obj_files $arch"
$cc -o"$name" $obj_files -m64

echo
echo "Done!"
