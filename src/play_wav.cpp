/* Audio Library for Teensy
   Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com

   Development of this audio library was funded by PJRC.COM, LLC by sales of
   Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
   open source software by purchasing Teensy or other PJRC products.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice, development funding notice, and this permission
   notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
*/

// (c) Frank Bösing, 07/2021

#include "play_wav.h"
#include <spi_interrupt.h>


#define STATE_STOP    0
#define STATE_PAUSED  1
#define STATE_PLAY    2

extern diskIO dioMB;  // One instance of diskIO.

//#define HANDLE_SPI    1 //TODO...

#if defined(KINETISL)
static const uint8_t _AudioPlayWavInstances = 1;
//static const int8_t _AudioPlayWavInstance = 0;
static const uint8_t _sz_mem_additional = 1;
#if AUDIO_BLOCK_SAMPLES < 128
//#warning WavePlay: AUDIO_BLOCK_SAMPLES is less than 128. Expect noise.
#endif
#else
static uint8_t _AudioPlayWavInstances = 0;
static int8_t _AudioPlayWavInstance = -1;
static uint8_t _sz_mem_additional = 1;
#endif


FLASHMEM
void AudioPlayWav::begin(void)
{
    state = STATE_STOP;
#if !defined(KINETISL)
    my_instance = _AudioPlayWavInstances;
    ++_AudioPlayWavInstances;
#endif
}

bool AudioPlayWav::play(PFsFile file)
{
    return play(file, false);
}

bool AudioPlayWav::play(File file)
{
    return play(file, false);
}

bool AudioPlayWav::play(PFsFile file, const bool paused)
{
    stop();

    wavefile = file;
    startUsingSPI();

    if (!readHeader( paused ? STATE_PAUSED : STATE_PLAY ))
    {
        stop();
        return false;
    }
    return true;
}

bool AudioPlayWav::play(File file, const bool paused)
{
    stop();

    wavfile = file;
    startUsingSPI();

    if (!readHeader( paused ? STATE_PAUSED : STATE_PLAY ))
    {
        stop();
        return false;
    }
    return true;
}

bool AudioPlayWav::play(const char *filename)
{
    return play(filename, false);
}

bool AudioPlayWav::play(const char *filename, const bool paused)
{
    stop();
    startUsingSPI();
    bool irq = stopInt();
	// Check which file system we are using LFS or PFsFile type.
	fileType = dioMB.getOsType((char *)filename);

	if(fileType < 0) {
		Serial.printf("play() Failed: code - %d\r\n",dioMB.error());
		return false;
	}
#if defined(ARDUINO_TEENSY41)
	// Open the file to play (LFS File type).
	if(fileType == FILE_TYPE)  {
		if(!dioMB.lfsOpen(&wavfile, (char *)filename, O_RDONLY)) {
			return false;
		}
	}
#endif
	// Open the file to play (PFsFile type).
	if(fileType == PFSFILE_TYPE)  {
		if(!dioMB.open(&wavefile, (char *)filename, O_RDONLY)) {
			return false;
		}
	}
    startInt(irq);

    if (!readHeader(paused ? STATE_PAUSED : STATE_PLAY))
    {
        stop();
        return false;
    }
    return true;
}

void AudioPlayWav::stop(void)
{

    state = STATE_STOP;

    bool irq = stopInt();
#if defined(ARDUINO_TEENSY41)
	if(fileType == FILE_TYPE)  {
		if (wavfile) dioMB.lfsClose(&wavfile);
	}
#endif
	if(fileType == PFSFILE_TYPE)  {
		if (wavefile) dioMB.close(&wavefile);
	}
    startInt(irq);

    stopUsingSPI();

    if (buffer)
    {
        free(buffer);
        buffer = nullptr;
        sz_mem = 0;
    }
}


