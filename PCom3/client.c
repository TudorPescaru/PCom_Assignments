// Copyright 2021 Pescaru Tudor-Mihai 321CA
#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>

#include "./helpers.h"
#include "./requests.h"
#include "./parson.h"

// Server path defines
#define REGISTER "/api/v1/tema/auth/register"
#define LOGIN "/api/v1/tema/auth/login"
#define ACCESS "/api/v1/tema/library/access"
#define BOOKS "/api/v1/tema/library/books"
#define LOGOUT "/api/v1/tema/auth/logout"
// Content type define
#define JSON "application/json"

// Loop over input buffer and read chars until buffer is empty
void clear_stdin() {
	char c = 0;
	while ((c = getchar()) != '\n' && c != EOF);
}

// Print server response in a nicer format
void print_response(char *response) {
    if (response == NULL) {
        return;
    }
    // Check if response has correct format
    if (!strncmp(response, "HTTP/1.1", strlen("HTTP/1.1"))) {
        // Print response status code and text
        char *line_end = strstr(response, "\r\n");
        int len = strlen(response + strlen("HTTP/1.1 ")) - strlen(line_end);
        printf("[Server %.*s]", len, response + strlen("HTTP/1.1 "));
    } else {
        printf("[Bad Response]\n");
        return;
    }
    // Check content of response for JSON or JSON arrya
    char *content_json = basic_extract_json_response(response);
    char *content_array_json = basic_extract_json_array_response(response);
    if (content_json != NULL && content_array_json == NULL) {
        // Print JSON object from response
        printf(" Server says:\n");
        // Parse and pretty format JSON from response
        JSON_Value *response_json = json_parse_string(content_json);
        char *serialized_string = json_serialize_to_string_pretty(response_json);
        printf("%s\n", serialized_string);
        // Free JSON elements used for parsing
        json_free_serialized_string(serialized_string);
        json_value_free(response_json);
    } else if (content_array_json != NULL) {
        // Print JSON array from response
        printf(" Server says:\n");
        // Parse and pretty format JSON from response
        JSON_Value *response_json = json_parse_string(content_array_json);
        char *serialized_string = json_serialize_to_string_pretty(response_json);
        printf("%s\n", serialized_string);
        // Free JSON elements used for parsing
        json_free_serialized_string(serialized_string);
        json_value_free(response_json);
    } else {
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    // Check if server IP and PORT were given
    if (argc < 3) {
        error("Usage: ./client <HOST> <PORT>");
    }

    char line_buf[LINELEN], *message, *response, *body[1], *cookies[1], *jwt;
    int sockfd, running = 1, portno;

    cookies[0] = NULL;
    jwt = NULL;

    // Convert port number to int
    portno = atoi(argv[2]);

    while (running) {
        // Read command from stdin
        char *cmd;
        fgets(line_buf, LINELEN, stdin);
        cmd = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
        snprintf(cmd, strlen(line_buf), "%s", line_buf);

        if (!strcmp(cmd, "register")) {
            char *username, *password;
            // Show username prompt and read from stdin
            printf("username=");
            fgets(line_buf, LINELEN, stdin);
            username = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(username, strlen(line_buf), "%s", line_buf);

            // Show password prompt and read from stdin
            printf("password=");
            fgets(line_buf, LINELEN, stdin);
            password = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(password, strlen(line_buf), "%s", line_buf);

            // Build JSON payload for request with username and password
            JSON_Value *payload = json_value_init_object();
            JSON_Object *payload_object = json_value_get_object(payload);
            json_object_set_string(payload_object, "username", username);
            json_object_set_string(payload_object, "password", password);
            // Convert JSON to string in pretty format
            body[0] = json_serialize_to_string_pretty(payload);

            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_post_request(argv[1], REGISTER, JSON, body, 1,
                                            NULL, 0, NULL);
            // Free elements used in JSON payload creation
            json_free_serialized_string(body[0]);
            json_value_free(payload);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            
            // Print response
            print_response(response);
            
            // Free allocated memory and close connection
            free(response);
            free(message);
            free(username);
            free(password);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "login")) {
            char *username, *password;
            // Show username prompt and read from stdin
            printf("username=");
            fgets(line_buf, LINELEN, stdin);
            username = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(username, strlen(line_buf), "%s", line_buf);

            // Show password prompt and read from stdin
            printf("password=");
            fgets(line_buf, LINELEN, stdin);
            password = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(password, strlen(line_buf), "%s", line_buf);

            // Build JSON payload for request with username and password
            JSON_Value *payload = json_value_init_object();
            JSON_Object *payload_object = json_value_get_object(payload);
            json_object_set_string(payload_object, "username", username);
            json_object_set_string(payload_object, "password", password);
            // Convert JSON to string in pretty format
            body[0] = json_serialize_to_string_pretty(payload);

            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_post_request(argv[1], LOGIN, JSON, body,
                                            1, NULL, 0, NULL);
            // Free elements used in JSON payload creation
            json_free_serialized_string(body[0]);
            json_value_free(payload);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            // Print response
            print_response(response);

            // Get session cookie from server response
            char *param = strstr(response, "Set-Cookie: connect.sid=");
            if (param != NULL) {
                cookies[0] = (char*)calloc(LINELEN, sizeof(char));
                char *cookie = strstr(response, "; Path");
                snprintf(cookies[0], strlen(param) - strlen("Set-Cookie: ") -
                            strlen(cookie) + 1, "%s",
                            param + strlen("Set-Cookie: "));
            }

            // Free allocated memory and close connection
            free(response);
            free(message);
            free(username);
            free(password);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "enter_library")) {
            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(argv[1], ACCESS, NULL,
                                            cookies, 1, NULL);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            // Print response
            print_response(response);

            // Get JWT token from response
            char *content = basic_extract_json_response(response);
            if (content != NULL) {
                // Parse response JSON content
                JSON_Value *token = json_parse_string(content);
                JSON_Object *token_object = json_value_get_object(token);
                // Check if JSON has token
                if (json_object_has_value(token_object, "token")) {
                    // Extract token from JSON
                    const char *token_val = json_object_get_string(token_object,
                                                                    "token");
                    jwt = (char*)calloc(strlen(token_val) + 2, sizeof(char));
                    snprintf(jwt, strlen(token_val) + 1, "%s", token_val);
                }
                // Free JSON value
                json_value_free(token);
            }

            // Free allocated memory and close connection
            free(response);
            free(message);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "get_books")) {
            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(argv[1], BOOKS, NULL,
                                            cookies, 1, jwt);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            // Print response
            print_response(response);

            // Free allocated memory and close connection
            free(response);
            free(message);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "get_book")) {
            int id;
            // Show book id prompt and read from stdin
            printf("id=");
            scanf("%d", &id);
            clear_stdin();

            // Add book id to books path
            char *book_path = (char*)calloc(LINELEN, sizeof(char));
            sprintf(book_path, "%s/%d", BOOKS, id);

            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(argv[1], book_path, NULL,
                                            cookies, 1, jwt);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            // Print response
            print_response(response);

            // Free allocated memory and close connection
            free(response);
            free(message);
            free(book_path);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "add_book")) {
            char *title, *author, *genre, *publisher;
            int page_count;
            // Show title prompt and read from stdin
            printf("title=");
            fgets(line_buf, LINELEN, stdin);
            title = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(title, strlen(line_buf), "%s", line_buf);

            // Show author prompt and read from stdin
            printf("author=");
            fgets(line_buf, LINELEN, stdin);
            author = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(author, strlen(line_buf), "%s", line_buf);

            // Show genre prompt and read from stdin
            printf("genre=");
            fgets(line_buf, LINELEN, stdin);
            genre = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(genre, strlen(line_buf), "%s", line_buf);

            // Show publisher prompt and read from stdin
            printf("publisher=");
            fgets(line_buf, LINELEN, stdin);
            publisher = (char*)calloc(strlen(line_buf) + 1, sizeof(char));
            snprintf(publisher, strlen(line_buf), "%s", line_buf);

            // Show page count prompt and read from stdin
            printf("page_count=");
            if (scanf("%d", &page_count) != 1 || page_count < 0) {
                // Skip command if input was not correct
                printf("Invalid format for page_count!\n");
                clear_stdin();
                free(title);
                free(author);
                free(genre);
                free(publisher);
                free(cmd);
                continue;
            }
            clear_stdin();

            // Build JSON payload for request with title, author,
            // genre, page count and publisher
            JSON_Value *payload = json_value_init_object();
            JSON_Object *payload_object = json_value_get_object(payload);
            json_object_set_string(payload_object, "title", title);
            json_object_set_string(payload_object, "author", author);
            json_object_set_string(payload_object, "genre", genre);
            json_object_set_number(payload_object, "page_count", page_count);
            json_object_set_string(payload_object, "publisher", publisher);
            body[0] = json_serialize_to_string_pretty(payload);

            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_post_request(argv[1], BOOKS, JSON, body, 1,
                                            cookies, 1, jwt);
            // Free elements used in JSON payload creation
            json_free_serialized_string(body[0]);
            json_value_free(payload);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            // Print response
            print_response(response);

            // Free allocated memory and close connection
            free(response);
            free(message);
            free(title);
            free(author);
            free(genre);
            free(publisher);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "delete_book")) {
            int id;
            // Show book id prompt and read from stdin
            printf("id=");
            scanf("%d", &id);
            clear_stdin();

            // Add book id to books path
            char *book_path = (char*)calloc(LINELEN, sizeof(char));
            sprintf(book_path, "%s/%d", BOOKS, id);

            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_delete_request(argv[1], book_path,
                                                cookies, 1, jwt);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            // Print response
            print_response(response);
            
            // Free allocated memory and close connection
            free(response);
            free(message);
            free(book_path);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "logout")) {
            // Open connection, send request and receive response
            sockfd = open_connection(argv[1], portno, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(argv[1], LOGOUT, NULL,
                                            cookies, 1, NULL);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            // Print response
            print_response(response);

            // Free allocated memory and close connection
            free(response);
            free(message);
            close_connection(sockfd);
        } else if (!strcmp(cmd, "exit")) {
            // Set running state to 0
            running = 0;
        } else {
            printf("Command unrecognized\n");
        }
        free(cmd);
    }

    // Free memory allocated for session cookie and jwt token
    if (cookies[0] != NULL) {
        free(cookies[0]);
    }
    if (jwt != NULL) {
        free(jwt);
    }

    return 0;
}
