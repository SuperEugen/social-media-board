/*
Copyright (c) 2020 Ingo Hoffmann. All right reserved.

UdemyApi - An Arduino wrapper for the Udemy Affiliate API.
Gets only the course model and here only the num_subscribers field.

This is based on the work of Brian Lough's Youtube Api.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

/*
https://www.udemy.com/api-2.0/courses/1084954/?fields[course]=title,num_subscribers
*/

#include "UdemyApi.h"

UdemyApi::UdemyApi(String authorization, Client &client)  {
  int strLen = authorization.length() + 1; 
  char tempStr[strLen];
  
  authorization.toCharArray(tempStr, strLen);

  UdemyApi(tempStr, client);
}

UdemyApi::UdemyApi(char *authorization, Client &client) {
  _authorization = authorization;
  this->client = &client;
}

int UdemyApi::sendGetToUdemy(char *command) {
  client->flush();
  client->setTimeout(UDEMYAPI_TIMEOUT);
  if (!client->connect(UDEMYAPI_HOST, UDEMYAPI_SSL_PORT)) {
    Serial.println(F("Connection failed"));
    return false;
  }
    
  // give the esp a breather
  yield();

  // Send HTTP request
  client->print(F("GET "));
  client->print(command);
  client->println(F("?fields[course]=title,num_subscribers HTTP/1.1"));

  //Headers
  client->print(F("Host: "));
  client->println(UDEMYAPI_HOST);

  client->println("ACCEPT: application/json, text/plain */*");
  client->print("AUTHORIZATION: Basic ");
  client->println(_authorization);
  client->println("CONTENT-TYPE: application/json;charset=utf-8");

  if (client->println() == 0) {
    Serial.println(F("Failed to send request"));
    return -1;
  }

  int statusCode = getHttpStatusCode();
    
  // Let the caller of this method parse the JSon from the client
  skipHeaders();
  return statusCode;
}

bool UdemyApi::getCourseStatistics(String courseId){
  int strLen = courseId.length() + 1; 
  char tempStr[strLen];
  
  courseId.toCharArray(tempStr, strLen);

  return getCourseStatistics(tempStr);
}

bool UdemyApi::getCourseStatistics(char *courseId) {
  char command[150] = "/api-2.0/courses/";
  char params[120];
  
  sprintf(params, courseId);
  strcat(command, params);
  
  if(_debug) { Serial.println(command); }

  bool wasSuccessful = false;
  
  // Get from https://arduinojson.org/v6/assistant/
  const size_t bufferSize = JSON_ARRAY_SIZE(1) +
                            JSON_OBJECT_SIZE(2) +
                            2 * JSON_OBJECT_SIZE(4) +
                            JSON_OBJECT_SIZE(5) +
                            330;

  int httpStatus = sendGetToUdemy(command);

  if (httpStatus == 200) {
    // Allocate DynamicJsonDocument
    DynamicJsonDocument doc(bufferSize);

    // Parse JSON object
    DeserializationError error = deserializeJson(doc, *client);
    if (!error) {
      wasSuccessful = true;

      courseStats.numSubscribers = doc["num_subscribers"].as<long>();
    } else {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(error.c_str());
    }
  } else {
    Serial.print("Unexpected HTTP Status Code: ");
    Serial.println(httpStatus);
  }
  closeClient();

  return wasSuccessful;
}

void UdemyApi::skipHeaders() {
  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client->find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return;
  }

  // Was getting stray characters between the headers and the body
  // This should toss them away
  while (client->available() && client->peek() != '{') {
    char c = 0;
    client->readBytes(&c, 1);
    if (_debug) {
      Serial.print("Tossing an unexpected character: ");
      Serial.println(c);
    }
  }
}

int UdemyApi::getHttpStatusCode() {
  // Check HTTP status
  if(client->find("HTTP/1.1")) {
    int statusCode = client->parseInt();
    return statusCode;
  } 

  return -1;
}

void UdemyApi::closeClient() {
  if(client->connected()) {
    if(_debug) { Serial.println(F("Closing client")); }
    client->stop();
  }
}
