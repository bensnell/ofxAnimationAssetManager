#include "ofApp.h"

void ofApp::setup(){

	CustomApp::setup();

	ofSetFrameRate(30);
	ofSetVerticalSync(true);
	ofEnableAlphaBlending();

	// LISTENERS
	ofAddListener(screenSetup.setupChanged, this, &ofApp::setupChanged);
	ofAddListener(RUI_GET_OF_EVENT(), this, &ofApp::remoteUIClientDidSomething);

	// PARAMS
	RUI_NEW_GROUP("PARAMS");
	RUI_SHARE_PARAM(drawInfo);
	RUI_SHARE_COLOR_PARAM(bgColor);

	// Path to image sequence folders
	string path = ofToDataPath("anims");

	float maxVramToUse = 9000; // use 7.5GB total
	int numThreadsToUse = std::thread::hardware_concurrency();

	// Load Assets
	// This can be done one of two ways.
	// (1) If all assets are included in a folder, then just pass the 
	// parent folder to the setup function.
	// Optionally, include custom loading options.
	//map<string, ofxAnimationAssetManager::AssetLoadOptions> assetLoadOptions;
	//assetLoadOptions["GIRL"].shouldUseDxtCompression = false;
	//assetLoadOptions["OVERSEER"].shouldUseDxtCompression = false;
	//assetLoadOptions["QUARRY"].shouldUseDxtCompression = false;
	//aam.setup(path, maxVramToUse, assetLoadOptions, numThreadsToUse);
	// (2) If the assets are in different places, add each one individually
	// after setting up general params.
	aam.setup(maxVramToUse, numThreadsToUse);
	ofxAnimationAssetManager::AssetLoadOptions options;
	options.shouldUseDxtCompression = false;
	aam.addAsset(ofToDataPath("anims/girl"), options);
	aam.addAsset(ofToDataPath("anims/overseer"), options);
	aam.addAsset(ofToDataPath("anims/quarry"), options);

	// Begin loading assets
	aam.startLoading();
}


void ofApp::update(){

	// Update one of two ways:
	// (1) Pass the time change
	//float dt = 1.0f / ofGetTargetFrameRate();
	//aam.update(dt);
	// (2) Allow AAM to take care of calculating the time change
	aam.update();

	// state machine, once assets are ready, start the "app"
	if(aam.getState() == ofxAnimationAssetManager::READY && state != READY){
		state = READY;
		//also, get all the asset ID's from the AnimationAssetManager
		staticImageIDs = aam.getStaticImageIDs();
		animationIDs = aam.getAnimationIDs();
		allAssetIDs.insert(allAssetIDs.end(), staticImageIDs.begin(), staticImageIDs.end());
		allAssetIDs.insert(allAssetIDs.end(), animationIDs.begin(), animationIDs.end());
	}
}


void ofApp::draw(){

	ofBackground(bgColor);

	if(state == LOADING_ASSETS){

		ofDrawBitmapStringHighlight(aam.getStatus(), 20 , 20 );

	}else{ //state == READY

		//draw all assets, also draw asset selection to ctrl animations

		//calc layout
		float w = ofGetWidth();
		float h = ofGetHeight();
		int n = allAssetIDs.size();
		float ar = float(w) / h;
		int columns = sqrtf(n / ar) * ar;
		if(columns == 0) return;
		int rows = ceil(float(n) / columns);
		float gridW = (float(w) / columns);
		float gridH = h / float(rows);
		float pad = 20;
		int c = 0;

		for(auto & ID : allAssetIDs){

			int i = (c % columns);
			int j = floor(c / columns);
			float xx = i * gridW + pad * 0.5;
			float yy = j * gridH + pad * 0.5;

			//draw a different grid BG for each asset type
			bool isAnimation = std::find(animationIDs.begin(), animationIDs.end(), ID) != animationIDs.end();
			if(isAnimation){
				if(aam.getAnimation(ID).getKeepTexturesInGpuMem()){
					ofSetColor(128,22,22,64); //preloaded anim
				}else{
					ofSetColor(22,22,128,64); //streaming from disk anim
				}
			}else{
				ofSetColor(0, 64); //static img
			}
			ofDrawRectangle(i * gridW, j * gridH, gridW, gridH);
			ofSetColor(255);

			//get texture of asset
			auto & tex = aam.getTexture(ID);
			if(tex.isAllocated()){ //tex might not be ready

				//calc tex size to fit in grid keeping the correct aspect ratio
				ofRectangle r = ofRectangle(0,0, tex.getWidth(), tex.getHeight());
				r.scaleTo(ofRectangle(0,0,gridW - pad, gridH - pad));

				//draw asset in grid
				tex.draw(xx, yy, r.width, r.height);
				ofDrawBitmapStringHighlight(ID, xx + 5, yy + 10); //draw asset ID

				//draw asset info
				if(drawInfo){
					ofSetColor(255, 128);
					if(isAnimation){
						auto status = aam.getAnimation(ID).getStatus();
						ofDrawBitmapString("ANIMATION\n" + status, xx + 10, yy + 30);
						ofSetColor(255);
					}else{
						ofSetColor(255, 128);
						string msg = "STATIC IMAGE\n" + ofToString(tex.getWidth()) + " x " + ofToString(tex.getHeight());
						ofDrawBitmapString(msg, xx + 10, yy + 30);
					}
				}

				//draw selection and rect frames
				ofNoFill();
				if(allAssetIDs[selectedAsset] == ID) ofSetColor(255,0,0);
				else ofSetColor(0,64);
				ofDrawRectangle(i * gridW + 1, j * gridH + 1 , gridW - 1, gridH - 1);
				ofSetColor(255);
				ofFill();
			}
			c++;
		}
	}
}


