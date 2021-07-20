enum gameStates {SETUP, CENTER, SENDING, WAITING, PLAYING_PUZZLE, PLAYING_PIECE, ERR};
byte gameState = SETUP;
bool firstPuzzle = false;

enum answerStates {INERT, CORRECT, WRONG, RESOLVE};
byte answerState = INERT;

byte centerFace = 0;

byte petalPacketStandard[5] = {0, 0, 0, 0, 0};
byte petalPacketPrime[5] = {0, 0, 0, 0, 0};

byte currentPuzzleLevel = 0;

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
      gameState = SENDING;
      firstPuzzle = true;
    }
  }
}

void centerLoop() {
  if (gameState = CENTER) {
    //here we just wait for clicks to launch a new puzzle
    if (buttonSingleClicked() || firstPuzzle) {
      gameState = SENDING;
      generatePuzzle();
      firstPuzzle = false;
    }
  } else if (gameState == SENDING) {
    //here we just wait for all neighbors to go into PLAYING_PIECE

    byte piecesPlaying = 0;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//a neighbor! this actually needs to always be true, or else we're in trouble
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getGameState(neighborData) == PLAYING_PIECE) {
          piecesPlaying++;
        }
      }
    }

    if (piecesPlaying == 6) {//all of the pieces have gone into playing, so can we
      gameState = PLAYING_PUZZLE;
    }

  } else if (gameState == PLAYING_PUZZLE) {
    //so in here, we just kinda hang out and wait to do... something?
    //I guess here we just listen for RIGHT/WRONG signals?
    //and I guess eventually ERROR HANDLING

    if (buttonDoubleClicked()) {//here we reveal the correct answer and move forward
      answerState = CORRECT;
      gameState = CENTER;
    }


  }
}

void generatePuzzle() {

}

void pieceLoop() {
  if (gameState == WAITING) {//check for datagrams, then go into playing
    //TODO: datagram check
    bool datagramReceived = true;

    if (datagramReceived) {
      gameState = PLAYING_PIECE;
    }
  } else if (gameState == PLAYING_PIECE) {//I guess just listen for clicks and signals?
    if (buttonSingleClicked()) {
      //is this right or wrong?
      //TODO: actually have an answer to this. For now... we'll just do a 50/50 split
      bool isCorrect = random(1);
      if (isCorrect) {
        answerState = CORRECT;
      } else {
        answerState = WRONG;
      }
      gameState = WAITING;
    }
  }

}

void answerLoop() {
  //so we gotta just listen around for all these signals
  if (answerState == INERT) {//listen for CORRECT or WRONG
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborAnswer = getAnswerState(getLastValueReceivedOnFace(f));
        if (neighborAnswer == CORRECT || neighborAnswer == WRONG) {
          answerState = neighborAnswer;

          if (gameState == PLAYING_PIECE) {
            gameState = WAITING;
          } else if (gameState == PLAYING_PUZZLE) {
            gameState = CENTER;
          }
        }
      }
    }
  } else if (answerState == CORRECT || answerState == WRONG) {//just wait to go to RESOLVE
    if (gameState == PLAYING_PIECE) {
      gameState = WAITING;
    } else if (gameState == PLAYING_PUZZLE) {
      gameState = CENTER;
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
    if (gameState == PLAYING_PIECE) {
      gameState = WAITING;
    } else if (gameState == PLAYING_PUZZLE) {
      gameState = CENTER;
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
      answerState = RESOLVE;
    }
  }
}

////DISPLAY FUNCTIONS

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

void centerDisplay() {
  //so we need some temp graphics
  switch (gameState) {
    case CENTER:
      setColor(YELLOW);
      setColorOnFace(WHITE, random(5));
      break;
    case SENDING:
      setColor(dim(YELLOW, 100));
      break;
    case PLAYING_PUZZLE:
      setColor(YELLOW);
      break;
  }
  //setColor(makeColorHSB(YELLOW_HUE, 255, 255));
  //setColorOnFace(makeColorHSB(YELLOW_HUE, 0, 255), random(5));
}

void pieceDisplay() {
  //some temp graphics
  switch (gameState) {
    case WAITING:
      setColor(OFF);
      setColorOnFace(GREEN, centerFace);
      break;
    case PLAYING_PIECE:
      setColor(CYAN);
      break;
  }

  //  setColor(OFF);
  //  setColorOnFace(makeColorHSB(GREEN_HUE, 255, 100), centerFace);
}

////CONVENIENCE FUNCTIONS

byte getGameState(byte data) {
  return (data >> 3);//returns the 1st, 2nd, and 3rd bit
}

byte getAnswerState(byte data) {
  return (data & 7);//returns the 5th and 6th bit
}