/*
  00000000  52494646 66EA6903 57415645 666D7420  RIFFf.i.WAVEfmt
  00000010  10000000 01000200 44AC0000 10B10200  ........D.......
  00000020  04001000 4C495354 3A000000 494E464F  ....LIST:...INFO
  00000030  494E414D 14000000 49205761 6E742054  INAM....I Want T
  00000040  6F20436F 6D65204F 76657200 49415254  o Come Over.IART
  00000050  12000000 4D656C69 73736120 45746865  ....Melissa Ethe
  00000060  72696467 65006461 746100EA 69030100  ridge. a..i...
  00000070  FEFF0300 FCFF0400 FDFF0200 0000FEFF  ................
  00000080  0300FDFF 0200FFFF 00000100 FEFF0300  ................
  00000090  FDFF0300 FDFF0200 FFFF0100 0000FFFF  ................
*/

static const uint32_t cRIFF = 0x46464952; //'RIFF'
static const uint32_t cWAVE = 0x45564157; //'WAVE'
static const uint32_t cFMT  = 0x20746D66; //'fmt '
static const uint32_t cDATA = 0x61746164; //'data'

typedef struct {
  unsigned long id;
  unsigned long len;
  unsigned long riffType;
} tFileHeader;

// https://docs.microsoft.com/de-de/windows/win32/api/mmreg/ns-mmreg-waveformat
typedef struct
{
  //unsigned long  chunkID;
  //unsigned long  chunkSize;
  unsigned short  wFormatTag;
  unsigned short  nChannels;
  unsigned long   nSamplesPerSec;
  unsigned long   nAvgBytesPerSec;
  unsigned short  nBlockAlign;
} __attribute__ ((__packed__)) tFmtHeader;

// https://docs.microsoft.com/en-us/previous-versions/dd757713(v=vs.85)
typedef struct
{
  //unsigned long  chunkID;
  //unsigned long  chunkSize;
  unsigned short wFormatTag;
  unsigned short wChannels;
  unsigned long  dwSamplesPerSec;
  unsigned long  dwAvgBytesPerSec;
  unsigned short wBlockAlign;
  unsigned short wBitsPerSample;
	unsigned short cbSize;
} __attribute__ ((__packed__)) tFmtHeaderEx;

// https://docs.microsoft.com/de-de/windows/win32/api/mmreg/ns-mmreg-waveformatextensible
typedef struct
{
	//unsigned long  chunkID;
  //unsigned long  chunkSize;
	//tFmtHeaderex fmtHeader;
  union {
    unsigned short wValidBitsPerSample;
    unsigned short wSamplesPerBlock;
    unsigned short wReserved;
  } Samples;
  unsigned long dwChannelMask;
  //GUID         SubFormat;
} __attribute__ ((__packed__)) tFmtHeaderExtensible;

typedef struct {
  unsigned long chunkID;
  unsigned long chunkSize;
} tDataHeader;


