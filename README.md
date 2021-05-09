# cse3442
A.K.A. THE IRON GIANT


Introduction
----------------------------------------------
This was a semester long project designed to exercise concepts and skills used in embedded systems; these concepts and skills include, but are not limited to:
-Utilizing the functionality of the TIVA TM4C123GXL Evaluation Kit with the TM4C123GH6PM Microcontroller.
-Odometry measurements for the robot’s wheels using edge timers controlled by the TIVA board.
-PWMs – controlled by the TIVA board as well – to drive each of the motors for the robot’s wheels.
-Ultrasonic sensor measuring distance from the wall by tracking response pulses and timing the response time.
-Using the TIVA board’s numerous timers in edge-triggered counters for input and clock-dependent timers.
-Using the TIVA board’s GPIO pins for each of these devices for numerous different functions (e.g., inputs, PWM outputs, LEDs for debugging)
-Writing a CLI to communicate with each of these devices using the Uart0 function on the TIVA board.
 
Theory of Operation
-----------------------------------------------------
Robot Movement and Odometry
•	The TIVA Board in this project is configured to run at a clock rate of 40 MHz.
•	The initialization of the board occurs first, enabling the clock for all necessary pins, PWM motors, and timers for triggering and counting.
•	Pulse-Width Modulation Generators were used for each of the wheels.
o	The PWM0 module has 4 separate generators; I am using 2 of those generators with 2 pins on each generator to drive the wheels of the robot. One of the pins drive one direction of the motor while the other pin drives the motor in the opposite direction.
o	The PWMs are configured to have a max frequency of 19.53 kHz, with a load value of 1024; the generators use this load value as a compare value to calculate a duty cycle. If the value of the PWM is higher than this compare value, the PWMs activate.
o	Once activated at the same load value of 1000, I noticed that the wheels did not drive in a straight direction; specifically the left wheel spun just a bit faster than the right. This is why when moving the robot, the left wheel gets a load value of 996, while the right wheel gets a load value of 1001. This ensures that the robot drives in a fairly straight line.
•	Wide-Timers were used for the odometry measurements of the wheels. 
o	Each of the motors have a gear with a magnet that spins when the motor activates. This gear, however, does not spin in phase with the wheels themselves; there is a designated ratio of one wheel rotation to magnet rotations, but this value was quickly thrown out due to inaccuracy and replaced with manual testing.
o	Two Hall effect sensors were placed directly in front of those magnets to detect rotations.
o	Two Wide-Timers on the TIVA board were configured as edge-triggered timers that would count each tick of magnet rotation.
o	Manual testing was required to fine-tune the | magnet tick : rotation | ratio. In my testing, I found that the magnets ticked roughly 40 times when my robot traveled 30 centimeters.
o	The robot can perform in-place rotations: that is, a rotation where the middle of the robot is the axis of rotation. This is done by having each of the wheels spin in opposite directions. Similar to the straight movement, when testing the in-place rotation, I found that the magnets ticked about 20 times for each 90 degrees of rotation.
 
Command Line Interface using UART0
•	The UART module on the TIVA board allows communication with, in this case, a serial connection with a PC to input data. The TIVA board is internally configured with receive and transfer pins to allow communication over the microUSB port.
•	In initialization and configuration, the data rate of the UART module is specified at 115200 bits/second.
•	A command line interface was designed by utilizing the UART’s buffer and buffer status registers to either wait until data was ready to retrieve or wait until the buffer was full to send data. This allowed functions to get and put chars and strings in and out of the UART.
•	A parsing algorithm was used to take the command input and convert it into a structure that contained the command and each necessary argument.
Instruction Queue
•	When a command is typed into the interface, if it is valid, it is added to an instruction array that will execute all included instructions when the ‘run’ command is issued.
•	Inserting a command into the queue is supported by shifting all necessary instructions in the queue forward, deleting the last one if necessary.
•	Deleting a command from the queue is supported by shifting instructions from that spot left one to overwrite the unwanted command.
•	Listing all of the queue is supported by utilizing sprintf() and passing that formatted string into the UART string function.



 
Ultrasonic Sensor
•	The sensor used for wall detection utilizes two pins for its main functionality. A high pulse is sent to the trigger pin and an ultrasonic signal is sent out; during this time, the second pin, the echo pin, goes to a high state. When the ultrasonic signal returns to the sensor, the echo pin goes low. (NOTE: The trigger pin must be high for roughly 10 microseconds)
•	Utilizing the timers on the TIVA board, a timer is enabled when the echo pin enters its high state (when the signal is sent out), and then that timer is disabled when the echo pin goes low (the signal returns). Then, a numerical conversion occurs to convert the raw timer value into centimeters from the object.
o	The timer calculates its value based on the system clock. The raw value is converted from microsecond to milliseconds.
o	The timer is enabled when the signal leaves the sensor initially. This means when the signal returns, the timer has been on for the entire round trip. The raw value is divided by 2 to account for this.
o	Finally, based on the specifications and manual testing, the time value is divided by 58 to roughly convert distance from the object in centimeters.
 
Final Thoughts/Conclusion
I thought the end product turned out really nice, but the process was not without its problems.
The initial construction of my hall effect sensor circuit led to feedback voltage back into the circuit and ruined a switch. I was very lucky it did not overdrive any of the other components in the circuit.
The measurements for moving forward and backward especially were very inconsistent. The same duty cycle and rotation showed a 5 cm difference on occasion. I noticed that specifically when reversing, the right wheel of the robot did not stop as soon as the left, so while the direction was straight during constant velocity, as soon as deceleration began, it skewed at a 25o~45o angle. This is likely due to the different frictions and resistances the motors have to deal with on each direction.
Another issue I ran into is due to the random position of where the magnet might be in relation to the hall effect sensor, the magnet might be in front of the sensor while not moving, leading to constantly triggering the counter. This means the robot might detect 5-10 ticks of movement when there is none due to how quickly the counter goes up. This means for very small values of movement (ex. 5 cm), it might detect that movement occurred when it really didn’t.

That being said, all required functionality is present. The PWMs drive the motors; the sensors detect the magnet rotations and the wide-timers count how many magnet ticks; and the ultrasonic sensor detects distance from objects.
