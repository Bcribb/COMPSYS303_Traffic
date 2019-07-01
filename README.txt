-------------------------------------
-----------README FOR G16------------
Created by Jerry Yang and Blain Cribb
-------------------------------------

Code is found in "software/Assignment1"

Setup instructions:
	1. Program the device through quartus using the sof found in /quartus_files/output_files/Lab1.sof
	2. Run the NIOS project through Eclipse using workspace "D:\bcri429"
	3. To switch between modes treat sw0 and sw1 as binary number corresponding to modes (e.g - sw0 = 1 sw1 = 0 for mode 2)
	4. Set up putty with these parameters
			- Serial mode
			- baud rate 115200
			- 8 data bits
			- 1 stop bit
			- No parity
			- No flow control
	
Mode Design Decisions:
	Mode 1 - Works as specified
	Mode 2  - Push KEY0 for NS pedestrian
		- Push KEY1 for EW pedestrian
		- Each press only queues for one cycle
		- LEDR0 and LED1 are used to display KEY0 and KEY1 being pressed respectively
		- Pushing during a green walk light will not queue again for that direction
	Mode 3 	- Set sw2 to high then low to queue up a change
		- Wait until the first red red state and then enter values into putty
		- Press enter once complete
		- Shows "Successful change" or "Enter value:" if invalid entry
		- Otherwise utilises mode 2 logic
	Mode 4 	- Push KEY2 for car entering / leaving intersection
		- If car enters on red: 
			- "Snapshot taken" is printed to putty
			- Press again to leave
		 - If entered on yellow:
			- "Camera activated" is printed and a timer starts
			- Press again to leave
			- If the car remains for the specified value, "Snapshot taken" is printed
		- If entered on green:
			- "Car passes on green" is printed
			- Press again to leave
			- If the car remains while the light turns yellow, "Camera activated" is printed and a timer starts
			- If the car remains for the specified value, "Snapshot taken" is printed
		- When the car leaves, "Vehicle left, time: X seconds Y milliseconds" is printed