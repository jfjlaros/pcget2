/* 
    amigaput: by Greg Craske 18/6/2001 (Freeware)
    Uses parallel port to send files to an Amiga that is running 'pcget'.
    Used with PS/2 compatible parallel ports.  Non-PS/2 ports will not
    allow the Amiga to send back error messages.

    Cable assembly instructions at 
      http://www.cs.rmit.edu.au/~craske/amiga

    Written specifically for Linux, compile with 
      gcc -O2 -o amigaput amigaput.c
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/io.h>
#include <sys/types.h>
#include <sys/stat.h>

/* BASEPORT addresses: specific to LPT1 */
#define DATAP 0x378
#define STATP 0x379
#define CONTP 0x37a


/* STATEMENTS TO DO PLATFORM SPECIFIC OPERATIONS 
   RTS from PC or Amiga and ACK from Amiga is 'edge triggered (either edges)', 
   STR from PC is 'pulse triggered (off-on-off)' */

#define REMOTE_RTS (inb(STATP)&0x10)
#define TOGGLE_RTS outb(inb(CONTP)^0x02,CONTP)
#define STR outb(inb(CONTP)|0x01,CONTP);outb(inb(CONTP)&0xfe,CONTP)
#define ACK (inb(STATP)&0x20)
#define READBYTE (inb(DATAP))
#define WRITEBYTE outb(writebyte,DATAP)
#define SETREADMODE outb(inb(CONTP)|0x20, CONTP)
#define SETWRITEMODE outb(inb(CONTP)&0xdf, CONTP)

/* LINUX SPECIFIC PERMISSION OPERATIONS TO USE IO PORT*/
#define GETPORTPERMS if (ioperm(DATAP, 3, 1)) { perror("ioperm: couldn't get permissions for registers."); exit(1);}
#define RELPORTPERMS if (ioperm(DATAP, 3, 0) ) { perror("ioperm: couldn't release permissions for registers."); exit(1); }



/* SPECIAL DATA BYTES */
#define FILENAME_HDR 0xaa
#define LENGTH_HDR 0xab
#define ERROR 0xee
#define END_ALL 0xff


/* ERROR CODES */
#define NO_ERR 0
#define ERR_RTS_CONFLICT 1 /* Both computers tried to send simultaneously */
#define ERR_ACK_TIMEOUT 2  /* Timed out waiting for ack from receiver */
#define ERR_RECEIVE_SIDE 3 /* Receiver sent back an error condition */
#define ERR_FILE_ERROR 11
#define ERR_REC_FILE_EXISTS 4
#define ERR_REC_BAD_CHECKSUM 5

void printerr(int err)
{
  switch (err) 
  {
    case ERR_RTS_CONFLICT: 
     printf("\nERROR %d: RTS Conflict between computers!\n", ERR_RTS_CONFLICT); 
     break;
    case ERR_ACK_TIMEOUT: 
     printf("\nERROR %d: ACK timed out! Check receiver.\n", ERR_ACK_TIMEOUT);
     break;
    case ERR_RECEIVE_SIDE: 
     printf("\nERROR %d: General receiver-side error!\n", ERR_RECEIVE_SIDE);
     break;
    case ERR_REC_FILE_EXISTS: 
     printf("\nERROR %d: File exists!\n", ERR_REC_FILE_EXISTS);
     break;
    case ERR_REC_BAD_CHECKSUM: 
     printf("\nERROR %d: Bad checksum from receiver!\n", ERR_REC_BAD_CHECKSUM);
     break;
    case ERR_FILE_ERROR: 
     printf("\nERROR %d: Receiver can't create file!\n", ERR_FILE_ERROR);
     break;
    case NO_ERR: break;
     default: printf("\nUnknown error: %x\n", err);
     break;
  }
}
    

/* Takes byte to be sent, sends and does the handshaking.
   Returns error code, or 0 if no errors */
int senddata(int dout);

/* Takes filename to be sent.  
   Returns error code, or 0 if no errors */
int send_filename(char *fname);

/* Takes file to get size of.  
   Returns error code, or 0 if no errors */
int send_filesize(char *fname);

