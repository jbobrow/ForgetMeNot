enum gameStates {SETUP, CENTER, SENDING, WAITING, PLAYING_PUZZLE, PLAYING_PIECE, ERR};
byte gameState = SETUP;
bool firstPuzzle = false;

Timer scoreboardTimer;
#define SCORE_DURATION 100000

enum answerStates {INERT, CORRECT, WRONG, RESOLVE, VICTORY};
byte answerState = INERT;

byte centerFace = 0;

//PACKET ARRANGEMENT: puzzleType, puzzlePalette, puzzleDifficulty, isAnswer, showTime, darkTime
uint16_t showTime[6] = {5000, 5000, 5000, 5000, 5000, 5000};
uint16_t darkTime[6] = {2000, 2000, 2000, 2000, 2000, 2000};
byte puzzlePacket[6] = {0, 0, 0, 0, 0, 0};

byte currentPuzzleLevel = 0;
#define MAX_LEVEL 59
Timer puzzleTimer;
bool puzzleStarted = false;
Timer answerTimer;

bool isScoreboard = false;

byte puzzleArray[60] =     {0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 2, 2, 1, 0, 2, 3, 3, 2, 0, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
byte difficultyArray[60] = {1, 1, 1, 1, 2, 1, 1, 2, 1, 2, 1, 1, 1, 2, 2, 1, 1, 2, 3, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

//byte petalHues[4] = {131, 159, 180, 223};//light blue, dark blue, violet, pink

#define LIGHTPINK makeColorRGB(255,200,255)
#define SALMON makeColorRGB(255,50,0)
#define PINK makeColorRGB(255,0,255)
#define MAUVE makeColorRGB(150,50,255)
#define INDIGO makeColorRGB(50,100,255)
#define PERIWINKLE makeColorRGB(0,150,255)

Color primaryColors[6] = {LIGHTPINK, SALMON, PINK, MAUVE, INDIGO, PERIWINKLE};

byte rotationBri[6] = {0, 0, 0, 0, 0, 0};
byte rotationFace = 0;
Timer rotationTimer;
#define ROTATION_RATE 100

bool canBloom = false;
Timer bloomTimer;
#define BLOOM_TIME 1000
#define GREEN_HUE 77
#define YELLOW_HUE 42

//Puzzle levels
// byte puzzleInfo[6] = {puzzleType, puzzlePalette, puzzleDifficulty, isAnswer, showTime, darkTime};

// COLOR_PETALS:  color changes on one of the petals
// LOCATION_PETALS:  one side on each petal is lit, and changes position
// animationPetlas: a basic animation clockwise or counterclockwise on each petal... one changes
// globalPetals: a
enum puzzleType {COLOR_PETALS, LOCATION_PETALS, DUO_PETALS, ROTATION_PETALS};

enum puzzlePallette  {primary, pink, blue};

// beginner: pick from two colours
// easy: pick from three colours
// medium: pick from four colours
// hard: pick from five colours
// extrahard: pick from 6 colours --- probably not
enum puzzleDifficulty {beginner, easy, medium, hard, extrahard};




void setup() {
  // put your setup code here, to run once:
  randomize();
}

void loop() {
  switch (gameState) {
    case SETUP:
      setupLoop();
      setupDisplay();
      break;
    case CENTER:
    case SENDING:
    case PLAYING_PUZZLE:
      centerLoop();
      centerDisplay();
      break;
    case WAITING:
    case PLAYING_PIECE:
      pieceLoop();
      pieceDisplay();
      break;
    case ERR:
      break;
  }

  answerLoop();

  //do communication
  byte sendData = (gameState << 3) | (answerState);
  setValueSentOnAllFaces(sendData);

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
      if (getGameState(getLastValueReceivedOnFace(f)) == SENDING || getGameState(getLastValueReceivedOnFace(f)) == CENTER) {//this neighbor is telling me to play the game
        gameState = WAITING;
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
      //react differently if you're a scoreboard
      if (isScoreboard) {
        gameState = CENTER;
        firstPuzzle = false;
      } else {
        gameState = CENTER;
        firstPuzzle = true;
      }
    }
  }
}

Timer datagramTimer;
#define DATAGRAM_TIMEOUT 250
byte puzzleInfo[6] = {0, 0, 0, 0, 0, 0};
byte stageOneData = 0;
byte stageTwoData = 0;
byte answerFace = 0;

void centerLoop() {
  if (gameState == CENTER) {
    //here we just wait for clicks to launch a new puzzle
    if (buttonSingleClicked() || firstPuzzle) {
      gameState = SENDING;
      generatePuzzle();
      firstPuzzle = false;
      datagramTimer.set(DATAGRAM_TIMEOUT);
    }
  } else if (gameState == SENDING) {
    //here we just wait for all neighbors to go into PLAYING_PIECE

    byte piecesPlaying = 0;
    byte whoPlaying[6] = {false, false, false, false, false, false};
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//a neighbor! this actually needs to always be true, or else we're in trouble
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getGameState(neighborData) == PLAYING_PIECE) {
          piecesPlaying++;
          whoPlaying[f] = true;
        }
      }
    }

    if (piecesPlaying == 6) {//all of the pieces have gone into playing, so can we
      gameState = PLAYING_PUZZLE;
    }

    if (datagramTimer.isExpired()) {
      //huh, so we still aren't playing
      //who needs a datagram again?
      FOREACH_FACE(f) {
        if (whoPlaying[f] == false) {
          //update puzzlePacket[5] to reflect the current face
          puzzlePacket[5] = f;

          if (f == answerFace) {
            puzzlePacket[3] = 1;
            sendDatagramOnFace( &puzzlePacket, sizeof(puzzlePacket), f);
          } else {
            puzzlePacket[3] = 0;
            sendDatagramOnFace( &puzzlePacket, sizeof(puzzlePacket), f);
          }
        }
      }
    }

  } else if (gameState == PLAYING_PUZZLE) {
    //so in here, we just kinda hang out and wait to do... something?
    //I guess here we just listen for RIGHT/WRONG signals?
    //and I guess eventually ERROR HANDLING

    //TURN THIS BACK ON TO GET THE DOUBLE-CLICK CHEAT
    //    if (buttonDoubleClicked()) {//here we reveal the correct answer and move forward
    //      answerState = CORRECT;
    //      answerTimer.set(2000);   //set answer timer for display
    //      gameState = CENTER;
    //    }


  }
}



