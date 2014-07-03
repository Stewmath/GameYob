#include <stdio.h>
#include <iostream>
#include <string.h>
#include <sstream>
#include "GBCPU.h"
#include "MMU.h"

using namespace std;

//int debugMode=0;

void printOp(int opcode)
{
	gbPC--;

	char* string;
	if (opcode == 0xCB)
		string = CBopcodeList[readMemory(gbPC)];
	else
		string = opcodeList[opcode];

	printf("%X: %s", gbPC, string);
	char lastChar=NULL;
	int numChars=0;
	int i;
	for (i=0; ; i++)
	{
		if (string[i] == '\0')
			break;
		if (string[i] == '#')
		{
			if (lastChar == '#')
				numChars = 2;
			else
				numChars = 1;
		}
		lastChar = string[i];
	}
	if (numChars == 1)
	{
		printf("; %X", readMemory(gbPC+1));
	}
	else if (numChars == 2)
		printf("; %X", readMemory(gbPC+1) | (readMemory(gbPC+2)<<8));
	printf("\n");

	gbPC++;
}

void parseCommand()
{
	while(true)
	{
		printf("> ");
		string input;
		string word;

		getline(cin, input);
		stringstream stream(input);
		stream >> word;

		if (word.compare("n") == 0)
			return;
		else if (word.compare("c") == 0)
		{
			debugMode = 0;
			return;
		}
		else if (word.compare("q") == 0)
		{
			quit = 1;
			debugMode = 0;
			return;
		}
		else if (word.compare("l") == 0)
		{
			saveLog();
		}
		else if (word.compare("w") == 0)
		{
			stream >> hex >> watchAddr;
			printf("Watching %x\n", watchAddr);
		}
		else if (word.compare("rw") == 0)
		{
			stream >> bankWatchAddr;
			stream >> hex >> readWatchAddr;
			printf("Watching %d:%x\n", bankWatchAddr, readWatchAddr);
		}
		/*else if (word.compare("p") == 0)
		{
			stream >> word;
			if (word.compare("banks") == 0)
				printf("ROM: %d\tRAM: %d\n", currentRomBank, currentRamBank);
			else
				printf("af: %x\tbc: %x\nde: %x\thl: %x\n", af.w, bc.w, de.w, hl.w);
		}*/
	}
}

void runDebugger(int opcode)
{
	printOp(opcode);
	parseCommand();
}

void saveLog()
{
	fclose(logFile);
	logFile = fopen("LOG", "a");
}