/* Takes calculated checksum.
   Returns error code, or 0 if all good.. */
int send_checksum(int checksum); 

/* Simply waits for ack signal.
   Returns errorcode if time out or remote RTS .  0 otherwise */
int wait_for_ack();

/* On getting remote RTS, attempt to receive error message.
   Returns error code, and prints to stdout the message. */
int get_error();

/* Reads filesizes
   (taken from 'The C Programming Language', by B.W.Kernighan & D.M.Ritchie)*/
int fsize(char *);


/* Naughty naughty global vars */
unsigned char oldack;
unsigned char oldremrts;

int main (int argc, char *argv[])
{
   int filenum = 0;
   int errors = 0;
   int endall = 0;
   unsigned short checksum = 0;
   int sumbyte;
   int newsum;
   char writebyte;

   long i = 0;
   char contr;
   FILE *infile;
   int inc = ' ';
   int byt = 0;
   int kbt = 0;
   int filesize;


   /* CHECK FOR PARAMS */
   if (argc < 2) 
   { 
     printf("Usage: %s {file} [{file(s)}]\n", argv[0]); 
     exit(1);
   }
   
   /* GET PORT PERMISSIONS (linux) */
   GETPORTPERMS

   oldremrts = REMOTE_RTS; /* Set old RTS value */
   printf("Amiga Sender Program (via parallel port) by Greg Craske.\n");
   printf("Waiting for receiver.");
   fflush(stdout);

   while (REMOTE_RTS == oldremrts && i++ < 20)
   {
     sleep(1);
     putchar('.');
     fflush(stdout);
   }
   if (i >= 20)
   {
     errors = ERR_ACK_TIMEOUT;
     printf("\nNo response! Exiting.\n");
     endall = 1;
   }
   else
     printf("\nOK.\n\n");

   oldremrts = REMOTE_RTS; /* Set old RTS value */

   SETWRITEMODE;

   /* MAIN LOOP FOR EACH FILE */
   while (++filenum < argc && !endall)
   {
     checksum = 0;
     sumbyte = 0;
     newsum = 0;
     oldack = ACK;

     if ((infile = fopen(argv[filenum], "r")) == NULL)
     {
       fprintf(stderr, "\nCan't open file %s.\n", argv[filenum]);
     }
     else
     { 
       errors = 0;
       byt = 0;
       kbt = 0;
       printf("\nSending %s, waiting for ack...\n", argv[filenum]);


       writebyte = 0xaa; WRITEBYTE;          /* Write start data */

       TOGGLE_RTS;
       ACK; /* Set old ACK value */
       errors = wait_for_ack();

       /* SEND FILENAME */
       if (!errors) errors = senddata(0xaa);
       if (!errors) errors = send_filename(argv[filenum]);

       /* Send File Size */
       if (!errors) errors = senddata(0xab);
       if (!errors) errors = send_filesize(argv[filenum]);

       if (!errors) errors = senddata(0);  /* Read to send data block */

       if (!errors)
         printf("OK. Kbytes sent:\n0");

       /* DATA SENDING LOOP */
       byt = 0;
       checksum = 0;
       while(!errors && ((inc = getc(infile)) != EOF)) 
       {
         errors = senddata(inc);

         /* calc checksum */
         if (byt & 0x01)
           checksum = checksum + inc;
         else
           checksum = checksum + (inc << 8);
         
         /* print amount sent (in 2k intervals) */
         if (++byt >= 2048)
         {
           kbt+=2;
           printf("\b\b\b\b\b%d", kbt);
           fflush(stdout);
           byt = 0;
         }
       }

       if (errors)
       {
         if (errors == ERR_RTS_CONFLICT)
           get_error();

         if (errors == ERR_REC_BAD_CHECKSUM ||
             errors == ERR_FILE_ERROR)
           endall = 0;
         else
           endall = 1;

         printerr(errors);
         sleep(1);
       }
       else
       {
         send_checksum(checksum);
       }


       if (!errors) 
       {
         if (byt >= 1024)
           kbt++;
         printf("\b\b\b\b\b%d\n", kbt);
       }

     } /* end: IF the file could be concat(else)*/

     TOGGLE_RTS;
     errors = wait_for_ack();
     fclose (infile);

   } /* end: WHILE there are more files to send */

   /* SEND FINISH */ 
   if (errors == NO_ERR || errors == ERR_REC_BAD_CHECKSUM ||
                           errors == ERR_FILE_ERROR)
   {
     printf("Sending end...\n"); 
     TOGGLE_RTS;
     senddata(END_ALL);
     TOGGLE_RTS;
     printf("\nende!\n");
   }   

   /* RELEASE PORT PERMISSIONS */
   RELPORTPERMS

   exit (0);
} /* END MAIN */

