This is a simple HTTP server implemented in C-style C++. No libraries are used, besides some standard IO functions like fopen and printf.
There is a Win32 port and a Linux port.

This program was made for a school project (early 2021). A certain list of features was requested, 
some of which are a little bizarre and you wouldn't actually want to use.
However, the project was interesting enough to me to implement the server in C rather than Java, port it to two different operating systems, 
use custom memory allocators exclusively, make my own thread job queue and implement/debug MD5 hash by hand.

I did not push the performance and security aspects very much. 
Although I tried to make sensible decisions along the way, it was never really in the scope of the project to harden and optimize the server.
It would also have conflicted with some of the requested features of the assignment.


## Features
- GET request handling. Other HTTP methods are ignored.
- Multithreaded connections was a required feature. Each TCP connection runs on its own separate thread. 
Main thread produces job entries, other threads consume job entries. Jobs are stored in a circular buffer. 
Number of created threads is hard-coded; when in doubt set it to the number of cores on the machine.
Each running job is given a memory arena (piece of memory with bump allocator system) to work with.
- By request, the client's IP and the request header are sent through stdout. 
If you run this server in a terminal, know that the terminal's runtime (rendering, parsing etc.) might be the slowest part.
- By request, a small config file is used to set the server port (80 by default) and the root folder path of websites to host.
- As requested, multisite support: the Host HTTP request header is taken into account to determine which files to load.
- By request, you can put a .htpasswd file in a folder to lock the folder tree. When an HTTP request tries to pull a locked file, 
the browser will prompt the client for a username and password, which it will send back to the server in a base64-encoded format
(that's the HTTP/1.1 basic authentication framework). The server then looks for an htpasswd file containing the username in plain text, and 
an MD5 hash of the password.


# Compiling
## Windows
We use Microsoft's C/C++ compiler MSVC. 
You can install Microsoft's compiler by installing Visual Studio.
The community edition should work fine.
Once you've installed Visual Studio, launch a terminal.
You want to look for the vcvarsall.bat file and run it *with the argument: x64*
(if you want to use the 64-bit compiler).
The path to this file will probably look like this:
```
C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat
```

Once you've run the vcvarsall.bat file, then _in that shell session_, 
you should be able to run the build.bat file contained in the source folder, from the terminal. 
That will compile the program into the build folder.
``` bat
cd src
build.bat
```

## A more automated build setup for development
To automate this process of running vcvarsall.bat for development, 
you can make a shell.bat file that contains this instruction:
``` bat
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
```
Then make a shortcut to the Windows cmd program:
modify the properties of the shortcut via right click -> properties,
setting the shortcut target field to:
```
C:\WINDOWS\system32\cmd.exe /k "your\path\to\shell.bat"
```
so that opening a terminal through this shortcut will automatically execute the shell.bat script.

## Linux
You need to have the gcc compiler installed.
Once that is done, compiling the program should be as simple as
``` bash
cd src
./build.sh
```
If successful, that will create the executable into the build folder.

Sometimes you may not have the execution right on the build.sh file. In that case, try: 
``` bash
chmod +x build.sh
```
before running build.sh.
     

# How to run the server
Once the program is compiled, run the executable file in the build folder from a terminal.
The executable should be either called server_win32.exe (for Windows) or server_linux (for Linux).
When you run the executable, it first tries to parse the config file located in the same folder as the
 executable, so that it retrieves a port number and the root folder path of your websites to host.
If successful, you should then be able to have your website show up in a browser.



## Testing in a web browser with multisite (and dealing with the hosts file)
When you enter a URL in a web browser, it will typically ask a DNS server for converting a domain name to an IP address.
But before the browser does that, it will look up for the OS's hosts file, which contains a table mapping host names to addresses in plain text.
You can edit that Hosts file to map the host name to your local address. For example, on Linux I have the /etc/hosts file which contains the following line:
```
127.0.0.1 localhost verti dopetrope
```

This way, I can type this URL in a web browser:
```
http://verti/index.html
```
and the browser will send an HTTP request to my local address, port 80, with the Host header set to verti, and the request path set to /index.html.
It will do so without sending a DNS request.

On Windows 10, the hosts file is located in ```C:\Windows\System32\drivers\etc```.

If changing the hosts file doesn't seem to work, try restarting the web browser.

## Testing in a web browser without multisite
Look for this piece of code in src/server.cpp:
``` c
#if 1
    // order of concatenation: root, slash, host, path
    u32 RootLength = StringLength(Root);
    u32 RequestLength = Request.RequestPath.Length;
    u32 HostLength = Request.Host.Length;
    u32 CompletePathLength = RootLength + 1 + HostLength + RequestLength;
    string CompletePath = StringBaseLength(PushArray(Arena, CompletePathLength + 2, char),
                                            CompletePathLength);
    SprintNoNull(CompletePath.Base, Root);
    SprintNoNull(CompletePath.Base + RootLength, "/");
    SprintNoNull(CompletePath.Base + RootLength + 1, Request.Host);
    Sprint(CompletePath.Base + RootLength + 1 + HostLength, Request.RequestPath);
#else
    // order of concatenation: root, path
    u32 RootLength = StringLength(Root);
    u32 RequestLength = Request.RequestPath.Length;
    u32 CompletePathLength = RootLength + RequestLength;
    string CompletePath = StringBaseLength(PushArray(Arena, CompletePathLength + 1, char),
                                            CompletePathLength);
    SprintNoNull(CompletePath.Base, Root);
    Sprint(CompletePath.Base + RootLength, Request.RequestPath);
#endif
```

Replace #if 1 with #if 0. Recompile.
Launch the server and you should be able to run one of the verti example website by entering a URL like this in a web browser,
assuming the port is 80 and localhost is mapped to you local address -- which it should be by default:
```
http://localhost/verti/index.html
```

## If the OS won't let the server listen to port 80
You can run the executable as an administrator / super user.
You can also try setting a different port number in the config file, but then you need to have the HTTP clients send the requests to that port.

## .htpasswd files and Basic authentication.
You can protect a resource file (an entire subtree of directories, in fact) by creating a .htpasswd file at the root level of that subtree. 
Siblings are included in the protection range. The .htpasswd file may contain lines of the form:
```
user:password_in_md5
```
When a client tries to access a protected file, they will be asked for a user and password.
The user:password data from the client is sent in a base64 encoded format, in clear.
This is not an encrypted format: base64 is easily reversible, which is why you probably don't want to use this authentication framework for anything serious.
When that data is received by the server, it is decoded back and the password is converted to an MD5 hash of itself, so that it can be compared with .htpasswd
entries. When there is a match, access is granted. See src/doc.org for a more in-depth explanation of the implementation, or preferably read the implementation itself.
   

# Brief architecture explanation
I was supposed to make documentation for this server, even though it's not really an API and the code is self-documenting for the most part.
I wrote and pushed a detailed walkthrough of the code in the past,
but my teacher's reaction back then suggests that he didn't read or understand the first sentence.
I also disliked the level of detail because it was a bit of a time sink, I found that I was rambling and basically repeating myself, 
and it just distracts the reader from reading the source code which, if you ignore the comments, is the ultimate truth in terms of the behavior of the program.
So I'm replacing that with a more concise, high-level review here. 

First it should be noted that this is a unity build. In a traditional C++ project you typically compile each .cpp file into an intermediate .obj file and the .obj files are then linked together to produce the executable.
Here we don't do that. There is only a single conceptual file, made of several .cpp and .h files (the extensions don't really matter), patched together by #includes, and we produce a single executable out of that file. 
There is no linking process except potentially for fetching syscalls and stdio implementations.

