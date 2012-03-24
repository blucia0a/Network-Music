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
AUNode *SynthNodes;
AudioUnit *SynthUnits;
AudioUnit OutputUnit;

AUGraph AudioGraph;
enum {
  kMIDINoteOff = 128,
  kMIDINoteOn = 144,
  kMIDICtrl = 176,
  kMIDIPrgm = 192
};

int voice = 0;
enum {
    kKeyboardAKey     = 0x00,
    kKeyboardBKey     = 0x0b,
    kKeyboardCKey     = 0x08,
    kKeyboardDKey     = 0x02,
    kKeyboardEKey     = 0x0e,
    kKeyboardFKey     = 0x03,
    kKeyboardGKey     = 0x05,
    kKeyboardHKey     = 0x04,
    kKeyboardIKey     = 0x22,
    kKeyboardJKey     = 0x26,
    kKeyboardKKey     = 0x28,
    kKeyboardLKey     = 0x25,
    kKeyboardMKey     = 0x2e,
    kKeyboardNKey     = 0x2d,
    kKeyboardOKey     = 0x1f,
    kKeyboardPKey     = 0x23,
    kKeyboardQKey     = 0x0c,
    kKeyboardRKey     = 0x0f,
    kKeyboardSKey     = 0x01,
    kKeyboardTKey     = 0x11,
    kKeyboardUKey     = 0x20,
    kKeyboardVKey     = 0x09,
    kKeyboardWKey     = 0x0d,
    kKeyboardXKey     = 0x07,
    kKeyboardYKey     = 0x10,
    kKeyboardZKey     = 0x06,

    kKeyboardEscapeKey = 0x35,
    kKeyboardSpaceKey  = 0x31,
    kKeyboardCommaKey  = 0x2b,
    kKeyboardStopKey   = 0x2f,
    kKeyboardSlashKey  = 0x2c,
    kKeyboardColonKey  = 0x29,
    kKeyboardQuoteKey  = 0x27,
    kKeyboardLBrktKey  = 0x21
};


typedef struct _key{
  bool down;
  int tone;/*0x3c is middle c*/
} key;

key Keys[13];
CGEventRef keyPressed(
   CGEventTapProxy proxy,
   CGEventType type,
   CGEventRef event,
   void *refcon
){
    AudioUnit *SUs = (AudioUnit *)refcon;
    
    if((type == kCGEventMouseMoved)){  
      CGPoint cursor = CGEventGetLocation(event);
      float my = cursor.y;
      float frac = my/900.; 
      int newval = (int)((float)65535 * frac);
      
      //fprintf(stderr,"%d\n",newval);

      if( doMouse ){
        for(int i = 0; i < NUM_SYNTHS; i++){
          MusicDeviceMIDIEvent(SUs[i],  224, (newval & 0xFF), ( newval & 0xFF00 ) >> 8, 0);
        }
      }
      return event;

    }

    if ((type != kCGEventKeyDown) && (type != kCGEventKeyUp)){
        return event;
    }

    // The incoming keycode.
    CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(
                                       event, kCGKeyboardEventKeycode);

    if( keycode == kKeyboardQKey && type == kCGEventKeyDown ){

      exit(0);

    }


    return event; 

}

typedef struct _playParams{

  int id;
  unsigned long period;
  unsigned long duration;
  unsigned tone;
  
} playParams;

