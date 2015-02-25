#include "BaseEngine.h"


BaseEngine::BaseEngine()
{
	isOpen		= false;
	
	didWriteFile = false;
	
	numMBps = 2.0;
	
	stopRequested = false;	 
	isStopping = false;
	isKeyframeValid = false;
	doFillBuffer = false;
	bufferAvailable = false;
	
	recordedFrameCounter = 0;
    isCurrentlyRecording = false;
    encoder = NULL;
    splitter = NULL;
    render = NULL;
    camera = NULL;
}   


OMX_ERRORTYPE BaseEngine::configureCameraResolution()
{
    
    OMX_ERRORTYPE error;
    error = DisableAllPortsForComponent(&camera);
	OMX_TRACE(error);
    
	OMX_CONFIG_REQUESTCALLBACKTYPE cameraCallback;
	OMX_INIT_STRUCTURE(cameraCallback);
	cameraCallback.nPortIndex	=	OMX_ALL;
	cameraCallback.nIndex		=	OMX_IndexParamCameraDeviceNumber;
	cameraCallback.bEnable		=	OMX_TRUE;
	
	error = OMX_SetConfig(camera, OMX_IndexConfigRequestCallback, &cameraCallback);
	OMX_TRACE(error);
    
	//Set the camera (always 0)
	OMX_PARAM_U32TYPE device;
	OMX_INIT_STRUCTURE(device);
	device.nPortIndex	= OMX_ALL;
	device.nU32			= 0;
	
	error = OMX_SetParameter(camera, OMX_IndexParamCameraDeviceNumber, &device);
	OMX_TRACE(error);
    
	//Set the resolution
	OMX_PARAM_PORTDEFINITIONTYPE cameraOutputPortDefinition;
	OMX_INIT_STRUCTURE(cameraOutputPortDefinition);
	cameraOutputPortDefinition.nPortIndex = CAMERA_OUTPUT_PORT;
	
	error =  OMX_GetParameter(camera, OMX_IndexParamPortDefinition, &cameraOutputPortDefinition);
    OMX_TRACE(error);
	if(error == OMX_ErrorNone) 
	{
        cameraOutputPortDefinition.format.video.nFrameWidth		= omxCameraSettings.width;
        cameraOutputPortDefinition.format.video.nFrameHeight	= omxCameraSettings.height;
        cameraOutputPortDefinition.format.video.xFramerate		= omxCameraSettings.framerate << 16;

        cameraOutputPortDefinition.format.video.nStride			= omxCameraSettings.width;
        cameraOutputPortDefinition.format.video.nSliceHeight	= omxCameraSettings.height;
        
        error =  OMX_SetParameter(camera, OMX_IndexParamPortDefinition, &cameraOutputPortDefinition);
        OMX_TRACE(error);
	}
	
    return error;
}

