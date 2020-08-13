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

#ifndef UdemyApi_h
#define UdemyApi_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Client.h>

#define UDEMYAPI_HOST "www.udemy.com"
#define UDEMYAPI_SSL_PORT 443
#define UDEMYAPI_TIMEOUT 1500

struct courseStatistics{
  long numSubscribers;
};

class UdemyApi
{
  public:
    UdemyApi (char *authorization, Client &client);
    UdemyApi (String authorization, Client &client);
    int sendGetToUdemy(char *command);
    bool getCourseStatistics(char *courseId);
    bool getCourseStatistics(String courseId);
    courseStatistics courseStats;
    bool _debug = false;

  private:
    char *_authorization;
    Client *client;
    int getHttpStatusCode();
    void skipHeaders();
    void closeClient();
};

#endif
