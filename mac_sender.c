#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "main.h"


void MacSender(void *argument)
{
	// TODO 
	//on New_Token Event -> Prepare a token frame 
	
	osStatus_t nextMacSElem, nextMsg;
	struct queueMsg_t queueMsg, queuePhyFrame, queueFrame, queueLCD;	

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
			&queueFrame,
			NULL,
			osWaitForever);
		//gTokenInterface.myAddress = MYADDRESS;

		
		switch ((queueFrame.type))
		{
			case NEW_TOKEN:
				phyPtr=osMemoryPoolAlloc(memPool, osWaitForever);
				
				for(uint8_t i=0; i<(TOKENSIZE-2); i++){
						if(i==0){
						(*phyPtr)=TOKEN_TAG;
						}
						else{
						(*phyPtr)=0x00;
						}
						phyPtr++;
					}
				queuePhyFrame.type = TO_PHY;
				queuePhyFrame.anyPtr=phyPtr;		
				osMessageQueuePut(queue_phyS_id, &queuePhyFrame, osPriorityNormal, osWaitForever);

				break;
			case START:
				//CHAT_SAPI
				break;
			case STOP:
				break;
			case DATA_IND :
				phyPtr=osMemoryPoolAlloc(memPool, osWaitForever);
										
				char * charPtr = queueFrame.anyPtr;
				dataSize = strlen(charPtr);
				totalSize = dataSize + 4;
				statusPos = userDataPos+dataSize;
					
				phyPtr[srcAddrPos] = (gTokenInterface.myAddress << 3) | queueFrame.sapi;	
				phyPtr[destAddrPos] = (queueFrame.addr << 3) | queueFrame.sapi;	
				phyPtr[dataLenPos] = dataSize;
				memcpy(&phyPtr[userDataPos],charPtr,dataSize);

				// Calculate CheckSum 					
					uint8_t tempCheckSum=0;
					for(uint8_t i=0; i<(statusPos);i++){
						tempCheckSum+=phyPtr[i];
					}
					
					//Write CheckSum in the frame
					phyPtr[statusPos]=(tempCheckSum<<2);	

					queuePhyFrame.type = TO_PHY;
					queuePhyFrame.anyPtr = phyPtr;
					
					// make a copy before send !!!!!!!!!
					//uint8_t* copyPtr = osmemoryPoolAlloc(memPool, osWaitForever);
					//memcpy(copyPtr,phyPtr,totalSize);
					
					// free string sent by Chat/Time (queueFrame.anyPtr)
					osMemoryPoolFree(memPool, &queueFrame.anyPtr);
					
					osMessageQueuePut(queue_macSToken_id, &queuePhyFrame,osPriorityNormal, osWaitForever);

				break;
			case TOKEN:
		
				nextMsg = osMessageQueueGet(queue_macSToken_id,&queueMsg,NULL,0);
			
				//queueFrame is of type Token, and has anyPtr pointing to the frame 0xFF....
				if(nextMsg==osOK){
								// make a copy before send !!!!!!!!!
					copyFrameSentPtr = osMemoryPoolAlloc(memPool, osWaitForever);
					memcpy(copyFrameSentPtr,queueMsg.anyPtr,totalSize);
					
					queueLCD.type=TOKEN_LIST;
					
					osMessageQueuePut(queue_lcd_id, &queueLCD, osPriorityNormal, osWaitForever);
					osMessageQueuePut(queue_phyS_id, &queueMsg,osPriorityNormal, osWaitForever);
				}
				else{

					//TODO Modify Sapis of my Station wrt gTokenInterface values
					//queueFrame.anyPtr[1+gTokenInterface.myAddress]
					osMessageQueuePut(queue_phyS_id, &queueFrame,osPriorityNormal, osWaitForever);
				}
				//TODO SEND BACK TOKEN ON THE RING
				//TODO ELSE SEND TOKEN_LIST / MAC_ERROR ? 
				break;
			case DATABACK:
				//free copyFrameSentPtr et queuePhyMsg.anyPtr
			osMemoryPoolFree(memPool, &copyFrameSentPtr); //if everything is corectly recieved
			default:
				break;
		}
	}
	
}