Secondly, there are two conceptual layers to the source code: platform-dependent or OS layer, and platform-independent or app layer.
The OS layer:
- depends on the target platform (Windows or Linux)
- includes common.h
- contains the entry point main()
- implements a thread job queue and creates some threads
- asks the OS for a block of memory, once and for all
- sets up a TCP socket to listen to as a server
- calls the app layer once at initialization, and once per TCP connection
- simply put, its job is to do all the work that requires OS-dependent code

The application layer:
- is made to be as platform-independent as possible (given that we are targetting x64 Linux and Windows)
- is where all the HTTP logic beyond TCP happens
- is mostly implemented by server.cpp
- has two conceptual entry points called by the OS layer: InitializeServerMemory() and PrepareHandshaking()
- at initialization, it reads and parses the config file, and partitions the memory for the workers, among other things

You can check out my previous explanation in src/doc.org if you want more details, but I'd rather encourage you to read the source code, I think.




## Some issues
### Performance
- The server always loads and sends the full files from disk if it can fit in main memory. 
I haven't tried anything to mitigate the costs of disk reads (maybe you could cache web pages into main memory, for example).
I suspect the best technical design here would depend on what kind of data you want to host (how big are your web pages),
what hardware you are using for the server, and what kind of work you are expecting to do.
My assignment left all those things relatively unspecified.
- It's all unoptimized, scalar code, did not profile it, etc.
- The system calls I use for the TCP handshakes are just listen() accept() send() and recv(). 
But from what I hear, if you want something serious you should look into the IO completion ports API for Windows, or io_uring on Linux.
### Other
- I exercised some caution to avoid some problems, but no guarantees against buffer overflow attacks, bad request paths, and other security vulnerabilities.
- The config file doesn't really add any value at all. It only sets a couple things, which I'd rather have implemented as two #defines, 
even for a user-facing program I think. We could remove a pretty significant chunk of parsing code (over 340 lines) if the config file wasn't required.
- Don't use the HTTP basic authentication framework in a real server. A man in the middle could get a client's username and password in base64, 
which is easy to decode. MD5 is also not really considered to be a secure hash against attackers who are aggressively looking for collisions. 
Furthermore, the model of using a .htpasswd file for each folder tree seems janky to me.
- Multisite works in theory, but I couldn't get it to work on my Windows setup (could be a problem with my hosts file?). 
I did make it work on Arch Linux though. Multisite can be toggled by changing a character in the source code.

