# monochord
Linux project in C

Monochord calculates sinus, based on data recieved from UDP port and sends it to rejestrator using Linux Real-Time signals.
Rejestrator is responsible for recieving data and saving it to txt file/stdout and binary file. 
Info_rejestrator sends RT signal to rejestrator on command, rejestrator sends back info about current working mode and info_rejestrator displays it.