bool AudioPlayWav::readHeader(int newState)
{

    size_t position, rd;
    tFileHeader fileHeader;
    tDataHeader dataHeader;
    bool irq, fmtok;

    if (buffer) {
        free(buffer);
        buffer = nullptr;
    }

    sz_mem = buffer_rd = total_length = data_length = 0;
    channelmask = sample_rate = channels = bytes = 0;

    last_err = APW_ERR_FILE;
#if defined(ARDUINO_TEENSY41)
	if(fileType == FILE_TYPE)  {
		if (!wavfile) return false;
	}
#endif
	if(fileType == PFSFILE_TYPE)  {
		if (!wavefile) return false;
	}
    irq = stopInt();
#if defined(ARDUINO_TEENSY41)
	if(fileType == FILE_TYPE)  {
		rd = dioMB.lfsRead(&wavfile, &fileHeader, sizeof(fileHeader));
	}
#endif
	if(fileType == PFSFILE_TYPE)  {
		rd = dioMB.read(&wavefile, &fileHeader, sizeof(fileHeader));
	}
    startInt(irq);

    if (rd < sizeof(fileHeader)) return false;

    last_err = APW_ERR_FORMAT;
	if ( fileHeader.id != cRIFF || fileHeader.riffType != cWAVE ) return false;

	position = sizeof(fileHeader);
    fmtok = false;

    do {
        irq = stopInt();
#if defined(ARDUINO_TEENSY41)
	if(fileType == FILE_TYPE)  {
        dioMB.lfsLseek(&wavfile, position, SEEK_SET);
        rd = dioMB.lfsRead(&wavfile, &dataHeader, sizeof(dataHeader));
	}
#endif
	if(fileType == PFSFILE_TYPE)  {
        dioMB.lseek(&wavefile, position, SEEK_SET);
        rd = dioMB.read(&wavefile, &dataHeader, sizeof(dataHeader));
	}
        startInt(irq);
        if (rd < sizeof(dataHeader)) return false;

        if (dataHeader.chunkID == cFMT) {
            tFmtHeaderEx fmtHeader;
            memset((void*)&fmtHeader, 0, sizeof(tFmtHeaderEx));

			//Serial.println(dataHeader.chunkSize);
            irq = stopInt();
            if (dataHeader.chunkSize < 16) {
#if defined(ARDUINO_TEENSY41)
				if(fileType == FILE_TYPE)  {
					dioMB.lfsRead(&wavfile, &fmtHeader, sizeof(tFmtHeader));
				}
#endif
				if(fileType == PFSFILE_TYPE)  {
					dioMB.read(&wavefile, &fmtHeader, sizeof(tFmtHeader));
				}
                bytes = 1;
            } else if (dataHeader.chunkSize == 16) {
#if defined(ARDUINO_TEENSY41)
				if(fileType == FILE_TYPE) {
					dioMB.lfsRead(&wavfile, &fmtHeader, sizeof(tFmtHeaderEx));
				}
#endif
				if(fileType == PFSFILE_TYPE) {
					dioMB.read(&wavefile, &fmtHeader, sizeof(tFmtHeaderEx));
				}
                bytes = fmtHeader.wBitsPerSample / 8;
            } else {
                tFmtHeaderExtensible fmtHeaderExtensible;
#if defined(ARDUINO_TEENSY41)
				if(fileType == FILE_TYPE)  {
					dioMB.lfsRead(&wavfile, &fmtHeader, sizeof(tFmtHeaderEx));
				}
#endif
				if(fileType == PFSFILE_TYPE)  {
					dioMB.read(&wavefile, &fmtHeader, sizeof(tFmtHeaderEx));
				}
                bytes = fmtHeader.wBitsPerSample / 8;
                memset((void*)&fmtHeaderExtensible, 0, sizeof(fmtHeaderExtensible));
#if defined(ARDUINO_TEENSY41)
				if(fileType == FILE_TYPE)  {
					dioMB.lfsRead(&wavfile, &fmtHeaderExtensible, sizeof(fmtHeaderExtensible));
				}
#endif
				if(fileType == PFSFILE_TYPE)  {
					dioMB.read(&wavefile, &fmtHeaderExtensible, sizeof(fmtHeaderExtensible));
				}
                channelmask = fmtHeaderExtensible.dwChannelMask;
				//Serial.printf("channel mask: 0x%x\n", channelmask);
            }
            startInt(irq);

			//Serial.printf("Format:%d Bits:%d\n", fmtHeader.wFormatTag, fmtHeader.wBitsPerSample);
            sample_rate = fmtHeader.dwSamplesPerSec;
            channels = fmtHeader.wChannels;
            if (bytes == 0 || bytes > 2) return false;
            if (channels == 0 || channels > _AudioPlayWav_MaxChannels) return false;
            if (fmtHeader.wFormatTag != 1 && fmtHeader.wFormatTag != 65534) return false;
            fmtok = true;
        }
        else if (dataHeader.chunkID == cDATA) break;

        position += sizeof(dataHeader) + dataHeader.chunkSize;
    } while(true);

    if (fmtok != true) return false;

    sz_frame = AUDIO_BLOCK_SAMPLES * channels;
    total_length = dataHeader.chunkSize;
	data_length = dataHeader.chunkSize / (sz_frame * bytes);

    //calculate the needed buffer memory:
    sz_mem = _AudioPlayWavInstances * sz_frame * bytes;
    sz_mem *= _sz_mem_additional;

    //allocate:
    buffer =  (int8_t*) malloc( sz_mem );
    if (buffer == nullptr) {
        sz_mem = 0;
		last_err = APW_ERR_OUT_OF_MEMORY;
		return false;
	}
    last_err = APW_ERR_OK;

#if !defined(KINETISL)
    if (_AudioPlayWavInstances > 1) {
        //For sync start, and to start immedeately:
        //The next instanceID is random. If it is not our instance, we have to fill the buffer with enough data.

        irq = stopInt();

        int inst = _AudioPlayWavInstance + 1;
        if (inst >= _AudioPlayWavInstances) inst = 0;

        //inst is now the id of the next running instance.
        if (inst != my_instance) {
            buffer_rd = sz_mem;
            do {
                buffer_rd -= sz_frame * bytes;
                if (++inst >= _AudioPlayWavInstances) inst = 0;
            } while (inst != my_instance);
#if defined(ARDUINO_TEENSY41)
			if(fileType == FILE_TYPE)  {
				dioMB.lfsRead(&wavfile, &buffer[buffer_rd], sz_mem - buffer_rd);
			}
#endif
			if(fileType == PFSFILE_TYPE)  {
				dioMB.read(&wavefile, &buffer[buffer_rd], sz_mem - buffer_rd);
			}
        }

        state = newState;

        startInt(irq);

    } else
       state = newState;
#else
    state = newState; //this *must* be the last instruction.
#endif
    return true;
}


