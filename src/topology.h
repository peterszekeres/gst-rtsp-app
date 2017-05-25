#pragma once

#include "gst/gst.h"
#include <string>
#include <map>
#include <vector>
#include <tuple>
#include <utility>

using namespace std;

class Topology {
public:

  Topology();
  ~Topology();

  bool LoadJson(string json);

  GstElement* GetPipe(string name);
  map<string, GstElement*>& GetPipes();

  GstElement* GetRtspPipe(string name);
  map<string, GstElement*>& GetRtspPipes();

  GstElement* GetElement(string name);
  map<string, GstElement*>& GetElements();

  GstCaps* GetCaps(string name);

 private:

  // Connects an element to a tee, creating a new branch on it
  static gboolean LinkToTee(GstElement* tee, GstElement* element);

  // Converts a pipe to use with the Gst RTSP Server
  bool ConnectRtspPipe(GstElement *rtsp_pipe,
                       GstElement *source_pipe,
                       GstElement *source_end_point,
                       GstElement *rtsp_start_point);

  // instances
  map<string, GstElement*> elements;
  map<string, GstElement*> pipes;
  map<string, GstElement*> rtsp_pipes;
  map<string, GstCaps*> caps;

  // json string dictionaries
  map<string, string> raw_elements;
  map<string, vector<string>> raw_pipes;
  map<string, vector<string>> raw_rtsp_pipes;

  map<string, tuple<string, string, string>> raw_rtsp_connections;
  map<string, vector<string>> raw_properties;
  map<string, vector<string>> raw_caps;

  vector<tuple<string, string, string>> raw_links;

};
