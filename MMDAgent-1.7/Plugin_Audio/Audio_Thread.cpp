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

/* headers */

#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

#ifdef __APPLE__
#if TARGET_OS_IPHONE
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AudioComponent.h>
/* need to rewrite function name in ios */
#define ComponentResult OSStatus
#define FSRef CFURLRef
#define ComponentDescription AudioComponentDescription
#define Component AudioComponent
#define FindNextComponent AudioComponentFindNext
#define OpenAComponent AudioComponentInstanceNew
#define CloseComponent AudioComponentInstanceDispose
#define kAudioUnitSubType_DefaultOutput kAudioUnitSubType_RemoteIO
#else
#include <Carbon/Carbon.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <audiounit/AudioUnit.h>
#endif /* TARGET_OS_IPHONE */
#endif /* __APPLE__ */

#ifdef __ANDROID__
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaCrypto.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaDrm.h>
#include <fcntl.h>
#include "portaudio.h"
#endif /* __ANDROID__ */

#include "MMDAgent.h"
#include "Audio_Thread.h"

#ifdef _WIN32
typedef struct _Audio {
   char buf[MMDAGENT_MAXBUFLEN];
   char ret[MMDAGENT_MAXBUFLEN];
} Audio;

static void Audio_initialize(Audio *audio)
{
}

static void Audio_clear(Audio *audio)
{
}

static bool Audio_openAndStart(Audio *audio, const char *alias, char *file)
{
   bool first = true;
   char *filepath;

   /* wait */
   filepath = MMDFiles_pathdup_from_application_to_system_locale(file);
   sprintf(audio->buf, "open \"%s\" alias _%s wait", filepath, alias);
   free(filepath);
   if (mciSendStringA(audio->buf, NULL, 0, 0) != 0) {
      return false;
   }

   /* enqueue */
   sprintf(audio->buf, "cue _%s output wait", alias);
   if (mciSendStringA(audio->buf, NULL, 0, 0) != 0) {
      sprintf(audio->buf, "close _%s wait", alias);
      mciSendStringA(audio->buf, NULL, 0, 0);
      return false;
   }

   /* start */
   sprintf(audio->buf, "play _%s", alias);
   if (mciSendStringA(audio->buf, NULL, 0, 0) != 0) {
      sprintf(audio->buf, "close _%s wait", alias);
      mciSendStringA(audio->buf, NULL, 0, 0);
      return false;
   }

   /* wait till sound starts */
   do {
      if(first == true)
         first = false;
      else
         MMDAgent_sleep(AUDIOTHREAD_STARTSLEEPSEC);
      /* check sound start */
      sprintf(audio->buf, "status _%s mode wait", alias);
      mciSendStringA(audio->buf, audio->ret, sizeof(audio->ret), NULL);
   } while(MMDAgent_strequal(audio->ret, "playing") == false);

   return true;
}

static void Audio_waitToStop(Audio *audio, const char *alias, bool *m_playing)
{
   do {
      /* check user's stop */
      if(*m_playing == false) {
         sprintf(audio->buf, "stop _%s wait", alias);
         mciSendStringA(audio->buf, NULL, 0, NULL);
         break;
      }
      MMDAgent_sleep(AUDIOTHREAD_ENDSLEEPSEC);
      /* check end of sound */
      sprintf(audio->buf, "status _%s mode wait", alias);
      mciSendStringA(audio->buf, audio->ret, sizeof(audio->ret), NULL);
   } while(MMDAgent_strequal(audio->ret, "playing") == true);
}

static void Audio_close(Audio *audio, const char *alias)
{
   sprintf(audio->buf, "close _%s wait", alias);
   mciSendStringA(audio->buf, NULL, 0, NULL);
}
#endif /* _WIN32 */

#ifdef __APPLE__
typedef struct _Audio {
   bool end;
   ExtAudioFileRef file;
   AudioUnit device;
   AudioBufferList buff;
} Audio;

