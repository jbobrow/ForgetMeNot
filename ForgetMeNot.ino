bool isCenter = false;
bool isPetal = false;
byte centerFace = 0;

byte petalPacketStandard[5] = {0, 0, 0, 0, 0};
byte petalPacketPrime[5] = {0, 0, 0, 0, 0};

byte petalHues[4] = {131, 159, 180, 223};//light blue, dark blue, violet, pink

bool canBloom = false;
Timer bloomTimer;
#define BLOOM_TIME 1000
#define GREEN_HUE 77
#define YELLOW_HUE 42

void setup() {
  // put your setup code here, to run once:
  randomize();
}

void loop() {
  if (!isCenter && !isPetal) {//in pure setup mode
    setupLoop();
    setupDisplay();
  } else if (isCenter) {
    centerLoop();
    centerDisplay();
  } else {
    petalLoop();
    petalDisplay();
  }


  //do communication
  setValueSentOnAllFaces(isCenter << 5);

  //dump button presses
  buttonSingleClicked();
  buttonDoubleClicked();
  buttonMultiClicked();
}

void setupLoop() {
  bool emptyNeighbor = false;
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) {//no neighbor
      emptyNeighbor = true;
    } else {
      if (getIsCenter(getLastValueReceivedOnFace(f)) == true) {//this neighbor is telling me to play the game
        isPetal = true;
        centerFace = f;
      }
    }
  }

  if (emptyNeighbor == true) {
    canBloom = false;
  } else {
    if (canBloom == false) {
      bloomTimer.set(BLOOM_TIME);
    }
    canBloom = true;
  }

  if (canBloom) {
    if (buttonSingleClicked()) {
      isCenter = true;
    }
  }
}

void centerLoop() {
//here we... huh
//I guess we have to listen for a single click to start the thing
}

void petalLoop() {

}

void centerDisplay() {
  setColor(makeColorHSB(YELLOW_HUE, 255, 255));
  setColorOnFace(makeColorHSB(YELLOW_HUE, 0, 255), random(5));
}

void petalDisplay() {
  setColor(OFF);
  setColorOnFace(makeColorHSB(GREEN_HUE, 255, 100), centerFace);
}

void setupDisplay() {
  if (canBloom) {

    byte bloomProgress = 255 - map(bloomTimer.getRemaining(), 0, BLOOM_TIME, 0, 255);

    byte bloomHue = map(bloomProgress, 0, 255, GREEN_HUE, YELLOW_HUE);
    byte bloomBri = map(bloomProgress, 0, 255, 100, 255);

    setColor(makeColorHSB(bloomHue, 255, bloomBri));
  } else {
    setColor(makeColorHSB(GREEN_HUE, 255, 100));
  }
}

byte getIsCenter(byte data) {
  return (data >> 5);//returns the 1st bit
}
