//
//  AnimationAssetManager.h
//  ofxImageSequenceVideo_Example
//
//  Created by Oriol Ferrer Mesià on 28/03/2019.
//
//

#pragma once
#include "ofMain.h"
#include "ofxDXT.h"
#include "ofxImageSequenceVideo.h"

class AnimationAssetManager{

public:

	enum State{
		UNINITED,
		CHECKING_ASSETS,
		COMPRESSING_ASSETS, //create DXT version of assets
		PRELOADING_ASSETS,
		READY
	};

	enum UserOption{
		YES,
		NO,
		DONT_CARE
	};

	enum AssetType{
		ANIMATION,
		STATIC_IMAGE,
		UNKNOWN_ASSET_TYPE
	};

	struct AssetLoadOptions{
		bool shouldUseDxtCompression = true; 			//set to false if you want to force non-compression of this asset
		int framerate = 30; 							//does not apply to static images
		int bufferFrames = 5;						//how many frames to pre-load for that animation
		int numThreads = 4;							//how many threads are allowed to work on the pre-loading of future frames
		UserOption shouldPreloadAsset = DONT_CARE; 	//let AnimationAssetManager decide given how much memory is available
													//use YES or NO to force otherwise - Note that StaticImages are always preloaded
	};

	AnimationAssetManager();
	~AnimationAssetManager();

	//provide all necessary info to setup the object
	void setup(const string & folder, float maxUsedVRAM/*in MBytes*/, const map<string, AssetLoadOptions> & options, int numThreads = std::thread::hardware_concurrency(), bool playAssetsInReverse = false);

	//starts checking provided assets folder, compressing assets if necessary
	void startLoading();

	void update(float dt);

	State getState(){return state;}

	void drawDebug(int x, int y, int w, int h); //draw all assets to screen and their state
	string getStatus(); //get obj status (as a string) for debug / setup progress

	//get all available asset IDs
	vector<string> getStaticImageIDs();
	vector<string> getAnimationIDs();
	AssetType getAssetType(const string & ID);

	//access to assets
	ofxImageSequenceVideo & getAnimation(const string & ID); //direct access to animation objects
	ofTexture & getTexture(const string & ID); //get the ofTexture of StaticImage or Animation indistinctively


protected:


	struct CheckInfo{
		string ID;
		bool done = false;
		bool needsCompression = false;
	};

	struct CompressInfo{
		string ID;
		bool done = false;
	};

	struct AssetInfo{
		AssetType type;
		string fullPath;
		bool isPreloaded = false;
		bool useDxtCompression = true;
		float estimatedSize = 0; //in Mbytes
	};

	State state = UNINITED; //global state of the object (loading, ready, etc)

	unordered_map<string, AssetInfo> info; //info and state about all the assets
	unordered_map<string, ofTexture> images; //only static images
	unordered_map<string, ofxImageSequenceVideo> animations; //only animations

	// PROCESS ASSETS /////////////////////////////

	struct ProgressInfo{
		float pct = 0;
	};

	//check assets stage
	vector<string> pendingCheck;
	map<string, CheckInfo> checked;
	vector<std::future<CheckInfo>> checkTasks;
	map<string, ProgressInfo> checkProgress;

	//compress assets stage
	vector<string> pendingCompression;
	map<string, CompressInfo> compressed;
	vector<std::future<CompressInfo>> compressTasks;
	map<string, ProgressInfo> compressProgress;

	//preload assets stage
	vector<string> pendingPreload;
	vector<string> preloaded;

	// THREAD PROCESS METHODS /////////////////////////////

	CheckInfo checkAsset(string ID, ProgressInfo * progress);
	CompressInfo compressAsset(string ID, ProgressInfo * progress);

	// UTILS //////////////////////////////////////

	std::string bytesToHumanReadable(long long bytes, int decimalPrecision);

	// STATE ///////////////////////////////////////

	void setState(State s);

	bool needsToStop = false; //to handle obj destruction
	int numThreadsToUse = 1;
	float maxUsedVRAM = 0; //in Mbytes - provided at setup
	string assetsFolder;
	map<string, AssetLoadOptions> assetLoadOptions;
	bool isSetup = false;

	// ENUM UTILS ////////////////////////////////////

	string toString(State e);
	State toEnum_State(const string & s);

	// ERR RETURNS ///////////////////////////////////
	ofxImageSequenceVideo nullAnim;
	ofTexture nullTexture;
    
    bool playAssetsInReverse = false;

};

