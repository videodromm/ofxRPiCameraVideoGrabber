#pragma once
#include "CameraDemo.h"


class DemoZoomCrop  : public CameraDemo
{
    
public:
    
    
    bool doCrop;
    bool resetCrop;
    
    bool doZoomIn;
    bool doZoomOut;
    bool doRandomZoom;
    bool resetZoom;
    void setup(ofxRPiCameraVideoGrabber* videoGrabber_)
    {
        CameraDemo::setup( videoGrabber_);

        doCrop = false;
        resetCrop = false;
        
        doZoomIn = false;
        doZoomOut = false;
        doRandomZoom = false;
        resetZoom = false;
    };
    
    void update()
    {
        if(resetCrop)
        {
            videoGrabber->getCameraSettings().setSensorCrop(0, 0,  100, 100);
            resetCrop = false;
        }
        
        if (doCrop) 
        {
            
            int randomPercentage = ofRandom(25, 100);
            videoGrabber->getCameraSettings().setSensorCrop(0, 0,  randomPercentage, randomPercentage);
            doCrop = false;
        }
        
        if(doZoomIn)
        {
            videoGrabber->getCameraSettings().zoomIn();
            doZoomIn = false;
        }
        if(doZoomOut)
        {
            videoGrabber->getCameraSettings().zoomOut();
            doZoomOut = false;
        }
        if (doRandomZoom) 
        {
            doRandomZoom = false;
            videoGrabber->getCameraSettings().setZoomLevelNormalized(ofRandom(0.0, 1.0f));
        }
        
        if(resetZoom)
        {
            videoGrabber->getCameraSettings().setZoomLevelNormalized(0);
            resetZoom = false;
        }
        
        stringstream info;
        
        info << "Crop LEFT %: "      << videoGrabber->getCameraSettings().getCropRectangle().getLeft()    <<  "\n";
        info << "Crop TOP %: "       << videoGrabber->getCameraSettings().getCropRectangle().getTop()     <<  "\n";
        info << "Crop WIDTH %: "     << videoGrabber->getCameraSettings().getCropRectangle().getWidth()   <<  "\n";
        info << "Crop HEIGHT %: "    << videoGrabber->getCameraSettings().getCropRectangle().getHeight()  <<  "\n";
        info << "CURRENT ZOOM LEVEL: "      <<  videoGrabber->getCameraSettings().getZoomLevelNormalized() << "\n";
        info << "\n";
        
        info << "Press 1 to randomize Crop" <<  "\n";
        info << "Press 2 to reset Crop"     <<  "\n";
        info << "\n";
        info << "Press 3 to Zoom In \n";
        info << "Press 4 to Zoom Out \n";
        info << "Press 5 for random Zoom \n";
        info << "Press 6 to reset Zoom \n";
        
        infoString = info.str();

        
    };
    
    void draw()
    {
      
    };
    
    void onKey(int key)
    {      
        if (key == '1')
        {
            doCrop = !doCrop;
        }
        if (key == '2')
        {
            resetCrop = true;
        }
        if (key == '3') 
        {
            doZoomIn = true;
        }
        
        if (key == '4') 
        {
            doZoomOut = true;
        }
        
        if (key == '5') 
        {
            doRandomZoom = true;
        }
        
        if (key == '6') 
        {
            resetZoom = true;
        }
    };
};
