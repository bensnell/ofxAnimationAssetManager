#pragma once

#include "ofMain.h"
#include "CustomApp.h"
#include "ofxImageSequenceVideo.h"
#include "ofxAnimationAssetManager.h"

// Note: The Macro OFX_IMAGE_SEQUENCE_VIDEO__STB_IMAGE_IMPLEMENTATION
// must be defined if no other addon (besides ofxImageSequenceVideo) uses
// the stb image library.

class ofApp : public CustomApp{

public:
	void setup();
	void update();
	void draw();
	void exit(){};

	void keyPressed(int key);

	enum State{
		LOADING_ASSETS,
		READY
	};

	State state = LOADING_ASSETS;
	int selectedAsset = 0; //cycling index for asset selection
	bool drawInfo = false;

	ofxAnimationAssetManager aam;
	ofColor bgColor;

	vector<string> staticImageIDs;
	vector<string> animationIDs;
	vector<string> allAssetIDs; //mixed ids of anims and imgs

	// OF crap //////////////////////////////////////////////

	void keyReleased(int key){};
	void mouseMoved(int x, int y ){};
	void mouseDragged(int x, int y, int button){};
	void mousePressed(int x, int y, int button){};
	void mouseReleased(int x, int y, int button){};
	void windowResized(int w, int h){};
	void dragEvent(ofDragInfo dragInfo){};
	void gotMessage(ofMessage msg){};

	// APP CALLBACKS ////////////////////////////////////////

	void setupChanged(ofxScreenSetup::ScreenSetupArg &arg);
	void remoteUIClientDidSomething(RemoteUIServerCallBackArg & arg);


};
