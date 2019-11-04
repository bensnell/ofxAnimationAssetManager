//
//  ofxAnimationAssetManager.cpp
//  ofxImageSequenceVideo_Example
//
//  Created by Oriol Ferrer Mesià on 28/03/2019.
//
//

#include "ofxAnimationAssetManager.h"

#if defined( TARGET_OSX ) || defined( TARGET_LINUX )
	#include <getopt.h>
	#include <dirent.h>
#else
	#include <dirent_vs.h>
#endif

#include "ofxTimeMeasurements.h"

ofxAnimationAssetManager::ofxAnimationAssetManager(){}


ofxAnimationAssetManager::~ofxAnimationAssetManager(){

	needsToStop = true;

	//wait for all threads to end
	for(int i = checkTasks.size() - 1; i >= 0; i--){
		std::future_status status = checkTasks[i].wait_for(std::chrono::microseconds(0));
		while(status != std::future_status::ready){
			ofSleepMillis(16);
			status = checkTasks[i].wait_for(std::chrono::microseconds(0));
		}
	}

	//wait for all threads to end
	for(int i = compressTasks.size() - 1; i >= 0; i--){
		std::future_status status = compressTasks[i].wait_for(std::chrono::microseconds(0));
		while(status != std::future_status::ready){
			ofSleepMillis(16);
			status = compressTasks[i].wait_for(std::chrono::microseconds(0));
		}
	}
}


string ofxAnimationAssetManager::getStatus(){

	string msg = "State: " + toString(state) + "\n";
	string list;

	if(state == CHECKING_ASSETS){
		for(auto & it : checkProgress){
			if(it.second.pct < 1.0f ){
				list += "  " + it.first + ": " + ofToString(100 * it.second.pct, 1) + "% done \n";
			}
		}
		msg += ofToString(checked.size()) + "/" + ofToString(pendingCheck.size() + checked.size() + checkTasks.size()) + " [" + ofToString(checkTasks.size()) + " active tasks]";
		msg += "\n" + list;
	}
	if(state == COMPRESSING_ASSETS){
		for(auto & it : compressProgress){
			if(it.second.pct < 1.0f){
				list += "  " + it.first + ": " + ofToString(100 * it.second.pct, 1) + "% done \n";
			}
		}
		msg += ofToString(compressed.size()) + "/" + ofToString(pendingCompression.size() + compressed.size() + compressTasks.size()) + " [" + ofToString(compressTasks.size()) + " active tasks]";
		msg += "\n" + list;
	}
	return msg;
}


vector<string> ofxAnimationAssetManager::getStaticImageIDs(){
	vector<string> ret;
	for(auto & img: images) ret.emplace_back(img.first);
	return ret;
}

vector<string> ofxAnimationAssetManager::getAnimationIDs(){
	vector<string> ret;
	for(auto & img: animations) ret.emplace_back(img.first);
	return ret;
}

ofxAnimationAssetManager::AssetType ofxAnimationAssetManager::getAssetType(const string & ID){
	auto it = info.find(ID);
	if(it != info.end()){
		return it->second.type;
	}
	return UNKNOWN_ASSET_TYPE;
}

void ofxAnimationAssetManager::setup(const string & folder, float maxUsedVRAM, const map<string, AssetLoadOptions> & options, int numThreads, bool playAssetsInReverse){

	setup(maxUsedVRAM, numThreads, playAssetsInReverse);

	assetLoadOptions = options;
    
	string newFolder = ofFilePath::getPathForDirectory(folder); 
	ofDirectory dir;
	dir.listDir(newFolder);

	int num = dir.numFiles();
	for(int i = 0; i < num; i++){

		string filenameAndExt = ofToUpper(dir.getName(i));

		if(dir.getFile(i).isDirectory()){ //animation
			info[filenameAndExt].type = ANIMATION;
			info[filenameAndExt].fullPath = dir.getPath(i);

			string ID = filenameAndExt;
			ofLogNotice("ofxAnimationAssetManager") << "found ANIMATION with ID \"" << ID << "\"";

		}else{ //img file

			string extension = ofFilePath::getFileExt(filenameAndExt);
			if(ofToUpper(extension) == "PNG" || ofToUpper(extension) == "TGA"){
				string ID = ofFilePath::getBaseName(filenameAndExt);
				info[ID].type = STATIC_IMAGE;
				info[ID].fullPath = dir.getPath(i);
				ofLogNotice("ofxAnimationAssetManager") << "found STATIC_IMAGE with ID \"" << ID << "\"";
			}
		}
	}
}

