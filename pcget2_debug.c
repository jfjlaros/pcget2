#include<stdio.h>
#include<stdlib.h>

/* DISK WRITE*/
#include <exec/ports.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/trackdisk.h>

#define NUMTRKS 5
#define TD_BUFSIZE (11 * 1024 * NUMTRKS)
#define NUMBLOCKS (NUMTRKS * 22)

typedef unsigned long ulong;
typedef unsigned char uchar;

struct MsgPort *td_mp;
struct IOStdReq *td_iob;

extern struct MsgPort *CreatePort();
extern struct IOStdReq *CreateStdIO();
extern long OpenDevice();
extern void *AllocMem();
/* END DISK WRITE*/

#define DATAP 0xBFE101
#define CONTP 0xBFD000

#define byte unsigned char
#define CIABpra *(byte*)0xbfd000
#define CIABddra *(byte*)0xbfd200
#define CIABprb *(byte*)0xbfd100
#define CIABddrb *(byte*)0xbfd300
#define CIABtodlo *(byte*)0xbfd800
#define CIABicr *(byte*)0xbfdd00

#define CIAApra *(byte*)0xbfe001
#define CIAAddra *(byte*)0xbfe201
#define CIAAprb *(byte*)0xbfe101
#define CIAAddrb *(byte*)0xbfe301
#define CIAAtodlo *(byte*)0xbfe801
#define CIAAicr *(byte*)0xbfed01

#define word unsigned short
#define INTENA *(word*)0xdff09a
#define INTENAR *(word*)0xdff01c
#define DMACON *(word*)0xdff096
#define DMACONR *(word*)0xdff002
#define INTREQR *(word*)0xdff01e
#define INTREQ *(word*)0xdff09c

#define REMOTE_RTS
#define TOGGLE_RTS (CIABpra=CIABpra^(byte)0x04)
#define STR (CIABpra=CIABpra^(byte)0x02)
#define ACK (CIAAicr & (byte)0x10)
#define READBYTE CIAAprb
#define WRITEBYTE CIAAprb=writebyte
#define SETREADMODE CIAAddrb=(byte)0x00
#define SETWRITEMODE CIAAddrb=(byte)0xff

/* ERROR CODES */
#define NO_ERR 0
#define ERR_BAD_FORMAT 10
#define ERR_FILE_ERROR 11

void send_error(byte ernum, byte oldlook);

/* #define BUFSIZE 32768                                 DISK WRITE */
#define WAITLOOP 100000

