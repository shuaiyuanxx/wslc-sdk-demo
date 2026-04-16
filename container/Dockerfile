FROM alpine:latest
RUN apk add --no-cache gcc musl-dev
COPY server.c /app/server.c
RUN gcc -O2 -o /app/server /app/server.c
EXPOSE 9000
CMD ["/app/server"]
