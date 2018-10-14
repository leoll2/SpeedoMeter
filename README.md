# SpeedoMeter

SpeedoMeter is a Linux kernel module that controls a homemade device to measure speed (namely a speed camera) and display it. It also offers a bunch of additional features, including the history of past measurements in a ranking fashion.

## Hardware

* Raspberry Pi 3 Model B
* 2x PIR sensors
* 2x LED
* 4-digits 7-segments display
* Breadboard
* 9x Resistors
* 6x NPN transistors
* Many wires

This is how the whole circuit looks like:

![](img/hardware.jpeg)

With a zoom on the breadboard:

![](img/electronics.jpeg)

And this is one of the two PIRs used:

![](img/pir_sensor.jpeg)

## How does it work

From a user perspective, the device is represented by an entry in /dev, but it is also present is sysfs.
PIR sensors are positioned at fixed distance (which can be specified when loading the module). When someone (or something) walks in front of any of the two sensors, an interrupt is sent to the kernel (via RPi GPIO) and handled, storing the associated timestamp. 
The speed is then easily computed dividing the sensors distance by the difference between these two timestamps.
The kernel also maintains a leaderboard of users and their best time, sorted by speed.

## How to use

### Compiling

First edit the Makefile specifying the kernel code path.
Then, simply do:

`make rpi_all`

If you want to revert the compilation, do:

`make rpi_clean`

You can also `make rpi` for both cleaning and compiling, in order.

### Loading / unloading module

To load the module:

`sudo insmod speed.ko sensors_dist=10`

sensors_dist is the distance (in decimeters) between the two PIRs and can be omitted, defaulting to 10.

To remove the module:

`sudo rmmod speed`

### Running

After loading the module, move to /dev/ and register yourself (or the name of the moving object) in speed: 

`sudo sh -c "echo 'leonardo' > speed"`

Now you can run in front of PIR1 and then PIR2 as fast as possible. Immediately after the display will show your time for 5 seconds, as below:

![](img/display.jpeg)

Your time and speed will be added to the leaderboard, which you can find in /sys/devices/virtual/misc/speed/
Check it with:

`sudo cat leaderboard`

The result should be something like this:

![](img/leaderboard.png)

Are you the leader in the ranking? Then your name will be stored in the corresponding attribute too:

`sudo cat leader`

Write anything in the reset attribute to completely flush the leaderboard:

`sudo sh -c "echo 'wabbit' > reset"`

## Additional notes

* The display will show a default pattern when not used.
* A led lights up when its corresponding PIR triggers. This is done in hardware, not software, so an interrupt may not necessarily be generated (if IRQs are disabled, for instance)
* A read-only device in /dev/ is also created for the PIRs, display and ranking. Try reading them!
* PIRs are encapsulated in a cardboard box with a small hole in order to cut their raw angle of view (which is ~120° without the box)

## Any question?

Feel free to ask here, send an email, open issues or whatever!