main(int argc, char *argv[]) {
  byte dat;
  byte lookstart;
  word moredata;
  word morefiles;
  int filesizer;
  int i = 0;
  word j = 0;
  int bi = 0;
  int by = 0;
  int outfile;
  int writ = 0;
  int incount = 0;
  byte *outbuf, *checkbuf;
  int nread = 0; 
  short prenread = 0;
  word errors = NO_ERR;
  byte checkin0;
  byte checkin1;

  unsigned short checksum = 0;
  unsigned short checksumin = 0;
  char filename[256];

  /* DISK WRITE*/
  int BUFSIZE = TD_BUFSIZE;
  int diskwrite = 0;
  long track = 0;
  int err = 0;
  /* END DISK WRITE*/

  printf("PC Receiver Program (via parallel port) by Greg Craske.\n"); 


  /* DISK WRITE*/
  if ((argv[1][0] == '-') && (argv[1][1] == 'w')) {
    BUFSIZE = TD_BUFSIZE; 
    diskwrite = 1;

    fprintf(stderr,"Insert disk and hit RETURN to continue:\n");
    getchar();

    td_mp = CreatePort(0L,0L);
    if(!td_mp) exit(99);
    td_iob = CreateStdIO(td_mp);
    if(!td_iob) {
      DeletePort(td_mp);
      exit(99);
    }
    if(OpenDevice("trackdisk.device",0L,td_iob,0L)) {
      printf("Unable to open floppy device driver.\n");
      DeleteStdIO(td_iob);
      DeletePort(td_mp);
      exit(99);
    }

    outbuf = AllocMem(TD_BUFSIZE,MEMF_PUBLIC|MEMF_CHIP);
    if(!outbuf) {
      printf("Unable to obtain %ld byte buffer in chip memory.\n",TD_BUFSIZE);
      CloseDevice(td_iob);
      DeleteStdIO(td_iob);
      DeletePort(td_mp);
      exit(99);
    }
    checkbuf = AllocMem(TD_BUFSIZE,MEMF_PUBLIC|MEMF_CHIP);
    if(!checkbuf) {
      printf("Unable to obtain %ld byte buffer in chip memory.\n",TD_BUFSIZE);
      CloseDevice(td_iob);
      DeleteStdIO(td_iob);
      DeletePort(td_mp);
      exit(99);
    }
    td_iob->io_Command = TD_MOTOR;
    td_iob->io_Length = 1;
    DoIO(td_iob);
  }
  else
    if ((outbuf = (char *)malloc(BUFSIZE)) == NULL)
    {
      perror("Can't allocate buffer!\n");
      exit(1);
    }
  /* END DISK WRITE*/

  CIAAddrb = (byte)0x00;  /*set DDR for data pins to INPUT */
  CIABddra = (byte)0xfe;  /*set DDR for control/status pins */
  lookstart = CIABpra;   /*get initial crl/stat */

  morefiles = 1;

  i = 0;
  j = 0;
  printf("Waiting for sender.");
  fflush(stdout);

  while(i < WAITLOOP)
  {
    for (i = 0; i < WAITLOOP-1; i++) 
      if ( (CIABpra & 0x01) != (lookstart & 0x01))
      {
        i = WAITLOOP;
        printf("\nOK\n\n");
      }

    if (i < WAITLOOP)
    {
      printf(".");
      fflush(stdout);
      TOGGLE_RTS;
    }
    if (j++ >= 20)
    {
      printf("\nWaiting timed out.  Exiting.\n");
      morefiles = 0;
      i = WAITLOOP;
    }
  }


  while (morefiles)
  { 

    /* wait for begin txn toggle */
    while(i < WAITLOOP)
    {
      for (i = 0; i < WAITLOOP-1; i++)
      {
        if ( (CIABpra & 0x01) != (lookstart & 0x01))
          i = WAITLOOP;
      }

      if (i < WAITLOOP)
        printf("waiting for begin send..\n");
    }

    if (morefiles == 1)
      printf("found send request.  receiving....\n");

    /* MAIN RECEIVE LOOP */
    moredata = 1;
    i = 0;
    writ = 0;
    /*lookstart = CIABpra;*/
    errors = 0;
    checksum = 0;
    nread = 0;
    prenread = 0;
    checksumin = 0;
    filesizer = 0;

    while(moredata)
    {
      /* WAIT FOR DATA CLKIN OR RTS GO LOW */
      while ( !(CIAAicr & 0x10) && ((lookstart & 0x01) != (CIABpra & 0x01)) )
        if(!(CIAApra & 0x80)) /* EMERGENCY QUIT (MOUSE BUTTON) */
        {
          moredata = 0;
          morefiles = 0;
          break;
        }

      if ((lookstart & 0x01) == (CIABpra & 0x01)) /* TEST IF IT WAS THE RTS */
      {
        CIABpra = CIABpra^(byte)0x02; /*send ack */
        moredata = 0;  /* end transmission */
        if (writ != 4)
          errors = ERR_BAD_FORMAT;
      }
      else
      {
        /* FOUND DATA CLKIN, GET DATA AND SEND ACK */
        dat = CIAAprb;       /*get new data */
        CIABpra = CIABpra^(byte)0x02; /*send ack */

        switch (writ) /* WOT TO DO WITH THE DATA */
        {
          case 0: if (dat == 0xaa)
                    break; 
                  else
                    if (dat == 0xff)
                    {
                      morefiles = 0;
                      moredata = 0;
                      break;
                    }
                    else
                      writ = 1;


          case 1: if (dat == 0xab)
                    { 
                      /* DISK WRITE*/
                      if (!diskwrite) {
                        filename[i] = '\0';

                        if (morefiles < argc)
                        {
                          printf("Writing file %s ", argv[morefiles]);
                          if ((outfile = creat(argv[morefiles], 0666)) == -1)
                          {
                            perror("- Can't open file!\n");
                            send_error(ERR_FILE_ERROR, lookstart);
                            moredata = 0;
                          }
                        }
                        else
                        { 
                          printf("Writing file %s ", filename);
                          if ((outfile = creat(filename, 0666)) == -1)
                          {
                            perror("- Can't write file!\n");
                            send_error(ERR_FILE_ERROR, lookstart);
                            moredata = 0;
                          }
                        }
                      }

                      morefiles++;
                      writ = 2;  
                      i = 0;   
                    }
                  else 
                    if (i < 255)
                    {
                      filename[i++] = dat;
                    } 
                  break;
          case 2: if (dat == 0)
                    { 
                      printf("(%d bytes)\n", filesizer);
                      writ = 3;
                    }
                    else if (i < 8)
                    {
                      filesizer+= dat << (8 * i++);
                    }
                  break;
          case 3: 
                  if (prenread++ == 0)
                    checkin0 = dat;
                  else
                  {
                    checkin1 = dat;
                    writ = 4;
                  }
                  break;
          case 4:
                  if (nread&0x01)
                  {
                    outbuf[nread] = checkin1;
                    checksum += checkin1;
                    checkin1 = dat;

                  }
                  else
                  {
                    outbuf[nread] = checkin0;
                    checksum += checkin0 << 8;
                    checkin0 = dat;
                  }

                  if (++nread == BUFSIZE)
                  {
                    /* DISK WRITE*/
                    if (diskwrite) {
                      printf("Writing tracks %ld to %ld.\r", track,
                          track + NUMTRKS - 1);
                      fflush(stdout);
                      td_iob->io_Command = TD_FORMAT;
                      td_iob->io_Data = (APTR) outbuf;
                      td_iob->io_Length = TD_BUFSIZE;
                      td_iob->io_Offset = 11 * 1024 * track;
                      DoIO(td_iob);
                      if(td_iob->io_Error) 
                        printf("Floppy write failed, error number %d.\n",
                            td_iob->io_Error);
                      td_iob->io_Command = CMD_UPDATE;
                      DoIO(td_iob);
                      if(td_iob->io_Error)
                        printf("Floppy write failed, error number %d.\n",
                            td_iob->io_Error);

                      td_iob->io_Command = CMD_READ;
                      td_iob->io_Data = (APTR) checkbuf;
                      DoIO(td_iob);
                      if (err = memcmp(outbuf, checkbuf, TD_BUFSIZE)) {
                        printf("FUCK! %i                              \n", err);
                        for (err = 0; err < 40; err++)
                          printf("%c", outbuf[err]);
                        printf("\n");
                        for (err = 0; err < 40; err++)
                          printf("%c", checkbuf[err]);
                        printf("\n");
                      }
                      track += NUMTRKS;
                    }
                    else
                      /* END DISK WRITE*/
                      write(outfile, outbuf, nread);
                    nread = 0;
                    break;            
                  }
          default: break;
        } /* END SWICH */
      } /* END IF */   
    } /* END LOOP */

    if (morefiles)
    {
      if (nread&0x01)
        checksumin = (int)((checkin0 &0xff)<<8) + (int)(checkin1 &0xff);
      else
        checksumin = (int)((checkin1 &0xff)<<8) + (int)(checkin0 &0xff);

      if (!errors)
      {
        if (checksum != checksumin)
          printf("CHECKSUM ERROR (received %x, calculated %x)\n", checksumin, checksum);

        if (nread != 0)
        {
          /* DISK WRITE*/
          if (diskwrite) {
            printf("Writing tracks %ld to %ld.\r", track, track + NUMTRKS - 1);
            fflush(stdout);
            td_iob->io_Command = TD_FORMAT;
            td_iob->io_Data = (APTR) outbuf;
            td_iob->io_Length = TD_BUFSIZE;
            td_iob->io_Offset = 11 * 1024 * track;
            DoIO(td_iob);
            if(td_iob->io_Error) 
              printf("Floppy write failed, error number %d.\n",
                  td_iob->io_Error);
            td_iob->io_Command = CMD_READ;
            td_iob->io_Data = (APTR) checkbuf;
            DoIO(td_iob);
            if (err = memcmp(outbuf, checkbuf, TD_BUFSIZE))
              printf("Read back failed, error number %i.\n", err);
          }
          else
            /* END DISK WRITE*/
            write(outfile, outbuf, nread);
        }
        printf("  done.\n");
      }
      close(outfile);
    }
    else
      printf("ende!\n");
  }
  /* DISK WRITE*/
  if (diskwrite) {
    printf("                             \r");
    fflush(stdout);

    FreeMem(outbuf,TD_BUFSIZE);
    FreeMem(checkbuf,TD_BUFSIZE);

    td_iob->io_Command = CMD_UPDATE;
    DoIO(td_iob);
    if(td_iob->io_Error) printf("Floppy update failed, error number %d.\n",
        td_iob->io_Error);

    td_iob->io_Command = TD_MOTOR;
    td_iob->io_Length = 0;
    DoIO(td_iob);

    CloseDevice(td_iob);
    DeleteStdIO(td_iob);
    DeletePort(td_mp);
  }
  else
    /* END DISK WRITE*/
    free(outbuf);


  CIAAprb = (byte)0xff;
  CIAAddrb = (byte)0x00;
  CIABpra = (byte)0xfe;
  CIABddra = (byte)0xc0;
}

void smalldelay()
{
  byte timerb;

  timerb = CIABtodlo & 0x80;
  while ((CIABtodlo & 0x80) == timerb);
  timerb = CIABtodlo & 0x80;
  while ((CIABtodlo & 0x80) == timerb);
}

void send_error(byte ernum, byte oldlook)
{
  byte writebyte;  
  int i = 30000;

  TOGGLE_RTS; /* send rts */

  writebyte = 0xee;
  WRITEBYTE;

  /* wait until remote rts goes */
  while ((oldlook & 0x01) != (CIABpra & 0x01)); 

  SETWRITEMODE;

  STR;
  while (!ACK);

  writebyte = ernum;
  WRITEBYTE;

  smalldelay();

  STR;
  while (!ACK);

  SETREADMODE;

  TOGGLE_RTS;

  /* wait until remote rts comes */
  while ((oldlook & 0x01) == (CIABpra & 0x01));
}
