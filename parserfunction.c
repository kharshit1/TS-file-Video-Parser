#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include "videoparser.h"

/*----------init, fill, get, skip bit data from packets------------*/
void initBitDta(BitData *bitReader, uint8_t *data, size_t size)
{
  bitReader->TsData = data;
  bitReader->TsSize = size;
  bitReader->TsdDataBank = 0;
  bitReader->TsNumBitsLeft = 0;
}

void filldatabank(BitData *bitReader)
{
  bitReader->TsdDataBank = 0;
  size_t i;
  for (i = 0; bitReader->TsSize > 0 && i < 4; ++i)
  {
    bitReader->TsdDataBank = (bitReader->TsdDataBank << 8) | *(bitReader->TsData);
    ++bitReader->TsData;
    --bitReader->TsSize;
  }
  bitReader->TsNumBitsLeft = 8 * i;
  bitReader->TsdDataBank <<= 32 - bitReader->TsNumBitsLeft;
}

uint32_t getBits(BitData *bitReader, size_t n)
{
  uint32_t result = 0;
  while (n > 0)
  {
    if (bitReader->TsNumBitsLeft == 0)
    {
      filldatabank(bitReader);
    }

    size_t m = n;
    if (m > bitReader->TsNumBitsLeft)
    {
      m = bitReader->TsNumBitsLeft;
    }
    result = (result << m) | (bitReader->TsdDataBank >> (32 - m));
    bitReader->TsdDataBank <<= m;
    bitReader->TsNumBitsLeft -= m;
    n -= m;
  }
  return result;
}

void skipBits(BitData *bitReader, size_t n)
{
  while (n > 32)
  {
    getBits(bitReader, 32);
    n -= 32;
  }
  if (n > 0)
  {
    getBits(bitReader, n);
  }
}

size_t numBitsLeft(BitData *bitReader)
{
  return bitReader->TsSize * 8 + bitReader->TsNumBitsLeft;
}

uint8_t *getBitReaderData(BitData *bitReader)
{
  return bitReader->TsData - bitReader->TsNumBitsLeft / 8;
}

/*----------Signal a discontinuity, Signal a discontinuity to a program, Signal a discontinuity to a stream------------*/

void signalDiscontinuity(TSParser *parser, int isSeek)
{
  TSPointersListItem *item;
  TSProgram *pProgram;
  while(parser->mPrograms.TsHead != NULL)
  {
    item = parser->mPrograms.TsHead;
    parser->mPrograms.TsHead = item->TsNext;
    if(item->TsData != NULL)
    {
      pProgram = (TSProgram *)item->TsData;
      signalDiscontinuityToProgram(pProgram, isSeek);
    }
  }
}

void signalDiscontinuityToProgram(TSProgram *program, int isSeek)
{
  TSPointersListItem *item;
  TSStream *pStream;
  while(program->TsStreams.TsHead != NULL)
  {
    item = program->TsStreams.TsHead;
    program->TsStreams.TsHead = item->TsNext;

    if(item->TsData != NULL)
    {
      pStream = (TSStream *)item->TsData;
      signalDiscontinuityToStream(pStream, isSeek);
    }
  }
}

void signalDiscontinuityToStream(TSStream *stream, int isSeek)
{
  stream->pPayloadStarted = 0;
  stream->pBufferSize = 0;
}

/* ----------------- Adding a new program, stream and pointers  to the list of programs*/

void addProgram(TSParser *parser, uint32_t programMapPID)
{
  TSProgram *program = (TSProgram *)malloc(sizeof(TSProgram));
  memset(program, 0, sizeof(TSProgram));
  program->TsProgramMapPID = programMapPID;
  addItemToList(&parser->mPrograms, program);
}

void addStream(TSProgram *program, uint32_t elementaryPID, uint32_t streamType)
{
  TSStream *stream = (TSStream *)malloc(sizeof(TSStream));
  memset(stream, 0, sizeof(TSStream));
  stream->pProgram = program;
  stream->pElementaryPID = elementaryPID;
  stream->pStreamType = streamType;
  addItemToList(&program->TsStreams, stream);
}

void addItemToList(TSPointersList *list, void *data)
{
  TSPointersListItem *item = (TSPointersListItem *)malloc(sizeof(TSPointersListItem));
  item->TsData = data;
  item->TsNext = NULL;
  if(list->TsTail != NULL)
  {
    list->TsTail->TsNext = item;
    list->TsTail = item;
  }
  else
  {
    list->TsHead = list->TsTail = item;
  }
}