void generatePuzzle() {

  // based on LEVEL, create a puzzle
  //  choose{puzzleType, puzzlePalette, puzzleDifficulty, isAnswer, showTime, darkTime};

  //  lookup puzzle type
  puzzlePacket[0] = puzzleArray[currentPuzzleLevel];

  //  choose a puzzle palette
  puzzlePacket[1] = 0;//TODO: multiple palettes

  //  lookup puzzle difficulty
  puzzlePacket[2] = difficultyArray[currentPuzzleLevel];

  //  current level
  puzzlePacket[4] = currentPuzzleLevel;

  //  what face am I
  puzzlePacket[5] = 0;//this changes when I send it, default to 0 is fine

  answerFace = random(5);//which face will have the correct answer?
  //answerFace = 0;//DEBUG MODE - ALWAYS THE SAME ANSWER FACE

  FOREACH_FACE(f) {
    //update puzzlePacket[5] to reflect the current face
    puzzlePacket[5] = f;
    if (f == answerFace) {
      puzzlePacket[3] = 1;  // isAnswer = true
      sendDatagramOnFace( &puzzlePacket, sizeof(puzzlePacket), f);
    } else {
      puzzlePacket[3] = 0; // is Answer = false;
      sendDatagramOnFace( &puzzlePacket, sizeof(puzzlePacket), f);
    }
  }
}

