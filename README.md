<p>
    Lightweight server with unified web API for various whisper implementations.<br>
</p>

### Version 1.0 (old version)
Capable to work only with whisper_cpp and have some issues...
- support MP3 / WAV (only single channel) files
- compatibility with OpenAI v1 http-api
- some extra parameters for tuning the whisper for a certain request

### Version 2.0
- supports the latest version of whisper_cpp (10.06.2024)<br> 
  with cpu/gpu and other features (see: mod-whisper-cpp)
- mp3/wav files<br>
 
Building the whisperd:<br>
 Let's say the installation path be: /opt/whisperd
 The server depends on following: <br>
 1) mpg123-1.32.3 or later <br>
    if you have already this in your system just correct MPG123_INC and MPG123_LIB in the Makefile <br>
    otherwise download and install it, if you don't want to dirty your systrem just copy libmpg123.so and libsyn123.so into '/opt/whisperd/lib' <br>
    and headers files into '/opt/whisperd/include/libmpg123' and '/opt/whisperd/include/libsyn123'. <br>

 2) libwstk (download the latest version from here: [wstk_c](https://github.com/akscf/wstk_c)) <br>
    unpack and 'make clean all', after, copy libwstk.so into '/opt/whisperd/lib' and headers to '/opt/whisperd/include/wstk' <br>
 
 Well, try to build the whisperd itself: 'make clean all install' <br>
 and if everything goes well all the necessary files will be copied to '/opt/whispers'. <br>
 The main configuration file is 'whisperd-conf.xml' (placed at: /opt/whispers/configs). <br>

Building the modules, in particular 'mod-whisper-cpp' : <br>
 This is the main module which works with whisper_cpp, if you have already installed the one <br>
 just correct its paths in 'mod-whisper-cpp/Makefile' (LIBWHISPER_INC and LIBWHISPER_LIB) otherwise <br>
 donwload and install it (and models too, for examle into: /opt/whisper_cpp/models). <br>
 After that the same as above: 'make clean all install', if successful you'll get 'mode-whisper-cpp.so' (at: /opt/whisperd/lib/mods)
 and the configuration 'mod-whisper-cpp-conf.xml' (at: /opt/whispers/configs), cpu/gpu and other settings tuned there. <br>

One more thing, each modules binds its own 'endpoint' (this is a virtual path that gives access to the module via web), you can change it in: 'whisperd-conf.xml' <br>

The whisper additional parameters can be specified trgouth the field 'opts': -F opts="{\"language\":\"en\"}" <br>
Available options: language=XX, tokens=N, translate=true/false, single=true/false<be>
 

## Example requests
```txt
# curl -v http://127.0.0.1:8080/v1/audio/transcriptions/ -H "Authorization: Bearer secret123" -H "Content-Type: multipart/form-data" -F model="whisper-1" -F file="@ivr-congratulations_you_pressed_star.wav"
*   Trying 127.0.0.1:8080...
* TCP_NODELAY set
* Connected to 127.0.0.1 port 8080 (#0)
> POST /v1/audio/transcriptions/ HTTP/1.1
> Host: 127.0.0.1:8080
> User-Agent: curl/7.68.0
> Accept: */*
> Authorization: Bearer secret123
> Content-Length: 150364
> Content-Type: multipart/form-data; boundary=------------------------42e4e856c88747a5
> Expect: 100-continue
> 

* Done waiting for 100-continue
* We are completely uploaded and fine
* Mark bundle as not supporting multiuse
< HTTP/1.1 200 OK
< Server: whsd/1.x
< Date: Tue, 11 Jun 2024 23:21:57 GMT
< Last-Modified: Tue, 11 Jun 2024 23:21:57 GMT
< Connection: keep-alive
< Content-Type: application/json;charset=UTF-8
< Content-Length: 145
< 
* Connection #0 to host 127.0.0.1 left intact

{text: "Congratulations, you press star.That does not mean you are a star.It simply means that you can press buttonsand probably have fingers." }

```