void ofxAnimationAssetManager::setup(float maxUsedVRAM, int numThreads, bool playAssetsInReverse) {

	numThreadsToUse = numThreads;
	this->maxUsedVRAM = maxUsedVRAM;
	this->playAssetsInReverse = playAssetsInReverse;

	ofSetLogLevel("ofxDXT", OF_LOG_WARNING); //silence notice or lower logs for the extra-chatty ofxDXT

	//init null texture to RED
	ofFbo fbo;
	fbo.allocate(32, 32, GL_RGB);
	fbo.begin();
	ofClear(255, 0, 0);
	fbo.end();
	nullTexture = fbo.getTexture();

	isSetup = true;
}

bool ofxAnimationAssetManager::addAsset(string ID, string& path, AssetLoadOptions& options) {

	// Save these options
	assetLoadOptions[ID] = options;

	// Save the asset
	return addAsset(ID, path);
}

bool ofxAnimationAssetManager::addAsset(string& path, AssetLoadOptions& options) {

	// The ID of this asset is the base name of the provided path (by default)
	string ID = ofFilePath::getBaseName(path);

	assetLoadOptions[ID] = options;

	return addAsset(ID, path);
}

bool ofxAnimationAssetManager::addAsset(string& path) {

	string ID = ofFilePath::getBaseName(path);
	return addAsset(ID, path);
}

bool ofxAnimationAssetManager::addAsset(string ID, string& path) {

	ofDirectory dir;
	dir.open(path);

	// If this is a directory and it contains more than 1 valid image, mark it as an animation
	if (dir.isDirectory()) { 
		dir.allowExt("png");
		dir.allowExt("tga");
		dir.listDir();
		if (dir.size() == 0) {			// invalid
			ofLogNotice("ofxAnimationAssetManager") << "can't load ANIMATION with ID \"" << ID << "\" because there are insufficient PNG or TGA files";
			return false;
		}
		else if (dir.size() == 1) {		// image
			// Load a single image inside this folder
			info[ID].type = STATIC_IMAGE;
			info[ID].fullPath = dir.getPath(0);
			ofLogNotice("ofxAnimationAssetManager") << "found STATIC_IMAGE with ID \"" << ID << "\"";
			return true;
		}
		else {							// animation
			info[ID].type = ANIMATION;
			info[ID].fullPath = path;
			ofLogNotice("ofxAnimationAssetManager") << "found ANIMATION with ID \"" << ID << "\"";
			return true;
		}
	}
	else {								// image
		string extension = ofFilePath::getFileExt(path);
		if (ofToUpper(extension) == "PNG" || ofToUpper(extension) == "TGA") {
			info[ID].type = STATIC_IMAGE;
			info[ID].fullPath = path;
			ofLogNotice("ofxAnimationAssetManager") << "found STATIC_IMAGE with ID \"" << ID << "\"";
			return true;
		}
		else {
			return false;
		}
	}
}

void ofxAnimationAssetManager::startLoading(){
	if(!isSetup) {
		ofLogError("ofxAnimationAssetManager") << "cant startLoading() as object is not setup!";
		return;
	}
	setState(CHECKING_ASSETS);
}


