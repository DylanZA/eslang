## Eslang

This is an experiment I started to see if C++ with coroutines (still experimental) can be used to develop using Erlang style. I.e. a framework where individual coroutines are extremely cheap to construct and asynchronous IO is easy to use. It's also type safe (unlike Erlang) and has the ability to segfault from anywhere (also unlike Erlang?).

It also served the purpose of teaching myself how coroutines work.

There are a couple of examples (in the examples/ directory) that I've written, and if I get more time I might even add more (for example a simple web server). The current ones show things like:
* Spawn a million "processes"
* TCP listener
* Back pressure on sending messages
* Receive messages with timeouts


### Shortcomings

So many right now:

* No unit tests
* I haven't actually tested it properly (valgrind)
* The API changes on my whims

### MultiThreading

I have some ideas for making it more multithreaded, but right now it only runs in one thread. This is quick and good enough for everything I've needed thus far. If it's good enough for Node, hopefully its good enough for this.

### Dependencies

The dependencies right now are just Facebook's [Folly](https://github.com/facebook/folly) library. This is unfortunately a bit of a nightmare to build (mostly I suspect on Windows which is what I am working with).

This has some other dependencies such as
* GLog
* GFlags
* OpenSSL
* double-conversioon
* libevent

### Building on Windows

This is an aboslute nightmare. I spent almost as much time on this as on writing code. For now this might work for you:
* Install Visual Studio 2017
* Install OpenSSL for Windows
* Download and extract Boost (eg. https://sourceforge.net/projects/boost/files/boost-binaries/)

* clone https://github.com/DylanZA/windowsExtHelp.git
* run ./clone.sh
* run ./build.sh \<path to boost checkout\>
* copy the extinstalls directory to the eslang root directory
* Run ./build.sh \<path to boost checkout\> In the eslang root directory
* you should now have two visual studio solutions: build/Debug/Eslang.sln and build/Release/Eslang.sln. These should both build the Debug and Release builds respectively.

### Building on Linux

No idea. Never tried it, but hopefully cmake just works? It can;t be worse than building on Windows.

### Is this stable? Is the API stable?
No.

### Docs?

Not yet.

The basic idea though, is that you create an s::Context, and then spawn some "Processes" into it. These are created by you, and have a `void run();`  method. These processes can then send and receive messages via the context and spawn further subprocesses. This is all kind of similar (in theory) to how Erlang does things, so if you know Erlang - this might look familiar.  

### Etymology

It kind of comes from Erlang, but C++ style. So (Er++)lang == Eslang. ish.

### Extreme Caveat

I have never actually used Erlang in anything coming close to a real project, so I may have completely misunderstood many things.