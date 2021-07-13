FROM 4pdosc/prpc-base:0.1.0

ADD . prpc
WORKDIR /prpc
RUN mkdir -p build && cd build && cmake .. && make -j && make test && make install && cd ..
