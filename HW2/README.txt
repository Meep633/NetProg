- I set listenfd's backlog to 10, not really sure what to normally set it to but 10 sounds 
  reasonable I guess
- I set a limit on the number of worker threads to be 2-4 as shown in the PDF and max clients
  to be MAX_CLIENTS since that's already defined in the skeleton code

Sources:
- https://stackoverflow.com/questions/3599160/how-can-i-suppress-unused-parameter-warnings-in-c
  Used this to deal with signal handler having an unused but necessary parameter
- https://www.youtube.com/watch?v=RtTlIvnBw10
  For explanation and example on conditional variables