### Aside: reasons HTTP sucks (imo)

- It forces the implementer to use TCP. Some use cases would work fine with UDP alone, which is faster and has much simpler usage code.
Custom TCP could be implemented on top of UDP if the need arised (this is mostly second-hand knowledge, I haven't toyed much with UDP/TCP yet).

- Like many other specs, it is open-ended and tries too hard to be generic, which introduces a lot of complexity for arguably not much benefit.
It makes it very hard for the implementer to cover all cases correctly and be fully compliant. 
Such specifications tend to be fairly obtuse as well, and oftentimes this all results in implementations that have security and performance problems.
They are also not very fun to implement, which puts off many people from trying to implement better software by themselves.
HTTPS (SSL/TLS on top of HTTP), which has become more or less necessary for any server doing something useful, is *much* worse in that regard.

- It is text-based, which means some parsing must be done both client-side and server-side.
If HTTP was based on binary data, one could just send and receive data in a buffer backed by a struct, and most things would be accessible in one read;
at worst you may have to deal with different endianness, internal pointers, struct and bit packing, and compression if you use it.
You wouldn't have to read and interpret many chars to even know which header you're dealing with.
Additionally, most HTTP headers are not required to be in a specific order, which means you can't predict which one will come next.
Performance overhead is probably even worse for HTML/CSS/JS: It all has to go through the network and has to be reinterpreted.
The fact that the web pioneers decided to use this format, which is inefficient for computers and inconvenient for programmers,
seems a little ridiculous and unbelievable considering how much slower hardware used to be back then, 
but it is what web programmers have to work with, and what a billion users have to put up with.


