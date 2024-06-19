FROM debian:bookworm-slim

EXPOSE 8080/tcp

RUN apt update ; \
    apt install \
    build-essential \
    cmake \
    pkg-config \
    git \
    vim \
    wget \
    libmpg123-dev -qq -y

RUN apt upgrade -qq -y

RUN mkdir -p /opt/whisper_cpp/models
RUN git clone https://github.com/ggerganov/whisper.cpp /opt/whisper_cpp/include
RUN cd /opt/whisper_cpp/include ; cmake -B ../lib ; cmake --build ../lib --config Release -- -j 12
RUN /opt/whisper_cpp/include/models/download-ggml-model.sh tiny
RUN /opt/whisper_cpp/include/models/download-ggml-model.sh base
RUN /opt/whisper_cpp/include/models/download-ggml-model.sh small
RUN mv /opt/whisper_cpp/include/models/ggml-tiny.bin /opt/whisper_cpp/models/ggml-model-whisper-tiny.bin
RUN mv /opt/whisper_cpp/include/models/ggml-base.bin /opt/whisper_cpp/models/ggml-model-whisper-base.bin
RUN mv /opt/whisper_cpp/include/models/ggml-small.bin /opt/whisper_cpp/models/ggml-model-whisper-small.bin

RUN git clone https://github.com/akscf/wstk_c /usr/local/src/wstk_c
RUN git clone https://github.com/akscf/whisperd /usr/local/src/whisperd

RUN /bin/bash -c 'mkdir -p /opt/whisperd/{bin,include,var,lib,configs}'
RUN mkdir -p /opt/whisperd/lib/mods
RUN cp -rfp /usr/local/src/wstk_c/libwstk/include /opt/whisperd/include/wstk
RUN cp -rfp /usr/local/src/whisperd/sources/whisperd/include /opt/whisperd/include/whisperd

RUN cd /usr/local/src/wstk_c/libwstk ; \
    make ; \
    cp libwstk.so /opt/whisperd/lib/libwstk.so

RUN cd /usr/local/src/whisperd/sources/whisperd ; \
    make ; \
    cp libwhisperd.so /opt/whisperd/lib/libwhisperd.so

RUN cp -rfp /usr/local/src/whisperd/sources/whisperd/whisperd \
    /opt/whisperd/bin/whisperd

RUN cd /usr/local/src/whisperd/sources/modules/mod-whisper-cpp ; \
    make ; \
    make install

RUN cp /usr/local/src/whisperd/sources/whisperd/misc/whisperd-conf.xml /opt/whisperd/configs/whisperd-conf.xml

CMD ["/opt/whisperd/bin/whisperd", "-a", "start-debug"]
