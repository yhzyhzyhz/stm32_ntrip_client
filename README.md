# STM32 NTRIP Client

Based on NUCLEO-STM32F767ZI board.
Using FreeRTOS and LWIP netconn API.

NTRIP process will start by default on power up. 
Use Blue user button to start/stop the NTRIP process. 

LD1 (Yellow) indicate NTRIP client is running
LD2 (Blue) will flash when receiving a message
LD3 (Red) will flash on a separate thread

DHCP is enabled, on startup Red LED indicate address is not received. 
It will goes off when a valid ip is obtained. 

Server address and mount points are manually write in the configuration sections. 

Error will be print via SWO. 

WIP: netconn does not send TCP RST when close