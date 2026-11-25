FROM devkitpro/devkita64:20260219 AS toolchain
WORKDIR /app
CMD ["make", "-j4"]

FROM toolchain AS build
ARG NAVIDROME_VERSION
COPY . .
RUN --mount=type=cache,target=/app/build.aurore make -j4

FROM toolchain AS overlay-build
COPY . .
RUN make -C overlay -j4
RUN make -C vendor/sys-tune/sys-tune/nxExt -j4 && \
    make -C vendor/sys-tune/sys-tune -j4 API_VERSION=4 WANT_MP3=1 WANT_FLAC=1 WANT_WAV=1
RUN mkdir -p /package/switch/.overlays /package/atmosphere/contents/4200000000000000/flags /package/licenses && \
    cp overlay/NavidromeNX.ovl /package/switch/.overlays/ && \
    cp vendor/sys-tune/sys-tune/sys-tune.nsp /package/atmosphere/contents/4200000000000000/exefs.nsp && \
    cp vendor/sys-tune/sys-tune/toolbox.json /package/atmosphere/contents/4200000000000000/ && \
    touch /package/atmosphere/contents/4200000000000000/flags/boot2.flag && \
    cp vendor/sys-tune/LICENSE /package/licenses/sys-tune.txt && \
    cp vendor/libultrahand/LICENSE /package/licenses/libultrahand.txt && \
    cp vendor/libultrahand/SUB_LICENSE /package/licenses/libultrahand-sub-license.txt && \
    cp docs/ultrahand.md /package/README.md

FROM scratch AS overlay-artifact
COPY --from=overlay-build /package/ /

FROM scratch AS artifact
COPY --from=build /app/navidrome_nx.nro /navidrome_nx.nro
