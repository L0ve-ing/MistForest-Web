FROM node:24-bookworm-slim AS builder
RUN apt-get update && apt-get install -y --no-install-recommends g++ && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY src ./src
COPY web/referee.cpp ./web/referee.cpp
COPY scripts/build-web.mjs ./scripts/build-web.mjs
RUN node scripts/build-web.mjs

FROM node:24-bookworm-slim
WORKDIR /app
COPY --from=builder /app/build-web ./build-web
COPY web/server.mjs ./web/server.mjs
COPY web/public ./web/public
COPY src/assets ./src/assets
COPY maps ./maps
ENV HOST=0.0.0.0 PORT=8787
USER node
EXPOSE 8787
CMD ["node","web/server.mjs"]
