#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "daad.h"


// External
extern void do_CLS();
extern void do_INKEY();
extern void do_NEWLINE();

// Global variables
uint8_t    *ddb;						// Where the DDB is allocated
DDB_Header *hdr;						// Struct pointer to DDB Header
Object     *objects;					// Memory allocation for objects data
uint8_t     flags[256];					// DAAD flags
char       *ramsave;					// Memory to store ram save (RAMSAVE)

#ifndef DISABLE_WINDOW
Window      windows[8];					// 0-7 windows definitions
#else
Window      windows[1];					// Only one if WINDOW condact is not used
#endif
Window     *cw;							// Pointer to current active window

#ifndef DISABLE_SAVEAT
uint8_t     savedPosX;					// For SAVEAT/BACKAT
uint8_t     savedPosY;					//  "    "      "
#endif

// Internal variables
uint8_t lsBuffer[TEXT_BUFFER_LEN/2+1];	// Logical sentence buffer [type+id]
char    tmpTok[6];
char   *tmpMsg;							// TEXT_BUFFER_LEN
char    lastPrompt;
uint8_t offsetText;

//=========================================================

/*
 * Function: initDAAD
 * --------------------------------
 * Initialize DDB and DAAD engine.
 * 
 * @return			none.
 */
bool initDAAD()
{
	uint16_t *p;
	
	loadFilesBin();

	hdr = (DDB_Header*)ddb;
	p = (uint16_t *)&hdr->tokensPos;

	#ifdef DEBUG
		printf("Version.......... %u\n", hdr->version);
		printf("Language......... %u\n", hdr->target.value.language);
		printf("Machine.......... %u\n", hdr->target.value.machine);
		printf("Magic............ 0x%02x\n", hdr->magic);
		printf("Num.Obj.......... %u\n", hdr->numObjDsc);
		printf("Num.Locations.... %u\n", hdr->numLocDsc);
		printf("Num.Usr.Msg...... %u\n", hdr->numUsrMsg);
		printf("Num.Sys.Msg...... %u\n", hdr->numSysMsg);
		printf("Num.Proc......... %u\n", hdr->numPrc);
		printf("Tokens pos....... 0x%04x\n", hdr->tokensPos);
		printf("Proc list pos.... 0x%04x\n", hdr->prcLstPos);
		printf("Obj. list pos.... 0x%04x\n", hdr->objLstPos);
		printf("Loc. list pos.... 0x%04x\n", hdr->locLstPos);
		printf("Usr. msg. pos.... 0x%04x\n", hdr->usrMsgPos);
		printf("Sys. msg. pos.... 0x%04x\n", hdr->sysMsgPos);
		printf("Connections pos.. 0x%04x\n", hdr->conLstPos);
		printf("Vocabulary pos... 0x%04x\n", hdr->vocPos);
		printf("Obj.Loc. list.... 0x%04x\n", hdr->objLocLst);
		printf("Obj.Name pos..... 0x%04x\n", hdr->objNamePos);
		printf("Obj.Attr pos..... 0x%04x\n", hdr->objAttrPos);
		printf("Obj.Extr pos..... 0x%04x\n", hdr->objExtrPos);
		printf("File length...... %u bytes\n", hdr->fileLength);
	#endif

	//If not a valid DDB version exits
	if (hdr->version != 2 || hdr->magic != 0x5f)
		return false;

	//Update header positions addresses
	for (int i=0; i<12; i++) {
		*(p++) += (uint16_t)ddb;
	}

	//Get memory for RAMSAVE
	ramsave = (char*)malloc(256+sizeof(Object)*hdr->numObjDsc);
	//Get memory for objects
	objects = (Object*)malloc(sizeof(Object)*hdr->numObjDsc);
	//Get memory for tmpMsg
	tmpMsg = (char*)malloc(TEXT_BUFFER_LEN);

	#ifdef DEBUG
		printf("\nDDB max size..... %u bytes\n", getFreeMemory());
	#endif

	return true;
}

/*
 * Function: initFlags
 * --------------------------------
 * Initialize DAAD flags and some structs.
 * 
 * @return			none.
 */