void * periodic_play(void *p){

  playParams *pps = (playParams*)p;
  while(1){
    MusicDeviceMIDIEvent(SynthUnits[pps->id], kMIDINoteOn, pps->tone, 127, 0);
    usleep( pps->duration ); 
    MusicDeviceMIDIEvent(SynthUnits[pps->id], kMIDINoteOff, pps->tone, 127, 0);
    usleep( pps->period); 
  }
   
}

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
    packetList.numPackets = 1;
    packetList.packet[0].length = 3;
    packetList.packet[0].data[0] = 0x90; // note ON
    //packetList.packet[0].data[1] = 60 & 0x7F; // C
    packetList.packet[0].data[1] = tone; // C
    packetList.packet[0].data[2] = level; // velocity
    packetList.packet[0].timeStamp = 0;
    OSStatus result = MIDISend(pps->portRef,pps->endpoint,&packetList); 
      fprintf(stderr,"%d\n",result); 

      //MusicDeviceMIDIEvent(SynthUnits[pps->id], kMIDINoteOn, pps->tone[i] * (1 + curCmd.dir), level, 0);
      usleep( pps->duration[i] ); 
    packetList.numPackets = 1;
    packetList.packet[0].length = 3;
    packetList.packet[0].data[0] = kMIDINoteOff; // note ON
    //packetList.packet[0].data[1] = 60 & 0x7F; // C
    packetList.packet[0].data[1] = tone; // C
    packetList.packet[0].data[2] = level; // velocity
    packetList.packet[0].timeStamp = 0;
    result = MIDISend(pps->portRef,pps->endpoint,&packetList); 
      

     // MusicDeviceMIDIEvent(SynthUnits[pps->id], kMIDINoteOff, pps->tone[i], level, 0);
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
  SynthNodes = (AUNode *)malloc(sizeof(AUNode) * NUM_SYNTHS);
  SynthUnits = (AudioUnit*)malloc(sizeof(AudioUnit) * NUM_SYNTHS);

  if( argc > 2 ){
    g_level = atoi(argv[2]);
  }else{
    g_level = 64;
  }

  srand(time(NULL));
  int i;
  for(i = 0; i < 13; i++){
    Keys[i].down = false;
    Keys[i].tone = 0x41 + (i);
  }

  pthread_t sources[NUM_SYNTHS];
  sequenceParams *sourceParams[NUM_SYNTHS];
  int instruments[NUM_SYNTHS]; 
    

    MIDIClientRef client;
    OSStatus result = MIDIClientCreate(
    CFStringCreateWithCString(NULL, "D-Ball's Client", kCFStringEncodingASCII),
    NULL,
    NULL,
    &client);

  MIDIEndpointRef endpoint = MIDIGetDestination(0);
  
  MIDIPortRef outputPort = NULL;
  MIDIOutputPortCreate(client, CFSTR("My output port"), &outputPort);
  
  //while (1) {
    MIDIPacketList packetList;
    packetList.numPackets = 1;
    packetList.packet[0].length = 3;
    packetList.packet[0].data[0] = 0x90; // note ON
    packetList.packet[0].data[1] = 60 & 0x7F; // C
    packetList.packet[0].data[2] = 127 & 0x7F; // velocity
    packetList.packet[0].timeStamp = 0;
    
    //printf("Sending...\n");
    result = MIDISend(outputPort, endpoint, &packetList);
    //printf("Got code %d\n", result);
    
    //usleep(2000000);
  //}


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
      
    instruments[i] = i*4;

    //MusicDeviceMIDIEvent(SynthUnits[i], 0xC0, instruments[i], 0, 0);
    pthread_create( &sources[i], NULL, sequence_play, (void *)s );
    
  }

  while(1){

    fprintf(stderr,"MOTHERFUCKER\n");
    int host;
    int port;
    int dir;
    scanf("%d %d %d",&host,&port,&dir);
    fprintf(stderr,"h=%u p=%u d=%u\n",host,port,dir);

    host = host % NUM_SYNTHS;
    pthread_mutex_lock(&(sourceParams[host]->lock));
    fprintf(stderr,"MOTHERFUCKER?\n");
    if( sourceParams[host]->cmdBuf->full == false ){

      fprintf(stderr,"MOTHERFUCKER was empty!\n");
      /*full is false, so it is ok to put a command in*/
      sourceParams[host]->cmdBuf->hostNum = host;
      sourceParams[host]->cmdBuf->port = port;
      sourceParams[host]->cmdBuf->dir = dir;
      sourceParams[host]->cmdBuf->full = true;
     
    }
    pthread_mutex_unlock(&(sourceParams[host]->lock));
     
    fprintf(stderr,"Done! MOTHERFUCKER!\n");
  }  

  return(0);
}