void ofxAnimationAssetManager::setState(State s){

	state = s;

	switch (s) {
		case UNINITED: break;

		case CHECKING_ASSETS:
			ofLogNotice("ofxAnimationAssetManager") << "## Start CHECKING Assets #########################################################";
			pendingCheck.clear();
			checked.clear();
			for(auto & it : info){
				pendingCheck.push_back(it.first);
			}
			break;
			
		case COMPRESSING_ASSETS:
			ofLogNotice("ofxAnimationAssetManager") << "## Start COMPRESSING Assets ######################################################";
			pendingCompression.clear();
			compressed.clear();
			for(auto & it : checked){
				if(it.second.needsCompression){
					pendingCompression.push_back(it.first);
				}
			}
			if(pendingCompression.size() == 0){ //if nobody need compression, skip stage
				setState(PRELOADING_ASSETS);
			}
			break;

		case PRELOADING_ASSETS:{

			ofLogNotice("ofxAnimationAssetManager") << "## Start PRELOADING Assets #######################################################";
			struct AnimInfo{
				string ID;
				float estimatedSizeFullSequence;
				int numFrames;
			};

			//lets store all the anims and their estimated preload size
			//the goal here is to automatically decide what to preload in VRAM and what to stream
			//given how much memory we can use.
			vector<AnimInfo> animInfos;

			for(auto & it : info){

				if(it.second.type == STATIC_IMAGE){

					pendingPreload.push_back(it.first); //all static images get preloaded
					info[it.first].isPreloaded = true;
					info[it.first].useDxtCompression = false; //static images never compressed

				}else{ //for animations, we need to decide if we preload or not

					auto option = assetLoadOptions.find(it.first);
					if(option == assetLoadOptions.end()){
						ofLogWarning("ofxAnimationAssetManager") << "Found asset in Folder but user did not supply AssetLoadOptions for it! (" << it.first << "). Will use default options";
						assetLoadOptions[it.first] = AssetLoadOptions();
						option = assetLoadOptions.find(it.first);
					}

					int numThreads = option->second.numThreads;
					int bufferFrames = option->second.bufferFrames;
					float framerate = option->second.framerate;
					bool useDXTcompression = option->second.shouldUseDxtCompression;

					info[it.first].useDxtCompression = useDXTcompression;
					animations[it.first].setup(numThreads, bufferFrames, useDXTcompression, playAssetsInReverse);
    
					animations[it.first].loadImageSequence(info[it.first].fullPath, framerate);
					auto estimatedSizeBytes = animations[it.first].getEstimatdVramUse();

					animations[it.first].setLoop(true);
					animations[it.first].setKeepTexturesInGpuMem(false); //default to no, will set to true later if requested (in preload stage)

					info[it.first].estimatedSize = estimatedSizeBytes / float(1024 * 1024); //MB
					ofLogNotice("ofxAnimationAssetManager") << "Animation \"" << it.first << "\" estimated VRAM use to preload whole sequence: " << bytesToHumanReadable(estimatedSizeBytes, 1);

					//store in temoprary data structure
					animInfos.push_back(AnimInfo{it.first, info[it.first].estimatedSize, animations[it.first].getNumFrames()});
				}
			}

			//sort all anim info by size, smallest first
			std::sort(animInfos.begin(), animInfos.end(),
					  [](const AnimInfo & a, const AnimInfo & b) -> bool{
						  return a.estimatedSizeFullSequence < b.estimatedSizeFullSequence;
					  });

			//now let's calculate who is preloaded in VRAM and who is to be streamed given how much
			//VRAM we can use (maxUsedVRAM)

			float memUsedByStaticImages = 0;
			for(auto & id : pendingPreload){ //first lets calculate how much memory the static images take
				int w, h, numChannels;
				bool imgOK;
				ofxImageSequenceVideo::getImageInfo(info[id].fullPath, w, h, numChannels, imgOK);
				if(imgOK){
					memUsedByStaticImages += ( w * h * numChannels / float(1024 * 1024));
				}
			}

			ofLogNotice("ofxAnimationAssetManager") << "Static Images will take " << memUsedByStaticImages << " Mb in VRAM.";
			float memUsedByAllAnimationsSingleFrame = 0;
			for(auto & anim : animInfos){
				float mb = anim.estimatedSizeFullSequence / anim.numFrames;
				//ofLogNotice("ofxAnimationAssetManager") << "Animation \"" << anim.ID << "\" one frame takes " << mb << " Mb of Vram.";
				memUsedByAllAnimationsSingleFrame += mb;
			}

			float availableMemForAnimationsPreload = maxUsedVRAM - memUsedByStaticImages - memUsedByAllAnimationsSingleFrame;

			//start by preloading the smallest animations, keep adding to the "pool" until we are out of space
			for(auto & anim : animInfos){
				if(assetLoadOptions[anim.ID].shouldPreloadAsset == DONT_CARE){
					if(availableMemForAnimationsPreload - anim.estimatedSizeFullSequence > 0){
						availableMemForAnimationsPreload -= anim.estimatedSizeFullSequence;
						pendingPreload.push_back(anim.ID);
						ofLogNotice("ofxAnimationAssetManager") << "Animation \"" << anim.ID << "\" will be preloaded because there's enough VRAM to fit it. " << availableMemForAnimationsPreload << " Mb left to use.";
					}
				}else{
					if(assetLoadOptions[anim.ID].shouldPreloadAsset == YES){
						pendingPreload.push_back(anim.ID);
						ofLogNotice("ofxAnimationAssetManager") << "Animation \"" << anim.ID << "\" will be preloaded because of user config requesting it.";
					}else{
						ofLogNotice("ofxAnimationAssetManager") << "Animation \"" << anim.ID << "\" will be NOT BE preloaded because of user config requesting it.";
					}
				}
			}
			}break;

		default:
			break;
	}
}