void initFlags()
{
	//Clear flags
	memset(flags, 0, 256);

	//Set screen flags
	#if SCREEN==5 || SCREEN==7
		flags[fScMode] = 13;	// EGA
	#endif
	#if SCREEN==6
		flags[fScMode] = 4;		// CGA
	#endif
	#if SCREEN==8
		flags[fScMode] = 141;	// VGA
	#endif

	//Initialize DAAD windows
	memset(windows, 0, sizeof(windows));
	flags[fCurWin] = 0;
	cw = &windows[0];
	cw->winX = 0;
	cw->winY = 0;
	cw->winW = MAX_COLUMNS;
	cw->winH = MAX_LINES;
	cw->cursorX = cw->cursorY = 0;
	#ifndef DISABLE_SAVEAT
		savedPosX = savedPosY = 0;
	#endif

	//Clear logical sentences
	clearLogicalSentences();

	offsetText = 0;
}

/*
 * Function: initObjects
 * --------------------------------
 * Initialize Objects.
 * 
 * @return			none.
 */
void initObjects()
{
	uint8_t  *objLoc = (uint8_t*)hdr->objLocLst;
	uint8_t  *attrLoc = (uint8_t*)hdr->objAttrPos;
	uint8_t  *extAttrLoc = (uint8_t*)hdr->objExtrPos;
	uint8_t  *nameObj = (uint8_t*)hdr->objNamePos;

	flags[fNOCarr] = 0;

	for (int i=0; i<hdr->numObjDsc; i++) {
		objects[i].location     = *(objLoc + i);
		objects[i].attribs.byte = *(attrLoc + i);
		objects[i].extAttr1     = *(extAttrLoc + i*2);
		objects[i].extAttr2     = *(extAttrLoc + i*2 + 1);
		objects[i].nounId       = *(nameObj + i*2);
		objects[i].adjectiveId  = *(nameObj + i*2 + 1);
		if (objects[i].location==LOC_CARRIED) flags[fNOCarr]++;
	}
}

/*
 * Function: mainLoop
 * --------------------------------
 * DAAD main loop start.
 * 
 * @return			none.
 */
void mainLoop()
{
	initFlags();
	initializePROC();

	pushPROC(0);
	processPROC();
}

/*
 * Function: prompt
 * --------------------------------
 * Wait for user entry text and fill tmpMsg 
 * variable with it.
 * 
 * @return			none.
 */
void prompt()
{
	char c, *p = tmpMsg, *extChars;

	while (kbhit()) getchar();
	gfxPutCh('>');
	*p = '\0';
	do {
		// Check first char Timeout flag
		if (p==tmpMsg) {
			if (waitForTimeout(TIME_FIRSTCHAR)) return;
		}
		while (!kbhit()) waitForPrompt();
		c = getchar();
		if (c=='\r' && p==tmpMsg) { c = 0; continue; }	// Avoid enter an empty text order
		if (c==0x08) {									// Back space (BS)
			if (p<=tmpMsg) continue;
			p--;
			if (cw->cursorX>0) cw->cursorX--; else { cw->cursorX = cw->winW-1; cw->cursorY--; }
			gfxPutChWindow(' ');
		} else {
			if (p-tmpMsg > TEXT_BUFFER_LEN) continue;
			extChars = strchr(getCharsTranslation(), c);
			if (extChars) c = (char)(extChars-getCharsTranslation()+0x10);
			gfxPutCh(c);
			*p++ = toupper(c);
		}
	} while (c!='\r');
	*--p = '\0';
}

/*
 * Function: parser
 * --------------------------------
 * Parse the words in user entry text and compare 
 * them with VOCabulary table.
 * 
 * @return			none.
 */
void parser()
{
	char *p = tmpMsg, *p2;
	uint8_t ils = 0;
	Vocabulary *voc;

	//Clear logical sentences buffer
	clearLogicalSentences();

	while (*p) {
		//Clear tmpTok
		memset(tmpTok, ' ', 5);

		//Copy first 5 chars max of word
		p2 = p;
		while (p2-p<5 && *p2!=' ' && *p2!='\0') p2++;
		memcpy(tmpTok, p, p2-p);
#ifdef VERBOSE2
printf("%u %c%c%c%c%c: ",p2-p, tmpTok[0],tmpTok[1],tmpTok[2],tmpTok[3],tmpTok[4]);
#endif
		for (int i=0; i<5; i++) tmpTok[i] = 255 - tmpTok[i];

		//Search it in VOCabulary table
		voc = (Vocabulary*)hdr->vocPos;
		while (voc->word[0]) {
			if (!memcmp(tmpTok, voc->word, 5)) {
				lsBuffer[ils++] = voc->id;
				lsBuffer[ils++] = voc->type;
#ifdef VERBOSE2
printf("Found! %u / %u\n",voc->id, voc->type);
#endif
				break;
			}
			voc++;
		}
#ifdef VERBOSE2
if (!voc->word[0]) printf("NOT FOUND!\n");
#endif
		p = strchr(p2, ' ');
		if (!p) break;
		p++;
	}
#ifdef VERBOSE2
printf("%02u %02u %02u %02u %02u %02u %02u %02u \n",lsBuffer[0],lsBuffer[1],lsBuffer[2],lsBuffer[3],lsBuffer[4],lsBuffer[5],lsBuffer[6],lsBuffer[7]);
#endif
}

