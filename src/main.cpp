#include <Arduino.h>
#include "vrt_list.h"

vrt_list_t list;

void setup()
{
  Serial.begin(115200);

  vrt_list_init(&list);

  Serial.println("VertexRT Started");
}

void loop()
{
}