/* Get Stream object given its pid*/
TSStream *getStreamByPID(TSProgram *program, uint32_t pid)
{
  TSPointersListItem *listItem;
  TSStream *stream;
  for(listItem = program->TsStreams.TsHead; listItem != NULL; listItem=listItem->TsNext)
  {
    stream = (TSStream *)listItem->TsData;
    if(stream != NULL && stream->pElementaryPID == pid)
    {
      return stream;
    }
  }
  return NULL;
}

/* Get a Program object given its pid*/
TSProgram *getProgramByPID(TSParser *parser, uint32_t pid)
{
  TSPointersListItem *listItem;
  TSProgram *program;
  for(listItem = parser->mPrograms.TsHead; listItem != NULL; listItem=listItem->TsNext)
  {
    program = (TSProgram *)listItem->TsData;
    if(program != NULL && program->TsProgramMapPID == pid)
    {
      return program;
    }
  }
  return NULL;
}

/*-----------Free resources------------------*/

void freeProgramResources(TSProgram *program)
{
  TSPointersListItem *item;
  TSStream *pStream;
  while(program->TsStreams.TsHead != NULL)
  {
    item = program->TsStreams.TsHead;
    program->TsStreams.TsHead = item->TsNext;
    if(item->TsData != NULL)
    {
      pStream = (TSStream *)item->TsData;
      free(pStream);
    }
    free(item);
  }
}

void freeParserResources(TSParser *parser)
{
  TSPointersListItem *item;
  TSProgram *pProgram;
  while(parser->mPrograms.TsHead != NULL)
  {
    item = parser->mPrograms.TsHead;
    parser->mPrograms.TsHead = item->TsNext;
    if(item->TsData != NULL)
    {
      pProgram = (TSProgram *)item->TsData;
      freeProgramResources(pProgram);
      free(pProgram);
    }
    free(item);
  }
}

void flushStreamData(TSStream *stream)
{
  BitData bitReader;
  initBitDta(&bitReader, (uint8_t *)stream->pBuffer, stream->pBufferSize);
  parsePES(stream, &bitReader);
  stream->pBufferSize = 0;
}

/*----------Parsing of packets ------------*/

void parseTSPacket(TSParser *parser, BitData *bitReader)
{
  uint32_t sync_byte;
  uint32_t transport_error_indicator;
  uint32_t payload_unit_start_indicator;
  uint32_t transport_priority;
  uint32_t pid;
  uint32_t transport_scrambling_control;
  uint32_t adaptation_field_control;
  uint32_t continuity_counter;

  sync_byte = getBits(bitReader, 8);
  transport_error_indicator = getBits(bitReader, 1);

  if(transport_error_indicator != 0)
  {
    printf("Packet with Error. Transport Error indicator: %u\n", transport_error_indicator);
  }

  payload_unit_start_indicator = getBits(bitReader, 1);
  transport_priority = getBits(bitReader, 1);
  pid = getBits(bitReader, 13);
  transport_scrambling_control = getBits(bitReader, 2);
  adaptation_field_control = getBits(bitReader, 2);
  continuity_counter = getBits(bitReader, 4);

/* For Adaptation field control = 01 there will be only adaptation field,
   For Adaptation field control = 11 there will be payload followed by adaptation field
   For Adaptation field control = 01 there will be only payload no adaptation field*/

  if(adaptation_field_control == 2 || adaptation_field_control == 3)
  {
    skipAdaptationField(parser, bitReader);
  }
  if(adaptation_field_control == 1 || adaptation_field_control == 3)
  {
    parseProgramId(parser, bitReader, pid, payload_unit_start_indicator);
  }
}

/* skip adaptation field bits */
void skipAdaptationField(TSParser *parser, BitData *bitReader)
{
  uint32_t adaptation_field_length = getBits(bitReader, 8);
  if (adaptation_field_length > 0)
  {
    skipBits(bitReader, adaptation_field_length * 8);
  }
}

/* Parse program Id */
void parseProgramId(TSParser *parser, BitData *bitReader, uint32_t pid, uint32_t payload_unit_start_indicator)
{
  int i, handled = 0;
  TSPointersListItem *listItem;
  TSProgram *pProgram;

  if (pid == 0)
  {
    if (payload_unit_start_indicator)
    {
      uint32_t skip = getBits(bitReader, 8);
      skipBits(bitReader, skip * 8);
    }

    parseProgramAssociationTable(parser, bitReader);
    return;
  }

  for(listItem = parser->mPrograms.TsHead; listItem != NULL; listItem=listItem->TsNext)
  {
    pProgram = (TSProgram *)listItem->TsData;
    if(pid == pProgram->TsProgramMapPID)
    {
      if(payload_unit_start_indicator)
      {
        uint32_t skip = getBits(bitReader, 8);
        skipBits(bitReader, skip * 8);
      }

      parseProgramMap(parser, pProgram, bitReader);
      handled = 1;
      break;
    }
    else
    {
      TSStream *pStream = getStreamByPID(pProgram, pid);
      if(pStream != NULL)
      {
        parseStream(pStream, payload_unit_start_indicator, bitReader);

        handled = 1;
        break;
      }
    }
  }

  if (!handled)
  {
    printf("PID 0x%04x not handled.\n", pid);
  }
}

