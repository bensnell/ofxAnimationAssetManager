#include "ofApp.h"


//--------------------------------------------------------------
void ofApp::setup() {

	ofSetFrameRate(120);

	// Setup params 
	RUI_SETUP();
	tracker.setupParams();
	ctool.setupParams();
	RUI_LOAD_FROM_XML();

	// Setup the tracker 
	tracker.setup();
	tracker.start();

	// Setup the calibration tool
	ctool.loadTargetPlan("targets.plan");

}

//--------------------------------------------------------------
void ofApp::update(){

	vector<Device*>* dvs = tracker.vive.devices.getTrackers();
	if (dvs != NULL && !dvs->empty()) {
		// Get the first tracker
		for (int i = 0; i < dvs->size(); i++) {
			if ((*dvs)[i]->isActive()) {
				ctool.update((*dvs)[i]->position);
				break;
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw(){

	ofBackground(200);

	ofSetColor(0);
	tracker.drawStatus(10, 20);

	ctool.drawStatus(10, 150);

	// Draw the points
	cam.setUpAxis(ctool.getZAxis());
	cam.begin();
	ctool.drawDebug();
	cam.end();
}

//--------------------------------------------------------------
void ofApp::exit() {

	tracker.exit();

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

	if (key == 'b') {
		ctool.beginCalibrationProtocol();
	}
	if (key == 'l') {
		ctool.lockTarget();
	}
	if (key == 'r') {
		ctool.resetCalibrationProtocol();
	}
	if (key == 'g') {
		ctool.loadCalibrationFile("raw2real.calibration");
	}
	if (key == 'n') {
		vector<Device*>* dvs = tracker.vive.devices.getTrackers();
		if (dvs != NULL && !dvs->empty()) {
			// Get the first tracker
			for (int i = 0; i < dvs->size(); i++) {
				if ((*dvs)[i]->isActive()) {
					ofLogNotice() << (*dvs)[i]->position;
					ofLogNotice() << ctool.getMapped(glm::translate((*dvs)[i]->position));
					break;
				}
			}
		}
	}

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
