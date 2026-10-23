- Max filename size is set to 100 bytes (in tftp.h)
- TFTP server functions are all in tftp.c and there's separate functions for sending an error, responding to 
   RRQ, and responding to WRQ (sendError(), handleRRQ(), handleWRQ())
- Globals are used in tftp.c to handle retransmits
- I didn't set any globals in tftp.c to be volatile since gcc was giving warnings about putting them in
   sendto().
