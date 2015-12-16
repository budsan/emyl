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

#pragma once

#include <cstdint>
#include <vector>
#include <set>
#include <string>
#include <thread>
#include <mutex>

#if defined(_WINDOWS)

#include <al.h>
#include <alc.h>
#include <windows.h>

#else

#include <AL/al.h>
#include <AL/alc.h>
#include <stdlib.h>

#endif

namespace Emyl
{

// Override Emyl Vec3 if your code have any
// Prerequisites:
//	- Empty constructor
//	- Copy constructor
//	- Components constructor

#if !defined(EMYL_VECTOR3_TYPE)

struct Vec3
{
	float x, y, z;
	
	Vec3() : x(0), y(0), z(0) {}
	Vec3(const Vec3& copy) : x(copy.x), y(copy.y), z(copy.z) {}
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

#else

using Vec3 = EMYL_VECTOR3_TYPE;

#endif

//---------------------------------------------------------------------------//

namespace internal
{
	#if defined(WIN32)
	#define __current__func__ __FUNCTION__
	#else
	#define __current__func__ __func__
	#endif

	#if defined(_DEBUG)
	#define EMYL_DEBUG
	#endif

	#ifdef ANDROID
	class ResourceStream;
	#endif

//---------------------------------------------------------------------------//

	#if defined (EMYL_DEBUG)

		#define EMYL_LOG(fmt, ...) fprintf(stderr, (fmt), __VA_ARGS__)
		#define EMYL_ASSERT(expression) assert(expression)

		#define alCheck(expr) do  \
		{ \
			expr; \
			Emyl::internal::alCheckError(__FILE__, __LINE__, #expr); \
		} while (0)

	#else

		#define EMYL_LOG(fmt, ...)
		#define EMYL_ASSERT(expression)
		#define alCheck(expr) (expr)

	#endif

	void alCheckError(const char* file, unsigned int line, const char* expression);

//---------------------------------------------------------------------------//

	#ifdef EMYL_ERRORS_AS_WARNINGS
	#define EMYL_ERROR EMYL_WARN
	#else
	#define EMYL_ERROR(...) do \
		{ \
		EMYL_LOG("%s -- ", __current__func__); \
		EMYL_LOG(__VA_ARGS__); \
		EMYL_LOG("\n"); \
		assert(0); \
		std::exit(-1); \
		} while (0)
	#endif

//---------------------------------------------------------------------------//

	#define EMYL_WARN(...) do \
		{ \
		EMYL_LOG("%s -- ", __current__func__); \
		EMYL_LOG(__VA_ARGS__); \
		EMYL_LOG("\n"); \
		} while (0)

//---------------------------------------------------------------------------//

	class Resource
	{
	protected:

		 Resource();
		~Resource();
	};

} // namespace internal

//---------------------------------------------------------------------------//

class Listener
{
public:
	static void setGlobalVolume(float volume);
	static float getGlobalVolume();

	static void setPosition(float x, float y, float z);
	static void setPosition(const Vec3& position);
	static Vec3 getPosition();

	static void setDirection(float x, float y, float z);
	static void setDirection(const Vec3& direction);
	static Vec3 getDirection();

	static void setUpVector(float x, float y, float z);
	static void setUpVector(const Vec3& upVector);
	static Vec3 getUpVector();
};

//---------------------------------------------------------------------------//

class Source : internal::Resource
{
public:

	enum State
	{
		Stopped,
		Paused,
		Playing
	};

	Source(const Source& copy);
	virtual ~Source();

	void setPitch(float pitch);
	void setVolume(float volume);
	void setPosition(float x, float y, float z);
	void setPosition(const Vec3& position);
	void setRelativeToListener(bool relative);
	void setMinDistance(float distance);
	void setAttenuation(float attenuation);

	float getPitch() const;
	float getVolume() const;
	Vec3 getPosition() const;
	bool isRelativeToListener() const;
	float getMinDistance() const;
	float getAttenuation() const;
	Source& operator =(const Source& right);

protected:

	Source();
	State getState() const;

	unsigned int m_source;
};

//---------------------------------------------------------------------------//

class Buffer;
class Sound : public Source
{
public:

	Sound();
	explicit Sound(const Buffer& buffer);
	Sound(const Sound& copy);
	~Sound();

	void play();
	void pause();
	void stop();
	void setBuffer(const Buffer& buffer);
	void setLoop(bool loop);
	void setPlayingOffset(ALfloat timeOffset);

	const Buffer* getBuffer() const;
	bool getLoop() const;
	ALfloat getPlayingOffset() const;
	State getState() const;
	Sound& operator =(const Sound& right);
	void resetBuffer();

private:
	const Buffer* m_buffer;
};

//---------------------------------------------------------------------------//

class InputStream
{
public:

	virtual ~InputStream() {}
	virtual std::int64_t read(void* data, std::int64_t size) = 0;
	virtual std::int64_t seek(std::int64_t position) = 0;
	virtual std::int64_t tell() = 0;
	virtual std::int64_t getSize() = 0;
};

//---------------------------------------------------------------------------//

class FileInputStream : public InputStream
{
public:

	FileInputStream();
	virtual ~FileInputStream();

	FileInputStream(const FileInputStream&) = delete;

	bool open(const std::string& filename);
	virtual std::int64_t read(void* data, std::int64_t size);
	virtual std::int64_t seek(std::int64_t position);
	virtual std::int64_t tell();
	virtual std::int64_t getSize();

private:

#ifdef ANDROID
	internal::ResourceStream* m_file;
#else
	std::FILE* m_file;
#endif
};

//---------------------------------------------------------------------------//

class MemoryInputStream : public InputStream
{
public:

	MemoryInputStream();

	void open(const void* data, std::size_t sizeInBytes);
	virtual std::int64_t read(void* data, std::int64_t size);
	virtual std::int64_t seek(std::int64_t position);
	virtual std::int64_t tell();
	virtual std::int64_t getSize();

private:

	const char* m_data;
	std::int64_t m_size;
	std::int64_t m_offset;
};

//---------------------------------------------------------------------------//

class SoundFileReader
{
public:

	struct Info
	{
		std::uint64_t sampleCount;
		unsigned int channelCount;
		unsigned int sampleRate;
	};

	virtual ~SoundFileReader() {}

	virtual bool open(InputStream& stream, Info& info) = 0;
	virtual void seek(std::uint64_t sampleOffset) = 0;
	virtual std::uint64_t read(std::int16_t* samples, std::uint64_t maxCount) = 0;
};

//---------------------------------------------------------------------------//

namespace internal
{

	struct ReaderFactory
	{
		bool (*check)(InputStream&);
		SoundFileReader* (*create)();
	};

	void ReaderFactoryAdd(ReaderFactory&);
	void ReaderFactoryRemove(SoundFileReader* (*)());

//---------------------------------------------------------------------------//

	template <typename T> SoundFileReader* CreateReader()
	{
		return new T;
	}

//---------------------------------------------------------------------------//

	template <typename T> void UnregisterReader()
	{
		ReaderFactoryRemove(&CreateReader<T>);
	}

//---------------------------------------------------------------------------//

	template <typename T> void RegisterReader()
	{
		UnregisterReader<T>();

		ReaderFactory factory;
		factory.check = &T::check;
		factory.create = &CreateReader<T>;

		ReaderFactoryAdd(factory);
	}

} // namespace internal

//---------------------------------------------------------------------------//

template <typename T> class SoundFileReaderRegistrer
{
public:
	SoundFileReaderRegistrer() { internal::RegisterReader<T>(); }
	~SoundFileReaderRegistrer() { internal::UnregisterReader<T>(); }
};

//---------------------------------------------------------------------------//

class InputSoundFile
{
public:

	 InputSoundFile();
	~InputSoundFile();

	InputSoundFile(const InputSoundFile&) = delete;

