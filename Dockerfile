ARG FIREBIRD_VERSION=5.0.3

FROM firebirdsql/firebird:${FIREBIRD_VERSION}-noble AS builder

WORKDIR /app

ADD https://github.com/FirebirdSQL/firebird.git#v${FIREBIRD_VERSION} /app/firebird

COPY *packages.txt .

RUN apt-get -qq update -y && apt-get -qq upgrade -y && \
    xargs apt-get -qq install --no-install-recommends -y < packages.txt || echo "no system packages" && \
    apt-get -qq clean -y && apt-get -qq autoremove -y && rm -rf /var/lib/apt/lists/*

COPY . .

WORKDIR /app/build

RUN cmake ../projects/CSVExport -DFIREBIRD_INCLUDE_DIR=/app/firebird/src/include && \
    make


FROM firebirdsql/firebird:${FIREBIRD_VERSION}-noble AS runner

COPY --from=builder /app/build /app/build

ENTRYPOINT ["/app/build/CSVExport"]
CMD ["-h"]
