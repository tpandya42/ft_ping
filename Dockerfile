FROM debian:stable-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential iputils-ping \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ft_ping
COPY . .
RUN make

CMD ["/bin/bash"]