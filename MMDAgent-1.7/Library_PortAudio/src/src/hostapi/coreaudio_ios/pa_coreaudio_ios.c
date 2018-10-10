/* ----------------------------------------------------------------- */
/*           The Toolkit for Building Voice Interaction Systems      */
/*           "MMDAgent" developed by MMDAgent Project Team           */
/*           http://www.mmdagent.jp/                                 */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2009-2016  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the MMDAgent project team nor the names of  */
/*   its contributors may be used to endorse or promote products     */
/*   derived from this software without specific prior written       */
/*   permission.                                                     */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

/* header */
#include <AudioToolBox/AudioQueue.h>
#include "portaudio.h"
#include "unistd.h"

/* audio output setting (48kHz/16bit) */
#define PLAYER_BUFFERNUMSAMPLES 3200
#define PLAYER_BUFFER_SIZE 3
#define PLAYER_WAITMS 10

/* audio input (16kHz/16bit) */
#define RECORDER_BUFFERNUMSAMPLES 800

/* AudioQueue Buffer */
#define NUM_BUFFERS 5

typedef struct
{
   AudioStreamBasicDescription  dataFormat;
   AudioQueueRef                queue;
   AudioQueueBufferRef          buffers[NUM_BUFFERS];
   SInt64                       currentPacket;
   bool                         recording;
} RecordData;

typedef struct
{
   AudioStreamBasicDescription  dataFormat;
   AudioQueueRef                queue;
   AudioQueueBufferRef          buffers[NUM_BUFFERS];
   SInt64                       currentPacket;
   bool                         playing;
} PlayData;


RecordData recordData;
PlayData playData;

/* audio output */
static SInt16 playerBuffer[PLAYER_BUFFER_SIZE][PLAYER_BUFFERNUMSAMPLES];
static unsigned long long outputWriteCount = 0;
static unsigned long long outputReadCount = 0;
static int outputWriteBufferIndex = 0;
static int outputReadBufferIndex = 0;
static int numQueuedPlayerBuffer = 0;

/* audio input */
static PaStreamCallback *sendRecordingBuffer = NULL;

/* Port Audio */
static PaHostApiInfo paHostApiInfo;
static PaDeviceInfo deviceInfo;
static PaStreamInfo paStreamInfo;

/* functions */
PaError Pa_StartStreamRecording();
PaError Pa_StartStreamPlayback();
void setInputParameters(RecordData* data, double sampleRate);
void setOutputParameters(PlayData* data, double sampleRate);

/* input callback */
void AudioInputCallback(void * inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer, const AudioTimeStamp * inStartTime, UInt32 inNumberPacketDescriptions, const AudioStreamPacketDescription * inPacketDescs)
{
   RecordData *recordData = (RecordData*)inUserData;
   int size = inBuffer->mAudioDataByteSize / sizeof(SInt16);
   SInt16 *buf = (SInt16 *)inBuffer->mAudioData;
   PaStreamCallbackFlags flag;

   sendRecordingBuffer(inBuffer->mAudioData, NULL, size, NULL, flag, NULL);

   recordData->currentPacket += inNumberPacketDescriptions;
   AudioQueueEnqueueBuffer(recordData->queue, inBuffer, 0, NULL);
}


/* output callback */
void AudioOutputCallback(void * inUserData, AudioQueueRef outAQ, AudioQueueBufferRef outBuffer)
{
   PlayData *playData = (PlayData*)inUserData;

   OSStatus status;
   AudioStreamPacketDescription *packetDescs;
   UInt32 numPackets = PLAYER_BUFFERNUMSAMPLES;

   if(outputReadCount < outputWriteCount) {
      memcpy(outBuffer->mAudioData, playerBuffer[outputReadBufferIndex], numPackets * sizeof(SInt16));
      outputReadBufferIndex = (outputReadBufferIndex + 1) % PLAYER_BUFFER_SIZE;
      outputReadCount++;
      numQueuedPlayerBuffer--;
   } else {
     memset(outBuffer->mAudioData, 0, sizeof(SInt16) * numPackets);
   }

   outBuffer->mAudioDataByteSize = PLAYER_BUFFERNUMSAMPLES * sizeof(SInt16);

   if (numPackets){
      status = AudioQueueEnqueueBuffer(playData->queue, outBuffer, 0, packetDescs);
      playData->currentPacket += numPackets;
   }
}


void setInputParameters(RecordData* data, double sampleRate)
{
   AudioStreamBasicDescription *format = &data->dataFormat;

   format->mSampleRate = sampleRate;
   format->mFormatID = kAudioFormatLinearPCM;
   format->mFramesPerPacket = 1;
   format->mChannelsPerFrame = 1;
   format->mBytesPerFrame = 2;
   format->mBytesPerPacket = 2;
   format->mBitsPerChannel = 16;
   format->mReserved = 0;
   format->mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
}


void setOutputParameters(PlayData* data, double sampleRate)
{
   AudioStreamBasicDescription *format = &data->dataFormat;
   
   format->mSampleRate = sampleRate;
   format->mFormatID = kAudioFormatLinearPCM;
   format->mFramesPerPacket = 1;
   format->mChannelsPerFrame = 1;
   format->mBytesPerFrame = 2;
   format->mBytesPerPacket = 2;
   format->mBitsPerChannel = 16;
   format->mReserved = 0;
   format->mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
}


PaError Pa_Initialize()
{
   /* initialize dummy information for PortAudio */
   memset(&paHostApiInfo, 0, sizeof(PaHostApiInfo));
   memset(&deviceInfo, 0, sizeof(PaDeviceInfo));
   memset(&paStreamInfo, 0, sizeof(PaStreamInfo));

   return paNoError;
}