static OSStatus renderCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
   UInt32 i;
   Audio *audio = (Audio*) inRefCon;
   OSStatus err;
   float *outL, *outR, *wave;

   /* initialize buffer */
   if(audio->buff.mBuffers[0].mDataByteSize < inNumberFrames * 2 * sizeof(float)) {
      if(audio->buff.mBuffers[0].mData != NULL)
         free(audio->buff.mBuffers[0].mData);
      audio->buff.mNumberBuffers = 1;
      audio->buff.mBuffers[0].mNumberChannels = 2;
      audio->buff.mBuffers[0].mData = malloc(inNumberFrames * 2 * sizeof(float) );
      audio->buff.mBuffers[0].mDataByteSize = inNumberFrames * 2 * sizeof(float);
   }

   /* read */
   err = ExtAudioFileRead(audio->file, &inNumberFrames, &audio->buff);
   if (err) {
      audio->end = true;
      return err;
   }

   /* end of file */
   if ( inNumberFrames == 0 ) {
      audio->end = true;
      return noErr;
   }

   /* copy */
   outL = (float*) ioData->mBuffers[0].mData;
   outR = (float*) ioData->mBuffers[1].mData;
   wave = (float*) audio->buff.mBuffers[0].mData;
   for (i = 0; i < inNumberFrames; i++) {
      *outL++ = *wave++;
      *outR++ = *wave++;
   }

   return noErr;
}

static void Audio_initialize(Audio *audio)
{
   memset(audio, 0, sizeof(Audio));
}

static void Audio_clear(Audio *audio)
{
   if(audio->buff.mBuffers[0].mData != NULL) {
      free(audio->buff.mBuffers[0].mData);
   }
   memset(audio, 0, sizeof(Audio));
}

static bool Audio_openAndStart(Audio *audio, const char *alias, char *file)
{
   UInt32 size;

   OSStatus err1;
   OSErr err2;
   ComponentResult err3;

   char *buff;
   FSRef path;
   AudioStreamBasicDescription desc, src, format;
   ComponentDescription findComp;
   Component comp;
   AURenderCallbackStruct input;

   buff = MMDAgent_pathdup_from_application_to_system_locale(file);

#if TARGET_OS_IPHONE
   /* convert file name */
   path = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFStringCreateWithCString(kCFAllocatorDefault, buff, kCFStringEncodingUTF8), kCFURLPOSIXPathStyle, false);
   free(buff);
   if (path == NULL)
      return false;

   /* open audio file */
   err1 = ExtAudioFileOpenURL(path, &audio->file);
   if(err1)
      return false;
#else
   /* convert file name */
   err1 = FSPathMakeRef ((const UInt8 *) buff, &path, NULL);
   free(buff);
   if(err1) {
      return false;
   }

   /* open audio file */
   err1 = ExtAudioFileOpen(&path, &audio->file);
   if(err1) {
      return false;
   }
#endif /* TARGET_OS_IPHONE */

   /* get audio file format */
   size = sizeof(AudioStreamBasicDescription);
   err1 = ExtAudioFileGetProperty(audio->file, kExtAudioFileProperty_FileDataFormat, &size, &src);
   if(err1) {
      ExtAudioFileDispose(audio->file);
      return false;
   }

   /* open audio device */
   findComp.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
   findComp.componentSubType = kAudioUnitSubType_RemoteIO;
#else
   findComp.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
   findComp.componentManufacturer = kAudioUnitManufacturer_Apple;
   findComp.componentFlags = 0;
   findComp.componentFlagsMask = 0;
   comp = FindNextComponent(NULL, &findComp);
   if(comp == 0) {
      ExtAudioFileDispose(audio->file);
      return false;
   }
   err2 = OpenAComponent(comp, &audio->device);
   if(err2) {
      ExtAudioFileDispose(audio->file);
      return false;
   }
   /* set callback func. */
   input.inputProc = renderCallback;
   input.inputProcRefCon = audio;
   size = sizeof(AURenderCallbackStruct);
   err3 = AudioUnitSetProperty(audio->device, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, size);
   if(err3) {
      CloseComponent(audio->device);
      ExtAudioFileDispose(audio->file);
      return false;
   }

   /* prepare audio device */
   err3 = AudioUnitInitialize(audio->device);
   if(err3) {
      CloseComponent(audio->device);
      ExtAudioFileDispose(audio->file);
      return false;
   }

   /* get output device format */
   size = sizeof(desc);
   OSStatus err = AudioUnitGetProperty(audio->device, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &desc, &size);
   if ( err != noErr ) {
      CloseComponent(audio->device);
      ExtAudioFileDispose(audio->file);
      return false;
   }

   /* set audiofile-to-renderer conversion format to pcm (32bit stereo with the same sampling rate) */
   format.mSampleRate = (desc.mSampleRate == 0) ? src.mSampleRate : desc.mSampleRate;
   format.mFormatID = kAudioFormatLinearPCM;
   format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
   format.mBytesPerPacket = 4 * 2;
   format.mFramesPerPacket = 1;
   format.mBytesPerFrame = 4 * 2;
   format.mChannelsPerFrame = 2;
   format.mBitsPerChannel = 32;
   format.mReserved = desc.mReserved;
   size = sizeof(AudioStreamBasicDescription);
   err1 = ExtAudioFileSetProperty(audio->file, kExtAudioFileProperty_ClientDataFormat, size, &format);
   if(err1) {
      CloseComponent(audio->device);
      ExtAudioFileDispose(audio->file);
      return false;
   }

   /* start */
   err3 = AudioOutputUnitStart(audio->device);
   if(err3) {
      AudioUnitUninitialize(audio->device);
      CloseComponent(audio->device);
      ExtAudioFileDispose(audio->file);
      return false;
   }

   return true;
}

