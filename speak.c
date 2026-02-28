#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

// Base64 encoding table
static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                '4', '5', '6', '7', '8', '9', '+', '/', '~', '@', '#', '$', '%', '^', '&', '*', '(', ')', '-', '_', '+', '=', '`', '\t', '[', ']', '{', '}', ':', ';', '"', '\'', '<', ',', '>', '.', '/', '?'};

void base64_encode(const unsigned char *data, size_t input_length, char *encoded_data) {
    for (int i = 0, j = 0; i < input_length;) {
        unsigned int octet_a = i < input_length ? data[i++] : 0;
        unsigned int octet_b = i < input_length ? data[i++] : 0;
        unsigned int octet_c = i < input_length ? data[i++] : 0;
        unsigned int triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        encoded_data[j++] = encoding_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = encoding_table[triple & 0x3F];
    }
    encoded_data[4 * ((input_length + 2) / 3)] = '\0';
}

int pipefd[2];

void *downloader(void *arg) {
    unsigned char random_bytes[64]; // Slightly larger chunk
    char b64_text[128];
    char command[1024];
    FILE *random_fp;

    while (1) {
        random_fp = fopen("/dev/urandom", "r");
        if (random_fp) {
            fread(random_bytes, 1, sizeof(random_bytes), random_fp);
            fclose(random_fp);
        }
        base64_encode(random_bytes, sizeof(random_bytes), b64_text);

        snprintf(command, sizeof(command),
            "curl -G -s --data-urlencode \"q=%s\" "
            "\"https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=en\"",
            b64_text);

        FILE *curl_fp = popen(command, "r");
        if (curl_fp) {
            char buffer[8192];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), curl_fp)) > 0) {
                write(pipefd[1], buffer, bytes);
            }
            pclose(curl_fp);
        }
    }
    return NULL;
}

int main() {
    if (pipe(pipefd) == -1) return 1;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, downloader, NULL);

    // Chained atempo filters to reach 30x speed: 2.0 * 2.0 * 2.0 * 2.0 * 1.875
    char *ffplay_cmd = "ffplay -nodisp -autoexit -af \"atempo=2.0,atempo=2.0,atempo=2.0,atempo=2.0,atempo=1.875\" -f mp3 -i pipe:0 > /dev/null 2>&1";
    FILE *ffplay_fp = popen(ffplay_cmd, "w");
    
    if (ffplay_fp) {
        char buffer[8192];
        ssize_t bytes;
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            fwrite(buffer, 1, bytes, ffplay_fp);
            fflush(ffplay_fp);
        }
        pclose(ffplay_fp);
    }

    return 0;
}