ofxImageSequenceVideo & ofxAnimationAssetManager::getAnimation(const string & ID){
	auto it = info.find(ID);
	if(it != info.end()){
		if(it->second.type == ANIMATION){
			return animations[ID];
		}else{
			ofLogError("ofxAnimationAssetManager") << "getAnimation() error! requested animation \"" << ID << "\" is a static image!";
			return nullAnim;
		}
	}
	ofLogError("ofxAnimationAssetManager") << "getAnimation() error! requested animation \"" << ID << "\" does not exist!";
	return nullAnim;
}



ofTexture & ofxAnimationAssetManager::getTexture(const string & ID){
	auto it = info.find(ID);
	if(it != info.end()){
		if(it->second.type == STATIC_IMAGE){
			return images[ID];
		}else{
			return animations[ID].getTexture();
		}
	}
	ofLogError("ofxAnimationAssetManager") << "getAnimation() error! requested animation \"" << ID << "\" does not exist!";
	return nullTexture;
}


void ofxAnimationAssetManager::update(float dt){

	switch (state) {
		case UNINITED: break;

		case CHECKING_ASSETS:
			if (checked.size() == info.size() ){ //done
				ofLogNotice("ofxAnimationAssetManager") << "done checking assets!";
				setState(COMPRESSING_ASSETS);
			}else{

				//cleanup finshed tasks threads
				for(int i = checkTasks.size() - 1; i >= 0; i--){
					//see if thread is done, gather results and remove from vector
					std::future_status status = checkTasks[i].wait_for(std::chrono::microseconds(0));
					if(status == std::future_status::ready){ //thread is done
						auto results = checkTasks[i].get();
						checked[results.ID] = results; //store check results
						checkTasks.erase(checkTasks.begin() + i);
					}
				}

				//spawn new ones
				while(checkTasks.size() < numThreadsToUse && pendingCheck.size()){ //spawn thread
					string id = pendingCheck.front();
					pendingCheck.erase(pendingCheck.begin());
					checkProgress[id] = ProgressInfo();
					checkTasks.push_back( std::async(std::launch::async, &ofxAnimationAssetManager::checkAsset, this, id, &checkProgress[id]) );
				}
			}
			break;

		case COMPRESSING_ASSETS:
			if (pendingCompression.size() == 0 && compressTasks.size() == 0 && compressed.size() > 0){ //done
				ofLogNotice("ofxAnimationAssetManager") << "done compressing assets!";
				setState(PRELOADING_ASSETS);
			}else{
				//cleanup finshed tasks threads
				for(int i = compressTasks.size() - 1; i >= 0; i--){
					//see if thread is done, gather results and remove from vector
					std::future_status status = compressTasks[i].wait_for(std::chrono::microseconds(0));
					if(status == std::future_status::ready){ //thread is done
						auto results = compressTasks[i].get();
						compressed[results.ID] = results; //store results
						compressTasks.erase(compressTasks.begin() + i);
					}
				}

				//spawn new ones
				while(compressTasks.size() < numThreadsToUse && pendingCompression.size()){ //spawn thread
					string id = pendingCompression.front();
					pendingCompression.erase(pendingCompression.begin());
					compressProgress[id] = ProgressInfo();
					compressTasks.push_back( std::async(std::launch::async, &ofxAnimationAssetManager::compressAsset, this, id, &compressProgress[id]) );
				}
			}
			break;

		case PRELOADING_ASSETS:{
			int numThisFrame = 1;
			if(pendingPreload.size())
            {
				while (numThisFrame > 0 && (pendingPreload.size()))
                {
					string ID = pendingPreload.front();
					pendingPreload.erase(pendingPreload.begin());
					if(info[ID].type == STATIC_IMAGE){

                        ofLoadImage(images[ID], info[ID].fullPath);
                        numThisFrame--;


					}else{
						animations[ID].setKeepTexturesInGpuMem(true);
					}
				}
			}else{
				setState(READY);
			}
			}break;

		case READY:
        {
			for(auto & it : animations){
				it.second.update(dt);

			}
			break;
        }
		default:
			break;
	}
}


