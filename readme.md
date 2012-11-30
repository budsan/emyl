#Emyl

Emyl is a C++ toolkit for OpenAL abstraction. It is a recursive acronym for "Emyl Makes You Listen".

It's designed to be added as a single file in your working project. It provides an abstraction for playing sounds and music without working with sources and buffers too much. It also includes OGG support for sounds and OGG streaming for music.

It depends of OpenAL and libvorbis + libogg libraries. Just by installing them in your environment should be OK.
* OpenAL: http://www.openal.org/
* libogg: http://xiph.org/vorbis/

It's not necessary to be an OpenAL expert for using Emyl, but it's recommended for getting full potential of OpenAL.

##Introduction:

There are 3 main classes in Emyl: manager, stream and sound. Manager handles all OpenAL resources (sources and buffers) and initializes OpenAL. This class uses singleton pattern and you must get it instance as follows:

```C++
#include "emyl.h"

int main(int argc, char** argv)
{
    emyl::manager* mng = emyl::manager::get_instance();
	if(!mng->init())
	{
		//ERROR
		return 0;
	}

	//CODE HERE

	delete mng; //deinit emyl
}
```

All initialisation can be changed freely by modifying emyl.cpp, it's designed to be modified easyly. For example, let's change how many sources Emyl create for us:

```C++
class manager
{
    public: 
	// [...]

	private:

	static const int NUM_SOURCES = 16; //SELF-SERVICE ;)
	// [...]
};
```

That's why I always try to keep this code as clean as possible and commented.

##Getting sources & buffers.

Emyl only handles one context that contains a single listener, a set of sources. For using sources, you should say to Emyl to reserve or free sources for using them. How does this works? User asks Emyl for getting a unused source, Emyl looks for it, reserve it and returns to you. As soon as you don't need that source, you must tell to emyl to free that source.

Here's an example:

```C++
emyl::manager mng = emyl::manger::get_instance();
ALuint source = mng-> source_reserve();
if ( source == 0 )
{
    //ERROR
}

//CODE HERE

mng->source_unreserve(source);
```

From here you may want to use raw OpenAL but, for your safety, Emyl have tools for avoding that. Let's keep going what manager can do:

```C++
emyl::manager mng = emyl::manger::get_instance();
ALuint buffer = mng->get_buffer("sound.ogg");
if(buffer == 0)
{
    //ERROR
}
```

The code above loads an OGG audio file and returns you the buffer id. There is a forced limitation that avoids loading files longer than 10 seconds (because we don't want a big sound uncompressed in memory), but can be erased easyly from emyl.cpp. I suggest you to use stream class for longer files. Let's continue:

```C++
emyl::manager mng = emyl::manger::get_instance();
ALuint buf0 = mng->get_buffer("sound.ogg");
if (buf0 == 0)
{
    //ERROR
}

ALuint buf1 = mng->get_buffer("sound.ogg");
if (buf0 == buf1) //ALWAYS TRUE
{
	mng->delete_buffer("sound.ogg");
}
```

Loading the same file twice makes the manager return you the same buffer ID. For forcing loading again, delete_buffer() deletes buffer from memory and frees "sound.ogg" from manager. And that's all manager can do.

##What more can do emyl for us?

Stream and Sound classes will make you think that it worth using Emyl. What are they?

###Sound
Sound is a container for one source and one buffer and it also includes a lot of useful methods:

```C++
emyl::sound  *sound = new emyl::sound();
if(!sound->set_source())
{
    //ERROR: ALL SOURCES RESERVED
}
```

The method set_source() makes the sound ask to manager for a unused source in the same way we read before. Luckly for us, taking advantage of C++, sounds free the source when this is deleted. 

    delete sound; // RUN SOURCE, RUN!

Of course, you always are free for forcing a sound free it's source using free_source() method. By doing that, the sound will lose all properties attached at the source like position, velocity, gain. etc. Let's go on:

```C++
ALuint buffer = mng->get_buffer("sound.ogg");
if(!sound->set_buffer(buffer))
{
    //ERROR: BUFFER INCORRECTLY LOADED.
}
```


It doesn't matter what function (set_source() and set_buffer()) you call first, it won't affect the final result. As soon as sound have both you are free to call play() method :)

From here you're free of use sounds as you like. Here is an example:

```C++
#include "emyl.h"

int main(int argc, char** argv)
{
    //INIT EMYL
	emyl::manager* mng = emyl::manager::get_instance();
	if(!mng->init()) return 0;

	//CREATING NEW SOUND AND LOADING SOUND FILE
	emyl::sound  *sound = new emyl::sound();
	ALuint buffer = mng->get_buffer("sound.ogg");
	if(buffer == 0) return 0;

	//LET'S GIVE IT A SOURCE AND A BUFFER, AND SET IT AS A SOUND LOOP
	sound->set_source();
	sound->set_buffer(buffer);
	sound->set_loop(true);

	//SOME LITTLE ADJUSTS
	//LINEAL DISTANCE CLAMPED MODEL
	ALuint source = sound->get_source();
	alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
	alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
	alSourcef(source, AL_REFERENCE_DISTANCE, 1.0f);
	alSourcef(source, AL_MAX_DISTANCE, 11.0f);

	//SOME LOGICS HERE
	float x = -10.0f;
	sound->set_position( x, 0.0f, -5.0f);
	sound->set_velocity( 1.0f, 0.0f, 0.0f);
	sound->play();

	//MAIN LOOP
	for(int i = 0; i < 20000; i++)
	{
		sound->set_position( x, 0.0f, -5.0f);
		sound->set_velocity(1.0f, 0.0f, 0.0f);
		SLEEP(1); //USE YOU'RE FAVORITE SLEEP FUNC
		x += 0.001f;
	}

	//FREE MEMORY AND DEINIT EMYL
	delete sound;
	delete mng;
}
```

###Stream

Stream class let you play sounds longer than 10 secs. Basically this class streams ogg files from disk in a queue of OpenAL buffers. You're only job is to keep this queue full of buffers by calling a function regularly. If you prefer, you could also use threads by your own for doing this automatically.

Let's see how this streams looks like:

```C++
emyl::stream *music = new emyl::stream();
if(!music->set_source())
{
    //ERROR: ALL SOURCES RESERVED
}

if(!music->load("music.ogg"))
{
	//ERROR: FILE COULDN'T BE LOADED
}
```

That's it. Now how you set it up is up to you, for example:

```C++
music->set_volume(0.5f);
music->set_loop(true);
music->seek(10000); //MILISECONDS
```

And finally, let's play the stream:

```C++
music->play();

while(music->playing())
{
	if(Key_ESC_pressed())
	music->stop();
}

delete music;
```

In the same way as sounds does, when stream is freed, it also will free source from manager, OpenAL used buffers for perform the streaming and the ogg structs. Remember, in OpenAL any STEREO sound will not be affected by distance atenuation or doppler effect, and this affect stream too.

###TODO: More examples.

##License

Copyright (c) 2008-2012 Jordi Santiago Provencio

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
