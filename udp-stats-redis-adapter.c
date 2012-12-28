#include <sys/socket.h>
#include <netinet/in.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "credis.h"
#include "parson.h"

int main (void) {

  // Setup sockfd to be a DGRAM socket and bind
  // to any:6667.
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family      = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port        = htons(6667);
  if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
    printf("Error binding.\n");
    return 1;
  }
  printf("Now listening on UDP port %i...\n", 6667);

  // Initialize redis connection.
  REDIS red = credis_connect(NULL, 6379, 2000);

  // Declare JSON values.
  JSON_Value  *json;
  JSON_Object *jobj;

  // Declare msg buffer and parts.
  char msg[1024];
  const char *msg_bucket;
  const char *msg_type;
  time_t      msg_timestamp;
  double      msg_value;

  // Declare small helper values.
  int    n, m;
  time_t tm;

  // Run forever:
  for (;;) {

    // Receive a message of length `n` < 1024 into buffer `msg`,
    // null-terminate the received message and determine current
    // timestamp.
    n      = recvfrom(sockfd, msg, 1023, 0, NULL, NULL);
    msg[n] = '\0';
    tm     = time(NULL);

    // Parse the message and check for a root object.
    json = json_parse_string(msg);
    if (json_value_get_type(json) == JSONObject) {

      // Get root object and retrieve values from it.
      jobj = json_value_get_object(json);
      msg_bucket    =         json_object_get_string(jobj, "bucket");
      msg_type      =         json_object_get_string(jobj, "type");
      msg_timestamp = (time_t)json_object_get_number(jobj, "timestamp");
      msg_value     =         json_object_get_number(jobj, "value");

      // If bucket is given, insert value into bucket.
      if (msg_bucket) {
        n = snprintf(&msg[0],   512, "stats:%s", msg_bucket);
        m = snprintf(&msg[512], 512, "%f",       msg_value);
        credis_zadd(red, &msg[0], msg_timestamp < 1 ? (double)tm : msg_timestamp, &msg[512]);
      }

      // If type is given, change type of bucket.
      if (msg_bucket && msg_type) {
        n = snprintf(msg, 1024, "stats:%s:type", msg_bucket);
        credis_set(red, msg, msg_type);
      }
    }
  }

}