__attribute__((hot))
void  AudioPlayWav::update(void)
{
size_t rd = 0;

#if defined(KINETISL)
    if ( state != STATE_PLAY ) return;
    if ( buffer_rd == 0)
#else
	if (++_AudioPlayWavInstance >= _AudioPlayWavInstances)
        _AudioPlayWavInstance = 0;

    if ( state != STATE_PLAY ) return;

    if (/*_AudioPlayWavInstance == my_instance &&*/ buffer_rd == 0 )
#endif
    {
#if defined(ARDUINO_TEENSY41)
		if(fileType == FILE_TYPE)  {
			rd = dioMB.lfsRead(&wavfile, buffer, sz_mem);
		}
#endif
		if(fileType == PFSFILE_TYPE)  {
			rd = dioMB.read(&wavefile, buffer, sz_mem);
		}

        //when EOF, fill remaining space:
        if ( rd < sz_mem ) {
            memset(&buffer[rd], (bytes == 1) ? 128:0 , sz_mem - rd);
        }
    }

    unsigned int chan;

	// allocate the audio blocks to transmit 
    audio_block_t *queue[channels];
    chan = 0;
    do {
		queue[chan] = AudioStream::allocate();
		if ( (queue[chan] == nullptr) ) {
			for (unsigned int i = 0; i != chan; ++i) AudioStream::release(queue[i]);
			last_err = APW_ERR_NO_AUDIOBLOCKS;
			//Serial.println("Waveplayer stopped: not enough AudioMemory().");
			stop();
            return;
		}
	} while (++chan < channels);


	// copy the samples to the audio blocks:
	if (bytes == 2)
    {

		// 16 bits:
        int16_t *p = (int16_t*) &buffer[buffer_rd];
        buffer_rd += sz_frame * 2;
        if (buffer_rd >= sz_mem ) buffer_rd = 0;

        __builtin_prefetch(p);
        size_t i = 0;
        do {
            chan = 0;
            do {
                queue[chan]->data[i] = *p++;
            } while (++chan < channels);
        } while (++i < AUDIO_BLOCK_SAMPLES);


	} else
    {

		// 8 bits:
		int8_t *p = &buffer[buffer_rd];
		buffer_rd += sz_frame;
        if (buffer_rd >= sz_mem ) buffer_rd = 0;

        __builtin_prefetch(p);
		size_t i = 0;
		do {
			chan = 0;
			do {
				queue[chan]->data[i] = ( *p++ - 128 ) << 8; //8 bit fmt is unsigned
			} while (++chan < channels);
        } while (++i < AUDIO_BLOCK_SAMPLES);

	}


	// transmit them:
    chan = 0;
    do
    {
		AudioStream::transmit(queue[chan], chan);
		AudioStream::release(queue[chan]);
	} while (++chan < channels);

    //Serial.printf("%d\n",data_length);
    --data_length;
	if (data_length <= 0) stop();
}

