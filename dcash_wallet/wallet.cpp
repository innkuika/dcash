#define RAPIDJSON_HAS_STDSTRING 1

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>

#include "WwwFormEncodedDict.h"
//#include "HttpClient.h"
#include "../shared/include/HttpClient.h"
#include "../shared/include/HTTPClientResponse.h"


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

void update_user_email(string email) {
    HttpClient *client = new HttpClient(string_to_char_array(API_SERVER_HOST), API_SERVER_PORT, false);
    client->set_header("x-auth-token", auth_token);
    WwwFormEncodedDict args;
    args.set("email", email);
    client->write_request("/users/" + user_id, "PUT", args.encode());
    HTTPClientResponse *response = client->read_response();

    if (!response->success()) { write_error_message(); }
}

void print_user_balance() {
    HttpClient *client = new HttpClient(string_to_char_array(API_SERVER_HOST), API_SERVER_PORT, false);
    client->set_header("x-auth-token", auth_token);
    client->write_request("/users/" + user_id, "GET", "");
    HTTPClientResponse *response = client->read_response();

    if (response->success()) {
        Document *document = response->jsonBody();
        int balance = (*document)["balance"].GetInt();
        cout << "Balance: $" << std::setprecision(2) << fixed << (float) balance / 100 << endl;
    } else {
        // some thing went wrong
        write_error_message();
    }
}

void make_deposit_request(string stripe_token, int amount) {
    WwwFormEncodedDict args;
    args.set("stripe_token", stripe_token);
    args.set("amount", amount);

    HttpClient *client = new HttpClient(string_to_char_array(API_SERVER_HOST), API_SERVER_PORT, false);
    client->set_header("x-auth-token", auth_token);
    client->write_request("/deposits", "POST", args.encode());
    HTTPClientResponse *response = client->read_response();

    if (response->success()) {
        print_user_balance();
    } else {
        write_error_message();
    }
}

void handle_send(int argc, char *argv[]) {
    if (argc != 3) {
        // check if number of arguments is correct
        write_error_message();
        return;
    }
    string to_username = argv[1];
    double amount_double = ::atof(argv[2]);

    WwwFormEncodedDict args;
    args.set("to", to_username);
    args.set("amount", (int) amount_double * 100);

    HttpClient *client = new HttpClient(string_to_char_array(API_SERVER_HOST), API_SERVER_PORT, false);
    client->set_header("x-auth-token", auth_token);
    client->write_request("/transfers", "POST", args.encode());
    HTTPClientResponse *response = client->read_response();

    if (response->success()) {
        print_user_balance();
    } else {
        write_error_message();
    }
}

void handle_deposit(int argc, char *argv[]) {
    if (argc != 6) {
        // check if number of arguments is correct
        write_error_message();
        return;
    }
    // call stripe API to get card token
    WwwFormEncodedDict args;
    args.set("card[number]", argv[2]);
    args.set("card[exp_month]", argv[4]);
    args.set("card[exp_year]", argv[3]);
    args.set("card[cvc]", argv[5]);
    HttpClient client("api.stripe.com", 443, true);
    client.set_header("Authorization", string("Bearer ") + PUBLISHABLE_KEY);
    client.write_request("/v1/tokens", "POST", args.encode());
    HTTPClientResponse *response = client.read_response();

    if (response->success()) {
        // stripe authenticated the card
        Document *document = response->jsonBody();
        string card_token = (*document)["id"].GetString();
        delete document;

        double amount_double = ::atof(argv[1]);
        // check if amount is valid
        if (amount_double < 0) {
            write_error_message();
            return;
        }

        // make http request to our own server
        make_deposit_request(card_token, (int) amount_double * 100);

    } else {
        write_error_message();
    }
}

void delete_auth_token(string old_auth_token) {
    HttpClient *client = new HttpClient(string_to_char_array(API_SERVER_HOST), API_SERVER_PORT, false);
    client->set_header("x-auth-token", old_auth_token);
    client->write_request("/auth-tokens/" + old_auth_token, "DELETE", "");
    HTTPClientResponse *response = client->read_response();

    if (!response->success()) {
        write_error_message();
    }
}

bool is_username_valid(string s) {
    for (int i = 0; i < (int) s.length(); ++i) {
        if (!islower(s[i])) {
            return false;
        }
    }
    return true;
}

void handle_auth(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        // check if number of arguments is correct
        write_error_message();
        return;
    }

    string username = argv[1];
    // check if username is valid
    if (!is_username_valid(username)) {
        write_error_message();
        return;
    }

    WwwFormEncodedDict args;
    args.set("username", argv[1]);
    args.set("password", argv[2]);
    HttpClient *client = new HttpClient(string_to_char_array(API_SERVER_HOST), API_SERVER_PORT, false);
    client->write_request("/auth-tokens", "POST", args.encode());
    HTTPClientResponse *response = client->read_response();

    if (response->success()) {
        // logged in, set auth token and user id
        Document *document = response->jsonBody();
        // keep a copy of old user token
        string old_auth_token = auth_token;
        string old_user_id = user_id;

        auth_token = (*document)["auth_token"].GetString();
        user_id = (*document)["user_id"].GetString();

        // delete old auth token
        if (!old_auth_token.empty() && old_auth_token.find_first_not_of(' ') != std::string::npos &&
            old_user_id != user_id) {
            delete_auth_token(old_auth_token);
        }

        delete document;

        if (argc == 4) {
            // update user email
            update_user_email(argv[3]);
        }

        // print user balance
        print_user_balance();
    } else {
        // some thing went wrong
        write_error_message();
    }
}

void handle_command(string command) {
    if (command.empty() || command.find_first_not_of(' ') == std::string::npos) {
        // empty line
        write_error_message();
        return;
    }

    pair<int, char **> p = split_string(string_to_char_array(command));
    char **command_array = p.second;
    int command_count = p.first;
    char *command_option = command_array[0];

    if (strcmp(command_option, "auth") == 0) {
        handle_auth(command_count, command_array);
    } else if (strcmp(command_option, "balance") == 0) {
        print_user_balance();
    } else if (strcmp(command_option, "deposit") == 0) {
        handle_deposit(command_count, command_array);
    } else if (strcmp(command_option, "send") == 0) {
        handle_send(command_count, command_array);
    } else if (strcmp(command_option, "logout") == 0) {
        delete_auth_token(auth_token);
        exit(0);
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
    auth_token = "";

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
