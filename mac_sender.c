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
	
	osStatus_t nextMacSElem, nextMsg, nextTempMsg;
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
	
			switch (fromAppQueue.type){
				
			// We recieve the instruction to generate a NEW TOKEN
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
				//Connect the station when a START Command is recieved
			case START:
				gTokenInterface.connected=1;
				break;
			// When a STOP command is recieved -> disconnect the station
			case STOP:
				gTokenInterface.connected=0;
				break;
			
			// When the message contains a DATA_IND -> Prepare the frame and put it on a temporary queue
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

				//Put the frame in the temporary queue
				nextTempMsg = osMessageQueuePut(queue_macWaitToken_id, &fromAppQueue,osPriorityNormal, 0);
				if(nextTempMsg != osOK){
					osMemoryPoolFree(memPool, fromAppQueue.anyPtr);
					CheckRetCode(nextTempMsg,__LINE__,__FILE__,CONTINUE);					
				}
				break;
					//We Have the TOKEN at hand, so we can send the next message from the temporary queue. 
					//The TOKEN is kept (not given back to the network) until we recieve a DATABACK with read and ack = 1
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
					CheckRetCode(nextMsg,__LINE__,__FILE__,CONTINUE);
					//TOKEN is sent back to the ring
					osMessageQueuePut(queue_phyS_id, &fromAppQueue,osPriorityNormal, osWaitForever);
					tokenPtr=NULL;
				}
				


				break;
			case DATABACK:
				//free copyFrameSentPtr et queuePhyMsg.anyPtr
				currentStatus = ((char*)fromAppQueue.anyPtr)[((char*)fromAppQueue.anyPtr)[USERDATALENPOS]+3] & 0x03;
			  //currentStatus = fromAppQueue.sapi;
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
		} else {
			CheckRetCode(nextMacSElem,__LINE__,__FILE__,CONTINUE);
		}
	}
}

