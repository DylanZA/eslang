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

The dependencies right now are Facebook's [Folly](https://github.com/facebook/folly) library and [Boost Beast](https://github.com/boostorg/beast) . 
Folly is unfortunately a bit complicated to build (mostly I suspect on Windows which is what I am working with).

This has some other dependencies such as
* GLog
* GFlags
* OpenSSL
* double-conversioon
* libevent

### Building on Windows

This used to be an aboslute nightmare, but then I learnt about [vcpkg](https://github.com/Microsoft/vcpkg).

* install vcpkg somewhere
* install (I think) folly and beast vcpkg things
* Folly is broken sometimes on windows. I tend to keep my branch working, so if it does not compile you can use [it here: ](https://github.com/dylanza/folly) by overwriting the [appropriate file in vcpkg](https://github.com/Microsoft/vcpkg/tree/master/ports/folly).
* build as per vcpkg instructions

### Building on Linux

No idea. Never tried it, but hopefully cmake just works? It cannot be worse than building on Windows.

### Is this stable? Is the API stable?
No.

### Docs?

Not yet.

The basic idea though, is that you create an s::Context, and then spawn some "Processes" into it. These are created by you, and have a `void run();`  method. These processes can then send and receive messages via the context and spawn further subprocesses. This is all kind of similar (in theory) to how Erlang does things, so if you know Erlang - this might look familiar.  

### Etymology

It kind of comes from Erlang, but C++ style. So (Er++)lang == Eslang. ish.

### Extreme Caveat

I have never actually used Erlang in anything coming close to a real project, so I may have completely misunderstood many things.