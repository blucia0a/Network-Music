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
  int velocity;
  int numNotes;
  int tone[12];
  unsigned duration;
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

        curCmd.velocity = pps->cmdBuf->velocity;
        curCmd.numNotes = pps->cmdBuf->numNotes;
        memcpy( curCmd.tone, pps->cmdBuf->tone, curCmd.numNotes * sizeof(int) );
        curCmd.duration = pps->cmdBuf->duration;
        pps->cmdBuf->full = false;
        pthread_mutex_unlock(&(pps->lock));
        break;

      }

      pthread_mutex_unlock(&(pps->lock));
    }

    //int tone = curCmd.tone;
    int level = curCmd.velocity;

    MIDIPacketList packetList;

    for(int i = 0; i < curCmd.numNotes; i++){
      
      /*On*/
      packetList.numPackets = 1;
      packetList.packet[0].length = 3;
      packetList.packet[0].data[0] = 0x90; // note ON
      packetList.packet[0].data[1] = 0x7f & curCmd.tone[i]; // C
      packetList.packet[0].data[2] = level; // velocity
      packetList.packet[0].timeStamp = 0;
      MIDISend(pps->portRef,pps->endpoint,&packetList); 
  
    }

    usleep( curCmd.duration ); 

    for(int i = 0; i < curCmd.numNotes; i++){

      /*Off*/
      packetList.numPackets = 1;
      packetList.packet[0].length = 3;
      packetList.packet[0].data[0] = kMIDINoteOff; // note ON
      packetList.packet[0].data[1] = 0x7f & curCmd.tone[i]; // C
      packetList.packet[0].data[2] = level; // velocity
      packetList.packet[0].timeStamp = 0;
      MIDISend(pps->portRef,pps->endpoint,&packetList); 

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
    
  //result = MIDISend(outputPort, endpoint, &packetList);


  for(int i = 0; i < NUM_SYNTHS; i++){
    
    MIDIEndpointRef e = MIDIGetDestination(i);

    int numsegs;
    numsegs = 1;

    sequenceParams *s = newSeq( numsegs ); 
    s->portRef = outputPort;
    s->endpoint = e;
    sourceParams[i] = s;
    pthread_mutex_init(&(s->lock),NULL);
    
    s->id = i;
    
     
    //int possiblenotes[15] = {36, 38, 40, 41, 43, 45, 47,
    int possiblenotes[15] = {48, 50, 52, 53, 55, 57, 59,
                             60, 62, 64, 65, 67, 69, 71,
                             72}; 

    for( int j = 0; j < numsegs; j++ ){
            
      s->tone[j] = possiblenotes[(rand() % 15)];
      s->period[j] =  (1000000 + (rand() % 3000000));
      s->duration[j] =  (1000000 + (rand() % 2000000));

    }
      

    pthread_create( &sources[i], NULL, sequence_play, (void *)s );
    
  }

  while(1){

    int instrument;
    int velocity;
    char tone[20];
    int chord_tones[12];
    int duration;
    scanf("%d %d %d %s",&instrument,&velocity,&duration,tone);

    instrument = instrument % NUM_SYNTHS;

    fprintf(stderr,"%d %d %d ",instrument,velocity,duration);
    memset(chord_tones, 0, 12 * sizeof(int));
    fprintf(stderr,"<%s>\n",tone);

    char *val;
    int noteCount = 0;
    for(val = strtok(tone,",");
        val && noteCount < 12;
        val = strtok(NULL,",")){

      chord_tones[noteCount++] = atoi(val); 
      fprintf(stderr,"%d ",chord_tones[noteCount - 1]);

    }
    fprintf(stderr,"\n");

    pthread_mutex_lock(&(sourceParams[instrument]->lock));

    if( sourceParams[instrument]->cmdBuf->full == false ){

      /*full is false, so it is ok to put a command in*/
      sourceParams[instrument]->cmdBuf->velocity = velocity;
      sourceParams[instrument]->cmdBuf->numNotes = noteCount;
      memcpy(sourceParams[instrument]->cmdBuf->tone, chord_tones, noteCount * sizeof(int));
      //sourceParams[instrument]->cmdBuf->tone = tone;
      sourceParams[instrument]->cmdBuf->duration = duration;
      sourceParams[instrument]->cmdBuf->full = true;
     
    }
    pthread_mutex_unlock(&(sourceParams[instrument]->lock));
     

  }  

  return(0);
}
