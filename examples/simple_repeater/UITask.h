#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <helpers/CommonCLI.h>

class MyMesh;

class UITask {
  mesh::MainBoard* _board;
  DisplayDriver* _display;
  unsigned long _next_read, _next_refresh, _auto_off;
  int _prevBtnState;
  NodePrefs* _node_prefs;
  MyMesh* _mesh;
  char _version_info[32];
#ifdef LOON_FIRMWARE
  const char* _firmware_version;
  const char* _build_date;
#endif
  unsigned long _powering_off_at = 0;
  unsigned long _started_at = 0;

  void renderCurrScreen();
public:
  UITask(mesh::MainBoard& board, DisplayDriver& display) : _board(&board), _display(&display) { _next_read = _next_refresh = 0; }
  void begin(MyMesh* mesh, const char* build_date, const char* firmware_version);

  void loop();
};
