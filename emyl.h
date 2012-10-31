#pragma once

#include <vector>
#include <list>
#include <map>
#include <string>

#ifdef _WINDOWS
#include <al.h>
#include <alc.h>
#include <windows.h>
#define SLEEP(X) Sleep(X)
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <stdlib.h>
#define SLEEP(X) usleep(X*1000)
#endif

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

#define STREAM_OGG_INIT    (1<<0)
#define STREAM_OGG_LOADED  (1<<1)
#define STREAM_OGG_PLAYING (1<<2)
#define STREAM_OGG_PAUSE   (1<<3)
#define STREAM_OGG_LOOP    (1<<4)

namespace emyl {

class stream
{
public:
	 stream();
	~stream();

	bool   load    (const std::string &_filename);
	bool   load_mem(const std::string &_filename);
	bool   load_generic(void *sndfile, ov_callbacks *ptr_callbacks);

	bool   set_source();
	bool   set_source(ALuint _source);
	ALuint get_source() {return m_uiSource;}
	void   free_source();

	void   update();
	void   play();
	void   pause() {m_uiFlags ^= STREAM_OGG_PAUSE;}
	void   stop();
	
	void   seek(double _secs);
	bool   playing();

	void   set_loop(bool _loop);
	void   set_volume(ALfloat _volume);

	char*  get_error();

	static void updateAll();

private:
	static const int NUM_BUFFERS = 8;
	static const int BUFFER_SIZE = 65536;

	void SetError(std::string _sErr);
	bool bStream(ALuint _buff);
	
	ALenum  m_eformat;
	ALsizei m_ifreq;

	OggVorbis_File   m_ogg;
	ALuint           m_vbuffers[NUM_BUFFERS];
	unsigned int     m_uiFlags;
	ALuint           m_uiSource;

	char*            m_sLastError;
};

class sound
{
public:
	 sound();
	~sound();
	
	bool   set_buffer(ALuint _buffer);
	ALuint get_buffer() {return m_vbuffer;}


	bool   set_source();
	bool   set_source(ALuint _source);

	ALuint get_source() {return m_uiSource;}

	void   free_source();

	void  play();
	void  pause();
	void  stop();

	void  play_buffer(ALuint _buffer);
	void  play_buffer(ALuint _buffer, int _loop);

	bool  playing();

	void  set_loop(int _loop);
	void  set_volume(ALfloat _volume);

	void set_position  (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ);
	void set_velocity  (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ);
	void set_direction (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ);

	char* get_error();
private:
	void SetError(std::string _sErr);

	ALuint m_vbuffer;
	ALuint m_uiSource;

	char*  m_sLastError;
};


class manager
{
public:

	~manager();

	static manager*  get_instance();
	bool             init();

	void             delete_buffer(std::string _filename);
	ALuint           get_buffer(std::string _filename);

	ALuint           source_reserve();
	void             source_unreserve(ALuint _srcID);

	void             set_volume(ALfloat _volume);
	char*            get_error();

	void set_position    (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ);
	void set_velocity    (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ);
	void set_orientation (ALfloat _fDX, ALfloat _fDY, ALfloat _fDZ,
	                      ALfloat _fUX, ALfloat _fUY, ALfloat _fUZ);

	void   sleep();
	void unsleep();

private:

	friend class stream;
	friend class sound;

	static const int NUM_SOURCES = 16;

	void SetError(std::string _sErr);

	manager();

	static void delete_instance();
	static manager*               s_pInstance;
	static bool                   s_bInstanced;

	ALCdevice*                    m_alDev;
	ALCcontext*                   m_alContext;

	ALuint                        m_vSources[NUM_SOURCES];
	bool                          m_vSourcesReserved[NUM_SOURCES];
	bool                          m_vSourcesSleeped[NUM_SOURCES];

	std::map<std::string, ALuint> m_mSounds;
	
	char*                         m_sLastError;
};

typedef void(*error_callback)(const std::string &_sErr);
void setErrorCallback(error_callback callback);

} //namespace emyl;

