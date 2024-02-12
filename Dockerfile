FROM ghcr.io/werto87/arch_linux_docker_image/archlinux_base_devel_conan:2024_02_11_12_32_48 as BUILD

COPY cmake /home/build_user/matchmaking_proxy/cmake
COPY matchmaking_proxy /home/build_user/matchmaking_proxy/matchmaking_proxy
COPY CMakeLists.txt /home/build_user/matchmaking_proxy
COPY test /home/build_user/matchmaking_proxy/test
COPY conanfile.py /home/build_user/matchmaking_proxy
COPY main.cxx /home/build_user/matchmaking_proxy
COPY ProjectOptions.cmake /home/build_user/matchmaking_proxy

WORKDIR /home/build_user/matchmaking_proxy

RUN sudo chown -R build_user /home/build_user && conan profile detect  && conan remote add artifactory http://195.128.100.39:8081/artifactory/api/conan/conan-local && conan install . --output-folder=build --settings compiler.cppstd=gnu20 --build=missing

WORKDIR /home/build_user/matchmaking_proxy/build

RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DBUILD_TESTS=True -D CMAKE_BUILD_TYPE=Release

RUN cmake --build .

RUN test/_test

FROM archlinux:latest

COPY --from=BUILD /home/build_user/matchmaking_proxy/build/run_server /home/build_user/matchmacking_proxy/matchmaking_proxy

CMD [ "/home/build_user/matchmacking_proxy/matchmaking_proxy"]