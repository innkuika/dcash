#define RAPIDJSON_HAS_STDSTRING 1

#include <map>
#include <string>
#include <cstring>
#include <random>

#include "AuthService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

std::string random_string(std::size_t length)
{
    const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    std::string random_string;

    for (std::size_t i = 0; i < length; ++i)
    {
        random_string += CHARACTERS[distribution(generator)];
    }

    return random_string;
}

bool is_username_valid(string s) {
    for (int i = 0; i < s.length(); ++i) {
        if (!islower(s[i])) {
            return false;
        }
    }
    return true;
}

AuthService::AuthService() : HttpService("/auth-tokens") {
}

void respond_username_and_auth_token(string user_id, string authToken, int statusCode, HTTPResponse *response) {
    // use rapidjson to create a return object
    Document document;
    Document::AllocatorType& a = document.GetAllocator();
    Value o;
    o.SetObject();

    // add a key value pair directly to the object
    o.AddMember("auth_token", authToken, a);
    o.AddMember("user_id", user_id, a);

    // now some rapidjson boilerplate for converting the JSON object to a string
    document.Swap(o);
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    document.Accept(writer);

    // set the return object
    response->setStatus(statusCode);
    response->setContentType("application/json");
    response->setBody(buffer.GetString() + string("\n"));
}

void AuthService::post(HTTPRequest *request, HTTPResponse *response) {
    WwwFormEncodedDict args = request->formEncodedBody();

    // check if user is in db
    string username = args.get("username");

    if (m_db->users.count(username) == 0) {
        // user not in db
        if (!is_username_valid(username)) {
            // username is invalid, e.g. should only contain lowercase
            response->setStatus(400);
            return;
        }
        // creating new user
        User* newUser = new User();
        newUser->username = username;
        newUser->password = args.get("password");
        try {
            newUser->email = args.get("email");
        } catch (...) {
            // email arg may not exist
        }
        newUser->balance = 0;
        newUser->user_id = random_string(24);
        m_db->users[username] = newUser;

        // return balance and return auth token
        string newAuthToken = random_string(24);
        m_db->auth_tokens[newAuthToken] = newUser;
        respond_username_and_auth_token(newUser->user_id, newAuthToken, 201, response);
        return;
    }

    string password = args.get("password");
    User* user = m_db->users[username];

    if (password != user->password) {
        // password doesn't match
        response->setStatus(403);
        return;
    }

    // return new auth token and user id
    string newAuthToken = random_string(24);
    m_db->auth_tokens[newAuthToken] = user;
    respond_username_and_auth_token(user->user_id, newAuthToken, 200, response);
}

void AuthService::del(HTTPRequest *request, HTTPResponse *response) {

}
