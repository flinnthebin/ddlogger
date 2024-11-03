# ddlogger

## Introduction
ddlogger is my Something Awesome Project, focusing on Linux systems-level security.

## Features
- Lightweight and efficient
- Compatible with any Linux distro
- Robust and resilient

## Report

Linux being my daily driver means that I have lots of fun challenges to solve if I want to actually use my computer for
any 'normal' tasks. My speakers work about half the time I can get pulseaudio working. My airpods work 100% of the time,
when I call the script in the right state.

## Airpods
<video width="600" controls>
  <source src="https://raw.githubusercontent.com/flinnthebin/ddlogger/main/airpods.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

Nice. See how right at the end of the video there the airpods script my bluetooth controller (bluetoothctl) leaks the
output:

[NEW] Transport /org/bluez/hci0/dev_B0_3F_64_21_7E_D7/sep1/fd4

That is the file descriptor for the bluetooth audio transport stream between my computer and my airpods.

## Fan

<video width="600" controls>
  <source src="https://raw.githubusercontent.com/flinnthebin/ddlogger/main/fan.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>

I don't have access to the Alienware Command Center, so I had to make my own fan controller and fan manager.
This is also achieved by virtue of file descriptors.

## What does that have to do with anything

Similarly,

## Conclusion
With DDLogger, tracking key events on Linux systems has never been easier. Our focus on security ensures that all data is handled responsibly.
