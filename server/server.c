#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 80
#define BUFFER_SIZE 4096
#define MAX_FILENAME_SIZE 256

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}



void send_response(int client_socket, const char *status, const char *content_type, const char *body) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s", status, content_type, strlen(body), body);
    send(client_socket, response, strlen(response), 0);
}

// Envoie la page HTML pageName
void serve_index(int client_socket, char* pageName) {
    FILE *fp = fopen(pageName, "r");
    if (fp == NULL) {
        send_response(client_socket, "500 Internal Server Error", "text/plain", "Failed to open HTML file");
        return;
    }

    char html[BUFFER_SIZE];
    memset(html, 0, sizeof(html));
    fread(html, sizeof(char), sizeof(html) - 1, fp);
    html[sizeof(html) - 1] = '\0'; // Assurez-vous que le buffer est null-terminé
    fclose(fp);
    
    send_response(client_socket, "200 OK", "text/html", html);
}

void handle_upload(int client_socket, const char *body, size_t body_length) {
    body += 5; // skip 'text='
    printf("Received text: [%.*s]\n", (int)body_length, body);
    
    // Enregistrer le texte dans un fichier
    FILE *fp = fopen("../soundQueue/text.txt", "a");
    if (fp == NULL) {
        send_response(client_socket, "500 Internal Server Error", "text/plain", "Failed to open file");
        return;
    }

    fwrite(body, sizeof(char), body_length, fp);
    fclose(fp);
    
    serve_index(client_socket, "source/uploadDone.html");
}


void handle_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    buffer[sizeof(buffer) - 1] = '\0'; //  null-terminé

    // GET /
    if (strstr(buffer, "GET / ") != NULL) 
    {
        serve_index(client_socket, "source/index.html"); // envoie la racine
        return;
    }

    // GET /upload
    if (strstr(buffer, "GET /upload ") != NULL) 
    {
        serve_index(client_socket, "source/upload.html"); // envoie le lien pour rediriger a la racine
        return;
    }

    if (strstr(buffer, "POST /upload") != NULL) {
        char *content_start = strstr(buffer, "\r\n\r\n");

        if (content_start) {
            content_start += 4; // Skip \r\n\r\n
            size_t body_length = strlen(content_start);
            
            handle_upload(client_socket, content_start, body_length);
        } else {
            send_response(client_socket, "400 Bad Request", "text/plain", "Invalid request");
        }
        return;
    }
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // Création du socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        error("socket failed");

    // Attacher le socket au port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        error("setsockopt");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Lier le socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
        error("bind failed");

    // Écouter les connexions entrantes
    if (listen(server_fd, 3) < 0)
        error("listen");

    printf("Serveur HTTP en attente de connexions sur le port %d...\n", PORT);

    while (1) {
        // Accepter une connexion
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            error("accept");
        }

        handle_request(client_socket);
        close(client_socket);
    }

    close(server_fd);
    return 0;
}