bool AudioPlayWav::stopInt()
{
    if ( NVIC_IS_ENABLED(IRQ_SOFTWARE) )
    {
        NVIC_DISABLE_IRQ(IRQ_SOFTWARE);
        return true;
    }
    return false;
}

void AudioPlayWav::startInt(bool enabled)
{
    if (enabled)
        NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
}

void AudioPlayWav::startUsingSPI(void)
{
//TODO... https://forum.pjrc.com/threads/67989-Teensyduino-1-55-Beta-1?p=287023&viewfull=1#post287023
//this must be smarter.
#if defined(HANDLE_SPI)
#if defined(HAS_KINETIS_SDHC)
   if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStartUsingSPI();
#else
    AudioStartUsingSPI();
#endif
#endif
}

void AudioPlayWav::stopUsingSPI(void)
{ //TODO...
#if defined(HANDLE_SPI)
#if defined(HAS_KINETIS_SDHC)
    if (!(SIM_SCGC3 & SIM_SCGC3_SDHC)) AudioStopUsingSPI();
#else
    AudioStopUsingSPI();
#endif
#endif
}

bool AudioPlayWav::addMemoryForRead(__attribute__ ((unused)) size_t mult)
{
#if !defined(KINETISL)
    if (mult < 1) mult = 1;
	_sz_mem_additional = mult;
#endif
	return true;
}

bool AudioPlayWav::isPlaying(void)
{
    return state == STATE_PLAY;
}

void AudioPlayWav::togglePlayPause(void)
{
    pause(state == STATE_PLAY);
}

void AudioPlayWav::pause(const bool pause)
{
    if (state == STATE_STOP) return;
    bool irq = stopInt();
    if (pause)
    {
        state = STATE_PAUSED;
        stopUsingSPI();
    } else {
        startUsingSPI();
        state = STATE_PLAY;
    }
    startInt(irq);
}

bool AudioPlayWav::isPaused(void)
{
    return (state == STATE_PAUSED);
}


bool AudioPlayWav::isStopped(void)
{
    return (state == STATE_STOP);
}

#define _positionMillis() ((AUDIO_BLOCK_SAMPLES * 1000.0f / AUDIO_SAMPLE_RATE_EXACT) * (total_length / (bytes * sz_frame) - data_length))

#if !defined(KINETISL)
__attribute__( ( always_inline ) ) static inline uint32_t __ldrexw(volatile uint32_t *addr)
{
   uint32_t result;
   asm volatile ("ldrex %0, [%1]" : "=r" (result) : "r" (addr) );
   return(result);
}

__attribute__( ( always_inline ) ) static inline uint32_t __strexw(uint32_t value, volatile uint32_t *addr)
{
   uint32_t result;
   asm volatile ("strex %0, %2, [%1]" : "=&r" (result) : "r" (addr), "r" (value) );
   return(result);
}

uint32_t AudioPlayWav::positionMillis(void)
{
    uint32_t safe_read, ret;
    //use an interrupt detector to make sure all vars are consistent.
    //strex will fail if an interrupt occured. An other way would be to block audio interrupts.
    do
    {
        __ldrexw(&safe_read);
        ret = _positionMillis();
	} while ( __strexw(1, &safe_read));

    return ret;
}
#else
uint32_t AudioPlayWav::positionMillis(void)
{
    bool irq;
    uint32_t ret;
    irq = stopInt();
    ret = _positionMillis();
    startInt(irq);
    return ret;
}
#endif

uint32_t AudioPlayWav::lengthMillis(void)
{
    return total_length * (1000.0f / AUDIO_SAMPLE_RATE_EXACT);
}

uint32_t AudioPlayWav::numBits(void)
{
    return bytes * 8;
};

uint32_t AudioPlayWav::numChannels(void)
{
    return channels;
}

uint32_t AudioPlayWav::sampleRate(void)
{
    return sample_rate;
}

uint32_t AudioPlayWav::channelMask(void)
{
	return channelmask;
}

uint8_t AudioPlayWav::lastErr(void)
{
	return last_err;
}

size_t AudioPlayWav::memUsed(void)
{
	return sz_mem;
}

uint8_t AudioPlayWav::instanceID(void)
{
	return my_instance;
}
