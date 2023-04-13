# STM32 NTRIP Client

Based on NUCLEO-STM32F767Zi board.
Using FreeRTOS and LWIP netconn API.

LD1 (Yellow) indicate NTRIP client is running
LD2 (Blue) will flash when receiving a message
LD3 (Red) will flash on a separate thread

DHCP is enabled, on startup Red LED indicate address is not received. 
It will goes off when a valid ip is obtained. 

Server address and mount points are manually write in the configuration sections. 

WIP: netconn does not send TCP RST when close