static void Audio_waitToStop(Audio *audio, const char *alias, bool *m_playing)
{
   /* wait */
   while(*m_playing == true && audio->end == false) {
      MMDAgent_sleep(AUDIOTHREAD_ENDSLEEPSEC);
   }

   /* stop */
   AudioOutputUnitStop(audio->device);
}

static void Audio_close(Audio *audio, const char *alias)
{
   /* uninitialize audio device */
   AudioUnitUninitialize(audio->device);

   /* close audio device */
   CloseComponent(audio->device);

   /* close audio file */
   ExtAudioFileDispose(audio->file);
}
#endif /* __APPLE__ */

#ifdef __ANDROID__
/* Audio structure */
typedef struct _Audio {
   AMediaExtractor *extractor;
   AMediaCodec *codec;
   PaStream *stream;
   int channels;
} Audio;

/* Audio_initialize: initialize audio */
static void Audio_initialize(Audio *audio)
{
   audio->extractor = NULL;
   audio->codec = NULL;
   audio->stream = NULL;
   audio->channels = 0;
}

/* Audio_clear: clear audio */
static void Audio_clear(Audio *audio)
{
   /* clear codec */
   if (audio->codec) {
      AMediaCodec_stop(audio->codec);
      AMediaCodec_delete(audio->codec);
   }
   /* clear extractor */
   if (audio->extractor)
      AMediaExtractor_delete(audio->extractor);
   /* clear stream */
   if (audio->stream) {
      Pa_StopStream(audio->stream);
      Pa_CloseStream(audio->stream);
      Pa_Terminate();
   }
   Audio_initialize(audio);
}

