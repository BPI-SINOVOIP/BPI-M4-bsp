## **BPI-M4-bsp**
Banana Pi M4 board bsp (u-boot 2015.7 & Kernel 4.9.119)


----------
**Prepare**

Get the docker image from [Sinovoip Docker Hub](https://hub.docker.com/r/sinovoip/bpi-build-linux-4.4/) , Build the source code with this docker environment.

 **Build**

Build all bsp packages, please run

`#./build.sh 1`

Target download packages in SD/bpi-m4 after build. Please check the build.sh and Makefile for detail

Build spi-loader, please run

    $ cd u-boot-rtk
    $ ./build_bananapi_m4_spi.sh 1   //1GB ddr
    $ ./build_bananapi_m4_spi.sh 2   //2GB ddr
  
 Target binary file in u-boot-rtk/DVRBOOT_OUT/

**Install**

Get the image from [bpi](http://wiki.banana-pi.org/Banana_Pi_BPI-M4#Image_Release) and download it to the SD card. After finish, insert the SD card to PC

    # ./build.sh 7

Choose the type, enter the SD dev, and confirm yes, all the build packages will be installed to target SD card.

![Install](https://raw.githubusercontent.com/Dangku/readme/master/m4/bpi-install.png)