/*
 * Function: getLogicalSentence
 * --------------------------------
 * Get the first logical sentence from parsed user entry 
 * and fill noun, verbs, adjectives, etc.
 * If no sentences prompt to user.
 * 
 * @return		Boolean with True if any logical sentence found.
 */
bool getLogicalSentence()
{
	char *p = lsBuffer, type, id, adj = fAdject1;
	bool ret = false;

	// If not logical sentences in buffer we ask to user again
	if (!*p) {
		char newPrompt;

		newPrompt = flags[fPrompt];
		if (!newPrompt)
			while ((newPrompt=(rand()%4)+2)==lastPrompt);
		printSystemMsg(newPrompt);
		do_NEWLINE();
		lastPrompt = newPrompt;

		prompt();
		parser();
	}

	// Clear parser flags
	flags[fVerb] = flags[fNoun1] = flags[fAdject1] = flags[fAdverb] = flags[fPrep] = flags[fNoun2] = flags[fAdject2] = 
		flags[fCPNoun] = flags[fCPAdject] = NULLWORD;
#ifdef VERBOSE2
printf("populateLogicalSentence()\n");
#endif
	while (*p && *(p+1)!=CONJUNCTION) {
		id = *p;
		type = *(p+1);
		if (type==VERB && flags[fVerb]==NULLWORD) {										// VERB
			flags[fVerb] = id;
			ret = true;
		} else if (type==NOUN && flags[fNoun1]==NULLWORD) {								// NOUN1
			flags[fNoun1] = id;
			if (id<20) flags[fVerb] = id;
			ret = true;
		} else if (type==NOUN && flags[fNoun2]==NULLWORD) {								// NOUN2
			flags[fNoun2] = id;
			adj = fAdject2;
			ret = true;
		} else if (type==ADVERB && flags[fAdverb]==NULLWORD) {							// ADVERB
			flags[fAdverb] = id;
			ret = true;
		} else if (type==PREPOSITION && flags[fPrep]==NULLWORD) {						// PREP
			flags[fPrep] = id;
			ret = true;
		} else if (type==ADJECTIVE && adj==fAdject1 && flags[fAdject1]==NULLWORD) {		// ADJ1
			flags[fAdject1] = id;
			ret = true;
		} else if (type==ADJECTIVE && adj==fAdject2 && flags[fAdject2]==NULLWORD) {		// ADJ2
			flags[fAdject2] = id;
			ret = true;
		}
		p+=2;
	}

	if (flags[fNoun2]!=NULLWORD) {
		uint8_t obj = getObjectById(flags[fNoun2], flags[fAdject2]);
		if (obj!=NULLWORD) flags[fO2Con] = objects[obj].attribs.mask.isContainer;
	}

#ifdef VERBOSE2
printf("VERB:%u NOUN1:%u ADJ1:%u, ADVERB:%u PREP: %u NOUN2:%u, ADJ2:%u %u\n",flags[fVerb],flags[fNoun1],flags[fAdject1],flags[fAdverb],flags[fPrep],flags[fNoun2],flags[fAdject2]);
#endif
	nextLogicalSentence();

	return ret;
}

/*
 * Function: clearLogicalSentences
 * --------------------------------
 * Clear pending logical sentences if any.
 * 
 * @return			none.
 */
void clearLogicalSentences()
{
#ifdef VERBOSE2
printf("clearLogicalSentences()\n");
#endif
	memset(lsBuffer, 0, sizeof(lsBuffer));
}

/*
 * Function: nextLogicalSentence
 * --------------------------------
 * Move next logical sentence to start of logical 
 * sentence buffer.
 * 
 * @return			none.
 */
