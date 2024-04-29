#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "main.h"

#define SRCADDRPOS 0
#define DESTADDRPOS 1
#define USERDATALENPOS  2
#define USERDATAPOS 3

void MacSender(void *argument){
	// TODO 
	//on New_Token Event -> Prepare a token frame 
	
	osStatus_t nextMacSElem, nextMsg;
	struct queueMsg_t fromQueueMacWait;
	struct queueMsg_t fromAppQueue;
	
	

	
	typedef struct frame_{
		uint8_t* ptr;
		uint8_t statusPos;
		uint8_t userDataSize;
		uint8_t srcAddr;
		uint8_t destAddr;
		uint8_t totalFrameSize;
	}frame;
					
	frame destFrame;
	frame localCopyFrame;
	

	uint8_t chekSumCalc;
	uint8_t *tokenPtr;
	uint8_t currentStatus ;
//	string error;
	

	for (;;){
		nextMacSElem = osMessageQueueGet( 	
			queue_macS_id,
			&fromAppQueue,
			NULL,
			osWaitForever);
		
		if(nextMacSElem == osOK){
	//		nxtMsgCharPtr = fromAppQueue.anyPtr;
			switch (fromAppQueue.type){
			//DONE
			case NEW_TOKEN:
				tokenPtr=osMemoryPoolAlloc(memPool, osWaitForever);
					
				for(uint8_t i=0; i<(TOKENSIZE-2); i++){
						if(i==0){
							tokenPtr[i]=TOKEN_TAG;
						}
						else{
							tokenPtr[i]=0x00;
						}
				}

				fromAppQueue.type = TO_PHY;
				fromAppQueue.anyPtr=tokenPtr;
				osMessageQueuePut(queue_phyS_id, &fromAppQueue, osPriorityNormal, osWaitForever);
				break;
				//DONE
			case START:
				gTokenInterface.connected=1;
				break;
			//DONE
			case STOP:
				gTokenInterface.connected=0;
				break;
			//DONE
			case DATA_IND :

				destFrame.ptr = osMemoryPoolAlloc(memPool, osWaitForever);				
				destFrame.srcAddr=(gTokenInterface.myAddress << 3) | fromAppQueue.sapi;
				destFrame.ptr[SRCADDRPOS]=destFrame.srcAddr;
		
				destFrame.destAddr = (fromAppQueue.addr << 3) | fromAppQueue.sapi;
				destFrame.ptr[DESTADDRPOS]=destFrame.destAddr;
				
				destFrame.userDataSize=strlen(fromAppQueue.anyPtr);
				destFrame.ptr[USERDATALENPOS]=destFrame.userDataSize;

				destFrame.statusPos = USERDATAPOS+destFrame.userDataSize;
			
				destFrame.totalFrameSize=destFrame.userDataSize+4;//srcAddr + destAddr + length + status = 4
				memcpy(&destFrame.ptr[USERDATAPOS],fromAppQueue.anyPtr,destFrame.userDataSize+1);

			// Calculate CheckSum 					
				chekSumCalc=0;
				for(uint8_t i=0; i<destFrame.statusPos;i++){
					chekSumCalc+=destFrame.ptr[i];
				}
				
				//Write CheckSum in the frame
				destFrame.ptr[destFrame.statusPos]=(chekSumCalc<<2);	

				// free string sent by Chat/Time (fromAppQueue.anyPtr)
				osMemoryPoolFree(memPool, fromAppQueue.anyPtr);
				
				
				
				fromAppQueue.type = TO_PHY;
				fromAppQueue.anyPtr = destFrame.ptr;  

				osMessageQueuePut(queue_macWaitToken_id, &fromAppQueue,osPriorityNormal, osWaitForever);
				break;
					//DONE
			case TOKEN:
				//First update the list of connected stations
				tokenPtr = fromAppQueue.anyPtr;
				if(gTokenInterface.connected)
				{
					tokenPtr[gTokenInterface.myAddress+1] = (1<<TIME_SAPI) | (1<<CHAT_SAPI);//+1 because first char is 0xFF 
				}
				else
				{
					tokenPtr[gTokenInterface.myAddress+1] = (1<<TIME_SAPI)|(0<<CHAT_SAPI);//+1 because first char is 0xFF 
				}
				for(uint8_t i=0;i<(TOKENSIZE-4);i++){
					gTokenInterface.station_list[i]=(*(tokenPtr+i+1));//+1 because first char is 0xFF 
				}
				fromAppQueue.type=TOKEN_LIST;
				osMessageQueuePut(queue_lcd_id, &fromAppQueue, osPriorityNormal, osWaitForever);

				//Now we have the token, we can extract the next fully formed frame 
				//from the intermediate queue : queue_macWaitToken_id
				nextMsg = osMessageQueueGet(queue_macWaitToken_id,&fromQueueMacWait,NULL,0);
			
				//fromAppQueue.anyPtr / tokenPtr has the token frame
				//fromQueueMacWait.anyPtr is pointing on a frame ready to be sent (fully formed frame)
				if(nextMsg==osOK){
							
					// make a copy of the frame to be sent before sending the frame
					localCopyFrame.ptr=osMemoryPoolAlloc(memPool, osWaitForever);
					localCopyFrame.totalFrameSize=(((char*)fromQueueMacWait.anyPtr)[USERDATALENPOS]+4);//+4 correspond to srcAddr(1Byte), destAddr(1Byte), length(1Byte), status(1Byte)
					memcpy(localCopyFrame.ptr,fromQueueMacWait.anyPtr,localCopyFrame.totalFrameSize);
					localCopyFrame.srcAddr=localCopyFrame.ptr[SRCADDRPOS];
					localCopyFrame.destAddr=localCopyFrame.ptr[DESTADDRPOS];
					localCopyFrame.userDataSize=localCopyFrame.ptr[USERDATALENPOS];
					localCopyFrame.statusPos=localCopyFrame.ptr[USERDATAPOS+localCopyFrame.userDataSize];
					osMessageQueuePut(queue_phyS_id, &fromQueueMacWait,osPriorityNormal, osWaitForever);
				}
				else{
					//TOKEN is sent back to the ring
					osMessageQueuePut(queue_phyS_id, &fromAppQueue,osPriorityNormal, osWaitForever);
					tokenPtr=NULL;
				}
				


				break;
			case DATABACK:
				//free copyFrameSentPtr et queuePhyMsg.anyPtr
				currentStatus = ((char*)fromAppQueue.anyPtr)[((char*)fromAppQueue.anyPtr)[USERDATALENPOS]+3] & 0x03;
				if(currentStatus  == 0x03){//Sapi is good (read=1), and CRC is Ok
					fromAppQueue.type=TO_PHY;
					
					osMemoryPoolFree(memPool, fromAppQueue.anyPtr);
					fromAppQueue.anyPtr=tokenPtr;	
					osMessageQueuePut(queue_phyS_id, &fromAppQueue,osPriorityNormal, osWaitForever);	
					osMemoryPoolFree(memPool, localCopyFrame.ptr);
				}
				else if(currentStatus==0x01){//No sapi to read (read=0), but CRC is correct (ack=1) => Do not send message again, send MAC_ERROR 
					osMemoryPoolFree(memPool, localCopyFrame.ptr);					
					fromAppQueue.addr=((char*)fromAppQueue.anyPtr)[DESTADDRPOS];
										//sEND THE tOKEN BACK
					fromAppQueue.type=TO_PHY;
					osMemoryPoolFree(memPool, fromAppQueue.anyPtr);
					fromAppQueue.anyPtr=tokenPtr;	
					osMessageQueuePut(queue_phyS_id, &fromAppQueue,osPriorityNormal, osWaitForever);
					
					//Prepare error message
					fromAppQueue.type=MAC_ERROR;
					fromAppQueue.anyPtr =osMemoryPoolAlloc(memPool, osWaitForever);
					sprintf(fromAppQueue.anyPtr, "Mac Error \n Sapi %d is incorrect !\n", (fromAppQueue.addr & 0x07));
					osMessageQueuePut(queue_lcd_id, &fromAppQueue,osPriorityNormal, osWaitForever);		
					
				}
				else if (currentStatus==0x02){//read=1,  but CRC is not correct (ack=0) => Send message again 
					fromAppQueue.type=TO_PHY;
					// make a copy of the frame to be sent before sending the frame
					memcpy(fromAppQueue.anyPtr, localCopyFrame.ptr,localCopyFrame.totalFrameSize);
					osMessageQueuePut(queue_phyS_id, &fromAppQueue,osPriorityNormal, osWaitForever);			
				}
				else{//currentStatus == 0x00 => message not read nor acked => send MAC_ERROR (station not connected) 
					osMemoryPoolFree(memPool, localCopyFrame.ptr);
					fromAppQueue.addr=(((char*)fromAppQueue.anyPtr)[DESTADDRPOS] & 0x78)>>3;
					
								//SEND THE tOKEN BACK
					fromAppQueue.type=TO_PHY;
					osMemoryPoolFree(memPool, fromAppQueue.anyPtr);
					fromAppQueue.anyPtr=tokenPtr;	
					osMessageQueuePut(queue_phyS_id, &fromAppQueue,osPriorityNormal, osWaitForever);


					fromAppQueue.type=MAC_ERROR;
					// make a copy of the string part of the frame before sending the string
					//memcpy(fromAppQueue.anyPtr, &localCopyFrame.ptr[USERDATAPOS],localCopyFrame.userDataSize);
					fromAppQueue.anyPtr =osMemoryPoolAlloc(memPool, osWaitForever);
					sprintf(fromAppQueue.anyPtr, "Mac Error \n station number %d don't answer \n", fromAppQueue.addr+1);	
					osMessageQueuePut(queue_lcd_id, &fromAppQueue,osPriorityNormal, osWaitForever);		
				}
				break;
			default:
				break;
		}
		}
	}
}