ofxAnimationAssetManager::CheckInfo ofxAnimationAssetManager::checkAsset(string ID, ofxAnimationAssetManager::ProgressInfo * progress){

	ofxAnimationAssetManager::CheckInfo inf;
	inf.ID = ID;
	inf.done = true;
	int c = 0;
	if(info[ID].type == ANIMATION){

		if(assetLoadOptions[ID].shouldUseDxtCompression){
			vector<string> allImages = ofxImageSequenceVideo::getImagesAtDirectory(info[ID].fullPath, false); //get list of all "png" (or other normal types) images
			//count all images whith a missing .dxt representation
			int needCompression = 0;
			for(auto & img : allImages){
				string fullPath = info[ID].fullPath + "/" + img + ".dxt";
				ofFile f = ofFile(fullPath);
				if(!f.exists() || f.getSize() == 0){
					needCompression++;
				}
				c++;
				progress->pct = c / float(allImages.size());
				if(needsToStop) break;
			}
			inf.needsCompression = needCompression > 0;
		}else{
			inf.needsCompression = false;
		}
	}else{
		inf.needsCompression = false;
		progress->pct = 1.0;
	}
	return inf;
}


ofxAnimationAssetManager::CompressInfo ofxAnimationAssetManager::compressAsset(string ID, ofxAnimationAssetManager::ProgressInfo * progress){

	ofxAnimationAssetManager::CompressInfo inf;
	inf.ID = ID;
	vector<string> allImages = ofxImageSequenceVideo::getImagesAtDirectory(info[ID].fullPath, false);
	int c = 0;
	for(auto & imgName : allImages){
		ofPixels pix;
		string fullPath = info[ID].fullPath + "/" + imgName;
		ofLoadImage(pix, fullPath);
		ofxDXT::Data compressedPix;
		ofxDXT::compressRgbaPixels(pix, compressedPix);
		ofxDXT::saveToDisk(compressedPix, fullPath + ".dxt");
		c++;
		progress->pct = c / float(allImages.size());
		if(needsToStop) break;
	}
	inf.done = true;
	return inf;
}


