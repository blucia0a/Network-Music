#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFBundle.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>

int NUM_SYNTHS;
#define NUM_PORTS 6
int g_level;
int doMouse = 0;

enum {
  kMIDINoteOff = 128,
  kMIDINoteOn = 144,
  kMIDICtrl = 176,
  kMIDIPrgm = 192
};


int voice = 0;

typedef struct _cmd{
  int hostNum;
  int port;
  int dir; 
  bool full;
} cmd;

typedef struct _sequenceParams{

  int id;

  int numSegments;

  unsigned long *period;
  unsigned long *duration;
  unsigned *tone;
  unsigned *target;
  cmd *cmdBuf;
  pthread_mutex_t lock;

  MIDIPortRef portRef;
  MIDIEndpointRef endpoint;
  
} sequenceParams;

sequenceParams *newSeq(int num){

  sequenceParams *n = (sequenceParams *)malloc( sizeof(sequenceParams) ); 
  n->period = (unsigned long *)malloc( num  * sizeof(unsigned long) );
  n->duration = (unsigned long *)malloc( num  * sizeof(unsigned long) );
  n->tone = (unsigned *)malloc( num  * sizeof(unsigned) );
  n->target = (unsigned *)malloc( num  * sizeof(unsigned) );
  n->numSegments = num; 
  n->cmdBuf = (cmd *)calloc(1,sizeof(cmd));
  return n;

}

void * sequence_play(void *p){

  sequenceParams *pps = (sequenceParams *)p;
  while(1){
 
    cmd curCmd;
    while(1){
      pthread_mutex_lock(&(pps->lock)); 
      if(pps->cmdBuf->full){
        curCmd.hostNum = pps->cmdBuf->hostNum;
        curCmd.port = pps->cmdBuf->port;
        curCmd.dir = pps->cmdBuf->dir;
        pps->cmdBuf->full = false;
        pthread_mutex_unlock(&(pps->lock));
        break;
      }
      pthread_mutex_unlock(&(pps->lock));
    }

    int level = (int)((((float)(65536 - curCmd.port))/65536.0f) * 127.0f);
    for(int i = 0; i < pps->numSegments; i++){

      int tone = pps->tone[i] * (1 + curCmd.dir);
      MIDIPacketList packetList;

      /*On*/
      packetList.numPackets = 1;
      packetList.packet[0].length = 3;
      packetList.packet[0].data[0] = 0x90; // note ON
      packetList.packet[0].data[1] = tone; // C
      packetList.packet[0].data[2] = level; // velocity
      packetList.packet[0].timeStamp = 0;
      MIDISend(pps->portRef,pps->endpoint,&packetList); 
  
      usleep( pps->duration[i] ); 

      /*Off*/
      packetList.numPackets = 1;
      packetList.packet[0].length = 3;
      packetList.packet[0].data[0] = kMIDINoteOff; // note ON
      packetList.packet[0].data[1] = tone; // C
      packetList.packet[0].data[2] = level; // velocity
      packetList.packet[0].timeStamp = 0;
      MIDISend(pps->portRef,pps->endpoint,&packetList); 
      
      usleep( pps->period[i] ); 

    }

  }
   
}

int main(int argc, char *argv[])
{

  if( argc > 1 ){
    NUM_SYNTHS = atoi(argv[1]);
  }else{
    NUM_SYNTHS = 8;
  }

  srand(time(NULL));

  pthread_t sources[NUM_SYNTHS];
  sequenceParams *sourceParams[NUM_SYNTHS];

  MIDIClientRef client;
  OSStatus result = MIDIClientCreate(
  CFStringCreateWithCString(NULL, "D-Ball's Client", kCFStringEncodingASCII),
  NULL,
  NULL,
  &client);

  MIDIEndpointRef endpoint = MIDIGetDestination(0);
  
  MIDIPortRef outputPort = NULL;
  MIDIOutputPortCreate(client, CFSTR("My output port"), &outputPort);
  
  MIDIPacketList packetList;
  packetList.numPackets = 1;
  packetList.packet[0].length = 3;
  packetList.packet[0].data[0] = 0x90; // note ON
  packetList.packet[0].data[1] = 60 & 0x7F; // C
  packetList.packet[0].data[2] = 127 & 0x7F; // velocity
  packetList.packet[0].timeStamp = 0;
    
  result = MIDISend(outputPort, endpoint, &packetList);


  for(int i = 0; i < NUM_SYNTHS; i++){
    
    MIDIEndpointRef e= MIDIGetDestination(i);

    int numsegs;
    numsegs = 1;

    sequenceParams *s = newSeq( numsegs ); 
    s->portRef = outputPort;
    s->endpoint = e;
    sourceParams[i] = s;
    pthread_mutex_init(&(s->lock),NULL);
    
    s->id = i;
  
    for( int j = 0; j < numsegs; j++ ){
      
      s->tone[j] = 0x20 + (rand() % 20);
      s->period[j] =  (1000000 + (rand() % 3000000));
      s->duration[j] =  (1000000 + (rand() % 2000000));

    }
      

    pthread_create( &sources[i], NULL, sequence_play, (void *)s );
    
  }

  while(1){

    int host;
    int port;
    int dir;
    scanf("%d %d %d",&host,&port,&dir);
    fprintf(stderr,"h=%u p=%u d=%u\n",host,port,dir);

    host = host % NUM_SYNTHS;
    pthread_mutex_lock(&(sourceParams[host]->lock));
    if( sourceParams[host]->cmdBuf->full == false ){

      /*full is false, so it is ok to put a command in*/
      sourceParams[host]->cmdBuf->hostNum = host;
      sourceParams[host]->cmdBuf->port = port;
      sourceParams[host]->cmdBuf->dir = dir;
      sourceParams[host]->cmdBuf->full = true;
     
    }
    pthread_mutex_unlock(&(sourceParams[host]->lock));
     

  }  

  return(0);
}
