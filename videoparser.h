#include <sys/types.h>
#include <stdint.h>

#define TS_PACKET_SIZE    188
#define TS_SYNC           0x47
#define TS_DISCONTINUITY  0x0
#define TS_STREAM_VIDEO   0x1b
#define TS_STREAM_AUDIO   0x0f

typedef struct BitData
{
  uint8_t   *TsData;
  size_t    TsSize;
  uint32_t  TsDataBank;  /*left-aligned bits*/
  size_t    TsNumBitsLeft;
}BitData;

/* Linked lists structure*/
typedef struct TSPointersListItem
{
  void                      *TsData;
  struct TSPointersListItem *TsNext;
}TSPointersListItem;

typedef struct TSSPointersList
{
  TSPointersListItem *TsHead;
  TSPointersListItem *TsTail;
}TSPointersList;

/* Programs (one TS file could have 1 or more programs)*/
typedef struct TSProgram
{
  uint32_t  TsProgramMapPID;
  uint64_t  TsFirstPTS;
  int       TsFirstPTSValid;
  TSPointersList TsStreams;
}TSProgram;

/*Streams (one program could have one or more streams)*/
typedef struct TSStream
{
  TSProgram  *pProgram;
  uint32_t   pElementaryPID;
  uint32_t   pStreamType;
  uint32_t   pPayloadStarted;
  char       pBuffer[128*1024];
  int        pBufferSize;
}TSStream;

/*Parser. Keeps a reference to the list of programs*/
typedef struct TSParser
{
  TSPointersList mPrograms;
}TSParser;

size_t     numBitsLeft(BitData *bitReader);
uint8_t    *getBitReaderData(BitData *bitReader);
uint32_t   getBits(BitData *bitReader, size_t n);
int64_t    parseTSTimestamp(BitData *bitReader);
TSStream   *getStreamByPID(TSProgram *program, uint32_t pid);
TSProgram  *getProgramByPID(TSParser *parser, uint32_t pid);

void initBitDta(BitData *bitReader, uint8_t *data, size_t size);
void skipBits(BitData *bitReader, size_t n);
void signalDiscontinuity(TSParser *parser, int isSeek);
void signalDiscontinuityToProgram(TSProgram *program, int isSeek);
void signalDiscontinuityToStream(TSStream *stream, int isSeek);
void parseTSPacket(TSParser *parser, BitData *bitReader);
void parseAdaptationField(TSParser *parser, BitData *bitReader);
void parseProgramId(TSParser *parser, BitData *bitReader, uint32_t pid, uint32_t payload_unit_start_indicator);
void parseProgramAssociationTable(TSParser *parser, BitData *bitReader);
void parseProgramMap(TSParser *parser, TSProgram *program, BitData *bitReader);
void parseStream(TSStream *stream, uint32_t payload_unit_start_indicator, BitData *bitReader);
void parsePES(TSStream *stream, BitData *bitReader);
void addProgram(TSParser *parser, uint32_t programMapPID);
void addStream(TSProgram *program, uint32_t elementaryPID, uint32_t streamType);
void flushStreaTsData(TSStream *stream);
void onPayloadData(TSStream *stream, uint32_t PTS_DTS_flag, uint64_t PTS, uint64_t DTS, uint8_t *data, size_t size);
void freeParserResources(TSParser *parser);
void addItemToList(TSPointersList *list, void *data);