/* Audio_openAndStart: open and start audio */
static bool Audio_openAndStart(Audio *audio, const char *alias, char *file)
{
   char *filePath;
   int fd;
   int exErr;
   AMediaFormat *format;
   int numTracks;
   const char *mime;
   int32_t samplingFrequency;
   int32_t channels;
   PaError err;
   PaStreamParameters parameters;

   /* get file descriptor to pass to MediaExtractor */
   filePath = MMDAgent_pathdup_from_application_to_system_locale(file);
   fd = open(filePath, O_RDONLY);
   free(filePath);
   if (fd < 0)
      return false;

   /* create MediaExtractor */
   audio->extractor = AMediaExtractor_new();
   exErr = AMediaExtractor_setDataSourceFd(audio->extractor, fd, 0, LONG_MAX);
   close(fd);
   if (exErr != 0) {
      Audio_clear(audio);
      return false;
   }

   /* get track count */
   numTracks = AMediaExtractor_getTrackCount(audio->extractor);
   if (numTracks <= 0) {
      Audio_clear(audio);
      return false;
   }

   /* get track format */
   format = AMediaExtractor_getTrackFormat(audio->extractor, 0);
   if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
      AMediaFormat_delete(format);
      Audio_clear(audio);
      return false;
   }

   /* if not audio, exit */
   if (! MMDAgent_strheadmatch(mime, "audio/")) {
      AMediaFormat_delete(format);
      Audio_clear(audio);
      return false;
   }

   /* create decoder */
   audio->codec = AMediaCodec_createDecoderByType(mime);
   if (audio->codec == NULL) {
      AMediaFormat_delete(format);
      Audio_clear(audio);
      return false;
   }
   if (AMediaCodec_configure(audio->codec, format, NULL, NULL, 0) != AMEDIA_OK) {
      AMediaFormat_delete(format);
      Audio_clear(audio);
      return false;
   }
   if (AMediaCodec_start(audio->codec) != AMEDIA_OK) {
      AMediaFormat_delete(format);
      Audio_clear(audio);
      return false;
   }
   /* select the first track to play */
   AMediaExtractor_selectTrack(audio->extractor, 0);

   /* get sampling rate and channel count */
   AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &samplingFrequency);
   AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
   audio->channels = channels;

   AMediaFormat_delete(format);

   /* initialize audio */
   err = Pa_Initialize();
   if (err != paNoError) {
      Audio_clear(audio);
      return false;
   }

   /* set output format */
   parameters.device = Pa_GetDefaultOutputDevice();
   parameters.channelCount = channels;
   parameters.sampleFormat = paInt16;
   parameters.suggestedLatency = Pa_GetDeviceInfo(parameters.device)->defaultLowOutputLatency;
   parameters.hostApiSpecificStreamInfo = NULL;

   /* open stream */
   err = Pa_OpenStream(&(audio->stream), NULL, &parameters, samplingFrequency, AUDIOTHREAD_FRAMEPERBUFFER, paClipOff, NULL, NULL);
   if (err != paNoError) {
      Audio_clear(audio);
      return false;
   }

   /* start stream */
   err = Pa_StartStream(audio->stream);
   if (err != paNoError) {
      Audio_clear(audio);
      return false;
   }

   return true;
}

/* Audio_waitToStop: wait to stop audio */
static void Audio_waitToStop(Audio *audio, const char *alias, bool *m_playing)
{
   bool inputEOS = false;
   bool outputEOS = false;
   ssize_t bufIndex;
   size_t bufSize;
   uint8_t *buf;
   int sampleSize;
   int64_t presentationTimeUs;
   AMediaCodecBufferInfo info;
   int status;
   AMediaFormat *format;
   int32_t samplingFrequency;
   int32_t channels;
   PaStreamParameters parameters;

   while ((outputEOS == false || inputEOS == false) && *m_playing == true) {
      if (inputEOS == false) {
         /* data exists in input stream */
         bufIndex = AMediaCodec_dequeueInputBuffer(audio->codec, AUDIOTHREAD_TIMEOUTUS);
         if (bufIndex >= 0) {
            /* read input samples */
            buf = AMediaCodec_getInputBuffer(audio->codec, bufIndex, &bufSize);
            sampleSize = AMediaExtractor_readSampleData(audio->extractor, buf, bufSize);
            if (sampleSize < 0) {
               /* end of input stream */
               sampleSize = 0;
               inputEOS = true;
            }
            /* enqueue samples to decoder */
            presentationTimeUs = AMediaExtractor_getSampleTime(audio->extractor);
            AMediaCodec_queueInputBuffer(audio->codec, bufIndex, 0, sampleSize, presentationTimeUs, inputEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);
            AMediaExtractor_advance(audio->extractor);
         }
      }
      if (outputEOS == false) {
         /* data exist in output buffer */
         status = AMediaCodec_dequeueOutputBuffer(audio->codec, &info, 1);
         if (status >= 0) {
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
               /* end of output stream */
               outputEOS = true;
            }
            if (info.size > 0) {
               /* dequeue samples from decoder */
               buf = AMediaCodec_getOutputBuffer(audio->codec, status, &bufSize);
               /* write the decoded samples to output device */
               Pa_WriteStream(audio->stream, buf, info.size / 2);
            }
            /* flush the processed samples from the queue */
            AMediaCodec_releaseOutputBuffer(audio->codec, status, false);
            MMDAgent_sleep(AUDIOTHREAD_OUTPUTFLUSHWAITSEC);
         } else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            /* format changes detected in the input stream */
            format = AMediaCodec_getOutputFormat(audio->codec);
            /* get the new format */
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, &samplingFrequency);
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, &channels);
            AMediaFormat_delete(format);
            /* re-open audio */
            Pa_StopStream(audio->stream);
            Pa_CloseStream(audio->stream);
            audio->channels = channels;
            parameters.device = Pa_GetDefaultOutputDevice();
            parameters.channelCount = channels;
            parameters.sampleFormat = paInt16;
            parameters.suggestedLatency = Pa_GetDeviceInfo(parameters.device)->defaultLowOutputLatency;
            parameters.hostApiSpecificStreamInfo = NULL;
            Pa_OpenStream(&(audio->stream), NULL, &parameters, samplingFrequency, AUDIOTHREAD_FRAMEPERBUFFER, paClipOff, NULL, NULL);
            Pa_StartStream(audio->stream);
         }
      }
   }
   Audio_clear(audio);
}