	bool openFromFile(const std::string& filename);
	bool openFromMemory(const void* data, std::size_t sizeInBytes);
	bool openFromStream(InputStream& stream);
	bool openForWriting(const std::string& filename, unsigned int channelCount, unsigned int sampleRate);
	
	std::uint64_t getSampleCount() const;
	unsigned int getChannelCount() const;
	unsigned int getSampleRate() const;
	ALfloat getDuration() const;
	
	void seek(std::uint64_t sampleOffset);
	void seek(ALfloat timeOffset);
	std::uint64_t read(std::int16_t* samples, std::uint64_t maxCount);

private:

	void close();

	SoundFileReader* m_reader;
	InputStream* m_stream;
	bool m_streamOwned;
	std::uint64_t m_sampleCount;
	unsigned int m_channelCount;
	unsigned int m_sampleRate;
};

//---------------------------------------------------------------------------//

class Buffer : internal::Resource
{
public:

	Buffer();
	Buffer(const Buffer& copy);
	~Buffer();
	bool loadFromFile(const std::string& filename);
	bool loadFromMemory(const void* data, std::size_t sizeInBytes);
	bool loadFromStream(InputStream& stream);
	bool loadFromSamples(const std::int16_t* samples, std::uint64_t sampleCount, unsigned int channelCount, unsigned int sampleRate);

	const std::int16_t* getSamples() const;
	std::uint64_t getSampleCount() const;
	unsigned int getSampleRate() const;
	unsigned int getChannelCount() const;
	ALfloat getDuration() const;

	Buffer& operator =(const Buffer& right);

private:

	friend class Sound;

	bool initialize(InputSoundFile& file);
	bool update(unsigned int channelCount, unsigned int sampleRate);
	void attachSound(Sound* sound) const;
	void detachSound(Sound* sound) const;

	typedef std::set<Sound*> SoundList;

	unsigned int m_buffer;
	std::vector<std::int16_t> m_samples;
	ALfloat m_duration;
	mutable SoundList m_sounds;
};

//---------------------------------------------------------------------------//

class Stream : public Source
{
public:

	struct Chunk
	{
		const std::int16_t* samples;
		std::size_t sampleCount;
	};

	virtual ~Stream();


	void play();
	void pause();
	void stop();

	unsigned int getChannelCount() const;
	unsigned int getSampleRate() const;

	State getState() const;
	void setPlayingOffset(ALfloat timeOffset);
	ALfloat getPlayingOffset() const;

	void setLoop(bool loop);
	bool getLoop() const;

protected:

	Stream();

	void initialize(unsigned int channelCount, unsigned int sampleRate);
	virtual bool onGetData(Chunk& data) = 0;
	virtual void onSeek(ALfloat timeOffset) = 0;

private:

	void streamData();
	bool fillAndPushBuffer(unsigned int bufferNum);

	bool fillQueue();
	void clearQueue();

	enum
	{
		BufferCount = 3
	};

	std::thread m_thread;
	mutable std::mutex m_threadMutex;

	State m_threadStartState;
	bool m_isStreaming;
	unsigned int m_buffers[BufferCount];
	unsigned int m_channelCount;
	unsigned int m_sampleRate;
	std::uint32_t m_format;
	bool m_loop;
	std::uint64_t m_samplesProcessed;
	bool m_endBuffers[BufferCount];
};

//---------------------------------------------------------------------------//

class Music : public Stream
{
public:

	Music();
	~Music();

	bool openFromFile(const std::string& filename);
	bool openFromMemory(const void* data, std::size_t sizeInBytes);
	bool openFromStream(InputStream& stream);

	ALfloat getDuration() const;

protected:

	virtual bool onGetData(Chunk& data);
	virtual void onSeek(ALfloat timeOffset);

private:

	void initialize();

	InputSoundFile m_file;
	ALfloat	m_duration;
	std::vector<std::int16_t> m_samples;
	std::mutex m_mutex;
};

} //namespace Emyl

