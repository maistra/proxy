FROM fedora:32

ARG BAZEL_VERSION=1.1.0

RUN curl -o /usr/local/bin/bazel -Ls https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-linux-x86_64 && \
    chmod +x /usr/local/bin/bazel

RUN mkdir /work
WORKDIR /work

RUN  dnf -y upgrade --refresh && \
     dnf -y install which git make clang ninja-build \
            autoconf automake libtool cmake python2 \
            patch lld libatomic libstdc++-static && \
     dnf -y clean all

ENV CC=clang CXX=clang++ USER=user HOME=/home/user
RUN mkdir -p /home/user && chmod 777 /home/user

ADD update-deps.sh /usr/local/bin/update-deps.sh
ENTRYPOINT /usr/local/bin/update-deps.sh