void nextLogicalSentence()
{
#ifdef VERBOSE2
printf("nextLogicalSentence()\n");
#endif
	char *p = lsBuffer, *c = lsBuffer;
	while (*p!=CONJUNCTION && *p!=0) p+=2;
	p+=2;
	for (;;) {
		*c++ = *p;
		*c++ = *(p+1);
		if (!*p) break;
		p+=2;
	}
	*c++ = 0;
	*c = 0;
}

//=========================================================
//UTILS

/*
 * Function: printBase10
 * --------------------------------
 * Prints a base 10 number.
 * 
 * @param value		Number to print.
 * @return			none.
 */
void printBase10(uint16_t value)
{
	if (value<10) {
		if (value) gfxPutCh('0'+(uint8_t)value);
		return;
	}
	printBase10(value/10);
	gfxPutCh('0'+(uint8_t)(value%10));
}

/*
 * Function: waitForTimeout
 * --------------------------------
 * Wait for a timeout or key pressed
 * 
 * @param timeFlag	A mask to compare with Timer Flag.
 * @return			Boolean for timeout if reached or not.
 */
bool waitForTimeout(uint16_t timerFlag)
{
	uint16_t timeout = flags[fTime]*50;

	while (kbhit()) getchar();
	if (flags[fTIFlags] & timerFlag) {
		flags[fTIFlags] &= TIME_TIMEOUT^255;
		setTime(0);
		while (!kbhit()) {
			waitForPrompt();
			if (getTime() > timeout) {
				flags[fTIFlags] |= TIME_TIMEOUT;
				return true;
			}
		}	
	} else {
		while (!kbhit()) { waitForPrompt(); }
	}
	return false;
}

//=========================================================

/*
 * Function: getToken
 * --------------------------------
 * Return the requested token.
 * 
 * @param num   	To get the token number 'num' in the token list.
 * @return			Return a pointer to the requested token.
 */
char* getToken(uint8_t num)
{
	char *p = (char*)hdr->tokensPos + 1;
	char i=0;

	while (num) {
		if (*p > 127) num--;
		p++;
		if (!num) break;
	}
	do {
		tmpTok[i++] = *p & 0x7f;
	} while (*p++ < 127);
	tmpTok[i]='\0';

	return tmpTok;
}

/*
 * Function: _printMsg
 * --------------------------------
 * Uncompress a tokenized string and print it or not.
 * 
 * @param lst		List of tokenized string (sysmes, usermes, desc...).
 * @param num   	To get the string number 'num' in that list.
 * @param print		Output the string to the current window or not.
 * @return			none.
 */
void _printMsg(uint16_t *lst, uint8_t num, bool print)
{
	char *p = &ddb[*(lst + num)];
	char c, *token;
	uint16_t i = 0;

	tmpMsg[0]='\0';
	do {
		c = 255 - *p++;
		if (c >= 128) {
			token = getToken(c - 128);
			while (*token) {
				tmpMsg[i++] = *token;
				if (*token==' ' || *token=='\r' || *token=='\n') {
					if (print) {
						tmpMsg[i] = '\0';
						gfxPuts(tmpMsg);
						i = 0;
					}
				}
				token++;
			}
		} else {
			if (print && (c=='_' || c=='@')) {
				printObjectMsgModif(flags[fCONum], c);
				i = 0;
				continue;
			}
			tmpMsg[i++] = c;
			if (c==' ' || c==0x0a) {
				if (c==0x0a) tmpMsg[--i] = '\0';
				if (print) {
					tmpMsg[i] = '\0';
					gfxPuts(tmpMsg);
					i=0;
				}
			}
		}
	} while (c != 0x0a);		// = 255 - 0xf5
}

/*
 * Function: getSystemMsg
 * --------------------------------
 * Extract system message.
 * 
 * @param num		Number of system message.
 * @return			none.
 */
void getSystemMsg(uint8_t num)
{
	_printMsg((uint16_t*)hdr->sysMsgPos, num, false);
}

/*
 * Function: printSystemMsg
 * --------------------------------
 * Extract system message and print it.
 * 
 * @param num		Number of system message.
 * @return			none.
 */
void printSystemMsg(uint8_t num)
{
	_printMsg((uint16_t*)hdr->sysMsgPos, num, true);
}

