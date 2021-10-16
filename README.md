![sample](/images/comp8-2.gif)

# pico-composite8
Based on *pico-mposite* by breakingtoprogram: 

*A hacked together demo for the Raspberry Pi Pico to output a composite video signal from the GPIO*
*Using a resistor ladder to translate a 5-bit binary number on the Pico GPIO to a voltage between 0v and 1v.* 

#### 8-Bit R2R Resistor DAC
The input impedance of the Composite Monitor is 75Ω; the output impedance of the R2R ladder is R.  In order to get 1V across a 75Ω load with a 3.3V source, R needs to be 2.3 * 75 = 172.5Ω.  Closest 'standard' values are 180Ω/360Ω.  
Unfortunately using those values doesn't work for 8-bits.  The values are low enough that the internal resistance on the Pico GPIO pins becomes important, and causes visible errors on the 7- and 8-bit.  Measuring the voltage drop on the pins gives a calculated series resistance of about 40Ω, so you will need to reduce the 2R value by that much to compensate.  I used two 160Ω resistors in series:

![Wiring](/fritzing/schematic.png)

The R2R DAC will draw about 19mA total from the Pico when all the bits are '1', with a maximum of 6.5mA on any single pin.  Here's the simulation:

![simulation](/images/circuit-20210605-1521.png)

You can access the simulation here: [R2R DAC Simulation](https://tinyurl.com/yyc6fewc)

#### Bitmaps
A portion of the NTSC signal is reserved for SYNC and BLANKing:

![NTSC](/images/NTSC.gif)

On a 256 scale, BLACK works out to about 86 (255/140 * 47.5), so the values from 86 - 255 are the range from BLACK to WHITE, or 170 values.  The scripts take the 256-gray image, compress it to 170-gray, and shift it up by 86 to leave room for the SYNC.

The 8-bit gray images are created with Gimp:
- convert to grayscale, 8-bit
- flatten image
- scale image and canvas size to get 512x384 image
- export as raw image data (*.data)
File size should be exactly 196608 bytes

Use *scripts/bit2h.pl* to create an '.h' file which can be included and compiled with the program.  There's only enough room for one 512x384 image in memory though. Remove the comment in front of `#define TESTPATTERN` in cvideo.c to enable that behavior.

The images can also be stored in flash, which has room for about 8 images; for that you'll need to install **picotool**.  Put the *.data files in a directory and run *scripts/mkbin.pl*.  The resulting 'flash.bin' file can be copied to the Pico with *scripts/flashbin.sh*.  The program will step through the images in a slideshow.

#### Timing
The 640x480 resolution is accomplished by interlacing 640x240 fields with this timing:
![Timing](/images/interlaceTiming.png)

The program is doing that and can render the Indian test pattern:

![Indian](/images/indianScale.jpg)

Note the concentric circles around the 30's and the vertical/horizontal resolution lines.

#### Compiling/Uploading

Compile using VSCode with the **GCC 9.2.1 arm-none-eabi** kit (which 
unfortunately isn't available on the Raspberry Pi (yet)).  

To deploy from the development platform (e.g. X86 Ubuntu) to the Pico using SWD, you need a Raspberry Pi running **openOCD**.  The Pico connects to the Raspberry Pi like this:

![SWD Wire](/images/SWDwire.png)

On the Raspberry Pi, execute openOCD:

`openocd -c "bindto 0.0.0.0" -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg`

On the Development Machine, click the **Run and Debug** icon in the left-side menu - it will execute the `.vscode/launch.json` included in the project.  Click the triangle next to **Pico Deploy** in the top-side menu, and enter the  address of the Raspberry Pi SWD Host in the box to the right:  

![SWD host](/images/SWDhost.png)

The program will compile, upload to the Pico, and break at `main()`.  You can either remove the programmed Pico, or continue with debug commands, top-side middle menu.

#### the BUG

The interlace fails and only gives half the vertical resolution unless there's a 30 usec delay in cvideo.c at the start of scan line #0 - before the interlace starts:

![Bug](/images/bug.jpg)

15 usec isn't enough, only 30 usec will work.  Other than that, it follows the timing above... Sort of stumbled into it by accident:  in an early version there was a memcpy there that took 30 usec to complete.  When I took that out, interlace failed until I added an equivalent 30 usec delay.  Don't know why...