/* =================================================== */

int senddata(int dout)
{
  int nogo = 0;
  long cnt = 0;
  char writebyte;

  writebyte = (char)dout;
  WRITEBYTE;
 
  STR;  /* send strobe */

  while(ACK == oldack)
  {
    if (REMOTE_RTS != oldremrts)
      return get_error();

    /* dodgy processor wait loop to wait for ACK from Amiga 
       (should've created a separate timing process but I'm too lazy) */
    if (++cnt == 100000)
    {
      STR;
      cnt = 0;

      if (nogo++ >= 80) 
      {
        return ERR_ACK_TIMEOUT;
        printf("  waiting for receiver...\n");
      }
    }
  }

  oldack = ACK;
  return NO_ERR;
}

/* ------------------------------------------------*/

int send_filename(char *fname)
{
  int errors = 0;

  while(*fname != '\0' && *fname != '\n' && !errors)
    errors = senddata(*fname++);

  return NO_ERR;
}

/* ------------------------------------------------*/

int send_filesize(char *fname)
{
  int filesize = 0;
  int piece;
  int i = 0;
  int errors = NO_ERR;

  if ((filesize = fsize(fname)) == 0)
  {
    return NO_ERR;
  }

  piece = filesize & 0xff;
  while (piece && !errors)
  {
    piece = piece >> (8*i);
    errors = senddata(piece);
    i++;
    piece = filesize & (0xff << (8*i));
  }
  
  return errors;
}

/* ------------------------------------------------*/

int send_checksum(int checksum)
{
  int errors = senddata(checksum&0xff);


  if (!errors)
    errors = senddata( (checksum & 0xff00) >> 8);

  return errors;
}

/* ------------------------------------------------*/

int wait_for_ack()
{
  int nogo = 0;
  long sleepcount = 0;
  int newack;


  while((newack = ACK) == oldack)
  {
    /* check if Amiga is trying to send to PC */
    if (REMOTE_RTS != oldremrts)
    { 
      /* If so, it's probably sending an error message */
      return get_error();
    }

    sleep(1);

    STR; /* In case Amiga didn't get previous one */

    if (sleepcount++ == 2)
    {
      sleepcount = 0;
      if (nogo++ >= 10) /* if effectively waiting for 20 seconds.. */
        return ERR_ACK_TIMEOUT;

      printf("still waiting for ack...\n");
    }
  }
  oldack = newack;
  return NO_ERR;
}

/* ------------------------------------------------*/

int get_error()
{
  int din;
  int writebyte = 0xaa;  

  SETREADMODE;

  oldremrts = REMOTE_RTS;

  TOGGLE_RTS; /* make RTS go down */
  
  wait_for_ack();
  if ((din = READBYTE) != ERROR)
    return ERR_RTS_CONFLICT;
  STR;  

  wait_for_ack();
  din = READBYTE;
  STR;

  while (REMOTE_RTS == oldremrts);
  TOGGLE_RTS; /* make rts go up */

  oldremrts = REMOTE_RTS;

  SETWRITEMODE;
  return din;
}

/* ------------------------------------------------*/

int fsize (char *name)
{
  struct stat stbuf;

  if (stat(name, &stbuf) == -1)
  {
    fprintf(stderr, "fsize: can't access %s\n", name);
    return 0;
  }

  if ((stbuf.st_mode & S_IFMT) == S_IFDIR)
    return 0;
  else
    return stbuf.st_size;
}

