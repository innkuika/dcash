#define RAPIDJSON_HAS_STDSTRING 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include "DepositService.h"
#include "Database.h"
#include "ClientError.h"
#include "HTTPClientResponse.h"
#include "HttpClient.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/stringbuffer.h"

using namespace rapidjson;
using namespace std;

DepositService::DepositService() : HttpService("/deposits") {}

void respond_balance_and_deposits(vector<Deposit *> deposits, int balance, string username, int statusCode,
                                  HTTPResponse *response) {
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

    for (int i = 0; i < (int) deposits.size(); i++) {
        if (deposits[i]->to->username == username) {
            // add an object to our array
            Value to;
            to.SetObject();
            to.AddMember("stripe_charge_id", deposits[i]->stripe_charge_id, a);
            to.AddMember("to", deposits[i]->to->username, a);
            to.AddMember("amount", std::to_string(deposits[i]->amount), a);
            array.PushBack(to, a);
        }
    }

    o.AddMember("deposits", array, a);
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

void DepositService::post(HTTPRequest *request, HTTPResponse *response) {
    User *user;
    try {
        user = getAuthenticatedUser(request);
    } catch (const ClientError &error) {
        response->setStatus(error.status_code);
        return;
    }

    WwwFormEncodedDict args = request->formEncodedBody();
    // check if stripe token and amount was passed in
    if (!args.keyExist("stripe_token") || !args.keyExist("amount")) {
        response->setStatus(400);
        return;
    }
    string stripe_token = args.get("stripe_token");
    int amount = std::stoi(args.get("amount"));

    // check if amount is >= 50
    if (amount < 50) {
        response->setStatus(400);
        return;
    }
    // from the gunrock server to Stripe
    HttpClient client("api.stripe.com", 443, true);
    client.set_basic_auth(m_db->stripe_secret_key, "");

    WwwFormEncodedDict body;
    body.set("amount", amount);
    body.set("currency", "usd");
    body.set("source", stripe_token);
    string encoded_body = body.encode();
    HTTPClientResponse *stripe_response = client.post("/v1/charges", encoded_body);

    // check if the transaction was successful
    if (stripe_response->success()) {
        // This method converts the HTTP body into a rapidjson document
        Document *d = stripe_response->jsonBody();
        string charge_id = (*d)["id"].GetString();
        delete d;

        // add money to user's account
        user->balance += amount;

        // add this transaction to db
        Deposit *deposit = new Deposit();
        deposit->stripe_charge_id = charge_id;
        deposit->to = user;
        deposit->amount = amount;
        m_db->deposits.push_back(deposit);

        // make http response
        respond_balance_and_deposits(m_db->deposits, user->balance, user->username, 200, response);
    } else {
        response->setStatus(403);
    }
}
