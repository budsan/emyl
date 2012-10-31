Emyl (Emyl Makes You Listen) is a C++ toolkit for OpenAL abstraction.

It's designed to be added as a single file in your working project. It provides an abstraction for playing sounds and music without working with sources and buffers too much. It also includes OGG support for sounds and OGG streaming for music.

It depends of OpenAL and libvorbis + libogg libraries. Just by installing them in your environment should be OK.
OpenAL: http://www.openal.org/
libogg: http://xiph.org/vorbis/

It's not necessary to be an OpenAL expert for using Emyl, but it's recommended for getting full potential of OpenAL.

Introduction:

There are 3 main classes in Emyl: manager, stream and sound. Manager handles all OpenAL resources (sources and buffers) and initializes OpenAL. This class uses singleton pattern and you must get it instance as follows:

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

All initialisation can be changed freely by modifying emyl.cpp, it's designed to be modified easyly. For example, let's change how many sources Emyl create for us:

class manager
{
 public:

   // [...]

 private:

   static const int NUM_SOURCES = 16; //SELF-SERVICE ;)

   // [...]
};

That's why I always try to keep this code as clean as possible and commented.

Getting sources & buffers.

Emyl only handles one context that contains a single listener, a set of sources. For using sources, you should say to Emyl to reserve or free sources for using them. How does this works? User asks Emyl for getting a unused source, Emyl looks for it, reserve it and returns to you. As soon as you don't need that source, you must tell to emyl to free that source.

Here's an example:

// [...]

   emyl::manager mng = emyl::manger::get_instance();
   ALuint source = mng-> source_reserve();

   if ( source == 0 )
   {
      //ERROR
   }

   //CODE HERE

   mng->source_unreserve(source);

// [...]

From here you may want to use raw OpenAL but, for your safety, Emyl have tools for avoding that. Let's keep going what manager can do:

// [...]

   emyl::manager mng = emyl::manger::get_instance();
   ALuint buffer = mng->get_buffer("sound.ogg");

   if(buffer == 0)
   {
      //ERROR
   }

// [...]

The code above loads an OGG audio file and returns you the buffer id. There is a forced limitation that avoids loading files longer than 10 seconds (because we don't want a big sound uncompressed in memory), but can be erased easyly from emyl.cpp. I suggest you to use stream class for longer files. Let's continue:

// [...]

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

// [...]

Loading the same file twice makes the manager return you the same buffer ID. For forcing loading again, delete_buffer() deletes buffer from memory and frees "sound.ogg" from manager. And that's all manager can do.

What more can do emyl for us?

Stream and Sound classes will make you think that it worth using Emyl. What are they?
Sound is a container for one source and one buffer and it also includes a lot of useful methods:

// [..]

   emyl::sound  *sound = new emyl::sound();

   if(!sound->set_source())
   {
      //ERROR: NO UNUSED SOURCES
   }

// [..]

The method set_source() makes the sound ask to manager for a unused source in the same way we read before. Luckly for us, taking advantage of C++, sounds free the source when this is deleted. 

delete sound; // RUN SOURCE, RUN!

Of course, you always are free for forcing a sound free it's source using free_source() method. By doing that, the sound will lose all properties attached at the source like position, velocity, gain. etc. Let's go on:

// [..]

   ALuint buffer = mng->get_buffer("sound.ogg");

   if(!sound->set_buffer(buffer))
   {
      //ERROR: BUFFER NO CORRECTLY LOADED.
   }

// [..]


You don't have to keep that set_source() and set_buffer() call order, it won't affect the final result. As soon as sound have both you are free to call play() method :)

TODO: STREAMS AND EXAMPLES.
