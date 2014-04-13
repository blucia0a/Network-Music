enum {
  kMIDINoteOff = 128,
  kMIDINoteOn = 144,
  kMIDICtrl = 176,
  kMIDIPrgm = 192
};

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
