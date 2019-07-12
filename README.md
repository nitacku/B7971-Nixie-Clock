# B7971-Nixie-Clock
Nixie clock based on the Burroughs B-7971 Alphanumeric Nixie Tube

## Schematic and Board files
The B7971-Nixie-Clock schematic and board files are available in the "schematic" directory. The schematic and board files were created using ExpressPCB, available for download from the following link: https://www.expresspcb.com/. There is a PDF version of the schematic as well if you prefer not to use ExpressPCB.

## Firmware
Follow the steps below to build the source for the B7971-Nixie-Clock.

This source can be built using two methods:
1. Using the Arduino IDE (after updating AVR-GCC).
2. Using a containerized environment with Docker.


[Method 1] Arduino IDE
-----------------------------------------------
1. Install the latest version of the Arduino IDE: https://www.arduino.cc/en/Main/Software
2. Update AVR-GCC used by Arduino to a version >=8.1. Follow this guide: http://blog.zakkemble.net/avr-gcc-builds/
3. Download additional libraries:
  - https://github.com/nitacku/nAudio
  - https://github.com/nitacku/nCoder
  - https://github.com/nitacku/nDisplay
  - https://github.com/nitacku/nI2C
  - https://github.com/nitacku/nRTC
  - https://github.com/photonicfusion/nStd
4. Follow this guide to install downloaded libraries to Arduino: https://www.arduino.cc/en/Guide/Libraries#toc4
5. Download the B7971-Nixie-Clock source and open /Firmware/B7971-Nixie-Clock/B7971-Nixie-Clock.ino with the Arduino IDE.
6. Press the build button in the Arduino IDE to build the source. If everything is correct, the build will complete without error.


[Method 2] Docker
-----------------------------------------------
1. Install Docker CE on your build system: https://docs.docker.com/install/
2. Install the Arduino Builder for Docker: https://hub.docker.com/r/photonicfusion/arduino_builder
3. Download additional libraries:
  - https://github.com/nitacku/nAudio
  - https://github.com/nitacku/nCoder
  - https://github.com/nitacku/nDisplay
  - https://github.com/nitacku/nI2C
  - https://github.com/nitacku/nRTC
  - https://github.com/photonicfusion/nStd
4. Create a directory structure with top level named "B7971-Nixie-Clock" and two sub-directories named "src" and "libs".
5. Extract the libraries downloaded from the previous step into the "libs" directory.
6. Download the B7971-Nixie-Clock source and copy /Firmware/B7971-Nixie-Clock to the "src" directory.
7. Save one of the convenience scripts provided on the Arduino Builder to the B7971-Nixie-Clock directory.
8. Run the convenience script to build the source.
9. If the build completed successfully, a .hex file will be located in the "src" directory.
