FROM nginx:alpine

COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY public/    /usr/share/nginx/html/
COPY docker-entrypoint.sh /docker-entrypoint.d/40-config.sh

RUN sed -i 's/\r$//' /docker-entrypoint.d/40-config.sh \
    && chmod +x /docker-entrypoint.d/40-config.sh

EXPOSE 80