/*
 * Function: printUserMsg
 * --------------------------------
 * Extract user message and print it.
 * 
 * @param num		Number of user message.
 * @return			none.
 */
void printUserMsg(uint8_t num)
{
	_printMsg((uint16_t*)hdr->usrMsgPos, num, true);
}

/*
 * Function: printLocationMsg
 * --------------------------------
 * Extract location message and print it.
 * 
 * @param num		Number of location message.
 * @return			none.
 */
void printLocationMsg(uint8_t num)
{
	_printMsg((uint16_t*)hdr->locLstPos, num, true);
}

/*
 * Function: printObjectMsg
 * --------------------------------
 * Extract object message and print it.
 * 
 * @param num		Number of object message.
 * @return			none.
 */
void printObjectMsg(uint8_t num)
{
	_printMsg((uint16_t*)hdr->objLstPos, num, true);
}

/*
 * Function: printObjectMsgModif
 * --------------------------------
 * Extract object message, change the article and print it:
 * "Una linterna" -> "La linterna"
 * "A lantern" -> "The Lantern"
 *  
 * @param num		Number of object name.
 * @param modif		Modifier for uppercase.
 * @return			none.
 */
void printObjectMsgModif(uint8_t num, char modif)
{
	char *ini = tmpMsg, *p = tmpMsg;
	_printMsg((uint16_t*)hdr->objLstPos, num, false);
#ifdef LANG_ES
	if (tmpMsg[2]==' ') {
		tmpMsg[0] = modif=='@'?'E':'e';
		tmpMsg[1] = 'l';
	} else
	if (tmpMsg[3]==' ') {
		ini++;
		tmpMsg[1] = modif=='@'?'L':'l';
	}
	while (*p) {
		if (*p=='.' || *p==0x0a) { *p--='\0'; }
		p++;
	}
#elif LANG_EN
	gfxPuts(modif=='@'?"Th":"th");
	tmpMsg[0] = 'e';
#endif
	gfxPuts(ini);
}

/*
 * Function: getObjectById
 * --------------------------------
 * Return de object ID by Noun+Adjc ID.
 *  
 * @param noun		Noun ID.
 * @param adjc		Adjective ID.
 * @return			Object ID if found or NULLWORD.
 */
uint8_t getObjectById(uint8_t noun, uint8_t adjc)
{
	for (int i=0; i<hdr->numObjDsc; i++) {
		if (objects[i].nounId==noun && objects[i].adjectiveId==adjc)
			return i;
	}
	return NULLWORD;
}

/*
 * Function: getObjectWeight
 * --------------------------------
 * Return the weight of a object by ID. Also can return 
 * the total weight of location or carried/worn objects
 * if objno is NULLWORD.
 *  
 * @param objno			Object ID or NULLWORD.
 * @param isCarriedWorn	Check carried/worn objects if True.
 * @return				Return the weight of one or a sum of objects.
 */
uint8_t getObjectWeight(uint8_t objno, bool isCarriedWorn)
{
	uint16_t weight = 0;
	Object *obj = objects;
	for (int i=0; i<hdr->numObjDsc; i++) {
		if ((objno==NULLWORD || objno==i) && (!isCarriedWorn || obj->location==LOC_CARRIED || obj->location==LOC_WORN)) {
			if (obj->attribs.mask.isContainer && obj->attribs.mask.weight!=0) {
				weight += getObjectWeight(i, false);
			}
			weight += obj->attribs.mask.weight;
		}
		obj++;
	}
	return weight>255 ? 255 : (uint8_t)weight;
}

/*
 * Function: referencedObject
 * --------------------------------
 * Modify DAAD flags to reference the las object used
 * in a logical sentence.
 *  
 * @param objno		Object ID.
 * @return			none.
 */
void referencedObject(uint8_t objno)
{
	flags[fCONum] = objno;
	flags[fCOLoc] = objects[objno].location;
	flags[fCOWei] = objects[objno].attribs.mask.weight;
	flags[fCOCon] = flags[fCOCon] & 0b01111111 | objects[objno].attribs.mask.isContainer << 7;
	flags[fCOWR]  = flags[fCOWR] & 0b01111111 | objects[objno].attribs.mask.isWareable << 7;
	flags[fCOAtt] = objects[objno].extAttr1;
	flags[fCOAtt+1] = objects[objno].extAttr2;
}