/*
#include "stm32f7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "main.h"


void MacSender(void *argument){
	// TODO 
	//on New_Token Event -> Prepare a token frame 
	
	osStatus_t nextMacSElem, nextMsg;
	
	struct queueMsg_t fromQueueAppObj, toQueuePhyFrame, fromQueueMacWait, toQueueLCD;
	
	
	uint8_t srcAddrPos=0;
	uint8_t destAddrPos=1;
	uint8_t dataLenPos=2;
	uint8_t userDataPos=3;
	uint8_t statusPos=0;
	uint8_t * tokenPtr;		
	uint8_t dataSize=0;
	uint8_t totalSize = 0; 

	uint8_t* copyFrameSentPtr;
	
	uint8_t *toPhyPtr, *tokPtr;
	
	for (;;){
		nextMacSElem = osMessageQueueGet( 	
			queue_macS_id,
			&fromQueueAppObj,
			NULL,
			osWaitForever);
		
		if(nextMacSElem ==osOK){
			char *myCharMacSPtr = fromQueueAppObj.anyPtr;
			switch (fromQueueAppObj.type){
			//DONE
			case NEW_TOKEN:
				tokPtr=osMemoryPoolAlloc(memPool, osWaitForever);
					
				for(uint8_t i=0; i<(TOKENSIZE-2); i++){
						if(i==0){
						tokPtr[i]=TOKEN_TAG;
						}
						else{
						tokPtr[i]=0x00;
						}
				}

				toQueuePhyFrame.type = TO_PHY;
				toQueuePhyFrame.anyPtr=tokPtr;
				osMessageQueuePut(queue_phyS_id, &toQueuePhyFrame, osPriorityNormal, osWaitForever);
				break;
				//DONE
			case START:
				gTokenInterface.connected=1;
				break;
			//DONE
			case STOP:
				gTokenInterface.connected=0;
				break;
			//DONE
			case DATA_IND :
				toPhyPtr=osMemoryPoolAlloc(memPool, osWaitForever);
										
				//char* charPtr = fromQueueAppObj.anyPtr;
				dataSize = strlen(myCharMacSPtr);
				totalSize = dataSize + 4;
				statusPos = userDataPos+dataSize;
					
				toPhyPtr[srcAddrPos] = (gTokenInterface.myAddress << 3) | fromQueueAppObj.sapi;	
				toPhyPtr[destAddrPos] = (fromQueueAppObj.addr << 3) | fromQueueAppObj.sapi;	
				toPhyPtr[dataLenPos] = dataSize;
				memcpy(&toPhyPtr[userDataPos],myCharMacSPtr,dataSize);

				// Calculate CheckSum 					
					uint8_t tempCheckSum=0;
					for(uint8_t i=0; i<statusPos;i++){
						tempCheckSum+=toPhyPtr[i];
					}
					
					//Write CheckSum in the frame
					toPhyPtr[statusPos]=(tempCheckSum<<2);	

					toQueuePhyFrame.type = TO_PHY;
					toQueuePhyFrame.anyPtr = toPhyPtr;

					// free string sent by Chat/Time (fromQueueAppObj.anyPtr)
					osMemoryPoolFree(memPool, fromQueueAppObj.anyPtr);
					osMessageQueuePut(queue_macWaitToken_id, &toQueuePhyFrame,osPriorityNormal, osWaitForever);
				break;
					//DONE
			case TOKEN:
				//First update the list of connected stations
				toQueueLCD.type=TOKEN_LIST;
				if(gTokenInterface.connected)
				{
					myCharMacSPtr[gTokenInterface.myAddress+1] = (1<<TIME_SAPI) | (1<<CHAT_SAPI);
				}
				else
				{
					myCharMacSPtr[gTokenInterface.myAddress+1] = (1<<TIME_SAPI)|(0<<CHAT_SAPI);
				}
				uint8_t i=0;
				for(i=0;i<(TOKENSIZE-4);i++){
					gTokenInterface.station_list[i]=(*(myCharMacSPtr+i+1));
				}
					osMessageQueuePut(queue_lcd_id, &toQueueLCD, osPriorityNormal, osWaitForever);
			
				nextMsg = osMessageQueueGet(queue_macWaitToken_id,&fromQueueMacWait,NULL,0);
			
				//fromQueueAppObj is of type Token, and has anyPtr pointing to the frame 0xFF....
				if(nextMsg==osOK){
					tokenPtr = fromQueueAppObj.anyPtr;
								// make a copy before send !!!!!!!!!
					copyFrameSentPtr = osMemoryPoolAlloc(memPool, osWaitForever);
			//		copyFrameSentPtr=fromQueueMacWait.anyPtr;
					totalSize = (uint8_t *)fromQueueMacWait.anyPtr[2] + 4; // pour les bricoleurs, enfin un peu moins
					memcpy(copyFrameSentPtr,fromQueueMacWait.anyPtr,totalSize);
					osMessageQueuePut(queue_phyS_id, &fromQueueMacWait,osPriorityNormal, osWaitForever);
				}
				else{
					//TOKEN is sent back to the ring
					osMessageQueuePut(queue_phyS_id, &fromQueueAppObj,osPriorityNormal, osWaitForever);
				}
				


				break;
			case DATABACK:
				//free copyFrameSentPtr et queuePhyMsg.anyPtr
				if((myCharMacSPtr[myCharMacSPtr[dataLenPos]+3] & 0x03)== 0x03){
				toQueuePhyFrame.type=TO_PHY;
				toQueuePhyFrame.anyPtr=tokenPtr;	
				osMessageQueuePut(queue_phyS_id, &toQueuePhyFrame,osPriorityNormal, osWaitForever);	
				osMemoryPoolFree(memPool, copyFrameSentPtr);
				osMemoryPoolFree(memPool, fromQueueAppObj.anyPtr);
				}
			
				
				//osMemoryPoolFree(memPool, &copyFrameSentPtr); //if everything is corectly recieved
			default:
				break;
		}
		}
	}
}
*/
