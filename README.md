Small testprogram for DG8SAQ AVR-USB based SI570 Controllers
It requires the original firmware from DG8SAQ as it is used in the kit
FA-Synthesizer "FA-SY 1", 10 - 160 MHz from
http://www.funkamteur.de

To build untar / unzip the archiv and switch to the Si570Synth directory.

run:
mkdir -p build
cd build
qmake -o Makefile ../Si570Synth.pro
make

The executable Si570Synth can be found in the build directory.
Start Si570Synth and try to connect to the device by pressing 
Test Usb

If the test succeeds you can read the registers and control the frequency.
Calibrating the device is possible, too.
Tune the device and measure the output frequency. Enter the output frequency into the calibration field.
After pressing 
calibrate
and restarting the device tuned frequency and measured output frequency should be the same.

