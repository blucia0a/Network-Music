#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFBundle.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include "Streamy.h"

using namespace std;
int NUM_SYNTHS;
MIDIPortRef outputPort;
bool done;
pthread_mutex_t doneLock;


void stopMySounds(int signum){
  /*
    This is a signal handler that gets called by worker threads
    when they receive SIGUSR1
  */
  pthread_exit(0);

}



void stopAllSounds(int signum){
  /*This is a signal handler that deals with SIGINTs.  It sets the 'done' variable
    that makes everyone else stop playing their sequences
  */
  done = true;

}



/*Constructor for a sequence*/
sequenceParams *newSeq(int num){

  sequenceParams *n = (sequenceParams *)malloc( sizeof(sequenceParams) ); 
  n->target = (unsigned *)malloc( num  * sizeof(unsigned) );
  n->cmdBuf = (cmd *)calloc(1,sizeof(cmd));
  return n;

}

/*
  Play the command specified in curCmd on the sequence player (thread) 
  specified in pps
*/ 
void playMIDICmd(cmd *curCmd, sequenceParams *pps){

    int level = curCmd->velocity;
    MIDIPacketList packetList;

    for(int i = 0; i < curCmd->numNotes; i++){
      
      /*On*/
      packetList.numPackets = 1;
      packetList.packet[0].length = 3;
      packetList.packet[0].data[0] = 0x90; // note ON
      packetList.packet[0].data[1] = 0x7f & curCmd->tone[i]; // C
      packetList.packet[0].data[2] = level; // velocity
      packetList.packet[0].timeStamp = 0;
      MIDISend(pps->portRef,pps->endpoint,&packetList); 
  
    }

    usleep( curCmd->duration ); 

    for(int i = 0; i < curCmd->numNotes; i++){

      /*Off*/
      packetList.numPackets = 1;
      packetList.packet[0].length = 3;
      packetList.packet[0].data[0] = kMIDINoteOff; // note OFF
      packetList.packet[0].data[1] = 0x7f & curCmd->tone[i]; // C
      packetList.packet[0].data[2] = level; // velocity
      packetList.packet[0].timeStamp = 0;
      MIDISend(pps->portRef,pps->endpoint,&packetList); 

    }

}

/*Play a sequence -- this is the thread main function*/
void * sequence_play(void *p){

  sigset_t set;
  int s;

  /*These threads block SIGINT so the main thread gets those*/
  /*These threads want SIGUSR1 and that will make them quit*/
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  s = pthread_sigmask(SIG_BLOCK, &set, NULL);
  if( s != 0 ){
    fprintf(stderr,"Warning: Signals will not be properly handled.\n");
  }
  signal(SIGUSR1,stopMySounds);

  /*Unpack this thread's thread information structure, passed from its creator*/
  sequenceParams *pps = (sequenceParams *)p;

  /*Process commands and send them out as MIDI messages until interrupted with a signal*/
  while(1){
 
    cmd curCmd;

    /*Spin checking the command buffer until it is non-empty*/
    /*TODO: condition variables to decrease CPU utilization*/
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
   
    /*If we got a 'done' command, then quit*/ 
    if( curCmd.isDoneMsg ){

      fprintf(stderr,"EXITING!\n");
      pthread_exit(NULL);

    }

    /*If it was not a 'done' message, then it was a note.  Play the note*/
    playMIDICmd(&curCmd,pps);

  }
   
}

/*This is the code that the main thread uses to stop everything and quit*/
void stopAllInstruments(pthread_t *sources){

  for(int inst = 0; inst < NUM_SYNTHS; inst++){

    pthread_kill(sources[inst],SIGUSR1);
    pthread_join(sources[inst],NULL);

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

  }

}
   
/*this function spawns threads, passing them information about themselves*/ 
void createInstrumentManager(pthread_t *thd, sequenceParams **sourceParams, int i){

    MIDIEndpointRef e = MIDIGetDestination(i);

    sequenceParams *s = newSeq( 1 ); 
    s->portRef = outputPort;
    s->endpoint = e;
    sourceParams[i] = s;
    pthread_mutex_init(&(s->lock),NULL);
    
    s->id = i;

    pthread_create( thd, NULL, sequence_play, (void *)s );
}


int main(int argc, char *argv[])
{

  /*Process the argument that records the number of instruments to use*/
  /*The default number of instruments is 8*/
  if( argc > 1 ){
    NUM_SYNTHS = atoi(argv[1]);
  }else{
    NUM_SYNTHS = 8;
  }

  /*Do some system initialization*/
  srand(time(NULL));
      
  pthread_mutex_init(&doneLock,NULL);

  pthread_t sources[NUM_SYNTHS];
  
  signal(SIGINT,stopAllSounds);

  sequenceParams *sourceParams[NUM_SYNTHS];

  /*MIDI Initialization*/
  MIDIClientRef client;
  OSStatus result = 
    MIDIClientCreate( CFStringCreateWithCString(NULL, "Streamy", kCFStringEncodingASCII), NULL, NULL, &client);

  MIDIOutputPortCreate(client, CFSTR("Streamy - output"), &outputPort);
  

  /*Create a thread for each instrument*/
  for(int i = 0; i < NUM_SYNTHS; i++){

    createInstrumentManager(&sources[i], sourceParams, i);    
    
  }

  /*This is the main loop that parses incoming note commands from MIDIShark*/
  std::string line;
  while(std::getline(std::cin, line)){

    std::istringstream iss(line);

    int instrument;
    int velocity;
    char tone[20];
    int chord_tones[12];
    int duration;

    /*Scan a line of input*/ 
    iss >> instrument >> velocity >> duration >> tone; 


    /*
     Mod the instrument number to be sure it is in the range of
     synth numbers we can accommodate.  This is defensive, because
     MIDIShark should bound the instrument number
    */
    instrument = instrument % NUM_SYNTHS;

    /*Clear the array of tones.  We allow at most 12 tones*/
    memset(chord_tones, 0, 12 * sizeof(int));

    /*Tokenize the parts of the command*/
    char *val;
    int noteCount = 0;
    for(val = strtok(tone,",");
        val && noteCount < 12;
        val = strtok(NULL,",")){

      chord_tones[noteCount++] = atoi(val); 

    }

    /*
      Send the parsed command along to the thread that will execute it and
      send it to the MIDI subsystem
    */
    pthread_mutex_lock(&(sourceParams[instrument]->lock));
    if( sourceParams[instrument]->cmdBuf->full == false ){

      sourceParams[instrument]->cmdBuf->velocity = velocity;
      sourceParams[instrument]->cmdBuf->numNotes = noteCount;
      memcpy(sourceParams[instrument]->cmdBuf->tone, chord_tones, noteCount * sizeof(int));
      sourceParams[instrument]->cmdBuf->duration = duration;
      sourceParams[instrument]->cmdBuf->full = true;
     
    }
    pthread_mutex_unlock(&(sourceParams[instrument]->lock));
    
    /*Check once per iteration whether some other thread has received a
      signal from the user to terminate everything and quit (SIGINT).  If
      one of those shows up, kill all the instruments.  'done' gets set in
      the signal handler for SIGINT
    */ 
    if( done ){
      stopAllInstruments(sources);
      return 0;

    }

  }  

  return 0;

}
