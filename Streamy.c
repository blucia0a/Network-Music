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


MIDIPortRef outputPort;

enum {
  kMIDINoteOff = 128,
  kMIDINoteOn = 144,
  kMIDICtrl = 176,
  kMIDIPrgm = 192
};


int voice = 0;
bool done;
pthread_mutex_t doneLock;

typedef struct _cmd{

  int velocity;
  int numNotes;
  int tone[12];
  unsigned duration;
  bool full;
  bool isDoneMsg;

} cmd;

typedef struct _sequenceParams{

  int id;
  unsigned *target;
  cmd *cmdBuf;
  pthread_mutex_t lock;

  MIDIPortRef portRef;
  MIDIEndpointRef endpoint;
  
} sequenceParams;


void stopMySounds(int signum){

  pthread_exit(0);

}

void stopAllSounds(int signum){

  fprintf(stderr,"Info: Thread %lu is handling SIGINT\n",(unsigned long)pthread_self());
  done = true;

}

sequenceParams *newSeq(int num){

  sequenceParams *n = (sequenceParams *)malloc( sizeof(sequenceParams) ); 
  n->target = (unsigned *)malloc( num  * sizeof(unsigned) );
  n->cmdBuf = (cmd *)calloc(1,sizeof(cmd));
  return n;

}

void * sequence_play(void *p){

  sigset_t set;
  int s;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  s = pthread_sigmask(SIG_BLOCK, &set, NULL);
  if( s != 0 ){
    fprintf(stderr,"Warning: Signals will not be properly handled.\n");
  }else{
    fprintf(stderr,"Info: Thread %lu is blocking SIGINT\n",(unsigned long)pthread_self());
  }
  
  signal(SIGUSR1,stopMySounds);

  sequenceParams *pps = (sequenceParams *)p;

  while(1){
 
    cmd curCmd;
    while(1){

      pthread_mutex_lock(&(pps->lock)); 

      if(pps->cmdBuf->full){

        curCmd.isDoneMsg = pps->cmdBuf->isDoneMsg;
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
    
    if( curCmd.isDoneMsg ){

      fprintf(stderr,"EXITING!\n");
      pthread_exit(NULL);

    }

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
      
  pthread_mutex_init(&doneLock,NULL);

  pthread_t sources[NUM_SYNTHS];
  sequenceParams *sourceParams[NUM_SYNTHS];

  MIDIClientRef client;
  OSStatus result = MIDIClientCreate(
  CFStringCreateWithCString(NULL, "D-Ball's Client", kCFStringEncodingASCII),
  NULL,
  NULL,
  &client);

  MIDIOutputPortCreate(client, CFSTR("My output port"), &outputPort);
  
  signal(SIGINT,stopAllSounds);

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
      sourceParams[instrument]->cmdBuf->duration = duration;
      sourceParams[instrument]->cmdBuf->full = true;
     
    }
    pthread_mutex_unlock(&(sourceParams[instrument]->lock));
     
    if( done ){

      for(int inst = 0; inst < NUM_SYNTHS; inst++){

        fprintf(stderr,"Killing instrument %d\n",inst);
        pthread_kill(sources[inst],SIGUSR1);
        pthread_join(sources[inst],NULL);
        fprintf(stderr,"Info: Thread %lu is shutdown.  Turning off it's stuff...",(unsigned long)sources[inst]);

        MIDIEndpointRef ep = MIDIGetDestination(inst);
        MIDIPacketList packetList;
        for(int i = 0; i < 127; i++){
          packetList.numPackets = 1;
          packetList.packet[0].length = 3;
          packetList.packet[0].data[0] = kMIDINoteOff; // note ON
          packetList.packet[0].data[1] = 0x7f & i; // C
          packetList.packet[0].data[2] = 127; // velocity
          packetList.packet[0].timeStamp = 0;
          MIDISend(outputPort,ep,&packetList); 
        }

        fprintf(stderr,"done!\n");

      }
      return 0;

    }

  }  

  return 0;
}
