#define RAPIDJSON_HAS_STDSTRING 1

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "WwwFormEncodedDict.h"
#include "HttpClient.h"

#include "rapidjson/document.h"

using namespace std;
using namespace rapidjson;

int API_SERVER_PORT = 8080;
string API_SERVER_HOST = "localhost";
string PUBLISHABLE_KEY = "";

string auth_token;
string user_id;

void write_error_message() {
    char error_message[30] = "Error\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

char *string_to_char_array(string s) {
    int n = s.length();
    char *char_array = new char[n + 1];
    strcpy(char_array, s.c_str());
    return char_array;
}

std::pair<int, char **> split_string(char *string_to_split) {
    char deliminator[] = " ";
    char *token = strtok(string_to_split, deliminator);
    int i = 0;
    char **array;
    array = new char *[20];

    while (token != NULL) {
        array[i++] = token;
        token = strtok(NULL, deliminator);
    }
    return std::make_pair(i, array);
}

void makeHTTPRequest(string http_method, string url, WwwFormEncodedDict args) {
    MySocket socket(string_to_char_array(API_SERVER_HOST), API_SERVER_PORT);
    stringstream request;

    request << http_method + " " + url + " " + "HTTP/1.1" << "\r\n";
    request << "Host: localhost:8080" << "\r\n";
    request << "User-Agent: GunrockClient/1.0" << "\r\n";
    request << "Accept: */*" << "\r\n";
    request << "Content-Length: " << args.encode().length() << "\r\n";
    request << "Content-Type: application/x-www-form-urlencoded" << "\r\n";
    request << "\r\n";
    request << args.encode();
    request << "\r\n";

    socket.write(request.str());
    try {
        while (true) {
            string text = socket.read();
            cout << text;
        }
    } catch (...) {
        // ignore the exception and continue executing
    }
}

void handle_auth(int argc, char *argv[]) {
    cout << "begin handling auth..." << endl;
    if (argc != 3 && argc != 4) {
        // check if number of arguments is correct
        write_error_message();
        return;
    }

    WwwFormEncodedDict args;
    args.set("username", argv[1]);
    args.set("password", argv[2]);
    if (argc == 4) {
        args.set("email", argv[3]);
    }

    makeHTTPRequest("POST", "/auth-tokens", args);
}

void handle_command(string command) {
    pair<int, char **> p = split_string(string_to_char_array(command));
    char **command_array = p.second;
    int command_count = p.first;
    char *command_option = command_array[0];

    if (strcmp(command_option, "auth") == 0) {
        cout << "handling auth..." << endl;
        handle_auth(command_count, command_array);

    } else if (strcmp(command_option, "balance") == 0) {
        cout << "handling balance..." << endl;
    } else if (strcmp(command_option, "deposit") == 0) {
        cout << "handling deposit..." << endl;
    } else if (strcmp(command_option, "send") == 0) {
        cout << "handling send..." << endl;
    } else if (strcmp(command_option, "logout") == 0) {
        cout << "handling logout..." << endl;
    } else {
        write_error_message();
    }
}

int main(int argc, char *argv[]) {
    stringstream config;
    int fd = open("config.json", O_RDONLY);
    if (fd < 0) {
        cout << "could not open config.json" << endl;
        exit(1);
    }
    int ret;
    char buffer[4096];
    while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
        config << string(buffer, ret);
    }
    Document d;
    d.Parse(config.str());
    // parse config file and set to global var
    API_SERVER_PORT = d["api_server_port"].GetInt();
    API_SERVER_HOST = d["api_server_host"].GetString();
    PUBLISHABLE_KEY = d["stripe_publishable_key"].GetString();

    if (argc == 1) {
        // interactive mode
        while (true) {
            string command;
            cout << "D$> " << std::flush;
            getline(std::cin, command);
            handle_command(command);
        }
    } else if (argc == 2) {
        // batch mode
        string command;
        ifstream file(argv[1]);
        if (file) {
            while (!file.eof()) {
                getline(file, command);
                handle_command(command);
            }
        } else {
            write_error_message();
            return 1;
        }
    } else {
        write_error_message();
        return 1;
    }

    return 0;
}