/*Parse Program Association table*/
void parseProgramAssociationTable(TSParser *parser, BitData *bitReader)
{
  size_t i;
  uint32_t table_id = getBits(bitReader, 8);
  uint32_t section_syntax_indicator = getBits(bitReader, 1);
  getBits(bitReader, 1);
  skipBits(bitReader, 2); /*reserved*/
  uint32_t section_length = getBits(bitReader, 12);
  uint32_t transport_stream_id = getBits(bitReader, 16);
  skipBits(bitReader, 2); /*reserved*/
  uint32_t version_number = getBits(bitReader, 5);
  uint32_t current_next_indicator=getBits(bitReader, 1);
  uint32_t section_number = getBits(bitReader, 8);
  uint32_t last_section_number = getBits(bitReader, 8);
  size_t numProgramBytes = (section_length - 5 - 4 );

  for (i = 0; i < numProgramBytes / 4; ++i)
  {
    uint32_t program_number = getBits(bitReader, 16);
    skipBits(bitReader, 3); /*reserved*/
    if (program_number == 0)
    {
      uint32_t network_PID = getBits(bitReader, 13));
    }
    else
    {
      unsigned programMapPID = getBits(bitReader, 13);
      addProgram(parser, programMapPID);
    }
  }
}

/*Parse Stream*/
void parseStream(TSStream *stream, uint32_t payload_unit_start_indicator, BitData *bitReader)
{
  size_t payloadSizeBits;

  if(payload_unit_start_indicator)
  {
    if(stream->pPayloadStarted)
    {
      flushStreamData(stream);
    }
    stream->pPayloadStarted = 1;
  }
  if(!stream->pPayloadStarted)
  {
    return;
  }
  payloadSizeBits = numBitsLeft(bitReader);
  memcpy(stream->pBuffer + stream->pBufferSize, getBitReaderData(bitReader), payloadSizeBits / 8);
  stream->pBufferSize += (payloadSizeBits / 8);
}

/*Parse a PES packet*/
void parsePES(TSStream *stream, BitData *bitReader)
{
  uint32_t packet_startcode_prefix = getBits(bitReader, 24);
  uint32_t stream_id = getBits(bitReader, 8);
  uint32_t PES_packet_length = getBits(bitReader, 16);

  if (stream_id  != 0xbc  /* program stream map */
    && stream_id != 0xbe  /* padding stream */
    && stream_id != 0xbf  /* private stream */
    && stream_id != 0xf0  /* ECM */
    && stream_id != 0xf1  /* EMM */
    && stream_id != 0xff  /* program stream directory */
    && stream_id != 0xf2  /* DSMCC */
    && stream_id != 0xf8) /* H.222 */
  {
    uint32_t PTS_DTS_flags;
    uint32_t ESCR_flag;
    uint32_t ES_rate_flag;
    uint32_t DSM_trick_mode_flag;
    uint32_t additional_copy_info_flag;
    uint32_t PES_header_data_length;
    uint32_t optional_bytes_remaining;
    uint64_t PTS = 0, DTS = 0;
    skipBits(bitReader, 8);
    PTS_DTS_flags = getBits(bitReader, 2);
    ESCR_flag = getBits(bitReader, 1);
    ES_rate_flag = getBits(bitReader, 1);
    DSM_trick_mode_flag = getBits(bitReader, 1);
    additional_copy_info_flag = getBits(bitReader, 1);
    skipBits(bitReader, 2);
    PES_header_data_length = getBits(bitReader, 8);
    optional_bytes_remaining = PES_header_data_length;
    if (PTS_DTS_flags == 2 || PTS_DTS_flags == 3)
    {
      skipBits(bitReader, 4);
      PTS = parseTSTimestamp(bitReader);
      skipBits(bitReader, 1);
      optional_bytes_remaining -= 5;
      if (PTS_DTS_flags == 3)
      {
        skipBits(bitReader, 4);
        DTS = parseTSTimestamp(bitReader);
        skipBits(bitReader, 1);
        optional_bytes_remaining -= 5;
      }
    }
    if (ESCR_flag)
    {
      skipBits(bitReader, 2);
      uint64_t ESCR = parseTSTimestamp(bitReader);
      skipBits(bitReader, 11);
      optional_bytes_remaining -= 6;
    }
    if (ES_rate_flag)
    {
      skipBits(bitReader, 24);
      optional_bytes_remaining -= 3;
    }
    skipBits(bitReader, optional_bytes_remaining * 8);
    if (PES_packet_length != 0)
    {
      uint32_t dataLength = PES_packet_length - 3 - PES_header_data_length;
      writePayLoadData(stream, PTS_DTS_flags, PTS, DTS, getBitReaderData(bitReader), dataLength);
      skipBits(bitReader, dataLength * 8);
    }
    else
    {
      size_t payloadSizeBits;
      writePayLoadData(stream, PTS_DTS_flags, PTS, DTS, getBitReaderData(bitReader), numBitsLeft(bitReader) / 8);
      payloadSizeBits = numBitsLeft(bitReader);
    }
  }
  else if (stream_id == 0xbe)
  {  
    skipBits(bitReader, PES_packet_length * 8);
  }
  else
  {
    skipBits(bitReader, PES_packet_length * 8);
  }
}

