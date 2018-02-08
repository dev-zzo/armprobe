# ARMProbe: a low-level debugging probe for fun and profit

This project is an attempt to take the magic out of the equation
when it comes to debugging ARM CPUs. Nothing is "black box", and
you are free to poke everything and peer everywhere!

The motivation behind this project is the need to have a fully 
customizable and controllable debugging interface, which most of
the available products don't provide -- rather, you tend to get a
black box that does all the magic. But it's the magic that is
very interesting here, especially from the security standpoint!

Not to mention that the hardware required is super cheap, and software
is free!

# Design

The probe consists of two parts:

* The actual probe, a STM32F103 running the firmware and talking SWD, written in C
* The software driving the probe and supporting all protocols above SWD, written in Python

