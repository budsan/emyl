/* Copyright (c) 2008-2015 Jordi Santiago Provencio

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "emyl.h"

#include <atomic>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <chrono>

#ifdef _WINDOWS

#include <al.h>
#include <alc.h>

#else

#include <AL/al.h>
#include <AL/alc.h>

#endif

#ifdef ANDROID

#include <android/asset_manager.h>

#endif

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

namespace Emyl {

namespace internal {

void alCheckError(const char* file, unsigned int line, const char* expression)
{
	// Get the last error
	ALenum errorCode = alGetError();

	if (errorCode != AL_NO_ERROR)
	{
		std::string fileString = file;
		std::string error = "Unknown error";
		std::string description = "No description";

		// Decode the error code
		switch (errorCode)
		{
			case AL_INVALID_NAME:
			{
				error = "AL_INVALID_NAME";
				description = "A bad name (ID) has been specified.";
				break;
			}

			case AL_INVALID_ENUM:
			{
				error = "AL_INVALID_ENUM";
				description = "An unacceptable value has been specified for an enumerated argument.";
				break;
			}

			case AL_INVALID_VALUE:
			{
				error = "AL_INVALID_VALUE";
				description = "A numeric argument is out of range.";
				break;
			}

			case AL_INVALID_OPERATION:
			{
				error = "AL_INVALID_OPERATION";
				description = "The specified operation is not allowed in the current state.";
				break;
			}

			case AL_OUT_OF_MEMORY:
			{
				error = "AL_OUT_OF_MEMORY";
				description = "There is not enough memory left to execute the command.";
				break;
			}
		}

		EMYL_WARN("An internal OpenAL call failed in %s (%d).\n"
		"Expression:\n%s\n"
		"Error description:\n   %s\n   %s\n",
		fileString.substr(fileString.find_last_of("\\/") + 1).c_str(),
		line, expression, error.c_str(), description.c_str());
	}
} 

//---------------------------------------------------------------------------//
//-Device--------------------------------------------------------------------//
//---------------------------------------------------------------------------//

class Device
{
public:
	 Device();
	~Device();

	static bool isExtensionSupported(const std::string& extension);
	static int getFormatFromChannelCount(unsigned int channelCount);

	static void setGlobalVolume(float volume);
	static float getGlobalVolume();

	static void setPosition(const Vec3& position);
	static Vec3 getPosition();

	static void setDirection(const Vec3& direction);
	static Vec3 getDirection();

	static void setUpVector(const Vec3& upVector);
	static Vec3 getUpVector();

private:

	friend class Resource;
	friend class Listener;

	bool initialize();
	void deinitialize();

	ALCdevice* m_alDev;
	ALCcontext* m_alContext;

	static Device* instance;
	static float listenerVolume;
	static Vec3 listenerPosition;
	static Vec3 listenerDirection;
	static Vec3 listenerUpVector;

};

//---------------------------------------------------------------------------//

Device* Device::instance(nullptr);

float Device::listenerVolume(100.f);
Vec3 Device::listenerPosition(0.f, 0.f, 0.f);
Vec3 Device::listenerDirection(0.f, 0.f, -1.f);
Vec3 Device::listenerUpVector (0.f, 1.f, 0.f);


//---------------------------------------------------------------------------//

Device::Device()
 : m_alDev(nullptr)
 , m_alContext(nullptr)
{
	initialize();
}

//---------------------------------------------------------------------------//

Device::~Device()
{
	deinitialize();
}

//---------------------------------------------------------------------------//

bool Device::initialize()
{
	m_alDev = alcOpenDevice(nullptr);

	if (m_alDev)
	{
		EMYL_LOG("Audio device name: %s.\n", alcGetString(m_alDev, ALC_DEVICE_SPECIFIER));
		EMYL_LOG("Audio device extensions: %s.\n", alcGetString(m_alDev, ALC_EXTENSIONS));

		m_alContext = alcCreateContext(m_alDev, nullptr);

		if (m_alContext)
		{
			alcMakeContextCurrent(m_alContext);
			float orientation[] = {
				listenerDirection.x,
				listenerDirection.y,
				listenerDirection.z,
				listenerUpVector.x,
				listenerUpVector.y,
				listenerUpVector.z
			};

			alCheck(alListenerf(AL_GAIN, listenerVolume * 0.01f));
			alCheck(alListener3f(AL_POSITION, listenerPosition.x, listenerPosition.y, listenerPosition.z));
			alCheck(alListenerfv(AL_ORIENTATION, orientation));
		}
		else
		{
			EMYL_WARN("OpenAL error: Context can't be created.\n");
			return false;	
		}
	}
	else
	{
		EMYL_WARN("OpenAL error: Could not init OpenAL.\n");
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------//

void Device::deinitialize()
{
	alcMakeContextCurrent(nullptr);
	if (m_alContext)
		alcDestroyContext(m_alContext);

	if (m_alDev)
		alcCloseDevice(m_alDev);
}

//---------------------------------------------------------------------------//

bool Device::isExtensionSupported(const std::string& extension)
{
	// Create a temporary audio device in case none exists yet.
	// This device will not be used in this function and merely
	// makes sure there is a valid OpenAL device for extension
	// queries if none has been created yet.
	ALCdevice* audioDevice;
	std::unique_ptr<Device> device;
	if (instance != nullptr && instance->m_alDev != nullptr)
		audioDevice = instance->m_alDev;
	else
	{
		device = std::unique_ptr<Device>(new Device());
		audioDevice = device->m_alDev;
	}
		
	if ((extension.length() > 2) && (extension.substr(0, 3) == "ALC"))
		return alcIsExtensionPresent(audioDevice, extension.c_str()) != AL_FALSE;
	else
		return alIsExtensionPresent(extension.c_str()) != AL_FALSE;
}

//---------------------------------------------------------------------------//

int Device::getFormatFromChannelCount(unsigned int channelCount)
{
	// Create a temporary audio device in case none exists yet.
	// This device will not be used in this function and merely
	// makes sure there is a valid OpenAL device for format
	// queries if none has been created yet.
	std::unique_ptr<Device> device;
	if (instance == nullptr || instance->m_alDev == nullptr)
		device = std::unique_ptr<Device>(new Device());

	// Find the good format according to the number of channels
	int format = 0;
	switch (channelCount)
	{
		case 1: format = AL_FORMAT_MONO16; break;
		case 2: format = AL_FORMAT_STEREO16; break;
		case 4: format = alGetEnumValue("AL_FORMAT_QUAD16"); break;
		case 6: format = alGetEnumValue("AL_FORMAT_51CHN16"); break;
		case 7: format = alGetEnumValue("AL_FORMAT_61CHN16"); break;
		case 8: format = alGetEnumValue("AL_FORMAT_71CHN16"); break;
		default: format = 0; break;
	}

	// Fixes a bug on OS X
	if (format == -1)
		format = 0;

	return format;
}

//---------------------------------------------------------------------------//

void Device::setGlobalVolume(float volume)
{
	if (instance && instance->m_alContext)
		alCheck(alListenerf(AL_GAIN, volume * 0.01f));

	listenerVolume = volume;
}

//---------------------------------------------------------------------------//

float Device::getGlobalVolume()
{
	return listenerVolume;
}

//---------------------------------------------------------------------------//

void Device::setPosition(const Vec3& position)
{
	if (instance && instance->m_alContext)
		alCheck(alListener3f(AL_POSITION, position.x, position.y, position.z));

	listenerPosition = position;
}

//---------------------------------------------------------------------------//

Vec3 Device::getPosition()
{
	return listenerPosition;
}

//---------------------------------------------------------------------------//

void Device::setDirection(const Vec3& direction)
{
	if (instance && instance->m_alContext)
	{
		float orientation[] = {direction.x, direction.y, direction.z, listenerUpVector.x, listenerUpVector.y, listenerUpVector.z};
		alCheck(alListenerfv(AL_ORIENTATION, orientation));
	}

	listenerDirection = direction;
}

//---------------------------------------------------------------------------//

Vec3 Device::getDirection()
{
	return listenerDirection;
}


//---------------------------------------------------------------------------//

void Device::setUpVector(const Vec3& upVector)
{
	if (instance && instance->m_alContext)
	{
		float orientation[] = {
			listenerDirection.x,
			listenerDirection.y,
			listenerDirection.z,
			upVector.x, upVector.y, upVector.z};
		alCheck(alListenerfv(AL_ORIENTATION, orientation));
	}

	listenerUpVector = upVector;
}

//---------------------------------------------------------------------------//

Vec3 Device::getUpVector()
{
	return listenerUpVector;
}

//---------------------------------------------------------------------------//
//-Resource------------------------------------------------------------------//
//---------------------------------------------------------------------------//

std::atomic<unsigned int> s_resourceCount(0);

//---------------------------------------------------------------------------//

Resource::Resource()
{
	unsigned int currentCount = s_resourceCount++;

	if (currentCount == 0)
	{
		Device::instance = new Device;
	}
}

//---------------------------------------------------------------------------//

Resource::~Resource()
{
	unsigned int currentCount = --s_resourceCount;

	if (currentCount == 0)
	{
		delete Device::instance;
		Device::instance = nullptr;
	}
}

//---------------------------------------------------------------------------//
//-Android-ResourceStream----------------------------------------------------//
//---------------------------------------------------------------------------//

#ifdef ANDROID

class ResourceStream : public InputStream
{
public:

	ResourceStream(const std::string& filename);
	~ResourceStream();
	std::int64_t read(void *data, std::int64_t size);
	std::int64_t seek(std::int64_t position);
	std::int64_t tell();
	std::int64_t getSize();

private:

	AAsset* m_file; ///< The asset file to read
};

//---------------------------------------------------------------------------//

ResourceStream::ResourceStream(const std::string& filename) :
m_file (NULL)
{
	ActivityStates* states = getActivity(NULL);
	Lock(states->mutex);
	m_file = AAssetManager_open(states->activity->assetManager, filename.c_str(), AASSET_MODE_UNKNOWN);
}

//---------------------------------------------------------------------------//

ResourceStream::~ResourceStream()
{
	AAsset_close(m_file);
}

//---------------------------------------------------------------------------//

std::int64_t ResourceStream::read(void *data, std::int64_t size)
{
	return AAsset_read(m_file, data, size);
}

//---------------------------------------------------------------------------//

std::int64_t ResourceStream::seek(std::int64_t position)
{
	return AAsset_seek(m_file, position, SEEK_SET);
}

//---------------------------------------------------------------------------//

std::int64_t ResourceStream::tell()
{
	return getSize() - AAsset_getRemainingLength(m_file);
}

//---------------------------------------------------------------------------//

std::int64_t ResourceStream::getSize()
{
	return AAsset_getLength(m_file);
}

#endif

//---------------------------------------------------------------------------//
//-SoundFileFactory----------------------------------------------------------//
//---------------------------------------------------------------------------//

class SoundFileFactory
{
public:

	static SoundFileReader* createReaderFromFilename(const std::string& filename);
	static SoundFileReader* createReaderFromMemory(const void* data, std::size_t sizeInBytes);
	static SoundFileReader* createReaderFromStream(InputStream& stream);

	static void AddReader(ReaderFactory& factory);
	static void RemoveReader(SoundFileReader* (*create)());

private:

	typedef std::vector<ReaderFactory> ReaderFactoryArray;
	static ReaderFactoryArray s_readers;
};

//---------------------------------------------------------------------------//

SoundFileFactory::ReaderFactoryArray SoundFileFactory::s_readers;

//---------------------------------------------------------------------------//

SoundFileReader* SoundFileFactory::createReaderFromFilename(const std::string& filename)
{
	// Wrap the input file into a file stream
	FileInputStream stream;
	if (!stream.open(filename)) {
		EMYL_WARN("Failed to open sound file \"" << filename << "\" (couldn't open stream)\n");
		return NULL;
	}

	// Test the filename in all the registered factories
	for (ReaderFactoryArray::const_iterator it = s_readers.begin(); it != s_readers.end(); ++it)
	{
		stream.seek(0);
		if (it->check(stream))
			return it->create();
	}

	// No suitable reader found
	EMYL_WARN("Failed to open sound file \"" << filename << "\" (format not supported)\n");
	return NULL;
}

//---------------------------------------------------------------------------//

SoundFileReader* SoundFileFactory::createReaderFromMemory(const void* data, std::size_t sizeInBytes)
{
	// Wrap the memory file into a file stream
	MemoryInputStream stream;
	stream.open(data, sizeInBytes);

	// Test the stream for all the registered factories
	for (ReaderFactoryArray::const_iterator it = s_readers.begin(); it != s_readers.end(); ++it)
	{
		stream.seek(0);
		if (it->check(stream))
			return it->create();
	}

	// No suitable reader found
	EMYL_WARN("Failed to open sound file from memory (format not supported)\n");
	return NULL;
}

//---------------------------------------------------------------------------//

SoundFileReader* SoundFileFactory::createReaderFromStream(InputStream& stream)
{
	// Test the stream for all the registered factories
	for (ReaderFactoryArray::const_iterator it = s_readers.begin(); it != s_readers.end(); ++it)
	{
		stream.seek(0);
		if (it->check(stream))
			return it->create();
	}

	// No suitable reader found
	EMYL_WARN("Failed to open sound file from stream (format not supported)\n");
	return NULL;
}

//---------------------------------------------------------------------------//

void SoundFileFactory::AddReader(ReaderFactory& factory)
{
	s_readers.push_back(factory);
}

//---------------------------------------------------------------------------//

void SoundFileFactory::RemoveReader(SoundFileReader* (*create)())
{
	for (ReaderFactoryArray::iterator it = s_readers.begin(); it != s_readers.end(); )
	{
		if (it->create == create)
			it = s_readers.erase(it);
		else
			++it;
	}
}

//---------------------------------------------------------------------------//
//-ReaderFactory-------------------------------------------------------------//
//---------------------------------------------------------------------------//

void ReaderFactoryAdd(ReaderFactory& factory)
{
	SoundFileFactory::AddReader(factory);
}

//---------------------------------------------------------------------------//

void ReaderFactoryRemove(SoundFileReader* (*create)())
{
	SoundFileFactory::RemoveReader(create);
}

} //namespace internal

//---------------------------------------------------------------------------//
//-Listener------------------------------------------------------------------//
//---------------------------------------------------------------------------//

void Listener::setGlobalVolume(float volume)
{
	internal::Device::setGlobalVolume(volume);
}

//---------------------------------------------------------------------------//

float Listener::getGlobalVolume()
{
	return internal::Device::getGlobalVolume();
}

//---------------------------------------------------------------------------//

void Listener::setPosition(float x, float y, float z)
{
	setPosition(Vec3(x, y, z));
}

//---------------------------------------------------------------------------//

void Listener::setPosition(const Vec3& position)
{
	internal::Device::setPosition(position);
}

//---------------------------------------------------------------------------//

Vec3 Listener::getPosition()
{
	return internal::Device::getPosition();
}

//---------------------------------------------------------------------------//

void Listener::setDirection(float x, float y, float z)
{
	setDirection(Vec3(x, y, z));
}

//---------------------------------------------------------------------------//

void Listener::setDirection(const Vec3& direction)
{
	internal::Device::setDirection(direction);
}

//---------------------------------------------------------------------------//

Vec3 Listener::getDirection()
{
	return internal::Device::getDirection();
}

//---------------------------------------------------------------------------//

void Listener::setUpVector(float x, float y, float z)
{
	setUpVector(Vec3(x, y, z));
}

//---------------------------------------------------------------------------//

void Listener::setUpVector(const Vec3& upVector)
{
	internal::Device::setUpVector(upVector);
}

//---------------------------------------------------------------------------//

Vec3 Listener::getUpVector()
{
	return internal::Device::getUpVector();
}

//---------------------------------------------------------------------------//
//-Source--------------------------------------------------------------------//
//---------------------------------------------------------------------------//

Source::Source()
{
	alCheck(alGenSources(1, &m_source));
	alCheck(alSourcei(m_source, AL_BUFFER, 0));
}

//---------------------------------------------------------------------------//

Source::Source(const Source& copy)
{
	alCheck(alGenSources(1, &m_source));
	alCheck(alSourcei(m_source, AL_BUFFER, 0));

	setPitch(copy.getPitch());
	setVolume(copy.getVolume());
	setPosition(copy.getPosition());
	setRelativeToListener(copy.isRelativeToListener());
	setMinDistance(copy.getMinDistance());
	setAttenuation(copy.getAttenuation());
}

//---------------------------------------------------------------------------//

Source::~Source()
{
	alCheck(alSourcei(m_source, AL_BUFFER, 0));
	alCheck(alDeleteSources(1, &m_source));
}


//---------------------------------------------------------------------------//

void Source::setPitch(float pitch)
{
	alCheck(alSourcef(m_source, AL_PITCH, pitch));
}


//---------------------------------------------------------------------------//

void Source::setVolume(float volume)
{
	alCheck(alSourcef(m_source, AL_GAIN, volume * 0.01f));
}


//---------------------------------------------------------------------------//

void Source::setPosition(float x, float y, float z)
{
	alCheck(alSource3f(m_source, AL_POSITION, x, y, z));
}

//---------------------------------------------------------------------------//

void Source::setPosition(const Vec3& position)
{
	setPosition(position.x, position.y, position.z);
}

//---------------------------------------------------------------------------//

void Source::setRelativeToListener(bool relative)
{
	alCheck(alSourcei(m_source, AL_SOURCE_RELATIVE, relative));
}

//---------------------------------------------------------------------------//

void Source::setMinDistance(float distance)
{
	alCheck(alSourcef(m_source, AL_REFERENCE_DISTANCE, distance));
}

//---------------------------------------------------------------------------//

void Source::setAttenuation(float attenuation)
{
	alCheck(alSourcef(m_source, AL_ROLLOFF_FACTOR, attenuation));
}

//---------------------------------------------------------------------------//

float Source::getPitch() const
{
	ALfloat pitch;
	alCheck(alGetSourcef(m_source, AL_PITCH, &pitch));

	return pitch;
}

//---------------------------------------------------------------------------//

float Source::getVolume() const
{
	ALfloat gain;
	alCheck(alGetSourcef(m_source, AL_GAIN, &gain));

	return gain * 100.f;
}

//---------------------------------------------------------------------------//

Vec3 Source::getPosition() const
{
	Vec3 position;
	alCheck(alGetSource3f(m_source, AL_POSITION, &position.x, &position.y, &position.z));

	return position;
}

//---------------------------------------------------------------------------//

bool Source::isRelativeToListener() const
{
	ALint relative;
	alCheck(alGetSourcei(m_source, AL_SOURCE_RELATIVE, &relative));

	return relative != 0;
}

//---------------------------------------------------------------------------//

float Source::getMinDistance() const
{
	ALfloat distance;
	alCheck(alGetSourcef(m_source, AL_REFERENCE_DISTANCE, &distance));

	return distance;
}

//---------------------------------------------------------------------------//

float Source::getAttenuation() const
{
	ALfloat attenuation;
	alCheck(alGetSourcef(m_source, AL_ROLLOFF_FACTOR, &attenuation));

	return attenuation;
}

//---------------------------------------------------------------------------//

Source& Source::operator=(const Source& right)
{
	setPitch(right.getPitch());
	setVolume(right.getVolume());
	setPosition(right.getPosition());
	setRelativeToListener(right.isRelativeToListener());
	setMinDistance(right.getMinDistance());
	setAttenuation(right.getAttenuation());

	return *this;
}

//---------------------------------------------------------------------------//

Source::State Source::getState() const
{
	ALint status;
	alCheck(alGetSourcei(m_source, AL_SOURCE_STATE, &status));

	switch (status)
	{
		case AL_INITIAL:
		case AL_STOPPED: return Stopped;
		case AL_PAUSED: return Paused;
		case AL_PLAYING: return Playing;
	}

	return Stopped;
}

//---------------------------------------------------------------------------//
//-Sound---------------------------------------------------------------------//
//---------------------------------------------------------------------------//

Sound::Sound()
 : m_buffer(nullptr)
{
}


//---------------------------------------------------------------------------//

Sound::Sound(const Buffer& buffer)
 : m_buffer(nullptr)
{
	setBuffer(buffer);
}


//---------------------------------------------------------------------------//

Sound::Sound(const Sound& copy)
 : Source(copy)
 , m_buffer(nullptr)
{
	if (copy.m_buffer)
		setBuffer(*copy.m_buffer);

	setLoop(copy.getLoop());
}

//---------------------------------------------------------------------------//

Sound::~Sound()
{
	stop();
	if (m_buffer)
		m_buffer->detachSound(this);
}

//---------------------------------------------------------------------------//

void Sound::play()
{
	alCheck(alSourcePlay(m_source));
}

//---------------------------------------------------------------------------//

void Sound::pause()
{
	alCheck(alSourcePause(m_source));
}

//---------------------------------------------------------------------------//

void Sound::stop()
{
	alCheck(alSourceStop(m_source));
}

//---------------------------------------------------------------------------//

void Sound::setBuffer(const Buffer& buffer)
{
	// First detach from the previous buffer
	if (m_buffer)
	{
		stop();
		m_buffer->detachSound(this);
	}

	// Assign and use the new buffer
	m_buffer = &buffer;
	m_buffer->attachSound(this);
	alCheck(alSourcei(m_source, AL_BUFFER, m_buffer->m_buffer));
}

//---------------------------------------------------------------------------//

void Sound::setLoop(bool loop)
{
	alCheck(alSourcei(m_source, AL_LOOPING, loop));
}

//---------------------------------------------------------------------------//

void Sound::setPlayingOffset(ALfloat timeOffset)
{
	alCheck(alSourcef(m_source, AL_SEC_OFFSET, timeOffset));
}

//---------------------------------------------------------------------------//

const Buffer* Sound::getBuffer() const
{
	return m_buffer;
}

//---------------------------------------------------------------------------//

bool Sound::getLoop() const
{
	ALint loop;
	alCheck(alGetSourcei(m_source, AL_LOOPING, &loop));

	return loop != 0;
}

//---------------------------------------------------------------------------//

ALfloat Sound::getPlayingOffset() const
{
	ALfloat secs = 0.f;
	alCheck(alGetSourcef(m_source, AL_SEC_OFFSET, &secs));

	return secs;
}

//---------------------------------------------------------------------------//

Sound::State Sound::getState() const
{
	return Source::getState();
}

//---------------------------------------------------------------------------//

Sound& Sound::operator =(const Sound& right)
{
	Source::operator=(right);

	if (m_buffer)
	{
		stop();
		m_buffer->detachSound(this);
		m_buffer = NULL;
	}

	if (right.m_buffer)
		setBuffer(*right.m_buffer);

	setLoop(right.getLoop());

	return *this;
}

//---------------------------------------------------------------------------//

void Sound::resetBuffer()
{
	stop();

	if (m_buffer)
	{
		alCheck(alSourcei(m_source, AL_BUFFER, 0));
		m_buffer->detachSound(this);
		m_buffer = nullptr;
	}
}

//---------------------------------------------------------------------------//
//-FileInputStream-----------------------------------------------------------//
//---------------------------------------------------------------------------//

#ifdef ANDROID

FileInputStream::FileInputStream()
 : m_file(NULL)
{
}

//---------------------------------------------------------------------------//

FileInputStream::~FileInputStream()
{
	if (m_file)
		delete m_file;
}

//---------------------------------------------------------------------------//

bool FileInputStream::open(const std::string& filename)
{

	if (m_file)
		delete m_file;
	m_file = new priv::ResourceStream(filename);
	return m_file->tell() != -1;
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::read(void* data, std::int64_t size)
{
	return m_file->read(data, size);
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::seek(std::int64_t position)
{
	return m_file->seek(position);
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::tell()
{
	return m_file->tell();
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::getSize()
{
	return m_file->getSize();
}

#else //---------------------------------------------------------------------//

FileInputStream::FileInputStream()
 : m_file(NULL)
{
}

//---------------------------------------------------------------------------//

FileInputStream::~FileInputStream()
{
	if (m_file)
		std::fclose(m_file);
}

//---------------------------------------------------------------------------//

bool FileInputStream::open(const std::string& filename)
{
	if (m_file)
		std::fclose(m_file);

	m_file = std::fopen(filename.c_str(), "rb");

	return m_file != NULL;
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::read(void* data, std::int64_t size)
{
	if (m_file)
		return std::fread(data, 1, static_cast<std::size_t>(size), m_file);
	else
		return -1;
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::seek(std::int64_t position)
{
	if (m_file)
	{
		std::fseek(m_file, static_cast<std::size_t>(position), SEEK_SET);
		return tell();
	}
	else
	{
		return -1;
	}
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::tell()
{
	if (m_file)
		return std::ftell(m_file);
	else
		return -1;
}

//---------------------------------------------------------------------------//

std::int64_t FileInputStream::getSize()
{
	if (m_file)
	{
		std::int64_t position = tell();
		std::fseek(m_file, 0, SEEK_END);
		std::int64_t size = tell();
		seek(position);
		return size;
	}
	else
	{
		return -1;
	}
}

#endif

//---------------------------------------------------------------------------//
//-MemoryInputStream---------------------------------------------------------//
//---------------------------------------------------------------------------//

MemoryInputStream::MemoryInputStream()
 : m_data  (NULL)
 , m_size  (0)
 , m_offset(0)
{
}

//---------------------------------------------------------------------------//

void MemoryInputStream::open(const void* data, std::size_t sizeInBytes)
{
	m_data = static_cast<const char*>(data);
	m_size = sizeInBytes;
	m_offset = 0;
}

//---------------------------------------------------------------------------//

std::int64_t MemoryInputStream::read(void* data, std::int64_t size)
{
	if (!m_data)
		return -1;

	std::int64_t endPosition = m_offset + size;
	std::int64_t count = endPosition <= m_size ? size : m_size - m_offset;

	if (count > 0)
	{
		std::memcpy(data, m_data + m_offset, static_cast<std::size_t>(count));
		m_offset += count;
	}

	return count;
}

//---------------------------------------------------------------------------//

std::int64_t MemoryInputStream::seek(std::int64_t position)
{
	if (!m_data)
		return -1;

	m_offset = position < m_size ? position : m_size;
	return m_offset;
}

//---------------------------------------------------------------------------//

std::int64_t MemoryInputStream::tell()
{
	if (!m_data)
		return -1;

	return m_offset;
}

//---------------------------------------------------------------------------//

std::int64_t MemoryInputStream::getSize()
{
	if (!m_data)
		return -1;

	return m_size;
}

//---------------------------------------------------------------------------//
//-InputSoundFile------------------------------------------------------------//
//---------------------------------------------------------------------------//

InputSoundFile::InputSoundFile()
 : m_reader(NULL)
 , m_stream(NULL)
 , m_streamOwned (false)
 , m_sampleCount (0)
 , m_channelCount(0)
 , m_sampleRate(0)
{
}

//---------------------------------------------------------------------------//

InputSoundFile::~InputSoundFile()
{
	close();
}

//---------------------------------------------------------------------------//

bool InputSoundFile::openFromFile(const std::string& filename)
{
	// If the file is already open, first close it
	close();

	// Find a suitable reader for the file type
	m_reader = internal::SoundFileFactory::createReaderFromFilename(filename);
	if (!m_reader)
		return false;

	// Wrap the file into a stream
	FileInputStream* file = new FileInputStream;
	m_stream = file;
	m_streamOwned = true;

	// Open it
	if (!file->open(filename))
	{
		close();
		return false;
	}

	// Pass the stream to the reader
	SoundFileReader::Info info;
	if (!m_reader->open(*file, info))
	{
		close();
		return false;
	}

	// Retrieve the attributes of the open sound file
	m_sampleCount = info.sampleCount;
	m_channelCount = info.channelCount;
	m_sampleRate = info.sampleRate;

	return true;
}

//---------------------------------------------------------------------------//

bool InputSoundFile::openFromMemory(const void* data, std::size_t sizeInBytes)
{
	// If the file is already open, first close it
	close();

	// Find a suitable reader for the file type
	m_reader = internal::SoundFileFactory::createReaderFromMemory(data, sizeInBytes);
	if (!m_reader)
		return false;

	// Wrap the memory file into a stream
	MemoryInputStream* memory = new MemoryInputStream;
	m_stream = memory;
	m_streamOwned = true;

	// Open it
	memory->open(data, sizeInBytes);

	// Pass the stream to the reader
	SoundFileReader::Info info;
	if (!m_reader->open(*memory, info))
	{
		close();
		return false;
	}

	// Retrieve the attributes of the open sound file
	m_sampleCount = info.sampleCount;
	m_channelCount = info.channelCount;
	m_sampleRate = info.sampleRate;

	return true;
}

//---------------------------------------------------------------------------//

bool InputSoundFile::openFromStream(InputStream& stream)
{
	// If the file is already open, first close it
	close();

	// Find a suitable reader for the file type
	m_reader = internal::SoundFileFactory::createReaderFromStream(stream);
	if (!m_reader)
		return false;

	// store the stream
	m_stream = &stream;
	m_streamOwned = false;

	// Don't forget to reset the stream to its beginning before re-opening it
	if (stream.seek(0) != 0)
	{
		EMYL_WARN("Failed to open sound file from stream (cannot restart stream)\n");
		return false;
	}

	// Pass the stream to the reader
	SoundFileReader::Info info;
	if (!m_reader->open(stream, info))
	{
		close();
		return false;
	}

	// Retrieve the attributes of the open sound file
	m_sampleCount = info.sampleCount;
	m_channelCount = info.channelCount;
	m_sampleRate = info.sampleRate;

	return true;
}

//---------------------------------------------------------------------------//

std::uint64_t InputSoundFile::getSampleCount() const
{
	return m_sampleCount;
}

//---------------------------------------------------------------------------//

unsigned int InputSoundFile::getChannelCount() const
{
	return m_channelCount;
}

//---------------------------------------------------------------------------//

unsigned int InputSoundFile::getSampleRate() const
{
	return m_sampleRate;
}

//---------------------------------------------------------------------------//

ALfloat InputSoundFile::getDuration() const
{
	return static_cast<float>(m_sampleCount) / m_channelCount / m_sampleRate;
}

//---------------------------------------------------------------------------//

void InputSoundFile::seek(std::uint64_t sampleOffset)
{
	if (m_reader)
		m_reader->seek(sampleOffset);
}

//---------------------------------------------------------------------------//

void InputSoundFile::seek(ALfloat timeOffset)
{
	seek(static_cast<std::uint64_t>(timeOffset * m_sampleRate * m_channelCount));
}

//---------------------------------------------------------------------------//

std::uint64_t InputSoundFile::read(std::int16_t* samples, std::uint64_t maxCount)
{
	if (m_reader && samples && maxCount)
		return m_reader->read(samples, maxCount);
	else
		return 0;
}

//---------------------------------------------------------------------------//

void InputSoundFile::close()
{
	// Destroy the reader
	delete m_reader;
	m_reader = NULL;

	// Destroy the stream if we own it
	if (m_streamOwned)
	{
		delete m_stream;
		m_streamOwned = false;
	}
	m_stream = NULL;

	// Reset the sound file attributes
	m_sampleCount = 0;
	m_channelCount = 0;
	m_sampleRate = 0;
}

//---------------------------------------------------------------------------//
//-Buffer--------------------------------------------------------------------//
//---------------------------------------------------------------------------//

Buffer::Buffer()
 : m_buffer(0)
 , m_duration()
{
	alCheck(alGenBuffers(1, &m_buffer));
}

//---------------------------------------------------------------------------//

Buffer::Buffer(const Buffer& copy)
 : m_buffer(0)
 , m_samples(copy.m_samples)
 , m_duration(copy.m_duration)
 , m_sounds()
{
	alCheck(alGenBuffers(1, &m_buffer));

	// Update the internal buffer with the new samples
	update(copy.getChannelCount(), copy.getSampleRate());
}

//---------------------------------------------------------------------------//

Buffer::~Buffer()
{
	SoundList sounds;
	sounds.swap(m_sounds);

	for (SoundList::const_iterator it = sounds.begin(); it != sounds.end(); ++it)
		(*it)->resetBuffer();

	if (m_buffer)
		alCheck(alDeleteBuffers(1, &m_buffer));
}

//---------------------------------------------------------------------------//

bool Buffer::loadFromFile(const std::string& filename)
{
	InputSoundFile file;
	if (file.openFromFile(filename))
		return initialize(file);
	else
		return false;
}

//---------------------------------------------------------------------------//

bool Buffer::loadFromMemory(const void* data, std::size_t sizeInBytes)
{
	InputSoundFile file;
	if (file.openFromMemory(data, sizeInBytes))
		return initialize(file);
	else
		return false;
}

//---------------------------------------------------------------------------//

bool Buffer::loadFromStream(InputStream& stream)
{
	InputSoundFile file;
	if (file.openFromStream(stream))
		return initialize(file);
	else
		return false;
}

//---------------------------------------------------------------------------//

bool Buffer::loadFromSamples(const std::int16_t* samples, std::uint64_t sampleCount, unsigned int channelCount, unsigned int sampleRate)
{
	if (samples && sampleCount && channelCount && sampleRate)
	{
		m_samples.assign(samples, samples + sampleCount);
		return update(channelCount, sampleRate);
	}
	else
	{
		EMYL_WARN("Failed to load sound buffer from samples ("
			"array: %p, count: %d, channels: %d, samplerate: %d)\n"
			, samples, sampleCount, channelCount, sampleRate);

		return false;
	}
}

//---------------------------------------------------------------------------//

const std::int16_t* Buffer::getSamples() const
{
	return m_samples.empty() ? nullptr : &m_samples[0];
}

//---------------------------------------------------------------------------//

std::uint64_t Buffer::getSampleCount() const
{
	return m_samples.size();
}

//---------------------------------------------------------------------------//

unsigned int Buffer::getSampleRate() const
{
	ALint sampleRate;
	alCheck(alGetBufferi(m_buffer, AL_FREQUENCY, &sampleRate));

	return sampleRate;
}

//---------------------------------------------------------------------------//

unsigned int Buffer::getChannelCount() const
{
	ALint channelCount;
	alCheck(alGetBufferi(m_buffer, AL_CHANNELS, &channelCount));

	return channelCount;
}

//---------------------------------------------------------------------------//

ALfloat Buffer::getDuration() const
{
	return m_duration;
}

//---------------------------------------------------------------------------//

Buffer& Buffer::operator =(const Buffer& right)
{
	Buffer temp(right);

	std::swap(m_samples, temp.m_samples);
	std::swap(m_buffer, temp.m_buffer);
	std::swap(m_duration, temp.m_duration);
	std::swap(m_sounds, temp.m_sounds); // swap sounds too, so that they are detached when temp is destroyed

	return *this;
}

//---------------------------------------------------------------------------//

bool Buffer::initialize(InputSoundFile& file)
{
	// Retrieve the sound parameters
	std::uint64_t sampleCount = file.getSampleCount();
	unsigned int channelCount = file.getChannelCount();
	unsigned int sampleRate = file.getSampleRate();

	// Read the samples from the provided file
	m_samples.resize(static_cast<std::size_t>(sampleCount));
	if (file.read(&m_samples[0], sampleCount) == sampleCount)
	{
		// Update the internal buffer with the new samples
		return update(channelCount, sampleRate);
	}
	else
	{
		return false;
	}
}

//---------------------------------------------------------------------------//

bool Buffer::update(unsigned int channelCount, unsigned int sampleRate)
{
	// Check parameters
	if (!channelCount || !sampleRate || m_samples.empty())
		return false;

	// Find the good format according to the number of channels
	ALenum format = internal::Device::getFormatFromChannelCount(channelCount);

	// Check if the format is valid
	if (format == 0)
	{
		EMYL_WARN("Failed to load sound buffer (unsupported number of channels: %d)\n",channelCount);
		return false;
	}

	// First make a copy of the list of sounds so we can reattach later
	SoundList sounds(m_sounds);

	// Detach the buffer from the sounds that use it (to avoid OpenAL errors)
	for (SoundList::const_iterator it = sounds.begin(); it != sounds.end(); ++it)
		(*it)->resetBuffer();

	// Fill the buffer
	ALsizei size = static_cast<ALsizei>(m_samples.size()) * sizeof(std::int16_t);
	alCheck(alBufferData(m_buffer, format, &m_samples[0], size, sampleRate));

	// Compute the duration
	m_duration = static_cast<float>(m_samples.size()) / sampleRate / channelCount;

	// Now reattach the buffer to the sounds that use it
	for (SoundList::const_iterator it = sounds.begin(); it != sounds.end(); ++it)
		(*it)->setBuffer(*this);

	return true;
}

//---------------------------------------------------------------------------//

void Buffer::attachSound(Sound* sound) const
{
	m_sounds.insert(sound);
}

//---------------------------------------------------------------------------//

void Buffer::detachSound(Sound* sound) const
{
	m_sounds.erase(sound);
}

//---------------------------------------------------------------------------//
//-Stream--------------------------------------------------------------------//
//---------------------------------------------------------------------------//

Stream::Stream()
 : m_thread()
 , m_threadMutex()
 , m_threadStartState(Stopped)
 , m_isStreaming(false)
 , m_buffers()
 , m_channelCount(0)
 , m_sampleRate(0)
 , m_format(0)
 , m_loop(false)
 , m_samplesProcessed(0)
 , m_endBuffers()
{

}

//---------------------------------------------------------------------------//

Stream::~Stream()
{
	// Stop the sound if it was playing

	// Request the thread to terminate
	{
		std::lock_guard<std::mutex> lock(m_threadMutex);
		m_isStreaming = false;
	}

	// Wait for the thread to terminate
	if(m_thread.joinable())
		m_thread.join();
}

//---------------------------------------------------------------------------//

void Stream::initialize(unsigned int channelCount, unsigned int sampleRate)
{
	m_channelCount = channelCount;
	m_sampleRate   = sampleRate;

	// Deduce the format from the number of channels
	m_format = internal::Device::getFormatFromChannelCount(channelCount);

	// Check if the format is valid
	if (m_format == 0)
	{
		m_channelCount = 0;
		m_sampleRate   = 0;
		EMYL_WARN("Unsupported number of channels (%d)\n", m_channelCount);
	}
}

//---------------------------------------------------------------------------//

void Stream::play()
{
	// Check if the sound parameters have been set
	if (m_format == 0)
	{
		EMYL_WARN("Failed to play audio stream: sound parameters have not been initialized (call initialize() first)\n");
		return;
	}

	bool isStreaming = false;
	State threadStartState = Stopped;

	{
		std::lock_guard<std::mutex> lock(m_threadMutex);

		isStreaming = m_isStreaming;
		threadStartState = m_threadStartState;
	}


	if (isStreaming && (threadStartState == Paused))
	{
		// If the sound is paused, resume it
		std::lock_guard<std::mutex> lock(m_threadMutex);
		m_threadStartState = Playing;
		alCheck(alSourcePlay(m_source));
		return;
	}
	else if (isStreaming && (threadStartState == Playing))
	{
		// If the sound is playing, stop it and continue as if it was stopped
		stop();
	}

	// Move to the beginning
	onSeek(0.f);

	// Start updating the stream in a separate thread to avoid blocking the application
	m_samplesProcessed = 0;
	m_isStreaming = true;
	m_threadStartState = Playing;
	m_thread = std::thread(&Stream::streamData, this);
}

//---------------------------------------------------------------------------//

void Stream::pause()
{
	// Handle pause() being called before the thread has started
	{
		std::lock_guard<std::mutex> lock(m_threadMutex);

		if (!m_isStreaming)
			return;

		m_threadStartState = Paused;
	}

	alCheck(alSourcePause(m_source));
}

//---------------------------------------------------------------------------//

void Stream::stop()
{
	// Request the thread to terminate
	{
		std::lock_guard<std::mutex> lock(m_threadMutex);
		m_isStreaming = false;
	}

	// Wait for the thread to terminate
	if(m_thread.joinable())
		m_thread.join();

	// Move to the beginning
	onSeek(0.f);

	// Reset the playing position
	m_samplesProcessed = 0;
}

//---------------------------------------------------------------------------//

unsigned int Stream::getChannelCount() const
{
	return m_channelCount;
}

//---------------------------------------------------------------------------//

unsigned int Stream::getSampleRate() const
{
	return m_sampleRate;
}

//---------------------------------------------------------------------------//

Stream::State Stream::getState() const
{
	State status = Source::getState();

	// To compensate for the lag between play() and alSourceplay()
	if (status == Stopped)
	{
		std::lock_guard<std::mutex> lock(m_threadMutex);

		if (m_isStreaming)
			status = m_threadStartState;
	}

	return status;
}

//---------------------------------------------------------------------------//

void Stream::setPlayingOffset(ALfloat timeOffset)
{
	// Get old playing status
	State oldState = getState();

	// Stop the stream
	stop();

	// Let the derived class update the current position
	onSeek(timeOffset);

	// Restart streaming
	m_samplesProcessed = static_cast<std::uint64_t>(timeOffset * m_sampleRate * m_channelCount);

	if (oldState == Stopped)
		return;

	m_isStreaming = true;
	m_threadStartState = oldState;
	m_thread = std::thread(&Stream::streamData, this);
}

//---------------------------------------------------------------------------//

ALfloat Stream::getPlayingOffset() const
{
	if (m_sampleRate && m_channelCount)
	{
		ALfloat secs = 0.f;
		alCheck(alGetSourcef(m_source, AL_SEC_OFFSET, &secs));

		return secs + static_cast<float>(m_samplesProcessed) / m_sampleRate / m_channelCount;
	}
	else
	{
		return 0.f;
	}
}

//---------------------------------------------------------------------------//

void Stream::setLoop(bool loop)
{
	m_loop = loop;
}

//---------------------------------------------------------------------------//

bool Stream::getLoop() const
{
	return m_loop;
}

//---------------------------------------------------------------------------//

void Stream::streamData()
{
	bool requestStop = false;

	{
		std::lock_guard<std::mutex> lock(m_threadMutex);

		// Check if the thread was launched Stopped
		if (m_threadStartState == Stopped)
		{
			m_isStreaming = false;
			return;
		}
	}

	// Create the buffers
	alCheck(alGenBuffers(BufferCount, m_buffers));
	for (int i = 0; i < BufferCount; ++i)
		m_endBuffers[i] = false;

	// Fill the queue
	requestStop = fillQueue();

	// Play the sound
	alCheck(alSourcePlay(m_source));

	{
		std::lock_guard<std::mutex> lock(m_threadMutex);

		// Check if the thread was launched Paused
		if (m_threadStartState == Paused)
			alCheck(alSourcePause(m_source));
	}

	for (;;)
	{
		{
			std::lock_guard<std::mutex> lock(m_threadMutex);
			if (!m_isStreaming)
				break;
		}

		// The stream has been interrupted!
		if (Source::getState() == Stopped)
		{
			if (!requestStop)
			{
				// Just continue
				alCheck(alSourcePlay(m_source));
			}
			else
			{
				// End streaming
				std::lock_guard<std::mutex> lock(m_threadMutex);
				m_isStreaming = false;
			}
		}

		// Get the number of buffers that have been processed (i.e. ready for reuse)
		ALint nbProcessed = 0;
		alCheck(alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &nbProcessed));

		while (nbProcessed--)
		{
			// Pop the first unused buffer from the queue
			ALuint buffer;
			alCheck(alSourceUnqueueBuffers(m_source, 1, &buffer));

			// Find its number
			unsigned int bufferNum = 0;
			for (int i = 0; i < BufferCount; ++i)
				if (m_buffers[i] == buffer)
				{
					bufferNum = i;
					break;
				}

			// Retrieve its size and add it to the samples count
			if (m_endBuffers[bufferNum])
			{
				// This was the last buffer: reset the sample count
				m_samplesProcessed = 0;
				m_endBuffers[bufferNum] = false;
			}
			else
			{
				ALint size, bits;
				alCheck(alGetBufferi(buffer, AL_SIZE, &size));
				alCheck(alGetBufferi(buffer, AL_BITS, &bits));

				// Bits can be 0 if the format or parameters are corrupt, avoid division by zero
				if (bits == 0)
				{
					EMYL_WARN("Bits in sound stream are 0: make sure that the audio format is not corrupt "
						  "and initialize() has been called correctly\n");

					// Abort streaming (exit main loop)
					std::lock_guard<std::mutex> lock(m_threadMutex);
					m_isStreaming = false;
					requestStop = true;
					break;
				}
				else
				{
					m_samplesProcessed += size / (bits / 8);
				}
			}

			// Fill it and push it back into the playing queue
			if (!requestStop)
			{
				if (fillAndPushBuffer(bufferNum))
					requestStop = true;
			}
		}

		// Leave some time for the other threads if the stream is still playing
		if (Source::getState() != Stopped)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			
	}

	// Stop the playback
	alCheck(alSourceStop(m_source));

	// Dequeue any buffer left in the queue
	clearQueue();

	// Delete the buffers
	alCheck(alSourcei(m_source, AL_BUFFER, 0));
	alCheck(alDeleteBuffers(BufferCount, m_buffers));
}

//---------------------------------------------------------------------------//

bool Stream::fillAndPushBuffer(unsigned int bufferNum)
{
	bool requestStop = false;

	// Acquire audio data
	Chunk data = {NULL, 0};
	if (!onGetData(data))
	{
		// Mark the buffer as the last one (so that we know when to reset the playing position)
		m_endBuffers[bufferNum] = true;

		// Check if the stream must loop or stop
		if (m_loop)
		{
			// Return to the beginning of the stream source
			onSeek(0.f);

			// If we previously had no data, try to fill the buffer once again
			if (!data.samples || (data.sampleCount == 0))
			{
				return fillAndPushBuffer(bufferNum);
			}
		}
		else
		{
			// Not looping: request stop
			requestStop = true;
		}
	}

	// Fill the buffer if some data was returned
	if (data.samples && data.sampleCount)
	{
		unsigned int buffer = m_buffers[bufferNum];

		// Fill the buffer
		ALsizei size = static_cast<ALsizei>(data.sampleCount) * sizeof(std::int16_t);
		alCheck(alBufferData(buffer, m_format, data.samples, size, m_sampleRate));

		// Push it into the sound queue
		alCheck(alSourceQueueBuffers(m_source, 1, &buffer));
	}

	return requestStop;
}

//---------------------------------------------------------------------------//

bool Stream::fillQueue()
{
	// Fill and enqueue all the available buffers
	bool requestStop = false;
	for (int i = 0; (i < BufferCount) && !requestStop; ++i)
	{
		if (fillAndPushBuffer(i))
			requestStop = true;
	}

	return requestStop;
}

//---------------------------------------------------------------------------//

void Stream::clearQueue()
{
	// Get the number of buffers still in the queue
	ALint nbQueued;
	alCheck(alGetSourcei(m_source, AL_BUFFERS_QUEUED, &nbQueued));

	// Dequeue them all
	ALuint buffer;
	for (ALint i = 0; i < nbQueued; ++i)
		alCheck(alSourceUnqueueBuffers(m_source, 1, &buffer));
}

//---------------------------------------------------------------------------//
//-Music---------------------------------------------------------------------//
//---------------------------------------------------------------------------//

Music::Music()
 : m_file()
 , m_duration()
{

}

//---------------------------------------------------------------------------//

Music::~Music()
{
	// We must stop before destroying the file
	stop();
}

//---------------------------------------------------------------------------//

bool Music::openFromFile(const std::string& filename)
{
	// First stop the music if it was already running
	stop();

	// Open the underlying sound file
	if (!m_file.openFromFile(filename))
		return false;

	// Perform common initializations
	initialize();

	return true;
}

//---------------------------------------------------------------------------//

bool Music::openFromMemory(const void* data, std::size_t sizeInBytes)
{
	// First stop the music if it was already running
	stop();

	// Open the underlying sound file
	if (!m_file.openFromMemory(data, sizeInBytes))
		return false;

	// Perform common initializations
	initialize();

	return true;
}

//---------------------------------------------------------------------------//

bool Music::openFromStream(InputStream& stream)
{
	// First stop the music if it was already running
	stop();

	// Open the underlying sound file
	if (!m_file.openFromStream(stream))
		return false;

	// Perform common initializations
	initialize();

	return true;
}

//---------------------------------------------------------------------------//

ALfloat Music::getDuration() const
{
	return m_duration;
}

//---------------------------------------------------------------------------//

bool Music::onGetData(Stream::Chunk& data)
{
	std::lock_guard<std::mutex>  lock(m_mutex);

	// Fill the chunk parameters
	data.samples	 = &m_samples[0];
	data.sampleCount = static_cast<std::size_t>(m_file.read(&m_samples[0], m_samples.size()));

	// Check if we have reached the end of the audio file
	return data.sampleCount == m_samples.size();
}

//---------------------------------------------------------------------------//

void Music::onSeek(ALfloat timeOffset)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_file.seek(timeOffset);
}

//---------------------------------------------------------------------------//

void Music::initialize()
{
	// Compute the music duration
	m_duration = m_file.getDuration();

	// Resize the internal buffer so that it can contain 1 second of audio samples
	m_samples.resize(m_file.getSampleRate() * m_file.getChannelCount());

	// Initialize the stream
	Stream::initialize(m_file.getChannelCount(), m_file.getSampleRate());
}

//---------------------------------------------------------------------------//
//-SoundFileReaderWav--------------------------------------------------------//
//---------------------------------------------------------------------------//

class SoundFileReaderWav : public SoundFileReader
{
public:

	static bool check(InputStream& stream);

public:

	SoundFileReaderWav();

	virtual bool open(InputStream& stream, Info& info);
	virtual void seek(std::uint64_t sampleOffset);
	virtual std::uint64_t read(std::int16_t* samples, std::uint64_t maxCount);

private:

	bool parseHeader(Info& info);

	InputStream* m_stream;
	unsigned int m_bytesPerSample;
	std::uint64_t m_dataStart;
};

namespace
{
	// The following functions read integers as little endian and
	// return them in the host byte order

	bool decode(InputStream& stream, std::uint8_t& value)
	{
		 return stream.read(&value, sizeof(value)) == sizeof(value);
	}

	bool decode(InputStream& stream, std::int16_t& value)
	{
		unsigned char bytes[sizeof(value)];
		if (stream.read(bytes, sizeof(bytes)) != sizeof(bytes))
			return false;

		value = bytes[0] | (bytes[1] << 8);

		return true;
	}

	bool decode(InputStream& stream, std::uint16_t& value)
	{
		unsigned char bytes[sizeof(value)];
		if (stream.read(bytes, sizeof(bytes)) != sizeof(bytes))
			return false;

		value = bytes[0] | (bytes[1] << 8);

		return true;
	}

	bool decode24bit(InputStream& stream, std::uint32_t& value)
	{
		unsigned char bytes[3];
		if (stream.read(bytes, sizeof(bytes)) != sizeof(bytes))
			return false;

		value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);

		return true;
	}

	bool decode(InputStream& stream, std::uint32_t& value)
	{
		unsigned char bytes[sizeof(value)];
		if (stream.read(bytes, sizeof(bytes)) != sizeof(bytes))
			return false;

		value = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);

		return true;
	}

	const std::uint64_t mainChunkSize = 12;
}

//---------------------------------------------------------------------------//

bool SoundFileReaderWav::check(InputStream& stream)
{
	char header[mainChunkSize];
	if (stream.read(header, sizeof(header)) < static_cast<std::int64_t>(sizeof(header)))
		return false;

	return (header[0] == 'R') && (header[1] == 'I') && (header[2] == 'F') && (header[3] == 'F')
		&& (header[8] == 'W') && (header[9] == 'A') && (header[10] == 'V') && (header[11] == 'E');
}

//---------------------------------------------------------------------------//

SoundFileReaderWav::SoundFileReaderWav()
 : m_stream(NULL)
 , m_bytesPerSample(0)
 , m_dataStart(0)
{
}

//---------------------------------------------------------------------------//

bool SoundFileReaderWav::open(InputStream& stream, Info& info)
{
	m_stream = &stream;

	if (!parseHeader(info))
	{
		EMYL_WARN("Failed to open WAV sound file (invalid or unsupported file)\n");
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------//

void SoundFileReaderWav::seek(std::uint64_t sampleOffset)
{
	EMYL_ASSERT(m_stream);

	m_stream->seek(m_dataStart + sampleOffset * m_bytesPerSample);
}

//---------------------------------------------------------------------------//

std::uint64_t SoundFileReaderWav::read(std::int16_t* samples, std::uint64_t maxCount)
{
	EMYL_ASSERT(m_stream);

	std::uint64_t count = 0;
	while (count < maxCount)
	{
		switch (m_bytesPerSample)
		{
			case 1:
			{
				std::uint8_t sample = 0;
				if (decode(*m_stream, sample))
					*samples++ = (static_cast<std::int16_t>(sample) - 128) << 8;
				else
					return count;
				break;
			}

			case 2:
			{
				std::int16_t sample = 0;
				if (decode(*m_stream, sample))
					*samples++ = sample;
				else
					return count;
				break;
			}

			case 3:
			{
				std::uint32_t sample = 0;
				if (decode24bit(*m_stream, sample))
					*samples++ = sample >> 8;
				else
					return count;
				break;
			}

			case 4:
			{
				std::uint32_t sample = 0;
				if (decode(*m_stream, sample))
					*samples++ = sample >> 16;
				else
					return count;
				break;
			}

			default:
			{
				EMYL_ASSERT(false);
				return 0;
			}
		}

		++count;
	}

	return count;
}

//---------------------------------------------------------------------------//

bool SoundFileReaderWav::parseHeader(Info& info)
{
	EMYL_ASSERT(m_stream);

	// If we are here, it means that the first part of the header
	// (the format) has already been checked
	char mainChunk[mainChunkSize];
	if (m_stream->read(mainChunk, sizeof(mainChunk)) != sizeof(mainChunk))
		return false;

	// Parse all the sub-chunks
	bool dataChunkFound = false;
	while (!dataChunkFound)
	{
		// Parse the sub-chunk id and size
		char subChunkId[4];
		if (m_stream->read(subChunkId, sizeof(subChunkId)) != sizeof(subChunkId))
			return false;
		std::uint32_t subChunkSize = 0;
		if (!decode(*m_stream, subChunkSize))
			return false;

		// Check which chunk it is
		if ((subChunkId[0] == 'f') && (subChunkId[1] == 'm') && (subChunkId[2] == 't') && (subChunkId[3] == ' '))
		{
			// "fmt" chunk

			// Audio format
			std::uint16_t format = 0;
			if (!decode(*m_stream, format))
				return false;
			if (format != 1) // PCM
				return false;

			// Channel count
			std::uint16_t channelCount = 0;
			if (!decode(*m_stream, channelCount))
				return false;
			info.channelCount = channelCount;

			// Sample rate
			std::uint32_t sampleRate = 0;
			if (!decode(*m_stream, sampleRate))
				return false;
			info.sampleRate = sampleRate;

			// Byte rate
			std::uint32_t byteRate = 0;
			if (!decode(*m_stream, byteRate))
				return false;

			// Block align
			std::uint16_t blockAlign = 0;
			if (!decode(*m_stream, blockAlign))
				return false;

			// Bits per sample
			std::uint16_t bitsPerSample = 0;
			if (!decode(*m_stream, bitsPerSample))
				return false;
			if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
			{
				EMYL_WARN("Unsupported sample size: %d bit (Supported sample sizes are 8/16/24/32 bit)\n", bitsPerSample);
				return false;
			}
			m_bytesPerSample = bitsPerSample / 8;

			// Skip potential extra information (should not exist for PCM)
			if (subChunkSize > 16)
			{
				if (m_stream->seek(m_stream->tell() + subChunkSize - 16) == -1)
					return false;
			}
		}
		else if ((subChunkId[0] == 'd') && (subChunkId[1] == 'a') && (subChunkId[2] == 't') && (subChunkId[3] == 'a'))
		{
			// "data" chunk

			// Compute the total number of samples
			info.sampleCount = subChunkSize / m_bytesPerSample;

			// Store the starting position of samples in the file
			m_dataStart = m_stream->tell();

			dataChunkFound = true;
		}
		else
		{
			// unknown chunk, skip it
			if (m_stream->seek(m_stream->tell() + subChunkSize) == -1)
				return false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------//

SoundFileReaderRegistrer<SoundFileReaderWav> SoundFileReaderRegistrerWav;

//---------------------------------------------------------------------------//
//-SoundFileReaderOgg--------------------------------------------------------//
//---------------------------------------------------------------------------//

#include <vorbis/vorbisfile.h>

class SoundFileReaderOgg : public SoundFileReader
{
public:

	static bool check(InputStream& stream);

public:

	SoundFileReaderOgg();
	~SoundFileReaderOgg();

	virtual bool open(InputStream& stream, Info& info);
	virtual void seek(std::uint64_t sampleOffset);
	virtual std::uint64_t read(std::int16_t* samples, std::uint64_t maxCount);

private:

	void close();

	OggVorbis_File m_vorbis;
	unsigned int   m_channelCount;
};

namespace
{
	size_t read(void* ptr, size_t size, size_t nmemb, void* data)
	{
		InputStream* stream = static_cast<InputStream*>(data);
		return static_cast<std::size_t>(stream->read(ptr, size * nmemb));
	}

	int seek(void* data, ogg_int64_t offset, int whence)
	{
		InputStream* stream = static_cast<InputStream*>(data);
		switch (whence)
		{
			case SEEK_SET:
				break;
			case SEEK_CUR:
				offset += stream->tell();
				break;
			case SEEK_END:
				offset = stream->getSize() - offset;
		}
		return static_cast<int>(stream->seek(offset));
	}

	long tell(void* data)
	{
		InputStream* stream = static_cast<InputStream*>(data);
		return static_cast<long>(stream->tell());
	}

	static ov_callbacks callbacks = {&read, &seek, NULL, &tell};
}

//---------------------------------------------------------------------------//

bool SoundFileReaderOgg::check(InputStream& stream)
{
	OggVorbis_File file;
	if (ov_test_callbacks(&stream, &file, NULL, 0, callbacks) == 0)
	{
		ov_clear(&file);
		return true;
	}
	else
	{
		return false;
	}
}

//---------------------------------------------------------------------------//

SoundFileReaderOgg::SoundFileReaderOgg()
 : m_vorbis ()
 , m_channelCount(0)
{
	m_vorbis.datasource = NULL;
}

//---------------------------------------------------------------------------//

SoundFileReaderOgg::~SoundFileReaderOgg()
{
	close();
}

//---------------------------------------------------------------------------//

bool SoundFileReaderOgg::open(InputStream& stream, Info& info)
{
	// Open the Vorbis stream
	int status = ov_open_callbacks(&stream, &m_vorbis, NULL, 0, callbacks);
	if (status < 0)
	{
		EMYL_WARN("Failed to open Vorbis file for reading\n");
		return false;
	}

	// Retrieve the music attributes
	vorbis_info* vorbisInfo = ov_info(&m_vorbis, -1);
	info.channelCount = vorbisInfo->channels;
	info.sampleRate = vorbisInfo->rate;
	info.sampleCount = static_cast<std::size_t>(ov_pcm_total(&m_vorbis, -1) * vorbisInfo->channels);

	// We must keep the channel count for the seek function
	m_channelCount = info.channelCount;

	return true;
}

//---------------------------------------------------------------------------//

void SoundFileReaderOgg::seek(std::uint64_t sampleOffset)
{
	EMYL_ASSERT(m_vorbis.datasource);

	ov_pcm_seek(&m_vorbis, sampleOffset / m_channelCount);
}

//---------------------------------------------------------------------------//

std::uint64_t SoundFileReaderOgg::read(std::int16_t* samples, std::uint64_t maxCount)
{
	EMYL_ASSERT(m_vorbis.datasource);

	// Try to read the requested number of samples, stop only on error or end of file
	std::uint64_t count = 0;
	while (count < maxCount)
	{
		int bytesToRead = static_cast<int>(maxCount - count) * sizeof(std::int16_t);
		long bytesRead = ov_read(&m_vorbis, reinterpret_cast<char*>(samples), bytesToRead, 0, 2, 1, NULL);
		if (bytesRead > 0)
		{
			long samplesRead = bytesRead / sizeof(std::int16_t);
			count += samplesRead;
			samples += samplesRead;
		}
		else
		{
			// error or end of file
			break;
		}
	}

	return count;
}

//---------------------------------------------------------------------------//

void SoundFileReaderOgg::close()
{
	if (m_vorbis.datasource)
	{
		ov_clear(&m_vorbis);
		m_vorbis.datasource = NULL;
		m_channelCount = 0;
	}
}

//---------------------------------------------------------------------------//

SoundFileReaderRegistrer<SoundFileReaderOgg> SoundFileReaderRegistrerOgg;

} //namespace Emyl