//Function Declarations for State string conversion ///////////////  for your *.c
string ofxAnimationAssetManager::toString(State e){
	switch(e){
		case State::UNINITED: return "UNINITED";
		case State::CHECKING_ASSETS: return "CHECKING_ASSETS";
		case State::COMPRESSING_ASSETS: return "COMPRESSING_ASSETS";
		case State::PRELOADING_ASSETS: return "PRELOADING_ASSETS";
		case State::READY: return "READY";
	}
	ofLogError() << "toString(State) Error!";
	return "Unknown State!";
}

ofxAnimationAssetManager::State ofxAnimationAssetManager::toEnum_State(const string & s){
	if(s == "UNINITED") return State::UNINITED;
	if(s == "CHECKING_ASSETS") return State::CHECKING_ASSETS;
	if(s == "COMPRESSING_ASSETS") return State::COMPRESSING_ASSETS ;
	if(s == "PRELOADING_ASSETS") return State::PRELOADING_ASSETS;
	if(s == "READY") return State::READY;
	ofLogError() << "toEnum_State(" << s << ") Error! State";
	return (State) 0;
}


std::string ofxAnimationAssetManager::bytesToHumanReadable(long long bytes, int decimalPrecision){
	std::string ret;
	if (bytes < 1024 ){ //if in bytes range
		ret = ofToString(bytes) + " bytes";
	}else{
		if (bytes < 1024 * 1024){ //if in kb range
			ret = ofToString(bytes / float(1024), decimalPrecision) + " KB";
		}else{
			if (bytes < (1024 * 1024 * 1024)){ //if in Mb range
				ret = ofToString(bytes / float(1024 * 1024), decimalPrecision) + " MB";
			}else{
				ret = ofToString(bytes / float(1024 * 1024 * 1024), decimalPrecision) + " GB";
			}
		}
	}
	return ret;
}


void ofxAnimationAssetManager::drawDebug(int x, int y, int w, int h){

	if(state == READY){

		int n = info.size();
		float ar = float(w) / h;
		int columns = sqrtf(n / ar) * ar;
		if(columns == 0) return;
		int rows = ceil(float(n) / columns);
		float gridW = (float(w) / columns);
		float gridH = h / float(rows);
		float pad = 20;
		int c = 0;

		for(auto & it : info){

			int i = (c % columns);
			int j = floor(c / columns);
			float xx = x + i * gridW + pad * 0.5;
			float yy = y + j * gridH + pad * 0.5;
			float margin = 10;

			if(it.second.type == ANIMATION){

				ofxImageSequenceVideo & v = animations[it.first];

				auto & tex = v.getTexture();
				if(tex.isAllocated()){
					float s = 1.0;
					ofRectangle r = ofRectangle(0,0, tex.getWidth(), tex.getHeight());
					r.scaleTo(ofRectangle(0,0,gridW - pad, gridH - pad));
					tex.draw(xx,yy, r.width, r.height);
				}

				//v.drawDebug(x + margin, y, gridW - pad - 2 * margin); //draws timeline
				if(v.getKeepTexturesInGpuMem()){
					if(!v.areAllTexturesPreloaded()){
						ofSetColor(255,32, 32);
					}else{
						ofSetColor(32,32, 255);
					}
				}else{
					ofSetColor(255);
				}
				ofDrawBitmapString(it.first, xx, yy);
				ofSetColor(255,128);
				ofDrawBitmapString(v.getStatus(), xx + 5, yy + 13);
				ofSetColor(255);


			}else{ // ANIMATION

				ofTexture & tex = images[it.first];

				if(tex.isAllocated()){
					ofRectangle r = ofRectangle(0,0, tex.getWidth(), tex.getHeight());
					r.scaleTo(ofRectangle(0,0,gridW - pad, gridH - pad));
					tex.draw(xx, yy, r.width, r.height);
				}

				ofSetColor(0,255,255);
				ofDrawBitmapString(it.first, xx, yy);
				ofSetColor(255);
			}
			c++;
		}
	}
}
