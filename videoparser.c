#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "videoparser.h"

#define AUDIO_FILE "AUD_AAC" 
#define VIDEO_FILE "VID_AVC"

int main(int argc, char *argv[])
{
	FILE *VD=NULL;
    FILE *AD=NULL;
	uint8_t packet_buffer[TS_PACKET_SIZE];
	int  fd, bytes_read, n_packets = 0;

	TSParser tsParser;
	memset(&tsParser, 0, sizeof(TSParser));

	fd = open(argv[1], O_RDONLY);
  	if(fd > 0)
	{
		printf("Error opening the video file <.ts File to parse>\n");
		return -1;
	}
    VD = fopen(AUDIO_FILE, "w");
    if (VD == NULL)
    {
       printf("Error while creatig the raw video file\n");
	   return -1;
    }
	FD = fopen(AUDIO_FILE, "w");
    if (AD == NULL)
    {
       printf("Error while creatig the raw audio file\n");
	   return -1;
    }
	while(1)
	{
		bytes_read = read(fd, packet_buffer, TS_PACKET_SIZE);

		if(packet_buffer[0] == TS_DISCONTINUITY)
		{
			printf("Discontinuity detected!\n");
			signalDiscontinuity(&tsParser, 0);
		}
		else if(bytes_read < TS_PACKET_SIZE)
		{
			printf("End of file!\n");
			break;
		}
		else if(packet_buffer[0] == TS_SYNC)
		{
			BitData bitReader;
			initBitDta(&bitReader, packet_buffer, bytes_read);

			parseTSPacket(&tsParser, &bitReader);

			n_packets++;
		}
	}

	printf("The TS file has been parsed!!!\nRaw Audio and Raw Video files has been created.\n");
	close(fd);
    fclose(AD);
    fclose(VD);
	freeParserResources(&tsParser);
    return 0;
}
