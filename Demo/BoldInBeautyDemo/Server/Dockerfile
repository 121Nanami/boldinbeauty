# Account service for the Boldinbeauty demo. Build: docker build -t boldinbeauty-accounts .  Run: see README.md
FROM node:22-alpine
ENV NODE_ENV=production
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci --omit=dev --no-audit --no-fund
COPY src ./src
USER node
EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=3s CMD wget -qO- http://127.0.0.1:${PORT:-8080}/health || exit 1
CMD ["node", "src/index.js"]