/* Audio_close: close audio */
static void Audio_close(Audio *audio, const char *alias)
{
}
#endif /* __ANDROID__ */

/* mainThread: main thread */
static void mainThread(void *param)
{
   Audio_Thread *audio_thread = (Audio_Thread *) param;
   audio_thread->run();
}

/* Audio_Thread::initialize: initialize thread */
void Audio_Thread::initialize()
{
   m_mmdagent = NULL;

   m_mutex = NULL;
   m_cond = NULL;
   m_thread = -1;

   m_count = 0;

   m_playing = false;
   m_kill = false;

   m_alias = NULL;
   m_file = NULL;
}

/* Audio_Thread::clear: free thread */
void Audio_Thread::clear()
{
   stop();
   m_kill = true;

   /* wait */
   if(m_cond != NULL)
      glfwSignalCond(m_cond);

   if(m_mutex != NULL || m_cond != NULL || m_thread >= 0) {
      if(m_thread >= 0) {
         glfwWaitThread(m_thread, GLFW_WAIT);
         glfwDestroyThread(m_thread);
      }
      if(m_cond != NULL)
         glfwDestroyCond(m_cond);
      if(m_mutex != NULL)
         glfwDestroyMutex(m_mutex);
   }

   if(m_alias) free(m_alias);
   if(m_file) free(m_file);

   initialize();
}

/* Audio_Thread::startLipsync: start lipsync if HTK label file is existing */
bool Audio_Thread::startLipsync(const char *file)
{
   bool result = false;
   int i;
   char *label;
   size_t len;
   FILE *fp;
   char buff[MMDAGENT_MAXBUFLEN];
   char message[MMDAGENT_MAXBUFLEN];
   int startLen, startTime, endLen, endTime, phonemeLen;
   bool first = true;
   PMDObject *objs;

   len = MMDAgent_strlen(file);
   if(len > 4) {
      label = MMDAgent_strdup(file);
      label[len - 4] = '.';
      label[len - 3] = 'l';
      label[len - 2] = 'a';
      label[len - 1] = 'b';
      fp = MMDAgent_fopen(label, "r");
      if(fp != NULL) {
         /* load HTK label */
         strcpy(message, "");
         while(1) {
            startLen = MMDAgent_fgettoken(fp, buff);
            startTime = MMDAgent_str2int(buff);
            endLen = MMDAgent_fgettoken(fp, buff);
            endTime = MMDAgent_str2int(buff);
            phonemeLen = MMDAgent_fgettoken(fp, buff);
            if(startLen > 0 && endLen > 0 && phonemeLen > 0 && startTime < endTime) {
               if(first)
                  sprintf(message, "%s,%d", buff, (int) ((double) (endTime - startTime) * 1.0E-04 + 0.5));
               else
                  sprintf(message, "%s,%s,%d", message, buff, (int) ((double) (endTime - startTime) * 1.0E-04 + 0.5));
               first = false;
            } else {
               break;
            }
         }
         fclose(fp);
         /* send lipsync message */
         if(first == false) {
            objs = m_mmdagent->getModelList();
            for (i = 0; i < m_mmdagent->getNumModel(); i++) {
               if (objs[i].isEnable() == true && objs[i].allowMotionFileDrop() == true) {
                  m_mmdagent->sendMessage(MMDAGENT_COMMAND_LIPSYNCSTART, "%s|%s", objs[i].getAlias(), message);
                  result = true;
               }
            }
         }
      }
      free(label);
   }

   return result;
}

