#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "AccountService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

AccountService::AccountService() : HttpService("/users") {
}

void respond_email_and_balance(string email, int balance, int statusCode, HTTPResponse *response) {
    // use rapidjson to create a return object
    Document document;
    Document::AllocatorType &a = document.GetAllocator();
    Value o;
    o.SetObject();

    // add a key value pair directly to the object
    o.AddMember("email", email, a);
    o.AddMember("balance", balance, a);

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

void AccountService::get(HTTPRequest *request, HTTPResponse *response) {
    User *user;
    try {
        user = getAuthenticatedUser(request);
    } catch (const ClientError &error) {
        response->setStatus(error.status_code);
    }

    // check if userid is in body
    if (request->getPathComponents().size() != 2) {
        // userid is not in body or there are more than 2 path components
        response->setStatus(400);
        return;
    }

    // check if user is in db
    if (m_db->users.count(user->username) == 0) {
        // user not in db
        response->setStatus(404);
        return;
    }

    // check if user and the user id passed in points to the same user
    string user_id = request->getPathComponents()[1];
    if (user->user_id != user_id) {
        response->setStatus(403);
        return;
    }

    respond_email_and_balance(user->email, user->balance, 200, response);
}

void AccountService::put(HTTPRequest *request, HTTPResponse *response) {
    User *user;
    try {
        user = getAuthenticatedUser(request);
    } catch (const ClientError &error) {
        response->setStatus(error.status_code);
    }

    // check if userid is in body
    if (request->getPathComponents().size() != 2) {
        // userid is not in body or there are more than 2 path components
        response->setStatus(400);
        return;
    }

    // check if user is in db
    if (m_db->users.count(user->username) == 0) {
        // user not in db
        response->setStatus(404);
        return;
    }

    // check if user and the user id passed in points to the same user
    string user_id = request->getPathComponents()[1];
    if (user->user_id != user_id) {
        response->setStatus(403);
        return;
    }

    WwwFormEncodedDict args = request->formEncodedBody();
    try {
        user->email = args.get("email");
    } catch (...) {
        // email was not passed in
        response->setStatus(400);
        return;
    }

    respond_email_and_balance(user->email, user->balance, 200, response);
}