void pieceLoop() {
  if (gameState == WAITING) {//check for datagrams, then go into playing
    //listen for a packet on master face
    bool datagramReceived = false;
    puzzleStarted = false;

    if (isDatagramReadyOnFace(centerFace)) {//is there a packet?
      if (getDatagramLengthOnFace(centerFace) == 6) {//is it the right length?
        byte *data = (byte *) getDatagramOnFace(centerFace);//grab the data
        markDatagramReadOnFace(centerFace);
        FOREACH_FACE(f) {
          puzzleInfo[f] = data[f];
        }

        datagramReceived = true;
      }
    }


    if (datagramReceived) {
      gameState = PLAYING_PIECE;
      //quickly do some figuring out based on puzzle figuring
      stageOneData = determineStages(puzzleInfo[0], puzzleInfo[2], puzzleInfo[3], 1);
      stageTwoData = determineStages(puzzleInfo[0], puzzleInfo[2], puzzleInfo[3], 2);
      puzzleStarted = false;
    }
  } else if (gameState == PLAYING_PIECE) {//I guess just listen for clicks and signals?

    //start the puzzle if the center wants me to start
    if (puzzleTimer.isExpired() && getGameState(getLastValueReceivedOnFace(centerFace)) == PLAYING_PUZZLE && puzzleStarted == false) {//I have not started the puzzle, but the center wants me to
      //BEGIN SHOWING THE PUZZLE!

      //Ok, so this
      //puzzleTimer.set((puzzleInfo[4] + puzzleInfo[5]) * 100); //the timing within the datagram is reduced 1/100
      puzzleTimer.set(7000);//TODO: this needs to change based on level
      puzzleStarted = true;
      rotationFace = centerFace;
    }

    if (buttonSingleClicked()) {
      //is this right or wrong?
      bool isCorrect = (puzzleInfo[3] != 0);

      if (isCorrect) {
        answerState = CORRECT;

        //if you are at MAX_LEVEL, you should go into a special kind of correct - VICTORY
        if (puzzleInfo[4] == MAX_LEVEL) {
          answerState = VICTORY;
        }

      } else {
        answerState = WRONG;
        isScoreboard = true;
        scoreboardTimer.set(SCORE_DURATION);
      }
      answerTimer.set(2000);   //set answer timer for display
      gameState = WAITING;
    }
  }

}

byte determineStages(byte puzzType, byte puzzDiff, byte amAnswer, byte stage) {
  if (stage == 1) {//determine the first stage - pretty much always a number 0-5, but in duoPetal it's a little more complicated
    if (puzzType == DUO_PETALS) {//special duo petal time!
      //choose a random interior color
      byte interior = random(5);
      //so based on the difficulty, we then choose another color
      byte distance = 5 - puzzDiff;

      byte exterior = 0;
      bool goRight = random(1);
      if (goRight) {
        exterior = (interior + distance) % 6;
      } else {
        exterior = (interior + 6 - distance) % 6;
      }
      return ((interior * 10) + exterior);
    } else if (puzzType == ROTATION_PETALS) {
      return (random(1));

    } else {//every other puzzle just chooses a random number 0-5
      return (random(5));
    }

  } else {//only change answer if amAnswer
    if (amAnswer) {//I gotta return a different value

      if (puzzType == DUO_PETALS) { //this is a duo petal, so we gotta reverse it
        byte newExterior = stageOneData / 10;
        byte newInterior = stageOneData % 10;
        return ((newInterior * 10) + newExterior);

      } else if (puzzType == ROTATION_PETALS) {//just swap from 0 to 1 and vice versa
        if (stageOneData == 0) {
          return (1);
        } else {
          return (0);
        }
      } else {//all other puzzles, just decide how far to rotate in the spectrum
        byte distance = 5 - puzzDiff;
        bool goRight = random(1);
        if (goRight) {
          return ((stageOneData + distance) % 6);
        } else {
          return ((stageOneData + 6 - distance) % 6);
        }
      }
    } else {//if you are not the answer, just return the stage one data
      return (stageOneData);
    }
  }
}

