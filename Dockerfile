FROM conanio/gcc13-ubuntu16.04

COPY cmake /home/conan/matchmaking_proxy/cmake
COPY matchmaking_proxy /home/conan/matchmaking_proxy/matchmaking_proxy
COPY CMakeLists.txt /home/conan/matchmaking_proxy
COPY test /home/conan/matchmaking_proxy/test
COPY conanfile.py /home/conan/matchmaking_proxy
COPY main.cxx /home/conan/matchmaking_proxy
COPY ProjectOptions.cmake /home/conan/matchmaking_proxy

WORKDIR /home/conan/matchmaking_proxy
#TODO this should be release not debug but there is a linker error "https://stackoverflow.com/questions/77959920/linker-error-defined-in-discarded-section-with-boost-asio-awaitable-operators
RUN sudo chown -R conan /home/conan  && conan remote add artifactory http://195.128.100.39:8081/artifactory/api/conan/conan-local && conan install . --output-folder=build --settings build_type=Debug  --settings compiler.cppstd=gnu20 --build=missing

WORKDIR /home/conan/matchmaking_proxy/build

#TODO this should be release not debug but there is a linker error "https://stackoverflow.com/questions/77959920/linker-error-defined-in-discarded-section-with-boost-asio-awaitable-operators"
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DBUILD_TESTS=True -D CMAKE_BUILD_TYPE=Debug

RUN cmake --build .

FROM conanio/gcc13-ubuntu16.04

COPY --from=0 /home/conan/matchmaking_proxy/build/run_server /home/conan/matchmacking_proxy/matchmaking_proxy

CMD [ "/home/conan/matchmacking_proxy/matchmaking_proxy"]