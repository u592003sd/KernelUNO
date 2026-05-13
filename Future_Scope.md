1. Add a root enable/disable feature if required.
2. PWM Mode. [IMPLEMENTED]
3. Something to do with Analog Mapping.
4. Error code for dmesg addition indicating dmesg log is full.
5. 'executeCommand' function should have a single point of return. It could be with retVal.
6. Support analogRead on executeCommand 'Read'. [IMPLEMENTED]
7. A test script to test the UNO CLI rigourously over the serial port.
8. Add 'fade' == pinStr to the gpio command which essentially fades an digital PWM pin. [IMPLEMENTED]
9. Improvement for file search algorithm instead of the entire FS array.