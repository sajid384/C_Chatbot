#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define INPUT_SIZE 1024
#define LOG_FILE "chat_history.txt"

struct Memory {
    char *data;
    size_t size;
};

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realSize = size * nmemb;
    struct Memory *mem = (struct Memory *)userp;

    char *ptr = realloc(mem->data, mem->size + realSize + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(mem->data + mem->size, contents, realSize);
    mem->size += realSize;
    mem->data[mem->size] = '\0';

    return realSize;
}

void escape_json(const char *input, char *output, size_t max) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < max - 1; i++) {
        if (input[i] == '"' || input[i] == '\\')
            output[j++] = '\\';
        output[j++] = input[i];
    }
    output[j] = '\0';
}

void log_conversation(FILE *file, const char *user, const char *bot) {
    fprintf(file, "You: %s\n", user);
    fprintf(file, "Bot: %s\n\n", bot);
    fflush(file);  
}

void ask_ollama(const char *prompt, FILE *logFile) {
    CURL *curl = curl_easy_init();
    if (!curl) return;

    struct Memory chunk = { malloc(1), 0 };

    char safe_prompt[2048];
    escape_json(prompt, safe_prompt, sizeof(safe_prompt));

    char json_data[4096];
    snprintf(json_data, sizeof(json_data),
             "{\"model\":\"llama3\",\"prompt\":\"%s\",\"stream\":false}",
             safe_prompt);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/generate");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

    if (curl_easy_perform(curl) == CURLE_OK) {
        char *resp = strstr(chunk.data, "\"response\":\"");
        if (resp) {
            resp += strlen("\"response\":\"");
            char *end = strstr(resp, "\",");
            if (end) *end = '\0';

            printf("Bot: %s\n", resp);
            log_conversation(logFile, prompt, resp); 
        } else {
            printf("Bot: (no response)\n");
        }
    } else {
        printf("Error contacting Ollama\n");
    }

    curl_easy_cleanup(curl);
    free(chunk.data);
}

int main() {
    char input[INPUT_SIZE];

    FILE *logFile = fopen(LOG_FILE, "a");  
    if (!logFile) {
        printf("Failed to open log file\n");
        return 1;
    }

    printf("🤖 Ollama C Chatbot (type 'exit' to quit)\n\n");

    while (1) {
        printf("You: ");
        if (!fgets(input, INPUT_SIZE, stdin))
            break;

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0)
            break;

        ask_ollama(input, logFile);
    }

    fclose(logFile);   
    printf("\nChat saved in %s\n", LOG_FILE);

    return 0;
}
