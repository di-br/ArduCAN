#include<stdio.h>

int main (void) {
   // the received/buffered CAN frames are 16 bytes
   unsigned char frame[16];

   // the timestamp was a long
   long timestamp;

   // we know the filename, so open file
   FILE *fptr;
   fptr = fopen("ANYCAN.RAW","r");
   // make sure the file was opened
   if (!fptr)
   {
      printf("FAILED to open raw data file\n");
      return 1;
   }

   while(fread(frame, sizeof(frame), 1, fptr))  // read a single CAN frame
   {
#ifdef DBG
      // print RAW data
      printf("RAW data:");
      for (int i=0; i<16; i++) printf(" %02x", frame[i]);
      printf("\n");
#endif

      // 1st byte is a dummy (which we could test for correctness...
      // next 4 bytes are the timestamp, mind endianess
      timestamp  = frame[4]; timestamp<<=8;
      timestamp += frame[3]; timestamp<<=8;
      timestamp += frame[2]; timestamp<<=8;
      timestamp += frame[1];
      printf("%8dms", timestamp);

      // next 2 bytes hold CAN Id
      printf("   ID: %03x", frame[5] + frame[6] * 256);
      // followed by a byte for length of message
      printf(" | Length: %d", frame[7]);
      // last 8 bytes hold data
      printf(" || ");
      for (int i=8; i<16; i++) printf(" %02x", frame[i]);
      printf("\n");
   }

   // clean up and finish
   fclose(fptr);
   return 0;
}