PaError Pa_Terminate()
{
   PaError result1 = Pa_CloseStream((PaStream *)'\x01');
   PaError result2 = Pa_CloseStream((PaStream *)'\x02');

   if (result1 != paNoError || result2 != paNoError)
      return paInternalError;

   return paNoError;
}

PaError Pa_OpenStream(PaStream** stream, const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData)
{
   if (outputParameters != NULL) {
      setOutputParameters(&playData, sampleRate);

      /* set dummy address */
      *stream = (PaStream *)'\x01';
   }

   if (inputParameters != NULL) {
      sendRecordingBuffer = streamCallback;
      setInputParameters(&recordData, sampleRate);

      /* set dummy address */
      *stream = (PaStream *)'\x02';
   }

   if (inputParameters == NULL && outputParameters == NULL)
      return paInternalError;

   return paNoError;
}

PaError Pa_WriteStream(PaStream *stream, const void *buffer, unsigned long frames)
{
   if(stream == (PaStream *)'\x01') {
      numQueuedPlayerBuffer++;
      memcpy(playerBuffer[outputWriteBufferIndex], buffer, frames * sizeof(SInt16));
      outputWriteCount++;
      outputWriteBufferIndex = (outputWriteBufferIndex + 1) % PLAYER_BUFFER_SIZE;

      while (numQueuedPlayerBuffer >= 2)
         usleep(PLAYER_WAITMS * 1000);
   }

   return paNoError;
}

void Pa_Sleep(long msec)
{
   usleep((unsigned int) (msec * 1000));
}

PaError Pa_CloseStream(PaStream *stream)
{
   Pa_StopStream(stream);

   /* output */
   if(stream == (PaStream *)'\x01') {
      playData.playing = false;
      playData.currentPacket = 0;
      playData.queue = NULL;
      outputWriteCount = 0;
      outputReadCount = 0;
      outputWriteBufferIndex = 0;
      outputReadBufferIndex = 0;
   }

   /* input */
   if(stream == (PaStream *)'\x02') {
      recordData.recording = false;
      recordData.currentPacket = 0;
      recordData.queue = NULL;
      sendRecordingBuffer = NULL;
   }

   return paNoError;
}

PaError Pa_StartStreamRecording()
{
   OSStatus status;
   UInt32 nSamplePerCallback = RECORDER_BUFFERNUMSAMPLES;
   UInt32 buffSize = nSamplePerCallback * sizeof(SInt16);

   status = AudioQueueNewInput(&recordData.dataFormat, AudioInputCallback, &recordData, CFRunLoopGetMain(), kCFRunLoopCommonModes, 0, &recordData.queue);

   if (status != 0)
      return paInternalError;

   for (int i = 0; i < NUM_BUFFERS; i++) {
      AudioQueueAllocateBuffer(recordData.queue, buffSize, &recordData.buffers[i]);
      AudioQueueEnqueueBuffer(recordData.queue, recordData.buffers[i], 0, NULL);
   }

   status = AudioQueueStart(recordData.queue, NULL);
   if (status != 0)
      return paInternalError;

   recordData.recording = true;

   return paNoError;
}

PaError Pa_StartStreamPlayback()
{
   OSStatus status;
   UInt32 nSamplePerCallback = PLAYER_BUFFERNUMSAMPLES;
   UInt32 buffSize = nSamplePerCallback * sizeof(SInt16);

   status = AudioQueueNewOutput(&playData.dataFormat,AudioOutputCallback, &playData, CFRunLoopGetMain(), kCFRunLoopCommonModes, 0, &playData.queue);

   if (status != 0)
      return paInternalError;

   for (int i = 0; i < NUM_BUFFERS; i++) {
      AudioQueueAllocateBuffer(playData.queue, buffSize, &playData.buffers[i]);
      AudioOutputCallback(&playData, playData.queue, playData.buffers[i]);
   }

   status = AudioQueueStart(playData.queue, NULL);
   if (status != 0)
      return paInternalError;

   playData.playing = true;
   return paNoError;
}


PaError Pa_StartStream(PaStream *stream)
{
   PaError r;

   if(stream == (PaStream *)'\x01') {
      r = Pa_StartStreamPlayback();
   } else if(stream == (PaStream *)'\x02') {
      r = Pa_StartStreamRecording();
   } else {
      r = paInternalError;
   }

   return r;
}


PaError Pa_StopStream(PaStream *stream)
{
   /* output */
   if(stream == (PaStream *)'\x02') {
      AudioQueueStop(playData.queue, false);
      for (int i = 0; i < NUM_BUFFERS; i++)
         AudioQueueFreeBuffer(playData.queue, playData.buffers[i]);
      playData.playing = false;
   }

   /* input */
   if(stream == (PaStream *)'\x02') {
      AudioQueueStop(recordData.queue, false);
      for (int i = 0; i < NUM_BUFFERS; i++)
         AudioQueueFreeBuffer(recordData.queue, recordData.buffers[i]);
      recordData.recording = false;
   }

   return paNoError;
}


PaError Pa_AbortStream(PaStream *stream)
{
   return Pa_CloseStream(stream);
}


const PaHostApiInfo *Pa_GetHostApiInfo(PaHostApiIndex hostApi)
{
   return &paHostApiInfo;
}


const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex device)
{
   return &deviceInfo;
}


const PaStreamInfo* Pa_GetStreamInfo(PaStream *stream)
{
   return &paStreamInfo;
}


const char *Pa_GetErrorText(PaError errorCode)
{
   return "";
}


PaDeviceIndex Pa_GetDeviceCount()
{
   return 0;
}


PaDeviceIndex Pa_GetDefaultInputDevice()
{
   return 0;
}


PaDeviceIndex Pa_GetDefaultOutputDevice()
{
   return 0;
}
