
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>

char html[32768];
char css[8192];
char js[8192];
char charset[64] = "UTF-8";  
char lang[64] = "en";       

void trim(char *s) {
    int len = strlen(s);
    while(len > 0 && (s[len-1]=='\n' || s[len-1]=='\r' || s[len-1]==' ' || s[len-1]=='\t'))
        s[--len] = 0;
}

char* extract_attr(const char* line, const char* attr) {
    static char val[64];
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    char* p = strstr(line, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    char* q = strchr(p, '"');
    if (!q) return NULL;
    size_t len = q - p;
    if (len >= sizeof(val)) len = sizeof(val)-1;
    strncpy(val, p, len);
    val[len] = 0;
    return val;
}

void parse_tlm(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        perror("Erro abrindo arquivo");
        exit(1);
    }

    css[0] = 0;
    js[0] = 0;
    char title[256] = {0};
    char body[16384] = {0};
    char linha[512];
    int in_style = 0, in_js = 0;

    while (fgets(linha, sizeof(linha), f)) {
        trim(linha);

        if (in_style) {
            if (strcmp(linha, "```") == 0) {
                in_style = 0;
            } else {
                strcat(css, linha);
                strcat(css, "\n");
            }
            continue;
        }

        if (in_js) {
            if (strcmp(linha, "```") == 0) {
                in_js = 0;
            } else {
                strcat(js, linha);
                strcat(js, "\n");
            }
            continue;
        }

        if (strncmp(linha, "TlmStyle```", 11) == 0) {
            in_style = 1;
            continue;
        }

        if (strncmp(linha, "TlmJs```", 8) == 0) {
            in_js = 1;
            continue;
        }

        if (strncmp(linha, "TlmTlt", 6) == 0) {
            char* p = strchr(linha, '"');
            if (p) {
                p++;
                char* q = strchr(p, '"');
                if (q) *q = 0;
                strncpy(title, p, sizeof(title)-1);
            }
            continue;
        }

        if (strncmp(linha, "Tlmp", 4) == 0) {
            char* p = strchr(linha, '"');
            if (p) {
                p++;
                char* q = strchr(p, '"');
                if (q) *q = 0;
                strcat(body, "<p>");
                strcat(body, p);
                strcat(body, "</p>\n");
            }
            continue;
        }

        if (strncmp(linha, "TlmBtn", 6) == 0) {
            char texto[256] = {0};
            char id[256] = {0};
            char* p = strstr(linha, "\"");
            if (!p) continue;
            p++;
            char* q = strchr(p, '"');
            if (!q) continue;
            strncpy(texto, p, q - p);
            texto[q - p] = 0;

            p = strstr(q, "id");
            if (p) {
                p = strstr(p, "\"");
                if (p) {
                    p++;
                    q = strchr(p, '"');
                    if (q) {
                        strncpy(id, p, q - p);
                        id[q - p] = 0;
                    }
                }
            }

            if (id[0]) {
                strcat(body, "<button id=\"");
                strcat(body, id);
                strcat(body, "\">");
                strcat(body, texto);
                strcat(body, "</button>\n");
            } else {
                strcat(body, "<button>");
                strcat(body, texto);
                strcat(body, "</button>\n");
            }
            continue;
        }
       
        if (strncmp(linha, "TlmMeta", 7) == 0) {
            char* c = extract_attr(linha, "charset");
            if (c) {
                strncpy(charset, c, sizeof(charset)-1);
                charset[sizeof(charset)-1] = 0;
                continue;
            }
            c = extract_attr(linha, "lang");
            if (c) {
                strncpy(lang, c, sizeof(lang)-1);
                lang[sizeof(lang)-1] = 0;
                continue;
            }
        }
    }

    fclose(f);

    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n<html lang=\"%s\">\n<head>\n<meta charset=\"%s\">\n<title>%s</title>\n<style>\n%s</style>\n</head>\n<body>\n%s\n<script>\n%s</script>\n</body>\n</html>\n",
        lang, charset, title, css, body, js);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Uso: %s arquivo.tlm port:2000\n", argv[0]);
        return 1;
    }

    parse_tlm(argv[1]);

    int port = atoi(strchr(argv[2], ':') + 1);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(s, 5) < 0) {
        perror("listen");
        exit(1);
    }

    printf("TlmTlt server running on http://localhost:%d\n", port);

    while (1) {
        int c = accept(s, NULL, NULL);
        if (c < 0) continue;

        char req[1024];
        read(c, req, sizeof(req) - 1);

        char header[256];
        snprintf(header, sizeof(header),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=%s\r\n\r\n",
            charset);

        write(c, header, strlen(header));
        write(c, html, strlen(html));
        close(c);
    }

    return 0;
}
