#include "emyl.h"

#include <vector>
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <string.h>

#ifdef _WINDOWS
#include <al.h>
#include <alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

namespace emyl {

void default_error_callback (const std::string &s) {}
error_callback _error_callback = default_error_callback;
void setErrorCallback(error_callback cb) {_error_callback = cb;}

/*---------------------------------------------------------------------------*/
/*-libvorbis-inits-----------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static int _fseek64_wrap(FILE *f, ogg_int64_t off, int whence){
	if(f==NULL)return(-1);
	return fseek(f,off,whence);
}

ov_callbacks callbacks = {
	(size_t (*) (void *, size_t, size_t, void *)) fread,
	(int (*)(void *, ogg_int64_t, int)) _fseek64_wrap,
	(int (*)(void *)) fclose,
	(long (*)(void *)) ftell
};

/*-file-memory---------------------------------------------------------------*/

struct file_mem
{
	unsigned char *begin;
	unsigned char *curr;
	unsigned char *end;
};

file_mem *fopen_mem (const char* filename)
{
	int r;

	FILE* f = fopen(filename, "rb");
	if (f == NULL) return NULL;

	r = fseek(f, 0, SEEK_END);
	if (r)
	{
		fclose(f);
		return NULL;
	}

	long size = ftell(f);

	r = fseek(f, 0, SEEK_SET);
	if (r)
	{
		fclose(f);
		return NULL;
	}

	unsigned char *data = (unsigned char*) malloc(size);

	size_t remain = (size_t) size;
	unsigned char *curr = data;
	while(remain)
	{
		size_t c = fread ( (void *) curr, 1, remain, f);
		if (c != remain && ferror(f))
		{
			free(data);
			fclose(f);
			return NULL;
		}
		else
		{
			remain -= c;
			curr   += c;
		}
	}

	file_mem *fm = (file_mem*) malloc(sizeof(file_mem));
	fm->begin = data;
	fm->curr  = data;
	fm->end   = data + size;

	return fm;
}

size_t fread_mem ( void * ptr, size_t size, size_t count, file_mem * stream )
{
	size_t total = size * count;
	size_t bytes = 0;
	unsigned char *ptr8 = (unsigned char*) ptr;
	while(bytes < total && stream->curr != stream->end)
	{
		*ptr8++ = *stream->curr++;
		bytes++;
	}

	return bytes;
}

int fseek_mem ( file_mem * stream, long int offset, int origin )
{
	switch(origin)
	{
	case SEEK_SET:
		stream->curr = stream->begin + offset; break;
	case SEEK_CUR:
		stream->curr = stream->curr + offset; break;
	case SEEK_END:
		stream->curr = stream->end  + offset; break;
	default:
		return -1;
	}

	if ((unsigned long) stream->curr > (unsigned long) stream->end ) stream->curr = stream->end;
	if ((unsigned long) stream->curr < (unsigned long) stream->begin) stream->curr = stream->begin;

	return 0;
}

int fclose_mem ( file_mem * stream )
{
	if (stream == NULL) return EOF;

	free(stream->begin);
	free(stream);

	return 0;
}

long int ftell_mem ( file_mem * stream )
{
	return (long int) stream->curr - (long int)stream->begin;
}

static int _fseek64_wrap_ram(file_mem *f, ogg_int64_t off, int whence){
	if(f==NULL) return(-1);
	return fseek_mem(f,off,whence);
}

ov_callbacks callbacks_mem = {
	(size_t (*) (void *, size_t, size_t, void *)) fread_mem,
	(int (*)(void *, ogg_int64_t, int)) _fseek64_wrap_ram,
	(int (*)(void *)) fclose_mem,
	(long (*)(void *)) ftell_mem
};

/*---------------------------------------------------------------------------*/
/*-manager-------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

manager* manager::s_pInstance = NULL;
bool manager::s_bInstanced = false;

/*---------------------------------------------------------------------------*/

manager::manager()
{
	m_sLastError = new char[1];
	m_sLastError[0] = '\0';

	if (!s_bInstanced)
	{
		atexit(manager::delete_instance);
		s_bInstanced = true;
	}
}

/*---------------------------------------------------------------------------*/

manager::~manager()
{
	alDeleteSources(NUM_SOURCES,m_vSources);
	
	// ELIMINA BUFFERS CREADOS.
	while(!m_mSounds.empty())
	{
		ALuint buffer = m_mSounds.begin()->second;
		alDeleteBuffers(1, &buffer);
		m_mSounds.erase(m_mSounds.begin());
	}

	m_alContext = alcGetCurrentContext();
	m_alDev     = alcGetContextsDevice(m_alContext);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(m_alContext);
	alcCloseDevice(m_alDev);

	s_pInstance = NULL;
}

/*---------------------------------------------------------------------------*/

manager* manager::get_instance()
{
	if(s_pInstance == NULL)
	{
		s_pInstance = new manager();
	}
	return s_pInstance;
}

/*---------------------------------------------------------------------------*/

void manager::delete_instance()
{
	if (s_pInstance) delete s_pInstance;
}

/*---------------------------------------------------------------------------*/

bool manager::init()
{
	
	m_alDev = alcOpenDevice(NULL);
	if (!m_alDev)
	{
		SetError("OpenAL error: Could not init OpenAL.\n");
		return false;
	}

#ifdef _DEBUG
	fprintf(stderr, "Audio device name: %s.\n", alcGetString(m_alDev, ALC_DEVICE_SPECIFIER));
	fprintf(stderr, "Audio device extensions: %s.\n", alcGetString(m_alDev, ALC_EXTENSIONS));
#endif

	m_alContext = alcCreateContext(m_alDev, NULL);	// Context para alDev
	if (!m_alContext)
	{
		SetError("OpenAL error: Context can't be created.\n");
		return false;	
	}
	alcMakeContextCurrent(m_alContext);

	alGetError();
	alGenSources(NUM_SOURCES, m_vSources);
	if(alGetError() != AL_NO_ERROR)
	{
		SetError("OpenAL error: Sources can't be created.\n");
		return false;
	}

	for(int i = 0; i < NUM_SOURCES; i++) {
		m_vSourcesReserved[i] = false;
		m_vSourcesSleeped[i] = false;
	}

	set_position   (0.0,0.0,0.0);
	set_velocity   (0.0,0.0,0.0);
	set_orientation(0.0,0.0,-1.0, 0.0,1.0,0.0);

	return true;
}

/*---------------------------------------------------------------------------*/

inline char *manager::get_error() {return m_sLastError;}

/*---------------------------------------------------------------------------*/

void manager::delete_buffer(std::string _filename)
{
	if(m_mSounds.find(_filename) != m_mSounds.end())
	{
		ALuint buffer = m_mSounds[_filename];
		alDeleteBuffers(1, &buffer);
		m_mSounds.erase(_filename);
	}
}

/*---------------------------------------------------------------------------*/

ALuint manager::get_buffer(std::string _filename)
{
	std::map<std::string,ALuint>::iterator iter = m_mSounds.find(_filename);
	if(iter != m_mSounds.end()) return iter->second;

	const char *filename = _filename.c_str();

	OggVorbis_File vf;
	FILE *sndfile = fopen(filename, "rb");
	if (sndfile == NULL)
	{
		SetError("Manager Error: File could not be opened.\n");
		return -1;
	}
	if (ov_open_callbacks(sndfile, &vf, NULL, 0, callbacks) < 0 ) {
		fclose(sndfile);
		SetError("OGG Error: It doesn't seem to be an OGG File.\n");
		return -1;
	}

	vorbis_info *vi = ov_info(&vf, -1);

	//LOS SONIDOS NO PUEDEN DURAR MAS DE 10 SEGS NI SER STEREO
	if((int)ov_pcm_total(&vf, -1) > (10*vi->rate) || vi->channels > 1) {
		fclose(sndfile);
		SetError("OGG Error: OGG File is too long to be a simple sound.\n");
		return -1;
	}
	if(vi->channels > 1) {
		fclose(sndfile);
		SetError("OGG Error: OGG File is not MONO.\n");
		return -1;
	}

	int samples  = (int) ov_pcm_total(&vf,-1);
	int channels = (int) vi->channels;
	int rate     = (int) vi->rate;
	signed short *data = new signed short[samples*channels];

	char *stream = (char*)data;
	int bytes = samples * channels * (int) sizeof(signed short);
	int current_section;
	while(bytes > 0) {
		long read_bytes = ov_read(&vf,stream,bytes,0,2,1,&current_section);

		if (!read_bytes) break; //EOF
		else if (read_bytes < 0) {
//				if (read_bytes == OV_HOLE)
//					LOG << "Error: OV_HOLE"     << std::endl;
//				else if(read_bytes == OV_EBADLINK)
//					LOG << "Error: OV_EBADLINK" << std::endl;
		}
		else {
			bytes  -= (int) read_bytes;
			stream += (int) read_bytes;
		}
	}

	ov_clear(&vf);

	ALuint newbuffer;
	alGetError();
	alGenBuffers(1, &newbuffer);
	if(alGetError() != AL_NO_ERROR)
	{
		SetError("OpenAL Error: Buffer could not be generated.\n");
		delete[] data;
		return -1;
	}
	
	alGetError();
	alBufferData(newbuffer, AL_FORMAT_MONO16, data, samples * (int) sizeof(short), rate);

	delete[] data;

	if(alGetError() != AL_NO_ERROR)
	{
		alDeleteBuffers(1, &newbuffer);
		SetError("OpenAL Error: Buffer could not be filled.\n");
		return -1;
	}

	

	m_mSounds[_filename] = newbuffer;
	return newbuffer;	
}

/*---------------------------------------------------------------------------*/

ALuint manager::source_reserve()
{
	int i;

	for(i = 0; i < NUM_SOURCES; i++)
	{
		if(!m_vSourcesReserved[i])
		{
			m_vSourcesReserved[i] = true;
			return m_vSources[i];
		}
	}

	return 0;
}

/*---------------------------------------------------------------------------*/

void manager::source_unreserve(ALuint _srcID)
{
	int i;

	for(i = 0; i < NUM_SOURCES; i++)
	{
		if(m_vSources[i] == _srcID)
		{
			m_vSourcesReserved[i] = false;
			alSourcei(_srcID, AL_BUFFER, 0);
			break;
		}
	}
}

/*---------------------------------------------------------------------------*/

void manager::set_volume(ALfloat _volume)
{
	if(_volume > 1.0f && _volume < 0.0f) return;	
	alListenerf(AL_GAIN,_volume);
}
/*---------------------------------------------------------------------------*/

inline
void manager::set_position(ALfloat _fX, ALfloat _fY, ALfloat _fZ)
{
	ALfloat vfListenerPos[3] = { _fX, _fY, _fZ};
	alListenerfv(AL_POSITION, vfListenerPos);
}
/*---------------------------------------------------------------------------*/

inline
void manager::set_velocity(ALfloat _fX, ALfloat _fY, ALfloat _fZ)
{
	ALfloat vfListenerVel[3] = { _fX, _fY, _fZ};
	alListenerfv(AL_VELOCITY, vfListenerVel);
}

/*---------------------------------------------------------------------------*/

inline
void manager::set_orientation(ALfloat _fDX, ALfloat _fDY, ALfloat _fDZ,
                              ALfloat _fUX, ALfloat _fUY, ALfloat _fUZ)
{
	ALfloat vfListenerOri[6] = { _fDX, _fDY, _fDZ, _fUX, _fUY, _fUZ};
	alListenerfv(AL_ORIENTATION, vfListenerOri);
}

/*---------------------------------------------------------------------------*/

void manager::SetError(std::string _sErr)
{
	delete[] m_sLastError;
	m_sLastError = new char[_sErr.size() + 1];

	strcpy(m_sLastError, _sErr.c_str());
	_error_callback(_sErr);

#ifdef _DEBUG
	fprintf(stderr, "%s.\n", _sErr.c_str());
#endif
}

void manager::sleep()
{
	for ( unsigned int i = 0; i < NUM_SOURCES; i++)
	{
		if (m_vSourcesReserved[i])
		{
			ALuint uiSource = m_vSources[i]; ALint state;
			alGetSourcei(uiSource, AL_SOURCE_STATE, &state);
			if (state == AL_PLAYING)
			{
				alSourcePause(uiSource);
				m_vSourcesSleeped[i] = true;
			}

		}
	}
}

void manager::unsleep()
{
	for ( unsigned int i = 0; i < NUM_SOURCES; i++)
	{
		if (m_vSourcesReserved[i] && m_vSourcesSleeped[i])
		{
			alSourcePlay(m_vSources[i]);
			m_vSourcesSleeped[i] = false;

		}
	}
}


/*---------------------------------------------------------------------------*/
/*-stream--------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static std::set<stream*> s_instances;

stream::stream()
{
	memset(this, 0, sizeof(stream));

	m_sLastError = new char[1];
	m_sLastError[0] = '\0';
}

/*---------------------------------------------------------------------------*/

stream::~stream()
{
	if(m_uiSource)
		free_source();
	if(m_uiFlags & STREAM_OGG_LOADED)
		ov_clear(&m_ogg);
	if(m_uiFlags & STREAM_OGG_INIT)
		alDeleteBuffers(NUM_BUFFERS, m_vbuffers);
}

/*---------------------------------------------------------------------------*/

bool stream::load(const std::string &_filename)
{
	const char *filename = _filename.c_str();
	FILE *sndfile = fopen(filename, "rb");
	if (sndfile == NULL)
	{
		SetError("File error: File doesn't exist.\n");
		return false;
	}

    bool success = load_generic((void *) sndfile, &callbacks);
	if(!success) fclose(sndfile);
	return success;
}

bool stream::load_mem(const std::string &_filename)
{
	const char *filename = _filename.c_str();
	file_mem *sndfile = fopen_mem(filename);
	if (sndfile == NULL)
	{
		SetError("File error: File doesn't exist.\n");
		return false;
	}

	bool success = load_generic((void *) sndfile, &callbacks_mem);
	if(!success) fclose_mem(sndfile);
	return success;
}

bool stream::load_generic(void *sndfile, ov_callbacks *ptr_callbacks)
{
	if(!(m_uiFlags & STREAM_OGG_INIT))
	{
		alGetError();
		alGenBuffers(NUM_BUFFERS, m_vbuffers);
		if(alGetError() != AL_NO_ERROR)
		{
			SetError("OpenAL Error: Buffer could not be generated.\n");
			return false;
		}
		m_uiFlags |= STREAM_OGG_INIT;
	}

	if(!(m_uiFlags & STREAM_OGG_INIT) ||
	     m_uiFlags & STREAM_OGG_PLAYING)
	{
		SetError("Error: Can't load while playing.\n");
		return false;
	}

	OggVorbis_File vf;
	if (ov_open_callbacks(sndfile, &vf, NULL, 0, *ptr_callbacks) < 0 ) {
		SetError("Ogg error: File doesn't seem to be OGG file.\n");
		return false;
	}

	vorbis_info *vi = ov_info(&vf, -1);

	if(vi->channels == 1) m_eformat = AL_FORMAT_MONO16;
	else                  m_eformat = AL_FORMAT_STEREO16;

	m_ifreq = (ALsizei) vi->rate;

	if(m_uiFlags & STREAM_OGG_LOADED) ov_clear(&m_ogg);
	memcpy(&m_ogg, &vf, sizeof(OggVorbis_File));
	m_uiFlags |= STREAM_OGG_LOADED;

	return true;
}

/*---------------------------------------------------------------------------*/

bool stream::set_source()
{
	manager* mng = manager::get_instance();
	ALuint source = mng->source_reserve();
	
	if (source == 0) return false;
	return set_source(source);
}

/*---------------------------------------------------------------------------*/

bool stream::set_source(ALuint _source)
{
	if(alIsSource(_source) == AL_FALSE)
	{
		SetError("OpenAL Error: Source isn't valid.\n");
		return false;
	}
	m_uiSource = _source;

	alSource3f(m_uiSource, AL_POSITION,        0.0, 0.0, 0.0);
	alSource3f(m_uiSource, AL_VELOCITY,        0.0, 0.0, 0.0);
	alSource3f(m_uiSource, AL_DIRECTION,       0.0, 0.0, 0.0);
	alSourcef (m_uiSource, AL_ROLLOFF_FACTOR,  0.0          );
	alSourcef (m_uiSource, AL_PITCH,           1.0          ); 
	alSourcei (m_uiSource, AL_SOURCE_RELATIVE, AL_TRUE      );
	alSourcei (m_uiSource, AL_SOURCE_TYPE,     AL_STREAMING );
	alSourceStop(m_uiSource);

	return true;
}

/*---------------------------------------------------------------------------*/

void stream::free_source()
{	
	stop();	

	if (m_uiSource)
	{
		manager* mng = manager::get_instance();
		mng->source_unreserve(m_uiSource);
		m_uiSource = 0;
	}
}

/*---------------------------------------------------------------------------*/

void stream::play()
{
	if (!m_uiSource) return;	
	
	if((m_uiFlags & STREAM_OGG_PLAYING) ||
	  !(m_uiFlags & STREAM_OGG_LOADED))
	{
		m_uiFlags &= ~STREAM_OGG_PAUSE;
		return;
	}

	m_uiFlags &= ~STREAM_OGG_PAUSE;
	
	alSourceStop(m_uiSource);
	
	ALint queued; ALuint buffer;
	alGetSourcei(m_uiSource, AL_BUFFERS_QUEUED, &queued);
	while(queued--) alSourceUnqueueBuffers(m_uiSource, 1, &buffer);
	
	//PLAY SOURCE AS SOON AS POSSIBLE.
	bStream(m_vbuffers[0]);
	alSourceQueueBuffers (m_uiSource, 1, m_vbuffers);
	alSourcePlay (m_uiSource);

	for( int i = 1; i < NUM_BUFFERS; i++)
	{
		bStream(m_vbuffers[i]);
		alSourceQueueBuffers (m_uiSource, 1, m_vbuffers+i);
	}

	//Lo aadimos
	s_instances.insert(this);

	m_uiFlags |= STREAM_OGG_PLAYING;
}
/*---------------------------------------------------------------------------*/
void stream::stop()
{
	if(m_uiFlags&STREAM_OGG_PLAYING)
	{
		m_uiFlags &= ~(STREAM_OGG_PLAYING|STREAM_OGG_PAUSE);
		s_instances.erase(this);
	}
}


/*---------------------------------------------------------------------------*/

void stream::update()
{
	if(!(m_uiFlags & STREAM_OGG_PLAYING)) return;

	ALint processed;
	alGetSourcei(m_uiSource, AL_BUFFERS_PROCESSED, &processed);

//	if(processed) LOG << processed << std::endl;

	while(processed--)
	{
		ALuint buffer;
		alSourceUnqueueBuffers(m_uiSource, 1, &buffer);

		if(bStream(buffer))
		{
			alSourceQueueBuffers(m_uiSource, 1, &buffer);
		}
		else // SE ACABO EL FICHERO.
		{
			m_uiFlags &= ~(STREAM_OGG_PLAYING|STREAM_OGG_PAUSE);
			break;
		}
	}

	ALint state;
	alGetSourcei(m_uiSource, AL_SOURCE_STATE, &state);

	if(m_uiFlags & STREAM_OGG_PAUSE)
	{
	     if(state != AL_PAUSED) alSourcePause(m_uiSource);
	}
	else
	{
		if(state != AL_PLAYING) alSourcePlay (m_uiSource);
	}
	
}

/*---------------------------------------------------------------------------*/

void stream::updateAll()
{
	std::set<stream*>::iterator iter = s_instances.begin();
	for(iter = s_instances.begin(); iter != s_instances.end(); iter++)
		(*iter)->update();
}

/*---------------------------------------------------------------------------*/

void stream::set_loop(bool _loop)
{
	if(_loop) m_uiFlags |=  STREAM_OGG_LOOP;
	else      m_uiFlags &= ~STREAM_OGG_LOOP;
}

/*---------------------------------------------------------------------------*/

void stream::seek(double _secs)
{
	ALint state;
	alGetSourcei(m_uiSource, AL_SOURCE_STATE, &state);
	if(state != AL_PLAYING && state != AL_PAUSED)
	{
#ifdef _DEBUG
		int result = ov_time_seek(&m_ogg,_secs);
		switch(result)
		{
			case OV_ENOSEEK:
				fprintf(stderr, "Bitstream is not seekable.\n");
				break;
			case OV_EINVAL:
				fprintf(stderr, "Invalid argument value; possibly called with \
				        an OggVorbis_File structure that isn't open.\n");
				break;
			case OV_EREAD:
				fprintf(stderr, "A read from media returned an error.\n");
				break;
			case OV_EFAULT:
				fprintf(stderr, "Internal logic fault; indicates a bug or \
				        heap/stack corruption.\n");
				break;
			case OV_EBADLINK:
				fprintf(stderr, "Invalid stream section supplied to \
				        libvorbisfile, or the requested link is corrupt. \n");
				break;
		}
#else
		ov_time_seek(&m_ogg,_secs);
#endif
	}
}

/*---------------------------------------------------------------------------*/

bool stream::playing()
{
	ALint state;
	alGetSourcei(m_uiSource, AL_SOURCE_STATE, &state);
	return (state == AL_PLAYING);// || (m_uiFlags & STREAM_OGG_PLAYING);
}

/*---------------------------------------------------------------------------*/

void stream::set_volume(ALfloat _volume)
{
	if(_volume > 1.0f && _volume < 0.0f) return;
	alSourcef(m_uiSource,AL_GAIN,_volume);
}

/*---------------------------------------------------------------------------*/

bool stream::bStream(ALuint _buff)
{

	char data[BUFFER_SIZE];
	int  bytes = 0;

	int  current_section;
	int  read_bytes;
	
	while(bytes < BUFFER_SIZE)
	{
		read_bytes = (int) ov_read(&m_ogg, data + bytes, BUFFER_SIZE - bytes, 0, 2, 1, &current_section);
		
		if(read_bytes > 0) bytes += read_bytes;
		else
		{
			if(read_bytes < 0)
			{
//				if (read_bytes == OV_HOLE)
//					LOG << "Error: OV_HOLE"     << std::endl;
//				else if(read_bytes == OV_EBADLINK)
//					LOG << "Error: OV_EBADLINK" << std::endl;
			}
			else //EOF
			{
				if (m_uiFlags & STREAM_OGG_LOOP)
					ov_raw_seek(&m_ogg, 0);
				else break;
			}
		}
	}
    
	if(bytes == 0) return false;

/*	//DEBUG	
	for(int i = 0; i < BUFFER_SIZE; i++)
		data[i] = rand() & 0xFFFF;
	
	bytes = BUFFER_SIZE;
*/	//END OF DEBUG	

	alBufferData(_buff, m_eformat, data, bytes, m_ifreq);
	
    return true;
}

/*---------------------------------------------------------------------------*/

inline char *stream::get_error() {return m_sLastError;}

/*---------------------------------------------------------------------------*/

void stream::SetError(std::string _sErr)
{
	delete m_sLastError;
	m_sLastError = new char[_sErr.size() + 1];

	strcpy(m_sLastError, _sErr.c_str());
	_error_callback(_sErr);

#ifdef _DEBUG
	fprintf(stderr, "%s.\n", _sErr.c_str());
#endif
}

/*---------------------------------------------------------------------------*/
/*-sound---------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

sound::sound()
{
	m_uiSource = 0;
	m_vbuffer = 0;

	m_sLastError = new char[1];
	m_sLastError[0] = '\0';
}

/*---------------------------------------------------------------------------*/

sound::~sound()
{
	if(m_uiSource) free_source();
}

/*---------------------------------------------------------------------------*/

bool sound::set_buffer(ALuint _buffer)
{
	if (! alIsBuffer(_buffer)) return false;

	m_vbuffer = _buffer;
	if(m_uiSource) alSourcei(m_uiSource, AL_BUFFER, m_vbuffer);

	return true;
}

/*---------------------------------------------------------------------------*/

bool sound::set_source()
{
	if (m_uiSource) return false;
	
	manager* mng = manager::get_instance();
	ALuint source = mng->source_reserve();
	
	if (source == 0) return false;
	return set_source(source);
}

/*---------------------------------------------------------------------------*/

bool sound::set_source(ALuint _source)
{
	if(alIsSource(_source) == AL_FALSE)
	{
		SetError("OpenAL Error: Source isn't valid.\n");
		return false;
	}
	m_uiSource = _source;

	alSource3f(m_uiSource, AL_POSITION,        0.0, 0.0, 0.0);
	alSource3f(m_uiSource, AL_VELOCITY,        0.0, 0.0, 0.0);
	alSource3f(m_uiSource, AL_DIRECTION,       0.0, 0.0, 0.0);
	alSourcef (m_uiSource, AL_ROLLOFF_FACTOR,            1.0);
	alSourcef (m_uiSource, AL_REFERENCE_DISTANCE,        1.0);
	alSourcef (m_uiSource, AL_PITCH,                     1.0); 
	alSourcei (m_uiSource, AL_SOURCE_RELATIVE, AL_FALSE     );

	if(m_vbuffer) alSourcei(m_uiSource, AL_BUFFER, m_vbuffer);

	return true;
}

/*---------------------------------------------------------------------------*/

void sound::free_source()
{
	stop();	

	if (m_uiSource)
	{
		manager* mng = manager::get_instance();
		mng->source_unreserve(m_uiSource);
		m_uiSource = 0;
	}
}

/*---------------------------------------------------------------------------*/

void sound::play()
{
	if (!m_uiSource) return;

	alSourcePlay(m_uiSource);
}

/*---------------------------------------------------------------------------*/

void sound::pause()
{
	if (!m_uiSource) return;

	alSourcePause(m_uiSource);
}

/*---------------------------------------------------------------------------*/

void sound::stop()
{
	if (!m_uiSource) return;

	alSourceStop(m_uiSource);
}

/*---------------------------------------------------------------------------*/

void sound::play_buffer(ALuint _buffer)
{
	if (!m_uiSource) return;

	alSourceStop(m_uiSource);
	set_buffer(_buffer);
	alSourcePlay(m_uiSource);
}

/*---------------------------------------------------------------------------*/

void sound::play_buffer(ALuint _buffer, int _loop)
{
	if (!m_uiSource) return;

	alSourceStop(m_uiSource);
	set_buffer(_buffer);
	alSourcePlay(m_uiSource);
	alSourcei(m_uiSource, AL_LOOPING, _loop ? AL_TRUE : AL_FALSE);
}

/*---------------------------------------------------------------------------*/

bool sound::playing()
{
	ALint state;
	alGetSourcei(m_uiSource, AL_SOURCE_STATE, &state);

	return state == AL_PLAYING;

}

/*---------------------------------------------------------------------------*/

void sound::set_loop(int _loop)
{
	if (!m_uiSource) return;
	
	alSourcei(m_uiSource, AL_LOOPING, _loop ? AL_TRUE : AL_FALSE);
}

/*---------------------------------------------------------------------------*/

void sound::set_volume(ALfloat _volume)
{
	if (!m_uiSource) return;	
	
	if(_volume > 1.0f && _volume < 0.0f) return;
	alSourcef(m_uiSource,AL_GAIN,_volume);
}

/*---------------------------------------------------------------------------*/

void sound::set_position    (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ)
{
	if (!m_uiSource) return;
	alSource3f(m_uiSource, AL_POSITION, _fX, _fY, _fZ);
}

/*---------------------------------------------------------------------------*/

void sound::set_velocity    (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ)
{
	if (!m_uiSource) return;
	alSource3f(m_uiSource, AL_VELOCITY, _fX, _fY, _fZ);
}

/*---------------------------------------------------------------------------*/

void sound::set_direction (ALfloat _fX,  ALfloat _fY,  ALfloat _fZ)
{
	if (!m_uiSource) return;
	alSource3f(m_uiSource, AL_DIRECTION, _fX, _fY, _fZ);
}

/*---------------------------------------------------------------------------*/

char* sound::get_error() {return m_sLastError;}

/*---------------------------------------------------------------------------*/

void sound::SetError(std::string _sErr)
{
	delete m_sLastError;
	m_sLastError = new char[_sErr.size() + 1];

	strcpy(m_sLastError, _sErr.c_str());
	_error_callback(_sErr);

#ifdef _DEBUG
	fprintf(stderr, "%s.\n", _sErr.c_str());
#endif
}

/*---------------------------------------------------------------------------*/

} //namespace emyl
