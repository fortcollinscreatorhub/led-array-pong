A program for playing Pong on an LED array.

LED array is connected to a set of Teensies running the VideoDisplay example
from https://github.com/PaulStoffregen/OctoWS2811. Other details are available
at http://www.pjrc.com/teensy/td_libs_OctoWS2811.html.

Raspberry Pi runs the game software in the host/ directory. This reads user
input from two I2C accelerometers, runs all the game logic, and sends the
video data over USB serial to the Teensies.

The I2C accelerometers are attached to a plank that's balanced over a large
PVC pipe.

More complete details TBD.
