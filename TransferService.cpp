#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "TransferService.h"
#include "ClientError.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

TransferService::TransferService() : HttpService("/transfers") { }

void respond_balance_and_transfers(vector<Transfer *> transfers, int balance, string from_username,  int statusCode, HTTPResponse *response) {
    // use rapidjson to create a return object
    Document document;
    Document::AllocatorType &a = document.GetAllocator();
    Value o;
    o.SetObject();

    // add a key value pair directly to the object
    o.AddMember("balance", balance, a);

    // create an array
    Value array;
    array.SetArray();

    for(int i = 0; i < (int) transfers.size(); i++) {
//        if (transfers[i]->to->username == from_username || transfers[i]->from->username == from_username) {
        if (transfers[i]->from->username == from_username) {
            // add an object to our array
            Value to;
            to.SetObject();
            to.AddMember("from", transfers[i]->from->username, a);
            to.AddMember("to", transfers[i]->to->username, a);
            to.AddMember("amount", std::to_string(transfers[i]->amount), a);
            array.PushBack(to, a);
        }
    }

    o.AddMember("transfers", array, a);
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
void TransferService::post(HTTPRequest *request, HTTPResponse *response) {
    User *from_user;
    try {
        from_user = getAuthenticatedUser(request);
    } catch (const ClientError &error) {
        response->setStatus(error.status_code);
        return;
    }

    WwwFormEncodedDict args = request->formEncodedBody();
    string to_username = args.get("to");
    int amount = std::stoi(args.get("amount"));

    // check if to_username exit in db
    if (m_db->users.count(to_username) == 0) {
        // user not in db
        response->setStatus(404);
        return;
    }
    // check if user have enough money
    if (amount > from_user->balance) {
        // doesn't have enough money
        response->setStatus(403);
        return;
    }
    User *to_user = m_db->users[to_username];
    from_user->balance -= amount;
    to_user += amount;

    // add this transaction to db
    Transfer *transfer = new Transfer();
    transfer->from = from_user;
    transfer->to = to_user;
    transfer->amount = amount;
    m_db->transfers.push_back(transfer);
    respond_balance_and_transfers(m_db->transfers, from_user->balance, from_user->username, 200, response);
}
