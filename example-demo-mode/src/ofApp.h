#pragma once

#include "ofMain.h"
#include "ofAppEGLWindow.h"
#include "TerminalListener.h"
#include "ofxRPiCameraVideoGrabber.h"
#include "CameraDemo.h"

#include "DemoExposureMode.h"
#include "DemoMirrorMode.h"
#include "DemoEnhancement.h"
#include "DemoFilters.h"
#include "DemoRotation.h"
#include "DemoZoomCrop.h"
#include "DemoExposurePresets.h"
#include "DemoManualControls.h"

class ofApp : public ofBaseApp, public KeyListener{
    
public:
    
    void setup();
    void update();
    void draw();
    void keyPressed(int key);
    
    void onCharacterReceived(KeyListenerEventData& e);
    TerminalListener consoleListener;
    
    SessionConfig sessionConfig;
    ofxRPiCameraVideoGrabber videoGrabber;
    
    vector<CameraDemo*> demos;
    int currentDemoID;
    CameraDemo* currentDemo;
    bool doNextDemo;
    vector<SessionConfig::Preset> presets;
    int currentPreset;
    bool doDrawInfo;
    bool doPrintInfo;
    bool doPresetChange;
};