/* Parse program map */
void parseProgramMap(TSParser *parser, TSProgram *program, BitData *bitReader)
{
  uint32_t program_info_length;
  size_t infoBytesRemaining;
  uint32_t streamType;
  uint32_t elementaryPID;
  uint32_t ES_info_length;
  uint32_t info_bytes_remaining;
  uint32_t table_id = getBits(bitReader, 8);
  uint32_t section_syntax_indicator = getBits(bitReader, 1);
  skipBits(bitReader, 3);   /*reserved*/
  uint32_t section_length = getBits(bitReader, 12);
  uint32_t program_number = getBits(bitReader, 16);
  skipBits(bitReader, 2);   /*reserved*/
  uint32_t version_number = getBits(bitReader, 5);
  uint32_t current_next_indicator = getBits(bitReader, 1);
  uint32_t  section_number = getBits(bitReader, 8);
  uint32_t  last_section_number = getBits(bitReader, 8);
  uint32_t  reserved =  getBits(bitReader, 3);
  uint32_t  PCR_PID = getBits(bitReader, 13);
  skipBits(bitReader, 4);   /*reserved*/
  program_info_length = getBits(bitReader, 12);
  skipBits(bitReader, program_info_length * 8);  /* skip descriptors*/

  /*infoBytesRemaining is the number of bytes that make up the
   variable length section of ES_infos. It does not include the
   final CRC.*/
  infoBytesRemaining = section_length - 9 - program_info_length - 4;

  while (infoBytesRemaining > 0)
  {
    streamType = getBits(bitReader, 8);
    /*Reserved*/
    skipBits(bitReader, 3);
    elementaryPID = getBits(bitReader, 13);
    /*Reserved*/
    skipBits(bitReader, 4);
    ES_info_length = getBits(bitReader, 12);
    info_bytes_remaining = ES_info_length;
    while (info_bytes_remaining >= 2)
    {
      uint32_t descLength;
      descLength = getBits(bitReader, 8);
      skipBits(bitReader, descLength * 8);
      info_bytes_remaining -= descLength + 2;
    }
    if(getStreamByPID(program, elementaryPID) == NULL)
      addStream(program, elementaryPID, streamType);

    infoBytesRemaining -= 5 + ES_info_length;
  }
}

int64_t parseTSTimestamp(BitData *bitReader)
{
  int64_t result = ((uint64_t)getBits(bitReader, 3)) << 30;
  skipBits(bitReader, 1);
  result |= ((uint64_t)getBits(bitReader, 15)) << 15;
  skipBits(bitReader, 1);
  result |= getBits(bitReader, 15);
  return result;
}

/*convert PTS to timestamp*/
int64_t convertPTSToTimestamp(TSStream *stream, uint64_t PTS)
{
  if (!stream->pProgram->TsFirstPTSValid)
  {
    stream->pProgram->TsFirstPTSValid = 1;
    stream->pProgram->TsFirstPTS = PTS;
    PTS = 0;
  }
  else if (PTS < stream->pProgram->TsFirstPTS)
  {
    PTS = 0;
  }
  else
  {
    PTS -= stream->pProgram->TsFirstPTS;
  }
  return (PTS * 100) / 9;
}

/* Function called when we have payload content */
void writePayLoadData(TSStream *stream, uint32_t PTS_DTS_flag, uint64_t PTS, uint64_t DTS, uint8_t *data, size_t size)
{
  int64_t timeUs = convertPTSToTimestamp(stream, PTS);
  if(stream->pStreamType == TS_STREAM_VIDEO)
  {
    fprintf(VD,"%s",data);
  }
  else if(stream->pStreamType == TS_STREAM_AUDIO)
  {
    fprintf(VD,"%s",data);
  }
}

