FROM 4pdosc/prpc-base:0.1.0

ADD . /prpc
WORKDIR /prpc
RUN J=`nproc | awk '{print int(($0 + 1)/ 2)}'` && \
    mkdir -p build && cd build && cmake .. && make -j$J && make test && make install && cd ..
