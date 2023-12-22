<p>
    Small lightweight daemon that provides similar to the OpenAI services for speech recognition through 'whisper_cpp'. <br>
    Was written in pure C and carries the whole dependencies with itself (except OpenSSL). <br>
</p>

### Version 1.0
- support MP3 / WAV (only single channel) files
- compatibility with OpenAI v1 http-api (with the same responses in JSON)
- some extra parameters for tuning the whisper for a certain request

### Build, install and starting
1) Go to the repo dir and perform: ./configure  (it configures necessary libs, such as: mpg123, libre, ...) <br>
   If everything went fine, try to build it: make clean all install <br>
   if you got some troubles on this stage (highly likely it depends with gcc optimisation options, such as: -mavx/sse/...) <br>
   you have to edit whisperd/Makefile and adjust options in 'OFLAFS' which more suitable for you hardware (for instance, try to replace '-march' on yours) <br>
   after that try to perform make again, if despite everything the troubles won't disappeared try to post ones here in the issues section, <br>
   I hope we'll manage to figure out together. <br>
   
3) Models, the models should be in ggml format and can be fouddn [here](https://github.com/ggerganov/whisper.cpp/tree/master/models) <br>
   Downlod them into: /opt/whisperd/models <br>

3) Configuire the whisperd <br>
   The main configuration file is placed here: /opt/whisperd/configs/whisperd-conf.xml <br>
   I have not ides what to write here, just adjust the options that seem to you not suitable... <br>

4) Try to start <br>
   for start: /opt/whisper/bin/whisperd -a start <br>
   for stop : /opt/whisper/bin/whisperd -a stop <br>

   It uses system-log so the informations about the troubles and other look for there. <br>

5) if you're going to use https don't forget to create certificate and put it into: /opt/whisperd/configs/server.pem <br>
   see: sources/whisperd/misc/gen-cect.sh <br>

### Usage example
Absolutely similar with the OpenAI:
```bash
# curl http://127.0.0.1/v1/audio/transcriptions \
   -H "Authorization: Bearer --YourSecertFromConfig--" \
   -H "Content-Type: multipart/form-data" \
   -F model="whisper-1" \
   -F file="@test.mp3"
```

With additional parameters (available only with this daemon):
```bash
# curl http://127.0.0.1/v1/audio/transcriptions \
   -H "Authorization: Bearer --YourSecertFromConfig--" \
   -H "Content-Type: multipart/form-data" \
   -F opts="{\"language\":\"ru\"}" \
   -F model="whisper-1" \
   -F file="@test.mp3"
```
available options: <br>
    - <b>language</b> (string) 2-letter abbreviations <br>
    - <b>max-tokens</b> (int) 0...X <br>
    - <b>translate</b> (bool) true/false <br>


