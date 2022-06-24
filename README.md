# ESP32 AOO test example

Prerequisites:

Install ESP IDF (follow steps 1-3):
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-1-install-prerequisites


Build steps:

1) In the "main" folder, create a symlink folder "aoo" to your AOO repo.
   Make sure to use the latest "esp32" branch!

2) export all the necessary environment variables
   $ . <IDF_PATH>/export.sh

3) configure the project:
   $ idf.py -DAOO_CODEC_OPUS=OFF -DOPUS_INSTALL_PKG_CONFIG_MODULE=OFF -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF -DAOO_BUILD_PD_EXTERNAL=OFF -DAOO_BUILD_SC_EXTENSION=OFF -DAOO_BUILD_SHARED_LIBRARY=OFF -DAOO_NET=ON set-target esp32
   
   If you want Opus support, use -DAOO_CODEC_OPUS=ON instead.

   //note from Eric: I had to change "-DAOO_NET=OFF" to "-DAOO_NET=ON"
   // and to save on space limitations, added "-DAOO_MAX_PACKET_SIZE=1024" (instead of default of 4096)

4) enable C++ exceptions:
   $ idf.py menuconfig
   "Compiler options" --> tick "Enable C++ exceptions" --> quit ("q") and save

5) build the project:
   $ idf.py build
   