void answerLoop() {
  //so we gotta just listen around for all these signals
  if (answerState == INERT) {//listen for CORRECT or WRONG
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborAnswer = getAnswerState(getLastValueReceivedOnFace(f));
        if (neighborAnswer == CORRECT) {
          answerState = neighborAnswer;
          answerTimer.set(2000);

          if (gameState == PLAYING_PIECE) {
            gameState = WAITING;
          } else if (gameState == PLAYING_PUZZLE) {
            gameState = CENTER;
            currentPuzzleLevel++;
          }
        } else if (neighborAnswer == WRONG) {
          answerState = neighborAnswer;
          gameState = SETUP;
          isScoreboard = true;
          scoreboardTimer.set(SCORE_DURATION);
          currentPuzzleLevel = 0;
        } else if (neighborAnswer == VICTORY) {
          answerState = neighborAnswer;
          gameState = SETUP;
          isScoreboard = true;
          scoreboardTimer.set(SCORE_DURATION);
          currentPuzzleLevel = 0;
        }
      }
    }
  } else if (answerState == CORRECT || answerState == WRONG || answerState == VICTORY) {//just wait to go to RESOLVE

    if (answerState == CORRECT) {
      if (gameState == PLAYING_PIECE) {
        gameState = WAITING;
      } else if (gameState == PLAYING_PUZZLE) {
        gameState = CENTER;
      }
    } else if (answerState == WRONG) {
      gameState = SETUP;
    } else if (answerState == VICTORY) {
      gameState = SETUP;
    }



    bool canResolve = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborAnswer = getAnswerState(getLastValueReceivedOnFace(f));
        if (neighborAnswer == INERT) {
          canResolve = false;
        }
      }
    }

    if (canResolve) {
      answerState = RESOLVE;
    }
  } else if (answerState == RESOLVE) {//wait to go to INERT

    if (answerState == CORRECT) {
      if (gameState == PLAYING_PIECE) {
        gameState = WAITING;
      } else if (gameState == PLAYING_PUZZLE) {
        gameState = CENTER;
      }
    } else if (answerState == WRONG) {
      gameState = SETUP;
    } else if (answerState == VICTORY) {
      gameState = SETUP;
    }

    bool canInert = true;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborAnswer = getAnswerState(getLastValueReceivedOnFace(f));
        if (neighborAnswer != INERT && neighborAnswer != RESOLVE) {
          canInert = false;
        }
      }
    }

    if (canInert) {
      answerState = INERT;
    }
  }
}

////DISPLAY FUNCTIONS

void setupDisplay() {
  if (canBloom) {

    byte bloomProgress = map(bloomTimer.getRemaining(), 0, BLOOM_TIME, 0, 255);

    byte bloomHue = map(bloomProgress, 0, 255, YELLOW_HUE, GREEN_HUE);
    byte bloomBri = map(255 - bloomProgress, 0, 255, 100, 255);

    setColor(makeColorHSB(bloomHue, 255, bloomBri));
    setColorOnFace(dim(WHITE, bloomBri), random(5));
  } else {
    setColor(makeColorHSB(GREEN_HUE, 255, 100));
  }

  if (isScoreboard) {

    if (puzzleInfo[4] == MAX_LEVEL) { //oh, this is a VICTORY scoreboard
      setColor(dim(YELLOW, scoreboardTimer.getRemaining() / 10));
    } else {//a regular failure scoreboard
      setColor(dim(WHITE, scoreboardTimer.getRemaining() / 10));
    }

    //    FOREACH_FACE(f) {
    //      if (f >= puzzleInfo[5]) {  // show the id of the petal
    //        if (puzzleInfo[3]) { // this petal is the answer
    //          setColorOnFace(ORANGE, f);
    //        }
    //        else {
    //          setColorOnFace(MAGENTA, f);
    //        }
    //      }
    //    }
  }
}



void centerDisplay() {
  //so we need some temp graphics
  switch (gameState) {
    case CENTER:
      if (!answerTimer.isExpired()) {
        if (answerState == CORRECT) {
          setColor(GREEN);
        } else if (answerState == WRONG) {
          setColor(RED);
        }
      } else {
        setColor(YELLOW);
        setColorOnFace(WHITE, random(5));
      }

      //      //TEMP SCORE DISPLAY
      //      setColorOnFace(BLUE, currentPuzzleLevel % 6);
      break;
    case SENDING:
      setColor(YELLOW);
      break;
    case PLAYING_PUZZLE:
      setColor(YELLOW);
      //setColorOnFace(WHITE, answerFace);//DEBUG MODE - INDICATING ANSWER
      break;
  }
  //setColor(makeColorHSB(YELLOW_HUE, 255, 255));
  //setColorOnFace(makeColorHSB(YELLOW_HUE, 0, 255), random(5));
}