void ofApp::keyPressed(int key){

	if(key == 'w'){
		screenSetup.cycleToNextScreenMode();
	}

	//if an animation is selected, control it
	if(std::find(animationIDs.begin(), animationIDs.end(), allAssetIDs[selectedAsset]) != animationIDs.end()){

		auto ID = allAssetIDs[selectedAsset];
		auto & anim = aam.getAnimation(ID);

		if(key == '1'){
			anim.play();
		}

		if(key == '2'){
			anim.pause();
		}

		if(key == '3'){
			anim.advanceOneFrame();
		}

		if(key == '4'){
			float pos = ofRandom(1);
			anim.setPosition(pos);
		}

		if(key == '5'){ //skip to random frame
			int pos = floor(ofRandom(anim.getNumFrames()));
			anim.seekToFrame(pos);
		}

		if(key == '6'){
			anim.seekToFrame(0);
		}

		if(key == '0'){
			anim.eraseAllPixelCache();
		}

		if(key == '9'){
			anim.eraseAllTextureCache();
		}
	}

	if(key == 'a'){ //play all
		for(auto & ID : animationIDs){
			aam.getAnimation(ID).play();
		}
	}

	if(key == 's'){ //pause all
		for(auto & ID : animationIDs){
			aam.getAnimation(ID).pause();
		}
	}

	if(key == 'd'){ //random position all
		for(auto & ID : animationIDs){
			aam.getAnimation(ID).setPosition(ofRandom(1));
		}
	}


	if(key == OF_KEY_RIGHT || key == OF_KEY_DOWN){
		selectedAsset ++;
		if(selectedAsset > allAssetIDs.size() - 1) selectedAsset = 0;
	}

	if(key == OF_KEY_LEFT || key == OF_KEY_UP){
		selectedAsset--;
		if(selectedAsset < 0) selectedAsset = allAssetIDs.size() - 1;
	}
}


//////// CALLBACKS //////////////////////////////////////

void ofApp::setupChanged(ofxScreenSetup::ScreenSetupArg &arg){
	ofLogNotice()	<< "ofxScreenSetup setup changed from " << screenSetup.stringForMode(arg.oldMode)
	<< " (" << arg.oldWidth << "x" << arg.oldHeight << ") "
	<< " to " << screenSetup.stringForMode(arg.newMode)
	<< " (" << arg.newWidth << "x" << arg.newHeight << ")";
}


//define a callback method to get notifications of client actions
void ofApp::remoteUIClientDidSomething(RemoteUIServerCallBackArg &arg){
	switch (arg.action) {
		case CLIENT_CONNECTED: cout << "CLIENT_CONNECTED" << endl; break;
		case CLIENT_DISCONNECTED: cout << "CLIENT_DISCONNECTED" << endl; break;
		case CLIENT_UPDATED_PARAM: cout << "CLIENT_UPDATED_PARAM: "<< arg.paramName << " - ";
			arg.param.print();
			break;
		case CLIENT_DID_SET_PRESET: cout << "CLIENT_DID_SET_PRESET" << endl; break;
		case CLIENT_SAVED_PRESET: cout << "CLIENT_SAVED_PRESET" << endl; break;
		case CLIENT_DELETED_PRESET: cout << "CLIENT_DELETED_PRESET" << endl; break;
		case CLIENT_SAVED_STATE: cout << "CLIENT_SAVED_STATE" << endl; break;
		case CLIENT_DID_RESET_TO_XML: cout << "CLIENT_DID_RESET_TO_XML" << endl; break;
		case CLIENT_DID_RESET_TO_DEFAULTS: cout << "CLIENT_DID_RESET_TO_DEFAULTS" << endl; break;
		default:
			break;
	}
}
