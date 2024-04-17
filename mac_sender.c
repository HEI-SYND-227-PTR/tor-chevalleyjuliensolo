#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "main.h"


void MacSender(void *argument){
	// TODO 
	//on New_Token Event -> Prepare a token frame 
	
	osStatus_t nextMacSElem, nextMsg;
	
	struct queueMsg_t fromQueueAppObj, toQueuePhyFrame, fromQueueMacS, toQueueLCD;
	uint8_t srcAddrPos=0;
	uint8_t destAddrPos=1;
	uint8_t dataLenPos=2;
	uint8_t userDataPos=3;
	uint8_t statusPos=0;
			
	uint8_t dataSize=0;
	uint8_t totalSize = 0; 

	uint8_t* copyFrameSentPtr;
	
	uint8_t* phyPtr;
	
	for (;;){
		nextMacSElem = osMessageQueueGet( 	
			queue_macS_id,
			&fromQueueAppObj,
			NULL,
			osWaitForever);
		
		uint8_t *myFirstFrameBytePtr = fromQueueAppObj.anyPtr;
		//gTokenInterface.myAddress = MYADDRESS;

		
		switch (fromQueueAppObj.type){
			case NEW_TOKEN:
				phyPtr=osMemoryPoolAlloc(memPool, osWaitForever);
				toQueuePhyFrame.anyPtr=phyPtr;	
			
				for(uint8_t i=0; i<(TOKENSIZE-2); i++){
						if(i==0){
						(*phyPtr)=TOKEN_TAG;
						}
						else{
						(*phyPtr)=0x00;
						}
						phyPtr++;
					}
				toQueuePhyFrame.type = TO_PHY;
					
				osMessageQueuePut(queue_phyS_id, &toQueuePhyFrame, osPriorityNormal, osWaitForever);

				break;
			case START:
				//TODO
				break;
			case STOP:
				//TODO
				break;
			case DATA_IND :
				phyPtr=osMemoryPoolAlloc(memPool, osWaitForever);
										
				//char* charPtr = fromQueueAppObj.anyPtr;
				dataSize = strlen(myFirstFrameBytePtr);
				totalSize = dataSize + 4;
				statusPos = userDataPos+dataSize;
					
				phyPtr[srcAddrPos] = (gTokenInterface.myAddress << 3) | fromQueueAppObj.sapi;	
				phyPtr[destAddrPos] = (fromQueueAppObj.addr << 3) | fromQueueAppObj.sapi;	
				phyPtr[dataLenPos] = dataSize;
				memcpy(&phyPtr[userDataPos],myFirstFrameBytePtr,dataSize);

				// Calculate CheckSum 					
					uint8_t tempCheckSum=0;
					for(uint8_t i=0; i<(statusPos);i++){
						tempCheckSum+=phyPtr[i];
					}
					
					//Write CheckSum in the frame
					phyPtr[statusPos]=(tempCheckSum<<2);	

					toQueuePhyFrame.type = TO_PHY;
					toQueuePhyFrame.anyPtr = phyPtr;
					
					// make a copy before send !!!!!!!!!
					//uint8_t* copyPtr = osmemoryPoolAlloc(memPool, osWaitForever);
					//memcpy(copyPtr,phyPtr,totalSize);
					
					// free string sent by Chat/Time (fromQueueAppObj.anyPtr)
					osMemoryPoolFree(memPool, &fromQueueAppObj.anyPtr);
					osMessageQueuePut(queue_macSToken_id, &toQueuePhyFrame,osPriorityNormal, osWaitForever);

				break;
			case TOKEN:
		
				nextMsg = osMessageQueueGet(queue_macSToken_id,&fromQueueMacS,NULL,0);
			
				//fromQueueAppObj is of type Token, and has anyPtr pointing to the frame 0xFF....
				if(nextMsg==osOK){
								// make a copy before send !!!!!!!!!
					copyFrameSentPtr = osMemoryPoolAlloc(memPool, osWaitForever);
					memcpy(copyFrameSentPtr,fromQueueMacS.anyPtr,totalSize);
					osMessageQueuePut(queue_phyS_id, &fromQueueMacS,osPriorityNormal, osWaitForever);
				}
				else{
						
					//TODO Modify Sapis of my Station wrt gTokenInterface values
					//fromQueueAppObj.anyPtr[1+gTokenInterface.myAddress]
					osMessageQueuePut(queue_phyS_id, &fromQueueAppObj,osPriorityNormal, osWaitForever);
				}
				
					toQueueLCD.type=TOKEN_LIST;
					uint8_t i=0;
					//uint8_t *myStationPtr = fromQueueAppObj.anyPtr;
				if(gTokenInterface.connected == 1)
				{
					myFirstFrameBytePtr[gTokenInterface.myAddress+1] = (1<<TIME_SAPI) | (1<<CHAT_SAPI);
				}
				else
				{
					myFirstFrameBytePtr[gTokenInterface.myAddress+1] = (1<<TIME_SAPI);
				}
					for(i=0;i<15;i++){
					gTokenInterface.station_list[i]=(*(myFirstFrameBytePtr+i+1));
					}
					osMessageQueuePut(queue_lcd_id, &toQueueLCD, osPriorityNormal, osWaitForever);
				//TODO SEND BACK TOKEN ON THE RING
				//TODO ELSE SEND TOKEN_LIST / MAC_ERROR ? 
				break;
			case DATABACK:
				//free copyFrameSentPtr et queuePhyMsg.anyPtr
			
				//osMemoryPoolFree(memPool, &copyFrameSentPtr); //if everything is corectly recieved
			default:
				break;
		}
	}
	
}