OMX_ERRORTYPE BaseEngine::configureEncoder()
{
		
    OMX_ERRORTYPE error;
    
    error = DisableAllPortsForComponent(&encoder);
    OMX_TRACE(error);

	// Encoder input port definition is done automatically upon tunneling
	
	// Configure video format emitted by encoder output port
	OMX_PARAM_PORTDEFINITIONTYPE encoderOutputPortDefinition;
	OMX_INIT_STRUCTURE(encoderOutputPortDefinition);
	encoderOutputPortDefinition.nPortIndex = ENCODER_OUTPUT_PORT;
	error =OMX_GetParameter(encoder, OMX_IndexParamPortDefinition, &encoderOutputPortDefinition);
    OMX_TRACE(error);

	if (error == OMX_ErrorNone) 
	{
        
        stringstream bufferInfo;
        bufferInfo << "BUFFER INFO:\n";
        bufferInfo << "nBufferCountMin: "       << encoderOutputPortDefinition.nBufferCountMin      << "\n";
        bufferInfo << "nBufferCountActual: "    << encoderOutputPortDefinition.nBufferCountActual   << "\n";
        bufferInfo << "nBufferSize: "           << encoderOutputPortDefinition.nBufferSize          << "\n";
        bufferInfo << "nBufferAlignment: "      << encoderOutputPortDefinition.nBufferAlignment     << "\n";

        bufferInfo << "ColorFormat: " << OMX_Maps::getInstance().colorFormatTypes[encoderOutputPortDefinition.format.video.eColorFormat] << "\n";
         
        ofLogVerbose(__func__) << bufferInfo.str();
        
	}
	
	
	
	//colorFormatTypes
	recordingBitRate = MEGABYTE_IN_BITS * numMBps;
	encoderOutputPortDefinition.format.video.nBitrate = recordingBitRate;
	error = OMX_SetParameter(encoder, OMX_IndexParamPortDefinition, &encoderOutputPortDefinition);
    OMX_TRACE(error);

	// Configure encoding bitrate
	OMX_VIDEO_PARAM_BITRATETYPE encodingBitrate;
	OMX_INIT_STRUCTURE(encodingBitrate);
	encodingBitrate.eControlRate = OMX_Video_ControlRateVariable;
	//encodingBitrate.eControlRate = OMX_Video_ControlRateConstant;
	
	encodingBitrate.nTargetBitrate = recordingBitRate;
	encodingBitrate.nPortIndex = ENCODER_OUTPUT_PORT;
	
	error = OMX_SetParameter(encoder, OMX_IndexParamVideoBitrate, &encodingBitrate);
    OMX_TRACE(error);

	// Configure encoding format
	OMX_VIDEO_PARAM_PORTFORMATTYPE encodingFormat;
	OMX_INIT_STRUCTURE(encodingFormat);
	encodingFormat.nPortIndex = ENCODER_OUTPUT_PORT;
	encodingFormat.eCompressionFormat = OMX_VIDEO_CodingAVC;
	
	error = OMX_SetParameter(encoder, OMX_IndexParamVideoPortFormat, &encodingFormat);
    OMX_TRACE(error);
    return error;

}
void BaseEngine::threadedFunction()
{
	while (isThreadRunning()) 
	{
		if(bufferAvailable) 
		{
			// The user wants to quit, but don't exit
			// the loop until we are certain that we have processed
			// a full frame till end of the frame, i.e. we're at the end
			// of the current key frame if processing one or until
			// the next key frame is detected. This way we should always
			// avoid corruption of the last encoded at the expense of
			// small delay in exiting.
			if(stopRequested && !isStopping) 
			{
				ofLogVerbose(__func__) << "Stop Requested, waiting for next key frame boundry...";
				isStopping = true;
				isKeyframeValid = encoderOutputBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME;
			}
			if(isStopping &&
               (isKeyframeValid ^ (encoderOutputBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME))
               ) 
			{
				ofLogVerbose(__func__) << "Key frame boundry reached, exiting loop...";
                doFillBuffer = false;
				writeFile();
				
				
			}else 
			{
				recordingFileBuffer.append((const char*) encoderOutputBuffer->pBuffer + encoderOutputBuffer->nOffset, 
                                           encoderOutputBuffer->nFilledLen);
                ofLogVerbose() << recordingFileBuffer.size();
                doFillBuffer = true;
                
			}
		}
		// Buffer flushed, request a new buffer to be filled by the encoder component
		if(doFillBuffer) 
		{
			doFillBuffer	= false;
			bufferAvailable = false;
			OMX_ERRORTYPE error = OMX_FillThisBuffer(encoder, encoderOutputBuffer);
			if(error != OMX_ErrorNone) 
			{
				ofLog(OF_LOG_ERROR, "encoder OMX_FillThisBuffer FAIL error: ",  omxErrorToCString(error) );
				closeEngine();
				
			}
		}
	}
}

void BaseEngine::stopRecording()
{
	
	if(isCurrentlyRecording)
	{
        encoderOutputBuffer->nFlags = OMX_BUFFERFLAG_EOS;
        OMX_ERRORTYPE error = OMX_FillThisBuffer(encoder, encoderOutputBuffer);
        OMX_TRACE(error);
		stopRequested = true;
	}	
}

bool BaseEngine::writeFile()
{

	stopThread();
    isCurrentlyRecording = false;
	stringstream fileName;
	fileName << ofGetTimestampString() << "_";
	
	fileName << omxCameraSettings.width << "x";
	fileName << omxCameraSettings.height << "_";
	fileName << omxCameraSettings.framerate << "fps_";
	
	fileName << numMBps << "MBps_";
	
	fileName << recordedFrameCounter << "numFrames_";
	
    switch(engineType)
    {
        case TEXTURE_ENGINE:
        {
            fileName << "TEXTURE_ENGINE";
            break;
        }
        case NON_TEXTURE_ENGINE:
        {
            fileName << "NON_TEXTURE_ENGINE";
            break;
        }
    }
	
	fileName << ".h264";
	
	string filePath;
	
	if (omxCameraSettings.recordingFilePath == "") 
	{
		filePath = ofToDataPath(fileName.str(), true);
	}else
	{
		filePath = omxCameraSettings.recordingFilePath;
	}
	
	didWriteFile = ofBufferToFile(filePath, recordingFileBuffer, true);
    ofLogVerbose(__func__) << "didWriteFile: " << didWriteFile << " filePath: " << filePath;
    
    recordingFileBuffer.clear();
    return didWriteFile;
}