/* Audio_Thread::stopLipsync: stop lipsync */
void Audio_Thread::stopLipsync()
{
   int i;
   PMDObject *objs;

   objs = m_mmdagent->getModelList();
   for (i = 0; i < m_mmdagent->getNumModel(); i++) {
      if (objs[i].isEnable() == true && objs[i].allowMotionFileDrop() == true) {
         m_mmdagent->sendMessage(MMDAGENT_COMMAND_LIPSYNCSTOP, "%s", objs[i].getAlias());
      }
   }
}

/* Audio_Thread::Audio_Thread: thread constructor */
Audio_Thread::Audio_Thread()
{
   initialize();
}

/* Audio_Thread::~Audio_Thread: thread destructor */
Audio_Thread::~Audio_Thread()
{
   clear();
}

/* Audio_Thread::setupAndStart: setup audio and start thread */
void Audio_Thread::setupAndStart(MMDAgent *mmdagent)
{
   m_mmdagent = mmdagent;

   glfwInit();
   m_mutex = glfwCreateMutex();
   m_cond = glfwCreateCond();
   m_thread = glfwCreateThread(mainThread, this);
   if(m_mutex == NULL || m_cond == NULL || m_thread < 0) {
      clear();
      return;
   }
}

/* Audio_Thread::stopAndRelease: stop thread and free audio */
void Audio_Thread::stopAndRelease()
{
   clear();
}

/* Audio_Thread::run: main thread loop for audio */
void Audio_Thread::run()
{
   Audio audio;
   char *alias, *file;
   bool lipsync;

   while (m_kill == false) {
      /* wait event */
      glfwLockMutex(m_mutex);
      while(m_count <= 0) {
         glfwWaitCond(m_cond, m_mutex, GLFW_INFINITY);
         if(m_kill == true)
            return;
      }
      alias = MMDAgent_strdup(m_alias);
      file = MMDAgent_strdup(m_file);
      m_count--;
      glfwUnlockMutex(m_mutex);

      m_playing = true;

      /* open and start audio */
      Audio_initialize(&audio);
      if(Audio_openAndStart(&audio, alias, file) == true) {

         lipsync = startLipsync(file);

         /* send SOUND_EVENT_START */
         m_mmdagent->sendMessage(AUDIOTHREAD_EVENTSTART, "%s", alias);

         /* wait to stop audio */
         Audio_waitToStop(&audio, alias, &m_playing);

         if(lipsync) stopLipsync();

         /* send SOUND_EVENT_STOP */
         m_mmdagent->sendMessage(AUDIOTHREAD_EVENTSTOP, "%s", alias);

         /* close audio file */
         Audio_close(&audio, alias);
      }

      if(alias) free(alias);
      if(file) free(file);
      Audio_clear(&audio);
      m_playing = false;
   }
}

/* Audio_Thread::isRunning: check running */
bool Audio_Thread::isRunning()
{
   if(m_kill == true || m_mutex == NULL || m_cond == NULL || m_thread < 0)
      return false;
   else
      return true;
}

/* Audio_Thread::isPlaying: check playing */
bool Audio_Thread::isPlaying()
{
   return m_playing;
}

/* checkAlias: check playing alias */
bool Audio_Thread::checkAlias(const char *alias)
{
   bool ret;

   /* check */
   if(isRunning() == false)
      return false;

   /* wait buffer mutex */
   glfwLockMutex(m_mutex);

   /* check audio alias */
   ret = MMDAgent_strequal(m_alias, alias);

   /* release buffer mutex */
   glfwUnlockMutex(m_mutex);

   return ret;
}

/* Audio_Thread::play: start playing */
void Audio_Thread::play(const char *alias, const char *file)
{
   /* check */
   if(isRunning() == false)
      return;
   if(MMDAgent_strlen(alias) <= 0 || MMDAgent_strlen(file) <= 0)
      return;

   /* wait buffer mutex */
   glfwLockMutex(m_mutex);

   /* save alias */
   if(m_alias) free(m_alias);
   if(m_file) free(m_file);
   m_alias = MMDAgent_strdup(alias);
   m_file = MMDAgent_strdup(file);
   m_count++;

   /* start playing thread */
   if(m_count <= 1)
      glfwSignalCond(m_cond);

   /* release buffer mutex */
   glfwUnlockMutex(m_mutex);
}

/* Audio_Thread::stop: stop playing */
void Audio_Thread::stop()
{
   if(isRunning() == true)
      m_playing = false;
}
