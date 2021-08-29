/*
  microBox.cpp - Library for Linux-Shell like interface for Arduino.
  Created by Sebastian Duell, 06.02.2015.
  More info under http://sebastian-duell.de
  Released under GPLv3.
  Heavily modified for Porting to Teensy T3.6, T4.0, T4.2, MicroMod?
  By: Warren Watson 08-08-21.
  See readme.md in this folder for info on microBox.
*/

#include <Audio.h>
#include <diskIOMB.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "mscFS.h"
#include "diskIO.h"
#include <play_wav.h>

// GUItool: begin automatically generated code
AudioPlayWav           playSdWav1;     //xy=323,171
AudioMixer4              mixer1;         //xy=647,123
AudioMixer4              mixer3;         //xy=648,212
//AudioOutputPT8211        pt8211_1;       //xy=828,169
AudioOutputI2S           audioOutput;
AudioConnection          patchCord1(playSdWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playSdWav1, 1, mixer3, 0);
AudioConnection          patchCord3(playSdWav1, 2, mixer1, 1);
AudioConnection          patchCord4(playSdWav1, 3, mixer3, 1);
AudioConnection          patchCord5(playSdWav1, 4, mixer1, 2);
AudioConnection          patchCord6(playSdWav1, 5, mixer3, 2);
AudioConnection          patchCord7(playSdWav1, 6, mixer1, 3);
AudioConnection          patchCord8(playSdWav1, 7, mixer3, 3);
AudioConnection          patchCord9(mixer1, 0, audioOutput, 0);
AudioConnection          patchCord10(mixer3, 0, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;

// GUItool: end automatically generated code

diskIO dioMB;  // One instance of diskIO.
microBox microbox;

CMD_ENTRY microBox::Cmds[] =
{
    {"cat", microBox::CatCB},
    {"cd", microBox::ChangeDirCB},
    {"echo", microBox::EchoCB},
    {"loadpar", microBox::LoadParCB},
    {"ld", microBox::ListDrivesCB},
    {"ls", microBox::ListDirCB},
    {"savepar", microBox::SaveParCB},
    {"watch", microBox::watchCB},
    {"watchcsv", microBox::watchcsvCB},
    {"clear", microBox::clearCB},
    {"mkdir", microBox::mkdirCB},
    {"rmdir", microBox::rmdirCB},
    {"rm", microBox::rmCB},
    {"rename", microBox::renameCB},
    {"cp", microBox::cpCB},
    {"help", microBox::helpCB},
    {"play", microBox::PlayCB},
    {NULL, NULL}
};

const char microBox::dirList[][5] PROGMEM =
{
    "bin", "dev", "etc", "proc", "sbin", "var", "lib", "sys", "tmp", "usr", ""
};

microBox::microBox()
{
    bufPos = 0;
    watchMode = false;
    csvMode = false;
    locEcho = false;
    watchTimeout = 0;
    escSeq = 0;
    historyWrPos = 0;
    historyBufSize = 0;
    historyCursorPos = -1;
    stateTelnet = TELNET_STATE_NORMAL;
}

microBox::~microBox()
{
}

void microBox::begin(PARAM_ENTRY *pParams, const char* hostName, bool localEcho, char *histBuf, int historySize)
{
    historyBuf = histBuf;
    if(historyBuf != NULL && historySize != 0)
    {
        historyBufSize = historySize;
        historyBuf[0] = 0;
        historyBuf[1] = 0;
    }
	
	dioMB.init();

	AudioMemory(50);
	// Comment these out if not using the audio adaptor board.
	// This may wait forever if the SDA & SCL pins lack
	// pullup resistors
	sgtl5000_1.enable();
	sgtl5000_1.volume(0.5);

    locEcho = localEcho;
    Params = pParams;
    machName = hostName;
    ParmPtr[0] = NULL;
//	sprintf(currentDir,"/%s/",dioMB.drvIdx[0].name);
    ShowPrompt();
}

bool microBox::AddCommand(const char *cmdName, void (*cmdFunc)(char **param, uint8_t parCnt))
{
    uint8_t idx = 0;

    while((Cmds[idx].cmdFunc != NULL) && (idx < (MAX_CMD_NUM-1)))
    {
        idx++;
    }
    if(idx < (MAX_CMD_NUM-1))
    {
        Cmds[idx].cmdName = cmdName;
        Cmds[idx].cmdFunc = cmdFunc;
        idx++;
        Cmds[idx].cmdFunc = NULL;
        Cmds[idx].cmdName = NULL;
        return true;
    }
    return false;
}

bool microBox::isTimeout(unsigned long *lastTime, unsigned long intervall)
{
    unsigned long m;

    m = millis();
    if(((m - *lastTime) >= intervall) || (*lastTime > m))
    {
        *lastTime = m;
        return true;
    }
    return false;
}

void microBox::ShowPrompt()
{
    Serial.print(F("root@"));
    Serial.print(machName);
    Serial.print(F(":"));
    Serial.print(dioMB.cwd());
    Serial.print(F(">"));
}

uint8_t microBox::ParseCmdParams(char *pParam)
{
    uint8_t idx = 0;

    ParmPtr[idx] = pParam;
    if(pParam != NULL)
    {
        idx++;
        while((pParam = strchr(pParam, ' ')) != NULL)
        {
            pParam[0] = 0;
            pParam++;
            ParmPtr[idx++] = pParam;
        }
    }
    return idx;
}

void microBox::ExecCommand()
{
    bool found = false;
    Serial.println();
    if(bufPos > 0)
    {
        uint8_t i=0;
        uint8_t dstlen;
        uint8_t srclen;
        char *pParam;

        cmdBuf[bufPos] = 0;
        pParam = strchr(cmdBuf, ' ');
        if(pParam != NULL)
        {
            pParam++;
            srclen = pParam - cmdBuf - 1;
        }
        else
            srclen = bufPos;

        AddToHistory(cmdBuf);
        historyCursorPos = -1;

        while(Cmds[i].cmdName != NULL && found == false)
        {
            dstlen = strlen(Cmds[i].cmdName);
            if(dstlen == srclen)
            {
                if(strncmp(cmdBuf, Cmds[i].cmdName, dstlen) == 0)
                {
                    (*Cmds[i].cmdFunc)(ParmPtr, ParseCmdParams(pParam));
                    found = true;
                    bufPos = 0;
                    ShowPrompt();
                }
            }
            i++;
        }
        if(!found)
        {
            bufPos = 0;
            ErrorDir(F("/bin/sh"));
            ShowPrompt();
        }
    }
    else
        ShowPrompt();
}

void microBox::cmdParser()
{
	// Check for volume change.
	if(playSdWav1.isPlaying()) {
		// uncomment these lines if you audio shield
		// has the optional volume pot soldered
		float vol = analogRead(15);
		vol = vol / 1024;
		sgtl5000_1.volume(vol);
	}

    if(watchMode)
    {
        if(Serial.available())
        {
            watchMode = false;
            csvMode = false;
        }
        else
        {
            if(isTimeout(&watchTimeout, 500))
                Cat_int(cmdBuf);

            return;
        }
    }
    while(Serial.available())
    {
        uint8_t ch;
        ch = Serial.read();
        if(ch == TELNET_IAC || stateTelnet != TELNET_STATE_NORMAL)
        {
            handleTelnet(ch);
            continue;
        }

        if(HandleEscSeq(ch))
            continue;

        if(ch == 0x7F || ch == 0x08)
        {
            if(bufPos > 0)
            {
                bufPos--;
                cmdBuf[bufPos] = 0;
                Serial.write(ch);
                Serial.print(F(" \x1B[1D"));
            }
            else
            {
                Serial.print(F("\a"));
            }
        }
        else if(ch == '\t')
        {
            HandleTab();
        }
        else if(ch != '\r' && bufPos < (MAX_CMD_BUF_SIZE-1))
        {
            if(ch != '\n')
            {
                if(locEcho)
                    Serial.write(ch);
                cmdBuf[bufPos++] = ch;
                cmdBuf[bufPos] = 0;
            }
        }
        else
        {
            ExecCommand();
        }
    }
}

bool microBox::HandleEscSeq(unsigned char ch)
{
    bool ret = false;

    if(ch == 27)
    {
        escSeq = ESC_STATE_START;
        ret = true;
    }
    else if(escSeq == ESC_STATE_START)
    {
        if(ch == 0x5B)
        {
            escSeq = ESC_STATE_CODE;
            ret = true;
        }
        else if(ch == 0x4f)
        {
            escSeq = ESC_STATE_CODE;
            ret = true;
        }
		else
            escSeq = ESC_STATE_NONE;
    }
    else if(escSeq == ESC_STATE_CODE)
    {
        if(ch == 0x41) // Cursor Up
        {
            HistoryUp();
        }
        else if(ch == 0x42) // Cursor Down
        {
            HistoryDown();
        }
        else if(ch == 0x43) // Cursor Right
        {
        }
        else if(ch == 0x44) // Cursor Left
        {
        }
        else if(ch == 0x46) // end key
        {
			if(playSdWav1.isPlaying())
				playSdWav1.stop();
		}
        escSeq = ESC_STATE_NONE;
        ret = true;
    }
    return ret;
}

uint8_t microBox::ParCmp(uint8_t idx1, uint8_t idx2, bool cmd)
{
    uint8_t i=0;

    const char *pName1;
    const char *pName2;

    if(cmd)
    {
        pName1 = Cmds[idx1].cmdName;
        pName2 = Cmds[idx2].cmdName;
    }
    else
    {
        pName1 = Params[idx1].paramName;
        pName2 = Params[idx2].paramName;
    }

    while(pName1[i] != 0 && pName2[i] != 0)
    {
        if(pName1[i] != pName2[i])
            return i;
        i++;
    }
    return i;
}

int8_t microBox::GetCmdIdx(char* pCmd, int8_t startIdx)
{
    while(Cmds[startIdx].cmdName != NULL)
    {
        if(strncmp(Cmds[startIdx].cmdName, pCmd, strlen(pCmd)) == 0)
        {
            return startIdx;
        }
        startIdx++;
    }
    return -1;
}

void microBox::HandleTab()
{
    int8_t idx, idx2;
    char *pParam = NULL;
    uint8_t i, len = 0;
    uint8_t parlen, matchlen, inlen;

    for(i=0;i<bufPos;i++)
    {
        if(cmdBuf[i] == ' ')
            pParam = cmdBuf+i;
    }
    if(pParam != NULL)
    {
        pParam++;
        if(*pParam != 0)
        {
            idx = GetParamIdx(pParam, true, 0);
            if(idx >= 0)
            {
                parlen = strlen(Params[idx].paramName);
                matchlen = parlen;
                idx2=idx;
                while((idx2=GetParamIdx(pParam, true, idx2+1))!= -1)
                {
                    matchlen = ParCmp(idx, idx2);
                    if(matchlen < parlen)
                        parlen = matchlen;
                }
                pParam = GetFile(pParam);
                inlen = strlen(pParam);
                if(matchlen > inlen)
                {
                    len = matchlen - inlen;
                    if((bufPos + len) < MAX_CMD_BUF_SIZE)
                    {
                        strncat(cmdBuf, Params[idx].paramName + inlen, len);
                        bufPos += len;
                    }
                    else
                        len = 0;
                }
            }
        }
    }
    else if(bufPos)
    {
        pParam = cmdBuf;

        idx = GetCmdIdx(pParam);
        if(idx >= 0)
        {
            parlen = strlen(Cmds[idx].cmdName);
            matchlen = parlen;
            idx2=idx;
            while((idx2=GetCmdIdx(pParam, idx2+1))!= -1)
            {
                matchlen = ParCmp(idx, idx2, true);
                if(matchlen < parlen)
                    parlen = matchlen;
            }
            inlen = strlen(pParam);
            if(matchlen > inlen)
            {
                len = matchlen - inlen;
                if((bufPos + len) < MAX_CMD_BUF_SIZE)
                {
                    strncat(cmdBuf, Cmds[idx].cmdName + inlen, len);
                    bufPos += len;
                }
                else
                    len = 0;
            }
        }
    }
    if(len > 0)
    {
        Serial.print(pParam + inlen);
    }
}

void microBox::HistoryUp()
{
    if(historyBufSize == 0 || historyWrPos == 0)
        return;

    if(historyCursorPos == -1)
        historyCursorPos = historyWrPos-2;

    while(historyBuf[historyCursorPos] != 0 && historyCursorPos > 0)
    {
        historyCursorPos--;
    }
    if(historyCursorPos > 0)
        historyCursorPos++;

    strcpy(cmdBuf, historyBuf+historyCursorPos);
    HistoryPrintHlpr();
    if(historyCursorPos > 1)
        historyCursorPos -= 2;
}

void microBox::HistoryDown()
{
    int pos;
    if(historyCursorPos != -1 && historyCursorPos != historyWrPos-2)
    {
        pos = historyCursorPos+2;
        pos += strlen(historyBuf+pos) + 1;

        strcpy(cmdBuf, historyBuf+pos);
        HistoryPrintHlpr();
        historyCursorPos = pos - 2;
    }
}

void microBox::HistoryPrintHlpr()
{
    uint8_t i;
    uint8_t len;

    len = strlen(cmdBuf);
    for(i=0;i<bufPos;i++)
        Serial.print('\b');
    Serial.print(cmdBuf);
    if(len<bufPos)
    {
        Serial.print(F("\x1B[K"));
    }
    bufPos = len;
}

void microBox::AddToHistory(char *buf)
{
    uint8_t len;
    int blockStart = 0;

    len = strlen(buf);
    if(historyBufSize > 0)
    {
        if(historyWrPos+len+1 >= historyBufSize)
        {
            while(historyWrPos+len-blockStart >= historyBufSize)
            {
                blockStart += strlen(historyBuf + blockStart) + 1;
            }
            memmove(historyBuf, historyBuf+blockStart, historyWrPos-blockStart);
            historyWrPos -= blockStart;
        }
        strcpy(historyBuf+historyWrPos, buf);
        historyWrPos += len+1;
        historyBuf[historyWrPos] = 0;
    }
}

// 2 telnet methods derived from https://github.com/nekromant/esp8266-frankenstein/blob/master/src/telnet.c
void microBox::sendTelnetOpt(uint8_t option, uint8_t value)
{
    uint8_t tmp[4];
    tmp[0] = TELNET_IAC;
    tmp[1] = option;
    tmp[2] = value;
    tmp[3] = 0;
    Serial.write(tmp, 4);
}

void microBox::handleTelnet(uint8_t ch)
{
    switch (stateTelnet)
    {
    case TELNET_STATE_IAC:
        if(ch == TELNET_IAC)
        {
            stateTelnet = TELNET_STATE_NORMAL;
        }
        else
        {
            switch(ch)
            {
            case TELNET_WILL:
                stateTelnet = TELNET_STATE_WILL;
                break;
            case TELNET_WONT:
                stateTelnet = TELNET_STATE_WONT;
                break;
            case TELNET_DO:
                stateTelnet = TELNET_STATE_DO;
                break;
            case TELNET_DONT:
                stateTelnet = TELNET_STATE_DONT;
                break;
            default:
                stateTelnet = TELNET_STATE_NORMAL;
                break;
            }
        }
        break;
    case TELNET_STATE_WILL:
        sendTelnetOpt(TELNET_DONT, ch);
        stateTelnet = TELNET_STATE_NORMAL;
        break;
    case TELNET_STATE_WONT:
        sendTelnetOpt(TELNET_DONT, ch);
        stateTelnet = TELNET_STATE_NORMAL;
        break;
    case TELNET_STATE_DO:
        if(ch == TELNET_OPTION_ECHO)
        {
            sendTelnetOpt(TELNET_WILL, ch);
            sendTelnetOpt(TELNET_DO, ch);
            locEcho = true;
        }
        else if(ch == TELNET_OPTION_SGA)
            sendTelnetOpt(TELNET_WILL, ch);
        else
            sendTelnetOpt(TELNET_WONT, ch);
        stateTelnet = TELNET_STATE_NORMAL;
        break;
    case TELNET_STATE_DONT:
        sendTelnetOpt(TELNET_WONT, ch);
        stateTelnet = TELNET_STATE_NORMAL;
        break;
    case TELNET_STATE_NORMAL:
        if(ch == TELNET_IAC)
        {
            stateTelnet = TELNET_STATE_IAC;
        }
        break;
    }
}


void microBox::ErrorDir(const __FlashStringHelper *cmd)
{
    Serial.print(cmd);
    Serial.print(F(": File or directory not found: Code "));
    Serial.print(dioMB.error());
    Serial.println("\r\n");
	
}

char *microBox::GetDir(char *pParam, bool useFile)
{
    uint8_t i=0;
    uint8_t len;
    char *tmp;

    dirBuf[0] = 0;
    if(pParam != NULL)
    {
        if(currentDir[1] != 0)
        {
            if(pParam[0] != '/')
            {
                if(!(pParam[0] == '.' && pParam[1] == '.'))
                {
                    return NULL;
                }
                else
                {
                    pParam += 2;
                    if(pParam[0] == 0)
                    {
                        dirBuf[0] = '/';
                        dirBuf[1] = 0;
                    }
                    else if(pParam[0] != '/')
                        return NULL;
                }
            }
        }
        if(pParam[0] == '/')
        {
            if(pParam[1] == 0)
            {
                dirBuf[0] = '/';
                dirBuf[1] = 0;
            }
            pParam++;
        }

        if((tmp=strchr(pParam, '/')) != 0)
        {
            len = tmp-pParam;
        }
        else
            len = strlen(pParam);
        if(len > 0)
        {
            while(pgm_read_byte_near(&dirList[i][0]) != 0)
            {
                if(strncmp_P(pParam, dirList[i], len) == 0)
                {
                    if(strlen_P(dirList[i]) == len)
                    {
                        dirBuf[0] = '/';
                        dirBuf[1] = 0;
                        strcat_P(dirBuf, dirList[i]);
                        return dirBuf;
                    }
                }
                i++;
            }
        }
    }
    if(dirBuf[0] != 0)
        return dirBuf;
    return NULL;
}

char *microBox::GetFile(char *pParam)
{
    char *file;
    char *t;

    file = pParam;
    while((t=strchr(file, '/')) != NULL)
    {
        file = t+1;
    }
    return file;
}

void microBox::ListDrives(char **pParam, uint8_t parCnt)
{
	Serial.printf(F("\r\nFound %d logical drives.\r\n"),dioMB.getVolumeCount());
	dioMB.listAvailableDrives(&Serial);
	return;
}

void microBox::ListDir(char **pParam, uint8_t parCnt, bool listLong)
{
    char *dir;

	dir = *pParam;
	if(dir != NULL) {
		if(!dioMB.lsDir(dir)) {
			ErrorDir(F("ls"));
			return;
		}
	} else {
		if(!dioMB.lsDir((char *)"")) {
			ErrorDir(F("ls"));
			return;
		}
	}
	return;
}

void microBox::ChangeDir(char **pParam, uint8_t parCnt)
{
    char *dir;

	dir = *pParam;
	if(dir != NULL) {
		if(!dioMB.chdir(dir)) {
			ErrorDir(F("cd"));
			return;
		} 
//       sprintf(currentDir, "%s", dioMB.drvIdx[dioMB.getCDN()].fullPath);
		return;
	}
	ErrorDir(F("cd"));
	return;
}

void microBox::PrintParam(uint8_t idx)
{
    if(Params[idx].getFunc != NULL)
        (*Params[idx].getFunc)(Params[idx].id);

    if(Params[idx].parType&PARTYPE_INT)
        Serial.print(*((int*)Params[idx].pParam));
    else if(Params[idx].parType&PARTYPE_DOUBLE)
        Serial.print(*((double*)Params[idx].pParam), 8);
    else
        Serial.print(((char*)Params[idx].pParam));

    if(csvMode)
        Serial.print(F(";"));
    else
        Serial.println();
}

int8_t microBox::GetParamIdx(char* pParam, bool partStr, int8_t startIdx)
{
    int8_t i=startIdx;
    char *dir;
    char *file;

    if(pParam != NULL)
    {
        dir = GetDir(pParam, true);
        if(dir == NULL)
            dir = dioMB.cwd();
        if(dir != NULL)
        {

//            if(strcmp_P(dir, PSTR("/dev")) == 0)
//            {
                file = GetFile(pParam);
                if(file != NULL)
                {
                    while(Params[i].paramName != NULL)
                    {
                        if(partStr)
                        {
                            if(strncmp(Params[i].paramName, file, strlen(file))== 0)
                            {
                                return i;
                            }
                        }
                        else
                        {
                            if(strcmp(Params[i].paramName, file)== 0)
                            {
                                return i;
                            }
                        }
                        i++;
                    }
                }
//            }
        }
    }
    return -1;
}

// Taken from Stream.cpp
double microBox::parseFloat(char *pBuf)
{
    boolean isNegative = false;
    boolean isFraction = false;
    long value = 0;
    unsigned char c;
    double fraction = 1.0;
    uint8_t idx = 0;

    c = pBuf[idx++];
    // ignore non numeric leading characters
    if(c > 127)
        return 0; // zero returned if timeout

    do{
        if(c == '-')
            isNegative = true;
        else if (c == '.')
            isFraction = true;
        else if(c >= '0' && c <= '9')  {      // is c a digit?
            value = value * 10 + c - '0';
            if(isFraction)
                fraction *= 0.1;
        }
        c = pBuf[idx++];
    }
    while( (c >= '0' && c <= '9')  || c == '.');

    if(isNegative)
        value = -value;
    if(isFraction)
        return value * fraction;
    else
        return value;
}

// echo 82.00 > /dev/param
void microBox::Echo(char **pParam, uint8_t parCnt)
{
    uint8_t idx;

    if((parCnt == 3) && (strcmp_P(pParam[1], PSTR(">")) == 0))
    {
        idx = GetParamIdx(pParam[2]);
        if(idx != -1)
        {
            if(Params[idx].parType & PARTYPE_RW)
            {
                if(Params[idx].parType & PARTYPE_INT)
                {
                    int val;

                    val = atoi(pParam[0]);
                    *((int*)Params[idx].pParam) = val;
                }
                else if(Params[idx].parType & PARTYPE_DOUBLE)
                {
                    double val;

                    val = parseFloat(pParam[0]);
                    *((double*)Params[idx].pParam) = val;
                }
                else
                {
                    if(strlen(pParam[0]) < Params[idx].len)
                        strcpy((char*)Params[idx].pParam, pParam[0]);
                }
                if(Params[idx].setFunc != NULL)
                    (*Params[idx].setFunc)(Params[idx].id);
            }
            else
                Serial.println(F("echo: File readonly"));
        }
        else
        {
            ErrorDir(F("echo"));
        }
    }
    else
    {
        for(idx=0;idx<parCnt;idx++)
        {
            Serial.print(pParam[idx]);
            Serial.print(F(" "));
        }
        Serial.println();
    }
}

void microBox::Play(char** pParam, uint8_t parCnt)
{
	char tempPath[256];
	if(pParam[0] == NULL) { // Invalid path spec.
		ErrorDir(F("play"));
		return;
	}	
	strcpy(tempPath, pParam[0]); // Preserve path spec.
	// Check which file system we are using LFS or PFsFile type.
	int fileType = dioMB.getOsType(tempPath);
	if(fileType < 0) {
		ErrorDir(F("play"));
		return;
	}


	// Check which file system we are using LFS or PFsFile type.
#if defined(ARDUINO_TEENSY41)
	if((fileType == FILE_TYPE))  {
		if(!dioMB.lfsExists(tempPath)) {
			ErrorDir(F("play"));
			return;
		}
	}
#endif
	if((fileType == PFSFILE_TYPE))  {
		if(!dioMB.exists(tempPath)) {
			ErrorDir(F("play"));
			return;
		}
	}
	Serial.printf("Playing: %s\r\n",tempPath);
	if(!playSdWav1.play(tempPath)) {
		ErrorDir(F("play"));
		return;
	}
}

uint8_t microBox::Play_int(char* pParam)
{
    return 0;
}

void microBox::Cat(char** pParam, uint8_t parCnt)
{
	char buff[8192]; // Disk IO buffer.
    int br = 0;      // File read count
	char tempPath[256];
	buff[0] = 0;
	// Create an instance of PFsFile.
	PFsFile mscfl; 
	File lfsfl;
	if(pParam[0] == NULL) { // Invalid path spec.
		ErrorDir(F("cat"));
		return;
	}	
	strcpy(tempPath, pParam[0]); // Preserve path spec.
	// Check which file system we are using LFS or PFsFile type.
	int fileType = dioMB.getOsType(tempPath);
	if(fileType < 0) {
		ErrorDir(F("cat"));
		return;
	}

	// Check which file system we are using LFS or PFsFile type.
#if defined(ARDUINO_TEENSY41)
	if((fileType == FILE_TYPE))  {
		if(!dioMB.lfsExists(tempPath)) {
			ErrorDir(F("cat"));
			return;
		}
	}
#endif
	if((fileType == PFSFILE_TYPE))  {
		if(!dioMB.exists(tempPath)) {
			
			ErrorDir(F("cat"));
			return;
		}
	}
#if defined(ARDUINO_TEENSY41)
	// Open the file to list (LFS type).
	if(fileType == FILE_TYPE)  {
		if(!dioMB.lfsOpen(&lfsfl, (char *)tempPath, O_RDONLY)) {
			ErrorDir(F("cat"));
			return;
		}
		for(;;) {
			br = dioMB.lfsRead(&lfsfl, buff, sizeof(buff));
			// If last read size is less than the buffer size, terminate the string
			// at actual size.
			if((char)br < sizeof(buff)) buff[br] = 0;
			Serial.printf("%s",buff);
			if (br <= 0) break; // EOF reached.
		}
		if(!dioMB.lfsClose(&lfsfl)) { // Always returns true with LFS.
			ErrorDir(F("cat"));
			return;
		}
	}
#endif	
	if((fileType == PFSFILE_TYPE))  {
		if(!dioMB.open(&mscfl, (char *)tempPath, O_RDONLY)) {
			ErrorDir(F("cat"));
			return;
		}
		for(;;) {
			br = mscfl.fgets(buff, sizeof(buff));
			if (br <= 0) break; // Error
			Serial.printf("%s",buff);
		}
		if(br < 0) {
			ErrorDir(F("cat"));
			return;
		}
		if(!dioMB.close(&mscfl)) {
			ErrorDir(F("cat"));
			return;
		}
	}
	return;
}

uint8_t microBox::Cat_int(char* pParam)
{
    return 0;
}

void microBox::watch(char** pParam, uint8_t parCnt)
{
    if(parCnt == 2)
    {
        if(strncmp_P(pParam[0], PSTR("cat"), 3) == 0)
        {
//            if(Cat(pParam[1]))
            {
                strcpy(cmdBuf, pParam[1]);
                watchMode = true;
            }
        }
    }
}

void microBox::watchcsv(char** pParam, uint8_t parCnt)
{
    watch(pParam, parCnt);
    if(watchMode)
        csvMode = true;
}

void microBox::ReadWriteParamEE(bool write)
{
    uint8_t i=0;
    uint8_t psize;
    int pos=0;

    while(Params[i].paramName != NULL)
    {
        if(Params[i].parType&PARTYPE_INT)
            psize = sizeof(uint16_t);
        else if(Params[i].parType&PARTYPE_DOUBLE)
            psize = sizeof(double);
        else
            psize = Params[i].len;

        if(write)
            eeprom_write_block(Params[i].pParam, (void*)pos, psize);
        else
            eeprom_read_block(Params[i].pParam, (void*)pos, psize);
        pos += psize;
        i++;
    }
}

void microBox::clear(char** pParam, uint8_t parCnt)
{
	Serial.printf("%c",12);
}

void microBox::help(char** pParam, uint8_t parCnt)
{
	Serial.printf(F("\r\nAvailable Commands:\r\n\r\n"));
	Serial.printf(F("clear  - Clear Screen (VT100 terminal Only)\r\n"));
	Serial.printf(F("ld     - List available logical drives.\r\n"));
	Serial.printf(F("ls     - List files and directories.\r\n"));
	Serial.printf(F("cd     - Change logical drives and directories.\r\n"));
	Serial.printf(F("mkdir  - Make directory.\r\n"));
	Serial.printf(F("rmdir  - Remove directory (must be empty).\r\n"));
	Serial.printf(F("rm     - Remove file.\r\n"));
	Serial.printf(F("rename - Rename file or directory.\r\n"));
	Serial.printf(F("cp     - Copy file (src dest).\r\n"));
	Serial.printf(F("cat    - List file (Ascii only).\r\n"));
	Serial.printf(F("play    - Play a Wav file. Press 'end' key to stop.(VT100 Terminal)\r\n"));
	Serial.printf(F("        - Cannot perform disk operation on same device WAV file \r\n"));
	Serial.printf(F("        - is playing from. It will lock up DiskIOMB!\r\n\r\n"));
	Serial.printf(F("All commands except clear and ld accept an optional drive spec.\r\n"));
	Serial.printf(F("The drive spec can be /volume name/ (forward slashes required)\r\n"));
	Serial.printf(F("or a logical drive number 0:-32: (colon after number required).\r\n"));
	Serial.printf(F("Examples: cp /QPINAND/test.txt 1:test.txt\r\n"));
	Serial.printf(F("          cp test.txt test1.txt\r\n"));
	Serial.printf(F("Both cp and rename require a space between arguments.\r\n"));
	Serial.printf(F("One space is required between command and argument.\r\n"));
	Serial.printf(F("Relative path specs and wilcards are supported.\r\n"));
	Serial.printf(F("Example: ls 16:a/b/../*??*.cpp.\r\n\r\n"));
	
}

void microBox::mkdir(char** pParam, uint8_t parCnt)
{
	char tempPath[256];

	if(pParam[0] == NULL) {
		ErrorDir(F("mkdir"));
		return;
	}	
	strcpy(tempPath, pParam[0]);
	if(!dioMB.mkdir(tempPath)) {
			ErrorDir(F("mkdir"));
			return;
		}
}

void microBox::rmdir(char** pParam, uint8_t parCnt)
{
	char tempPath[256];

	if(pParam[0] == NULL) {
		ErrorDir(F("rmdir"));
		return;
	}	
	strcpy(tempPath, pParam[0]);
	if(!dioMB.rmdir(tempPath)) {
			ErrorDir(F("rmdir"));
			return;
		}
}

void microBox::rm(char** pParam, uint8_t parCnt)
{
	char tempPath[256];

	if(pParam[0] == NULL) {
		ErrorDir(F("rm"));
		return;
	}	
	strcpy(tempPath, pParam[0]);
	if(!dioMB.rm(tempPath)) {
			ErrorDir(F("rm"));
			return;
		}
}

void microBox::rename(char** pParam, uint8_t parCnt)
{
	char tempPath[256];

	if(pParam[0] == NULL) {
		ErrorDir(F("rename"));
		return;
	}	
	strcpy(tempPath, pParam[0]);
	if(!dioMB.rename(pParam[0], pParam[1])) {
			ErrorDir(F("rename"));
			return;
		}
}

void microBox::cp(char** pParam, uint8_t parCnt)
{
    int32_t br = 0, bw = 0;          // File read/write count
	uint32_t bufferSize = 8*1024; // Buffer size. Play with this:)
	uint32_t buffer[bufferSize];  // File copy buffer
	uint32_t cntr = 0;
	uint32_t start = 0, finish = 0;
	uint32_t bytesRW = 0;

	PFsFile src; 
	File lfsSrc;
	PFsFile dest; 
	File lfsDest; 

	if((pParam[0] == NULL) || (pParam[1] == NULL)) {
		ErrorDir(F("cp"));
		return;
	}	
	uint8_t srcType = dioMB.getOsType(pParam[0]);
	uint8_t destType = dioMB.getOsType(pParam[1]);
	// Does source file exist?
	if(srcType == PFSFILE_TYPE)  {
		if(!dioMB.exists(pParam[0])) {
				ErrorDir(F("cp"));
				return;
		}
	}
#if defined(ARDUINO_TEENSY41)
	if(srcType == FILE_TYPE)  {
		if(!dioMB.lfsExists(pParam[0])) {
				ErrorDir(F("cp"));
				return;
		}
	}
#endif
	// Open source file.
	if(srcType == PFSFILE_TYPE)  {
		if(!dioMB.open(&src, (char *)pParam[0], O_RDONLY)) {
			ErrorDir(F("cp"));
			return;
		}
	}
#if defined(ARDUINO_TEENSY41)
	if(srcType == FILE_TYPE)  {
		if(!dioMB.lfsOpen(&lfsSrc, (char *)pParam[0], FILE_READ)) {
			ErrorDir(F("cp"));
			return;
		}
	}
#endif
	// Open destination file.
	if(destType == PFSFILE_TYPE)  {
		if(!dioMB.open(&dest, (char *)pParam[1], O_WRITE | O_CREAT | O_TRUNC)) {
			ErrorDir(F("cp"));
			return;
		}
	}
#if defined(ARDUINO_TEENSY41)
	if(destType == FILE_TYPE)  {
		if(!dioMB.lfsOpen(&lfsDest, (char *)pParam[1], FILE_WRITE_BEGIN)) {
			ErrorDir(F("cp"));
			return;
		}
	}
#endif
	/* Copy source to destination */
	start = micros();
	for (;;) {
#if 1
		cntr++;
		if(!(cntr % 10)) Serial.printf("*");
		if(!(cntr % 640)) Serial.printf("\n");

#endif
		// Read source file.
		if(srcType == PFSFILE_TYPE)  {
			br = dioMB.read(&src, buffer, sizeof(buffer));  // Read buffer size of source file.
			if (br <= 0) break; // Error or EOF
		}
#if defined(ARDUINO_TEENSY41)
		if(srcType == FILE_TYPE)  {
			br = dioMB.lfsRead(&lfsSrc, buffer, sizeof(buffer));  // Read buffer size of source file.
			if (br <= 0) break; // Error or EOF
		}
#endif
		// Write destination file.
		if(destType == PFSFILE_TYPE)  {
			bw = dioMB.write(&dest, buffer, br); // Write br bytes to the destination file.
			if (bw < br) {
				break; // Error or disk is full
			}
		}
#if defined(ARDUINO_TEENSY41)
		if(destType == FILE_TYPE)  {
			bw = dioMB.lfsWrite(&lfsDest, buffer, br); // Write br bytes to the destination file.
			if (bw < br) {
				break; // Error or disk is full
			}
		}
#endif
		bytesRW += (uint32_t)bw;
	}
	// Flush destination file.
	if(destType == PFSFILE_TYPE)  {
		dioMB.fflush(&dest); // Flush write buffer.
	} else {
		dioMB.fflush(&lfsDest); // Flush write buffer.
	}
	// Close open files
	if(srcType == PFSFILE_TYPE)  {
		dioMB.close(&src);
	} else {
#if defined(ARDUINO_TEENSY41)
		dioMB.lfsClose(&lfsSrc);
#endif
	}
	if(destType == PFSFILE_TYPE)  {
		dioMB.close(&dest);
	} else {
#if defined(ARDUINO_TEENSY41)
		dioMB.lfsClose(&lfsDest);
#endif
	}
	// Proccess posible errors.
	if(br < 0) {
		dioMB.setError(READ_ERROR);
		ErrorDir(F("cp"));
		return;
	} else if(bw < br) {
		dioMB.setError(WRITE_ERROR); // Can also be disk full error.
		ErrorDir(F("cp"));
		return;
	}
	finish = (micros() - start); // Get total copy time.
	float MegaBytes = (bytesRW*1.0f)/(1.0f*finish);
#if 1
	Serial.printf("\nCopied %u bytes in %f seconds. Speed: %f MB/s\n",
					bytesRW,(1.0*finish)/1000000.0,MegaBytes);
#endif
	return;
}

void microBox::ListDirCB(char **pParam, uint8_t parCnt)
{
    microbox.ListDir(pParam, parCnt);
}

void microBox::ListDrivesCB(char **pParam, uint8_t parCnt)
{
    microbox.ListDrives(pParam, parCnt);
}

void microBox::ChangeDirCB(char **pParam, uint8_t parCnt)
{
    microbox.ChangeDir(pParam, parCnt);
}

void microBox::EchoCB(char **pParam, uint8_t parCnt)
{
    microbox.Echo(pParam, parCnt);
}

void microBox::CatCB(char** pParam, uint8_t parCnt)
{
    microbox.Cat(pParam, parCnt);
}

void microBox::PlayCB(char** pParam, uint8_t parCnt)
{
    microbox.Play(pParam, parCnt);
}

void microBox::watchCB(char** pParam, uint8_t parCnt)
{
    microbox.watch(pParam, parCnt);
}

void microBox::watchcsvCB(char** pParam, uint8_t parCnt)
{
    microbox.watchcsv(pParam, parCnt);
}

void microBox::LoadParCB(char **pParam, uint8_t parCnt)
{
    microbox.ReadWriteParamEE(false);
}

void microBox::SaveParCB(char **pParam, uint8_t parCnt)
{
    microbox.ReadWriteParamEE(true);
}

void microBox::clearCB(char** pParam, uint8_t parCnt)
{
    microbox.clear(pParam, parCnt);
}

void microBox::helpCB(char** pParam, uint8_t parCnt)
{
    microbox.help(pParam, parCnt);
}

void microBox::mkdirCB(char** pParam, uint8_t parCnt)
{
    microbox.mkdir(pParam, parCnt);
}

void microBox::rmdirCB(char** pParam, uint8_t parCnt)
{
    microbox.rmdir(pParam, parCnt);
}

void microBox::rmCB(char** pParam, uint8_t parCnt)
{
    microbox.rm(pParam, parCnt);
}

void microBox::renameCB(char** pParam, uint8_t parCnt)
{
    microbox.rename(pParam, parCnt);
}

void microBox::cpCB(char** pParam, uint8_t parCnt)
{
    microbox.cp(pParam, parCnt);
}