BaseEngine::~BaseEngine()
{
    if(isOpen)
    {
        closeEngine();
    }
}

void BaseEngine::closeEngine()
{
    if(isCurrentlyRecording && !didWriteFile)
    {
        writeFile();
        
    }
    
    OMX_ERRORTYPE error;
    error =  OMX_SendCommand(camera, OMX_CommandFlush, CAMERA_OUTPUT_PORT, NULL);
    OMX_TRACE(error, "camera: OMX_CommandFlush");
    
    if(encoder)
    {
        error =  OMX_SendCommand(encoder, OMX_CommandFlush, ENCODER_INPUT_PORT, NULL);
        OMX_TRACE(error, "encoder: OMX_CommandFlush ENCODER_INPUT_PORT");
        error =  OMX_SendCommand(encoder, OMX_CommandFlush, ENCODER_OUTPUT_PORT, NULL);
        OMX_TRACE(error, "encoder: OMX_CommandFlush ENCODER_OUTPUT_PORT");
    }
    
    //DisableAllPortsForComponent
    error = DisableAllPortsForComponent(&camera, "camera");
    OMX_TRACE(error, "DisableAllPortsForComponent: camera");
    
    if(splitter)
    {
        error = DisableAllPortsForComponent(&splitter, "splitter");
        OMX_TRACE(error, "DisableAllPortsForComponent splitter");
    }
    
    if(encoder)
    {
        error = DisableAllPortsForComponent(&encoder, "encoder");
        OMX_TRACE(error, "DisableAllPortsForComponent encoder");
    }
    
    switch(engineType)
    {
        case TEXTURE_ENGINE:
        {
            error = OMX_FreeBuffer(render, EGL_RENDER_OUTPUT_PORT, eglBuffer);
            OMX_TRACE(error, "OMX_FreeBuffer(render, EGL_RENDER_OUTPUT_PORT");
            error = DisableAllPortsForComponent(&render, "egl_render");
            OMX_TRACE(error, "DisableAllPortsForComponent: render");
            break;
        }
        case NON_TEXTURE_ENGINE:
        {
            error = DisableAllPortsForComponent(&render, "video_render");
            OMX_TRACE(error, "DisableAllPortsForComponent: render");
            break;
        }
    }
    
    

    
    //OMX_FreeBuffer
    if(encoder)
    {
        error = OMX_FreeBuffer(encoder, ENCODER_OUTPUT_PORT, encoderOutputBuffer);
        OMX_TRACE(error, "OMX_FreeBuffer(encoder, ENCODER_OUTPUT_PORT");
    }
    
    
    //OMX_StateIdle
    error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error, "camera->OMX_StateIdle");
    
    if(splitter)
    {
        error = OMX_SendCommand(splitter, OMX_CommandStateSet, OMX_StateIdle, NULL);
        OMX_TRACE(error, "splitter->OMX_StateIdle");
    }
    
    
    if(encoder)
    {
        error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        OMX_TRACE(error, "encoder->OMX_StateIdle");
        
        
        OMX_STATETYPE encoderState;
        error = OMX_GetState(encoder, &encoderState);
        OMX_TRACE(error, "encoderState: "+ getStateString(encoderState));
    }
    
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error, "render->OMX_StateIdle");
    
   
    
    //OMX_StateLoaded
    error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    OMX_TRACE(error, "camera->OMX_StateLoaded");
    
    if(splitter)
    {
        error = OMX_SendCommand(splitter, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        OMX_TRACE(error, "splitter->OMX_StateLoaded");
    }
    
    if(encoder)
    {
        error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
        OMX_TRACE(error, "encoder->OMX_StateLoaded");
    }
    
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    OMX_TRACE(error, "render->OMX_StateLoaded");
    
 
    //OMX_FreeHandle
    error = OMX_FreeHandle(camera);
    OMX_TRACE(error, "OMX_FreeHandle(camera)");
    
    if(splitter)
    {
        error = OMX_FreeHandle(splitter);
        OMX_TRACE(error, "OMX_FreeHandle(splitter)");
    }
    
    if(encoder)
    {
        error = OMX_FreeHandle(encoder);
        OMX_TRACE(error, "OMX_FreeHandle(encoder)"); 
    }  
    
    error =  OMX_FreeHandle(render);
    OMX_TRACE(error, "OMX_FreeHandle(render)");
    isOpen = false;

}