void pieceDisplay() {

  //  switch (gameState) {
  //    case WAITING:
  //      setColor(YELLOW);
  //      break;
  //    case PLAYING_PIECE:
  //      setColor(GREEN);
  //      break;
  //    default:
  //      break;
  //  }
  //
  //  if (puzzleStarted) {
  //    setColorOnFace(WHITE, centerFace);
  //  } else {
  //    setColorOnFace(RED, centerFace);
  //  }

  if (gameState == WAITING) {//just waiting

    //setColor(OFF);
    //setColorOnFace(GREEN, centerFace);
    if (!answerTimer.isExpired()) {
      if (answerState == CORRECT) {
        setColor(GREEN);
      } else if (answerState == WRONG) {
        setColor(RED);
      }
    } else {
      setColor(OFF);
      setColorOnFace(GREEN, centerFace);
    }


  } else {//show the puzzle
    if (puzzleStarted) {
      if (puzzleTimer.isExpired()) {//show the last stage of the puzzle (forever)
        displayStage(stageTwoData);
      } else if (puzzleTimer.getRemaining() <= 2000) { //show darkness TODO: this should change with each level like the initial setting
        setColor(OFF);
        setColorOnFace(dim(GREEN, 100), centerFace);
      } else {//show the first stage of the puzzle
        displayStage(stageOneData);
      }
    } else {
      setColor(OFF);
      setColorOnFace(dim(GREEN, 100), centerFace);
    }
  }

  //temp answer display
  //  byte oppFace = (centerFace + 3) % 6;
  //  switch (answerState) {
  //    case INERT:
  //      setColorOnFace(WHITE, oppFace);
  //      break;
  //    case CORRECT:
  //      setColorOnFace(GREEN, oppFace);
  //      break;
  //    case WRONG:
  //      setColorOnFace(RED, oppFace);
  //      break;
  //    case RESOLVE:
  //      setColorOnFace(BLUE, oppFace);
  //      break;
  //  }
}

void displayStage( byte stageData ) {
  //TODO: take into account color palette, defaulting to basics for now
  //puzzleType, puzzlePalette, puzzleDifficulty, isAnswer, showTime, darkTime
  switch (puzzleInfo[0]) {
    case COLOR_PETALS:
      setColor(primaryColors[stageData]);
      break;
    case LOCATION_PETALS://dark, with a single lit face
      setColor(OFF);
      setColorOnFace(WHITE, stageData);
      break;
    case DUO_PETALS:
      {
        byte interiorColor = (stageData / 10);
        setColor(primaryColors[interiorColor]);//setting the interior color

        byte exteriorColor = (stageData % 10);
        setColorOnFace(primaryColors[exteriorColor], (centerFace + 2) % 6);
        setColorOnFace(primaryColors[exteriorColor], (centerFace + 3) % 6);
        setColorOnFace(primaryColors[exteriorColor], (centerFace + 4) % 6);
      }
      break;
    case ROTATION_PETALS:
      { //I need to do this because I'm gonna make a byte
        if (rotationTimer.isExpired()) {
          rotationTimer.set(ROTATION_RATE);
          if (stageData == 0) { // CW Rotation
            rotationFace = (rotationFace + 1) % 6;
          } else {  // CCW Rotation
            rotationFace = (rotationFace + 5) % 6;
          }
          rotationBri[rotationFace] = 255;
        }

        FOREACH_FACE(f) {
          if ( rotationBri[f] >= 5 ) {
            rotationBri[f] -= 5;
          }
          setColorOnFace(dim(WHITE, rotationBri[f]), f);
        }
      }
      break;
  }
}

////CONVENIENCE FUNCTIONS

byte getGameState(byte data) {
  return (data >> 3);//returns the 1st, 2nd, and 3rd bit
}

byte getAnswerState(byte data) {
  return (data & 7);//returns the 4th, 5th and 6th bit
}
