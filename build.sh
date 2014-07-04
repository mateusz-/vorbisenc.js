#!/bin/bash

dir=`pwd`

cd ogg

make clean
git reset --hard HEAD
git clean -f
./autogen.sh
sed -i "s/-O20/-O2/g" configure 
sed -i "s/-O4/-O2/g" configure 
emconfigure ./configure
emmake make
rm src/.libs/libogg.a

cd ../vorbis

make clean
git reset --hard HEAD
git clean -f
sed -i "s/\$srcdir\/configure/#/" autogen.sh
./autogen.sh
sed -i "s/-O20/-O2/g" configure
sed -i "s/-O4/-O2/g" configure
sed -i "s/\$ac_cv_func_oggpack_writealign/yes/" configure
emconfigure ./configure --disable-oggtest --with-ogg=$dir/ogg --with-ogg-libraries=$dir/ogg/src/.libs --with-ogg-includes=$dir/ogg/include
EMCC_CFLAGS="--ignore-dynamic-linking" emmake make
rm lib/.libs/libvorbis.a
rm lib/.libs/libvorbisenc.a
cd examples
EMCC_CFLAGS="--ignore-dynamic-linking" emmake make

emcc -O2 --closure 0 -I $dir/vorbis/include -I $dir/ogg/include -L$dir/vorbis/lib/.libs -L$dir/ogg/src/.libs -lvorbis -lvorbisenc -logg $dir/node_encoder_example.c -o $dir/node_encoder_example.js

cd $dir

emcc --bind --js-library $dir/web-worker/web-worker-library.js -s TOTAL_MEMORY=134217728 -s EXPORTED_FUNCTIONS='["_lexy_encoder_start", "_lexy_encoder_write", "_lexy_encoder_finish"]' -I $dir/vorbis/include -I $dir/ogg/include -L$dir/vorbis/lib/.libs -L$dir/ogg/src/.libs -lvorbis -lvorbisenc -logg $dir/web-worker/web-worker.cpp -o $dir/web-worker/web-worker.js 
cat $dir/web-worker/web-worker-message.js >> $dir/web-worker/web-worker.js
# emcc -s ASM_JS=1 -s -s EXPORTED_FUNCTIONS='["_lexy_encoder_start", "_lexy_encoder_write", "_lexy_encoder_finish", "_lexy_test", "_lexy_write_test", "_lexy_get_buffer_length", "_lexy_get_buffer"]' -I $dir/vorbis/include -I $dir/ogg/include -L$dir/vorbis/lib/.libs -L$dir/ogg/src/.libs -lvorbis -lvorbisenc -logg $dir/web-worker/web-worker.cpp -o $dir/web-worker/web-worker.js 
# emcc -O2 -s ASM_JS=1 -s BUILD_AS_WORKER=1 -s EXPORTED_FUNCTIONS='["_lexy_encoder_start", "_lexy_encoder_write", "_lexy_encoder_finish", "_lexy_test", "_lexy_write_test", "_lexy_get_buffer_length", "_lexy_get_buffer"]' -I $dir/vorbis/include -I $dir/ogg/include -L$dir/vorbis/lib/.libs -L$dir/ogg/src/.libs -lvorbis -lvorbisenc -logg ogg-vorbis-encoder.cpp -o vorbis.small.js
